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

#include "restartvmcommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib.h"
#include "xen/network/connection.h"
#include "xen/vm.h"
#include "xen/actions/vm/vmrebootaction.h"
#include "xencache.h"
#include <QMessageBox>
#include <QTimer>

RestartVMCommand::RestartVMCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool RestartVMCommand::canRun() const
{
    QString vmRef = this->getSelectedVMRef();
    if (vmRef.isEmpty())
        return false;

    // Check if VM is running AND clean_reboot is allowed (matches C# RebootVMCommand.CanRun)
    if (!this->isVMRunning(vmRef))
        return false;

    QVariantMap vmData = this->mainWindow()->xenLib()->getCache()->ResolveObjectData("vm", vmRef);
    QVariantList allowedOperations = vmData.value("allowed_operations").toList();

    return allowedOperations.contains("clean_reboot");
}

void RestartVMCommand::run()
{
    QString vmRef = this->getSelectedVMRef();
    QString vmName = this->getSelectedVMName();

    if (vmRef.isEmpty() || vmName.isEmpty())
        return;

    // Show confirmation dialog
    int ret = QMessageBox::question(this->mainWindow(), "Restart VM",
                                    QString("Are you sure you want to restart VM '%1'?").arg(vmName),
                                    QMessageBox::Yes | QMessageBox::No);

    if (ret != QMessageBox::Yes)
        return;

    // Get XenConnection from XenLib
    XenConnection* conn = this->mainWindow()->xenLib()->getConnection();
    if (!conn || !conn->isConnected())
    {
        QMessageBox::warning(this->mainWindow(), "Not Connected",
                             "Not connected to XenServer");
        return;
    }

    // Create VM object (lightweight wrapper)
    VM* vm = new VM(conn, vmRef);

    // Create VMCleanReboot action (parent is MainWindow to prevent premature deletion)
    VMCleanReboot* action = new VMCleanReboot(vm, this->mainWindow());

    // Register with OperationManager for history tracking (matches C# ConnectionsManager.History.Add)
    OperationManager::instance()->registerOperation(action);

    // Connect completion signal for cleanup
    connect(action, &AsyncOperation::completed, this, [action]() {
        // Auto-delete when complete (matches C# GC behavior)
        action->deleteLater();
    });

    // Run action asynchronously (matches C# pattern - no modal dialog)
    // Progress shown in status bar via OperationManager signals
    action->runAsync();
}

QString RestartVMCommand::menuText() const
{
    return "Restart VM";
}

QString RestartVMCommand::getSelectedVMRef() const
{
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    QString objectType = this->getSelectedObjectType();
    if (objectType != "vm")
        return QString();

    return this->getSelectedObjectRef();
}

QString RestartVMCommand::getSelectedVMName() const
{
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    QString objectType = this->getSelectedObjectType();
    if (objectType != "vm")
        return QString();

    return item->text(0);
}

bool RestartVMCommand::isVMRunning(const QString& vmRef) const
{
    // Use cache instead of async API call
    QVariantMap vmData = this->mainWindow()->xenLib()->getCache()->ResolveObjectData("vm", vmRef);
    QString powerState = vmData.value("power_state", "Halted").toString();

    return (powerState == "Running");
}
