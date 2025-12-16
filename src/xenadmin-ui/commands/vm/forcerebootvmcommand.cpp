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

#include "forcerebootvmcommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib.h"
#include "xencache.h"
#include "xen/connection.h"
#include "xen/vm.h"
#include "xen/actions/vm/vmrebootaction.h"
#include <QMessageBox>

ForceRebootVMCommand::ForceRebootVMCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool ForceRebootVMCommand::canRun() const
{
    // Matches C# ForceVMRebootCommand.CanRun() logic
    QString vmRef = this->getSelectedVMRef();
    if (vmRef.isEmpty())
        return false;

    return this->canForceReboot(vmRef);
}

void ForceRebootVMCommand::run()
{
    // Matches C# ForceVMRebootCommand.Run() with confirmation dialog
    QString vmRef = this->getSelectedVMRef();
    QString vmName = this->getSelectedVMName();

    if (vmRef.isEmpty() || vmName.isEmpty())
        return;

    // Get VM data from cache
    QVariantMap vmData = this->mainWindow()->xenLib()->getCache()->resolve("vm", vmRef);
    if (vmData.isEmpty())
        return;

    // Check if VM has running tasks (matches C# HelpersGUI.HasRunningTasks logic)
    bool hasRunningTasks = this->hasRunningTasks(vmRef);

    // Build confirmation message (matches C# ConfirmationDialogText logic)
    QString message;
    if (hasRunningTasks)
    {
        message = QString("'%1' has tasks in progress that will be cancelled. "
                          "Are you sure you want to force it to reboot?\n\n"
                          "This is equivalent to pressing the reset button and may cause data loss.")
                      .arg(vmName);
    } else
    {
        message = QString("Are you sure you want to force '%1' to reboot?\n\n"
                          "This is equivalent to pressing the reset button and may cause data loss.")
                      .arg(vmName);
    }

    // Show confirmation dialog (matches C# ConfirmationRequired pattern)
    QMessageBox::StandardButton reply = QMessageBox::warning(
        this->mainWindow(),
        "Force Reboot VM",
        message,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    // Get XenConnection from XenLib
    XenConnection* conn = this->mainWindow()->xenLib()->getConnection();
    if (!conn || !conn->isConnected())
    {
        QMessageBox::warning(this->mainWindow(),
                             tr("Not Connected"),
                             tr("Not connected to XenServer"));
        return;
    }

    // Create VM object (lightweight wrapper)
    VM* vm = new VM(conn, vmRef);

    // Create the hard reboot action
    VMHardReboot* action = new VMHardReboot(vm, this->mainWindow());

    // Register with OperationManager for history tracking
    OperationManager::instance()->registerOperation(action);

    // Connect completion signal for cleanup
    connect(action, &AsyncOperation::completed, this, [action]() {
        action->deleteLater();
    });

    // Run action asynchronously (matches C# pattern)
    action->runAsync();
}

QString ForceRebootVMCommand::menuText() const
{
    // Matches C# Messages.MAINWINDOW_FORCE_REBOOT
    return "Force Reboot";
}

QString ForceRebootVMCommand::getSelectedVMRef() const
{
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    QString objectType = this->getSelectedObjectType();
    if (objectType != "vm")
        return QString();

    return this->getSelectedObjectRef();
}

QString ForceRebootVMCommand::getSelectedVMName() const
{
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    QString objectType = this->getSelectedObjectType();
    if (objectType != "vm")
        return QString();

    return item->text(0);
}

bool ForceRebootVMCommand::canForceReboot(const QString& vmRef) const
{
    // Matches C# ForceVMRebootCommand.CanRun() logic:
    // if (vm != null && !vm.is_a_template && !vm.Locked)
    // {
    //     if (vm.power_state == vm_power_state.Running && HelpersGUI.HasRunningTasks(vm))
    //         return true;
    //     else
    //         return vm.allowed_operations != null && vm.allowed_operations.Contains(vm_operations.hard_reboot)
    //                && EnabledTargetExists(vm.Home(), vm.Connection);
    // }

    QVariantMap vmData = this->mainWindow()->xenLib()->getCache()->resolve("vm", vmRef);
    if (vmData.isEmpty())
        return false;

    bool isTemplate = vmData.value("is_a_template", false).toBool();
    bool isLocked = vmData.value("locked", false).toBool();

    if (isTemplate || isLocked)
        return false;

    QString powerState = vmData.value("power_state", "").toString();

    // CA-16960: If the VM is up and has a running task, we will disregard the allowed_operations
    // and always allow forced options.
    if (powerState == "Running" && this->hasRunningTasks(vmRef))
        return true;

    // Otherwise check allowed_operations and enabled target
    QVariantList allowedOps = vmData.value("allowed_operations", QVariantList()).toList();
    bool hasHardReboot = false;
    for (const QVariant& op : allowedOps)
    {
        if (op.toString() == "hard_reboot")
        {
            hasHardReboot = true;
            break;
        }
    }

    if (!hasHardReboot)
        return false;

    // Check if an enabled target host exists (matches C# EnabledTargetExists logic)
    return this->enabledTargetExists(vmRef);
}

bool ForceRebootVMCommand::hasRunningTasks(const QString& vmRef) const
{
    // Matches C# HelpersGUI.HasRunningTasks(vm) logic
    // Check if VM has current_operations (running tasks)
    QVariantMap vmData = this->mainWindow()->xenLib()->getCache()->resolve("vm", vmRef);
    if (vmData.isEmpty())
        return false;

    QVariantMap currentOps = vmData.value("current_operations", QVariantMap()).toMap();
    return !currentOps.isEmpty();
}

bool ForceRebootVMCommand::enabledTargetExists(const QString& vmRef) const
{
    // Matches C# EnabledTargetExists(host, connection) logic:
    // If the vm has a home server check it's enabled, otherwise check if any host is enabled

    QVariantMap vmData = this->mainWindow()->xenLib()->getCache()->resolve("vm", vmRef);
    if (vmData.isEmpty())
        return false;

    // Check if VM has a resident_on (home server)
    QString residentOn = vmData.value("resident_on", "").toString();
    if (!residentOn.isEmpty() && residentOn != "OpaqueRef:NULL")
    {
        // VM has a home server - check if it's enabled
        QVariantMap hostData = this->mainWindow()->xenLib()->getCache()->resolve("host", residentOn);
        if (!hostData.isEmpty())
        {
            return hostData.value("enabled", false).toBool();
        }
    }

    // No home server or home server not found - check if any host is enabled
    QList<QVariantMap> hosts = this->mainWindow()->xenLib()->getCache()->getAll("host");
    for (const QVariantMap& host : hosts)
    {
        if (host.value("enabled", false).toBool())
            return true;
    }

    return false;
}
