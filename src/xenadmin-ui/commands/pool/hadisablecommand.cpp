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

#include "hadisablecommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib.h"
#include "xen/connection.h"
#include "xen/actions/pool/disablehaaction.h"
#include "xencache.h"
#include <QMessageBox>

HADisableCommand::HADisableCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool HADisableCommand::canRun() const
{
    QString poolRef = this->getSelectedPoolRef();
    if (poolRef.isEmpty())
        return false;

    // Can disable HA if pool is connected and HA is enabled
    return this->isPoolConnected() && this->isHAEnabled();
}

void HADisableCommand::run()
{
    QString poolRef = this->getSelectedPoolRef();
    QString poolName = this->getSelectedPoolName();

    if (poolRef.isEmpty())
        return;

    // Show confirmation dialog
    int ret = QMessageBox::question(this->mainWindow(), "Disable High Availability",
                                    QString("Are you sure you want to disable High Availability for pool '%1'?\n\n"
                                            "VMs will no longer restart automatically if a host fails.")
                                        .arg(poolName),
                                    QMessageBox::Yes | QMessageBox::No);

    if (ret != QMessageBox::Yes)
        return;

    // Create and run the DisableHAAction
    XenConnection* connection = this->mainWindow()->xenLib()->getConnection();
    if (!connection)
    {
        QMessageBox::critical(this->mainWindow(), "Error", "No active connection.");
        return;
    }

    DisableHAAction* action = new DisableHAAction(
        connection,
        poolRef,
        this);

    // Register with OperationManager for history tracking
    OperationManager::instance()->registerOperation(action);

    // Connect completion signals
    connect(action, &AsyncOperation::completed, [this, poolName, action]() {
        this->mainWindow()->showStatusMessage(
            QString("High Availability disabled for pool '%1'").arg(poolName), 5000);
        action->deleteLater();
    });

    connect(action, &AsyncOperation::failed, [this, poolName, action](const QString& error) {
        QMessageBox::critical(this->mainWindow(), "Disable HA Failed",
                              QString("Failed to disable HA for pool '%1':\n%2")
                                  .arg(poolName, error));
        action->deleteLater();
    });

    // Run action asynchronously
    action->runAsync();
    this->mainWindow()->showStatusMessage(QString("Disabling HA on pool '%1'...").arg(poolName), 0);
}

QString HADisableCommand::menuText() const
{
    return "Disable";
}

QString HADisableCommand::getSelectedPoolRef() const
{
    QString objectType = this->getSelectedObjectType();
    if (objectType != "pool")
        return QString();

    return this->getSelectedObjectRef();
}

QString HADisableCommand::getSelectedPoolName() const
{
    QString poolRef = this->getSelectedPoolRef();
    if (poolRef.isEmpty())
        return QString();

    if (!this->mainWindow()->xenLib())
        return QString();

    XenCache* cache = this->mainWindow()->xenLib()->getCache();
    if (!cache)
        return QString();

    QVariantMap poolData = cache->resolve("pool", poolRef);
    return poolData.value("name_label", "").toString();
}

bool HADisableCommand::isPoolConnected() const
{
    if (!this->mainWindow()->xenLib())
        return false;

    XenConnection* conn = this->mainWindow()->xenLib()->getConnection();
    return conn && conn->isConnected();
}

bool HADisableCommand::isHAEnabled() const
{
    QString poolRef = this->getSelectedPoolRef();
    if (poolRef.isEmpty())
        return false;

    if (!this->mainWindow()->xenLib())
        return false;

    XenCache* cache = this->mainWindow()->xenLib()->getCache();
    if (!cache)
        return false;

    QVariantMap poolData = cache->resolve("pool", poolRef);
    return poolData.value("ha_enabled", false).toBool();
}
