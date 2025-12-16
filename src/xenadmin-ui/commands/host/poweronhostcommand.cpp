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

#include "poweronhostcommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib.h"
#include "xencache.h"
#include "xen/connection.h"
#include <QMessageBox>

PowerOnHostCommand::PowerOnHostCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool PowerOnHostCommand::canRun() const
{
    // Matches C# PowerOnHostCommand.CanRunCore() logic
    QString hostRef = this->getSelectedHostRef();
    if (hostRef.isEmpty())
        return false;

    return this->canPowerOn(hostRef);
}

void PowerOnHostCommand::run()
{
    // Matches C# PowerOnHostCommand.RunCore() logic
    QString hostRef = this->getSelectedHostRef();
    QString hostName = this->getSelectedHostName();

    if (hostRef.isEmpty() || hostName.isEmpty())
        return;

    // Get host data from cache
    QVariantMap hostData = this->mainWindow()->xenLib()->getCache()->resolve("host", hostRef);
    if (hostData.isEmpty())
        return;

    QString powerOnMode = hostData.value("power_on_mode", "").toString();

    // Check if power_on_mode is set (matches C# GetCantRunReasonCore logic)
    if (powerOnMode.isEmpty())
    {
        QMessageBox::warning(this->mainWindow(), "Cannot Power On Host",
                             QString("Cannot power on host '%1' because its power-on mode is not set.\n\n"
                                     "Configure the host's management interface in the host properties.")
                                 .arg(hostName));
        return;
    }

    // Get XenConnection from XenLib
    XenConnection* conn = this->mainWindow()->xenLib()->getConnection();
    if (!conn || !conn->isConnected())
    {
        QMessageBox::warning(this->mainWindow(), "Not Connected",
                             "Not connected to XenServer");
        return;
    }

    // TODO: Create HostPowerOnAction when implemented
    // For now, call the XenAPI directly
    // Matches C# HostPowerOnAction(host)

    QMessageBox::information(this->mainWindow(), "Power On Host",
                             QString("Power on host '%1' will be implemented when HostPowerOnAction is added.\n\n"
                                     "This will use %2 to power on the host.")
                                 .arg(hostName)
                                 .arg(powerOnMode.isEmpty() ? "the configured power-on method" : powerOnMode));

    /* When HostPowerOnAction is implemented:
    HostPowerOnAction* action = new HostPowerOnAction(conn, hostRef, this->mainWindow());

    // Register with OperationManager for history tracking
    OperationManager::instance()->registerOperation(action);

    // Connect completion signal for cleanup
    connect(action, &AsyncOperation::completed, this, [action]() {
        action->deleteLater();
    });

    // Run action asynchronously
    action->runAsync();
    */
}

QString PowerOnHostCommand::menuText() const
{
    // Matches C# Messages.MAINWINDOW_POWER_ON
    return "Power On";
}

QString PowerOnHostCommand::getSelectedHostRef() const
{
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    QString objectType = this->getSelectedObjectType();
    if (objectType != "host")
        return QString();

    return this->getSelectedObjectRef();
}

QString PowerOnHostCommand::getSelectedHostName() const
{
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    QString objectType = this->getSelectedObjectType();
    if (objectType != "host")
        return QString();

    return item->text(0);
}

bool PowerOnHostCommand::canPowerOn(const QString& hostRef) const
{
    // Matches C# PowerOnHostCommand.CanRun() logic:
    // return host != null
    //     && !host.IsLive()
    //     && host.allowed_operations != null && host.allowed_operations.Contains(host_allowed_operations.power_on)
    //     && !HelpersGUI.HasActiveHostAction(host)
    //     && host.power_on_mode != "";

    QVariantMap hostData = this->mainWindow()->xenLib()->getCache()->resolve("host", hostRef);
    if (hostData.isEmpty())
        return false;

    // Check if host is not live (not running)
    if (this->isHostLive(hostRef))
        return false;

    // Check if power_on is in allowed_operations
    QVariantList allowedOps = hostData.value("allowed_operations", QVariantList()).toList();
    bool hasPowerOn = false;
    for (const QVariant& op : allowedOps)
    {
        if (op.toString() == "power_on")
        {
            hasPowerOn = true;
            break;
        }
    }

    if (!hasPowerOn)
        return false;

    // Check if host has active actions (matches C# HelpersGUI.HasActiveHostAction)
    if (this->hasActiveHostAction(hostRef))
        return false;

    // Check if power_on_mode is set
    QString powerOnMode = hostData.value("power_on_mode", "").toString();
    if (powerOnMode.isEmpty())
        return false;

    return true;
}

bool PowerOnHostCommand::isHostLive(const QString& hostRef) const
{
    // Matches C# Host.IsLive() logic - check if host is enabled/running
    QVariantMap hostData = this->mainWindow()->xenLib()->getCache()->resolve("host", hostRef);
    if (hostData.isEmpty())
        return false;

    // A host is "live" if it's enabled (matches C# logic)
    return hostData.value("enabled", false).toBool();
}

bool PowerOnHostCommand::hasActiveHostAction(const QString& hostRef) const
{
    // Matches C# HelpersGUI.HasActiveHostAction(host) logic
    // Check if host has current_operations (active tasks)
    QVariantMap hostData = this->mainWindow()->xenLib()->getCache()->resolve("host", hostRef);
    if (hostData.isEmpty())
        return false;

    QVariantMap currentOps = hostData.value("current_operations", QVariantMap()).toMap();
    return !currentOps.isEmpty();
}
