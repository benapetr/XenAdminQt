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

#include "deletepoolcommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib.h"
#include "xen/connection.h"
#include "xencache.h"
#include "xen/actions/pool/destroypoolaction.h"
#include <QMessageBox>
#include <QDebug>

DeletePoolCommand::DeletePoolCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool DeletePoolCommand::canRun() const
{
    QString poolRef = this->getSelectedPoolRef();
    if (poolRef.isEmpty())
        return false;

    // Can delete pool if connected and single host
    return this->isPoolConnected() && !this->hasMultipleHosts();
}

void DeletePoolCommand::run()
{
    QString poolRef = this->getSelectedPoolRef();

    if (!this->mainWindow()->xenLib())
        return;

    XenCache* cache = this->mainWindow()->xenLib()->getCache();
    if (!cache)
        return;

    QVariantMap poolData = cache->resolve("pool", poolRef);
    QString poolName = poolData.value("name_label", "").toString();

    if (poolRef.isEmpty())
        return;

    // Check if HA is enabled or enabling
    bool haEnabled = poolData.value("ha_enabled", false).toBool();
    QVariantMap currentOps = poolData.value("current_operations", QVariantMap()).toMap();

    bool haEnabling = false;
    for (const QVariant& opVar : currentOps.values())
    {
        QString op = opVar.toString();
        if (op == "ha_enable")
        {
            haEnabling = true;
            break;
        }
    }

    if (haEnabling)
    {
        QMessageBox::warning(
            this->mainWindow(),
            "Cannot Delete Pool",
            QString("Cannot delete pool '%1' because High Availability is currently being enabled.\n\n"
                    "Please wait for the operation to complete, then disable HA before deleting the pool.")
                .arg(poolName));
        return;
    }

    if (haEnabled)
    {
        QMessageBox::warning(
            this->mainWindow(),
            "Cannot Delete Pool",
            QString("Cannot delete pool '%1' because High Availability is enabled.\n\n"
                    "You must disable HA before deleting the pool.")
                .arg(poolName));
        return;
    }

    // Check if pool has multiple hosts
    if (this->hasMultipleHosts())
    {
        QMessageBox::warning(
            this->mainWindow(),
            "Cannot Delete Pool",
            QString("Pool '%1' contains multiple servers.\n\n"
                    "You must eject all servers except the coordinator before deleting the pool.\n\n"
                    "To eject servers, right-click on each non-coordinator server and select 'Eject from Pool'.")
                .arg(poolName));
        return;
    }

    // Show confirmation dialog
    QMessageBox msgBox(this->mainWindow());
    msgBox.setWindowTitle("Delete Pool");
    msgBox.setText(QString("Are you sure you want to delete pool '%1'?").arg(poolName));
    msgBox.setInformativeText("This will convert the pool back to a standalone server. "
                              "The server configuration will remain unchanged.");
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    int ret = msgBox.exec();

    if (ret != QMessageBox::Yes)
        return;

    qDebug() << "DeletePoolCommand: Deleting pool" << poolName << "(" << poolRef << ")";

    // Create and run destroy pool action
    DestroyPoolAction* action = new DestroyPoolAction(
        this->mainWindow()->xenLib()->getConnection(),
        poolRef,
        this);

    // Register with OperationManager for history tracking
    OperationManager::instance()->registerOperation(action);

    // Connect completion signal for cleanup and status update
    connect(action, &AsyncOperation::completed, [this, poolName, action]() {
        if (action->state() == AsyncOperation::Completed && !action->isFailed())
        {
            this->mainWindow()->showStatusMessage(QString("Successfully deleted pool '%1'").arg(poolName), 5000);
        } else
        {
            QMessageBox::warning(
                this->mainWindow(),
                "Delete Pool Failed",
                QString("Failed to delete pool '%1'.\n\n%2")
                    .arg(poolName, action->errorMessage()));
        }
        // Auto-delete when complete
        action->deleteLater();
    });

    // Run action asynchronously
    action->runAsync();
}

QString DeletePoolCommand::menuText() const
{
    return "Delete Pool";
}

QString DeletePoolCommand::getSelectedPoolRef() const
{
    QString objectType = this->getSelectedObjectType();
    if (objectType != "pool")
        return QString();

    return this->getSelectedObjectRef();
}

bool DeletePoolCommand::isPoolConnected() const
{
    if (!this->mainWindow()->xenLib())
        return false;

    XenConnection* conn = this->mainWindow()->xenLib()->getConnection();
    return conn && conn->isConnected();
}

bool DeletePoolCommand::hasMultipleHosts() const
{
    if (!this->mainWindow()->xenLib())
        return false;

    XenCache* cache = this->mainWindow()->xenLib()->getCache();
    if (!cache)
        return false;

    // Get all hosts
    QList<QPair<QString, QString>> allObjects = cache->getAllObjects();
    int hostCount = 0;
    for (const auto& obj : allObjects)
    {
        if (obj.first == "host")
            hostCount++;
    }

    return hostCount > 1;
}
