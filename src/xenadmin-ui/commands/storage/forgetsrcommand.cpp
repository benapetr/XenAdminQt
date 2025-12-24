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

#include "forgetsrcommand.h"
#include "../../../xenlib/xen/actions/sr/forgetsraction.h"
#include "../../../xenlib/xen/sr.h"
#include "../../../xenlib/xencache.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include <QMessageBox>
#include <QDebug>

ForgetSRCommand::ForgetSRCommand(MainWindow* mainWindow, QObject* parent)
    : SRCommand(mainWindow, parent)
{
}

bool ForgetSRCommand::CanRun() const
{
    QSharedPointer<SR> sr = this->getSR();
    if (!sr)
    {
        return false;
    }

    XenCache* cache = sr->connection()->getCache();
    QVariantMap srData = sr->data();

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
                    qDebug() << "ForgetSRCommand: SR has running VM" << vmRef;
                    return false;
                }
            }
        }
    }

    // Check if SR allows forget operation
    QVariantList allowedOps = srData.value("allowed_operations").toList();
    for (const QVariant& op : allowedOps)
    {
        if (op.toString() == "forget")
        {
            return true;
        }
    }

    qDebug() << "ForgetSRCommand: SR doesn't allow 'forget' operation";
    return false;
}

void ForgetSRCommand::Run()
{
    if (!this->CanRun())
    {
        qWarning() << "ForgetSRCommand: Cannot run";
        return;
    }

    QSharedPointer<SR> sr = this->getSR();
    if (!sr)
        return;

    QString srRef = sr->opaqueRef();
    QString srName = sr->nameLabel();

    if (srName.isEmpty())
    {
        srName = srRef;
    }

    // Show confirmation dialog
    QMessageBox msgBox(this->mainWindow());
    msgBox.setWindowTitle("Forget Storage Repository");
    msgBox.setText(QString("Are you sure you want to forget SR '%1'?").arg(srName));
    msgBox.setInformativeText("This will remove the SR from the XenServer database.\n"
                              "Backend storage will NOT be deleted, and the SR can be re-introduced later.\n\n"
                              "WARNING: You should only forget SRs that were created with XenCenter.");
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    int ret = msgBox.exec();

    if (ret != QMessageBox::Yes)
    {
        return;
    }

    qDebug() << "ForgetSRCommand: Forgetting SR" << srName << "(" << srRef << ")";

    // Get connection from SR object for multi-connection support
    XenConnection* conn = sr->connection();
    if (!conn || !conn->isConnected())
    {
        QMessageBox::warning(this->mainWindow(), "Not Connected",
                             "Not connected to XenServer");
        return;
    }

    // Create and run forget action
    ForgetSrAction* action = new ForgetSrAction(
        conn,
        srRef,
        srName,
        this);

    // Register with OperationManager for history tracking
    OperationManager::instance()->registerOperation(action);

    // Connect completion signal for cleanup and status update
    connect(action, &AsyncOperation::completed, [this, srName, action]() {
        if (action->state() == AsyncOperation::Completed && !action->isFailed())
        {
            this->mainWindow()->showStatusMessage(QString("Successfully forgotten SR '%1'").arg(srName), 5000);
        } else
        {
            QMessageBox::warning(
                this->mainWindow(),
                "Forget SR Failed",
                QString("Failed to forget SR '%1'").arg(srName));
        }
        // Auto-delete when complete
        action->deleteLater();
    });

    // Run action asynchronously
    action->runAsync();
}
