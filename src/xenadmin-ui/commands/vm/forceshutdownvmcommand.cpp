
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

#include "forceshutdownvmcommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib.h"
#include "xencache.h"
#include "xen/network/connection.h"
#include "xen/vm.h"
#include "xen/actions/vm/vmshutdownaction.h"
#include <QMessageBox>

ForceShutdownVMCommand::ForceShutdownVMCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool ForceShutdownVMCommand::canRun() const
{
    // Matches C# ForceVMShutDownCommand.CanRun() logic
    QString vmRef = this->getSelectedVMRef();
    if (vmRef.isEmpty())
        return false;

    return this->canForceShutdown(vmRef);
}

void ForceShutdownVMCommand::run()
{
    // Matches C# ForceVMShutDownCommand.Run() with confirmation dialog
    QString vmRef = this->getSelectedVMRef();
    QString vmName = this->getSelectedVMName();

    if (vmRef.isEmpty() || vmName.isEmpty())
        return;

    // Get VM data from cache
    QVariantMap vmData = this->mainWindow()->xenLib()->getCache()->ResolveObjectData("vm", vmRef);
    if (vmData.isEmpty())
        return;

    // Check if VM has running tasks (matches C# HelpersGUI.HasRunningTasks logic)
    bool hasRunningTasks = this->hasRunningTasks(vmRef);

    // Build confirmation message (matches C# ConfirmationDialogText logic)
    QString message;
    if (hasRunningTasks)
    {
        message = QString("'%1' has tasks in progress that will be cancelled. "
                          "Are you sure you want to force it to shut down?\n\n"
                          "This is equivalent to pulling the power cable out and may cause data loss.")
                      .arg(vmName);
    } else
    {
        message = QString("Are you sure you want to force '%1' to shut down?\n\n"
                          "This is equivalent to pulling the power cable out and may cause data loss.")
                      .arg(vmName);
    }

    // Show confirmation dialog (matches C# ConfirmationRequired pattern)
    QMessageBox::StandardButton reply = QMessageBox::warning(
        this->mainWindow(),
        "Force Shutdown VM",
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

    // Create the hard shutdown action
    VMHardShutdown* action = new VMHardShutdown(vm, this->mainWindow());

    // Register with OperationManager for history tracking
    OperationManager::instance()->registerOperation(action);

    // Connect completion signal for cleanup
    connect(action, &AsyncOperation::completed, this, [action]() {
        action->deleteLater();
    });

    // Run action asynchronously (matches C# pattern)
    action->runAsync();
}

QString ForceShutdownVMCommand::menuText() const
{
    // Matches C# Messages.MAINWINDOW_FORCE_SHUTDOWN
    return "Force Shutdown";
}

QString ForceShutdownVMCommand::getSelectedVMRef() const
{
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    QString objectType = this->getSelectedObjectType();
    if (objectType != "vm")
        return QString();

    return this->getSelectedObjectRef();
}

QString ForceShutdownVMCommand::getSelectedVMName() const
{
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    QString objectType = this->getSelectedObjectType();
    if (objectType != "vm")
        return QString();

    return item->text(0);
}

bool ForceShutdownVMCommand::canForceShutdown(const QString& vmRef) const
{
    // Matches C# ForceVMShutDownCommand.CanRun() logic:
    // if (vm != null && !vm.Locked && !vm.is_a_template)
    // {
    //     if (vm.power_state == vm_power_state.Running && HelpersGUI.HasRunningTasks(vm))
    //         return true;
    //     else
    //         return vm.allowed_operations != null && vm.allowed_operations.Contains(vm_operations.hard_shutdown);
    // }

    QVariantMap vmData = this->mainWindow()->xenLib()->getCache()->ResolveObjectData("vm", vmRef);
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

    // Otherwise check allowed_operations
    QVariantList allowedOps = vmData.value("allowed_operations", QVariantList()).toList();
    for (const QVariant& op : allowedOps)
    {
        if (op.toString() == "hard_shutdown")
            return true;
    }

    return false;
}

bool ForceShutdownVMCommand::hasRunningTasks(const QString& vmRef) const
{
    // Matches C# HelpersGUI.HasRunningTasks(vm) logic
    // Check if VM has current_operations (running tasks)
    QVariantMap vmData = this->mainWindow()->xenLib()->getCache()->ResolveObjectData("vm", vmRef);
    if (vmData.isEmpty())
        return false;

    QVariantMap currentOps = vmData.value("current_operations", QVariantMap()).toMap();
    return !currentOps.isEmpty();
}
