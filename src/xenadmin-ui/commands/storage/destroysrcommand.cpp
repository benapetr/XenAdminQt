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

#include "destroysrcommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib.h"
#include "xencache.h"
#include "xen/actions/sr/destroysraction.h"
#include <QMessageBox>
#include <QDebug>

DestroySRCommand::DestroySRCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool DestroySRCommand::CanRun() const
{
    QString srRef = getSelectedSRRef();
    if (srRef.isEmpty())
        return false;

    return canSRBeDestroyed();
}

void DestroySRCommand::Run()
{
    QString srRef = getSelectedSRRef();
    QString srName = getSelectedSRName();

    if (srRef.isEmpty())
        return;

    // Show critical warning dialog (double confirmation)
    QMessageBox msgBox(mainWindow());
    msgBox.setWindowTitle("Destroy Storage Repository");
    msgBox.setText(QString("WARNING: Are you about to DESTROY storage repository '%1'!").arg(srName));
    msgBox.setInformativeText(
        "This will PERMANENTLY DELETE all data on the storage repository!\n\n"
        "This action CANNOT be undone!\n\n"
        "Are you absolutely sure you want to continue?");
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    int ret = msgBox.exec();

    if (ret != QMessageBox::Yes)
    {
        return;
    }

    qDebug() << "DestroySRCommand: Destroying SR" << srName << "(" << srRef << ")";

    // Create and run destroy action
    DestroySrAction* action = new DestroySrAction(
        mainWindow()->xenLib()->getConnection(),
        srRef,
        srName,
        this);

    // Register with OperationManager for history tracking
    OperationManager::instance()->registerOperation(action);

    // Connect completion signal for cleanup and status update
    connect(action, &AsyncOperation::completed, [this, srName, action]() {
        if (action->state() == AsyncOperation::Completed && !action->isFailed())
        {
            mainWindow()->showStatusMessage(QString("Successfully destroyed SR '%1'").arg(srName), 5000);
        } else
        {
            QMessageBox::warning(
                mainWindow(),
                "Destroy SR Failed",
                QString("Failed to destroy SR '%1'.\n\n%2").arg(srName, action->errorMessage()));
        }
        // Auto-delete when complete
        action->deleteLater();
    });

    // Run action asynchronously
    action->runAsync();
}

QString DestroySRCommand::MenuText() const
{
    return "Destroy Storage Repository";
}

QString DestroySRCommand::getSelectedSRRef() const
{
    QString objectType = getSelectedObjectType();
    if (objectType != "sr")
        return QString();

    return getSelectedObjectRef();
}

QString DestroySRCommand::getSelectedSRName() const
{
    QString srRef = getSelectedSRRef();
    if (srRef.isEmpty())
        return QString();

    if (!mainWindow()->xenLib())
        return QString();

    XenCache* cache = mainWindow()->xenLib()->getCache();
    if (!cache)
        return QString();

    QVariantMap srData = cache->ResolveObjectData("sr", srRef);
    return srData.value("name_label", "").toString();
}

bool DestroySRCommand::canSRBeDestroyed() const
{
    QString srRef = getSelectedSRRef();
    if (srRef.isEmpty())
        return false;

    if (!mainWindow()->xenLib())
        return false;

    XenCache* cache = mainWindow()->xenLib()->getCache();
    if (!cache)
        return false;

    QVariantMap srData = cache->ResolveObjectData("sr", srRef);

    // Cannot destroy if SR has VDIs (contains data)
    QVariantList vdis = srData.value("VDIs", QVariantList()).toList();
    if (!vdis.isEmpty())
        return false;

    // Cannot destroy shared SRs (must be detached first)
    bool shared = srData.value("shared", false).toBool();
    if (shared)
    {
        QVariantList pbds = srData.value("PBDs", QVariantList()).toList();
        for (const QVariant& pbdRefVar : pbds)
        {
            QString pbdRef = pbdRefVar.toString();
            QVariantMap pbdData = cache->ResolveObjectData("pbd", pbdRef);

            if (pbdData.value("currently_attached", false).toBool())
                return false; // SR is still attached
        }
    }

    // Cannot destroy ISO or tools SR
    QString contentType = srData.value("content_type", "").toString();
    if (contentType == "iso" || contentType == "tools")
        return false;

    return true;
}
