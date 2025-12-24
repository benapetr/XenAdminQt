/*
 * Copyright (c) 2025, Petr Bena <petr@bena.rocks>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "detachsrcommand.h"
#include "xen/actions/sr/detachsraction.h"
#include "xen/sr.h"
#include "xencache.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include <QMessageBox>
#include <QDebug>

DetachSRCommand::DetachSRCommand(MainWindow* mainWindow, QObject* parent)
    : SRCommand(mainWindow, parent)
{
}

void DetachSRCommand::setTargetSR(const QString& srRef)
{
    this->m_overrideSRRef = srRef;
}

bool DetachSRCommand::CanRun() const
{
    QString srRef = this->currentSR();
    if (srRef.isEmpty())
    {
        return false;
    }

    QSharedPointer<SR> sr = this->getSR();
    if (!sr)
    {
        return false;
    }

    XenCache* cache = sr->connection()->getCache();

    QVariantMap srData = sr->data();

    // Check if SR is already detached
    QVariantList pbds = srData.value("PBDs").toList();
    bool hasAttachedPBD = false;

    for (const QVariant& pbdVar : pbds)
    {
        QString pbdRef = pbdVar.toString();
        QVariantMap pbdData = cache->ResolveObjectData("pbd", pbdRef);
        if (pbdData.value("currently_attached").toBool())
        {
            hasAttachedPBD = true;
            break;
        }
    }

    if (!hasAttachedPBD)
    {
        qDebug() << "DetachSRCommand: SR" << srRef << "is already detached";
        return false;
    }

    // Check if SR has running VMs
    QVariantList vdis = srData.value("VDIs").toList();
    for (const QVariant& vdiVar : vdis)
    {
        QString vdiRef = vdiVar.toString();
        QVariantMap vdiData = cache->ResolveObjectData("vdi", vdiRef);
        QVariantList vbds = vdiData.value("VBDs").toList();

        for (const QVariant& vbdVar : vbds)
        {
            QString vbdRef = vbdVar.toString();
            QVariantMap vbdData = cache->ResolveObjectData("vbd", vbdRef);
            QString vmRef = vbdData.value("VM").toString();

            if (!vmRef.isEmpty())
            {
                QVariantMap vmData = cache->ResolveObjectData("vm", vmRef);
                QString powerState = vmData.value("power_state").toString();

                if (powerState == "Running" || powerState == "Paused")
                {
                    qDebug() << "DetachSRCommand: SR has running VM" << vmRef;
                    return false;
                }
            }
        }
    }

    return true;
}

void DetachSRCommand::Run()
{
    QString srRef = this->currentSR();
    if (srRef.isEmpty())
    {
        qWarning() << "DetachSRCommand: Cannot run";
        return;
    }

    QSharedPointer<SR> sr = this->getSR();
    if (!sr)
    {
        return;
    }

    QString srName = sr->nameLabel();
    if (srName.isEmpty())
    {
        srName = srRef;
    }

    // Show confirmation dialog
    QMessageBox msgBox(this->mainWindow());
    msgBox.setWindowTitle("Detach Storage Repository");
    msgBox.setText(QString("Are you sure you want to detach SR '%1'?").arg(srName));
    msgBox.setInformativeText("This will disconnect the storage repository from all hosts in the pool.\n"
                              "No data will be deleted, and the SR can be re-attached later.");
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    int ret = msgBox.exec();

    if (ret != QMessageBox::Yes)
    {
        return;
    }

    qDebug() << "DetachSRCommand: Detaching SR" << srName << "(" << srRef << ")";

    // Get connection from SR object for multi-connection support
    XenConnection* conn = sr->connection();
    if (!conn || !conn->isConnected())
    {
        QMessageBox::warning(this->mainWindow(), "Not Connected",
                             "Not connected to XenServer");
        return;
    }

    // Create and run action
    DetachSrAction* action = new DetachSrAction(
        conn,
        srRef,
        srName,
        false, // Don't destroy PBDs, just unplug
        this);

    // Register with OperationManager for history tracking
    OperationManager::instance()->registerOperation(action);

    // Connect completion signal for cleanup and status update
    connect(action, &AsyncOperation::completed, [this, srName, action]() {
        if (action->state() == AsyncOperation::Completed && !action->isFailed())
        {
            this->mainWindow()->showStatusMessage(QString("Successfully detached SR '%1'").arg(srName), 5000);
        } else
        {
            QMessageBox::warning(
                this->mainWindow(),
                "Detach SR Failed",
                QString("Failed to detach SR '%1'").arg(srName));
        }
        // Auto-delete when complete
        action->deleteLater();
    });

    // Run action asynchronously
    action->runAsync();
}

QString DetachSRCommand::currentSR() const
{
    if (!this->m_overrideSRRef.isEmpty())
    {
        return this->m_overrideSRRef;
    }

    if (this->getSelectedObjectType() != "sr")
    {
        return QString();
    }

    return this->getSelectedObjectRef();
}
