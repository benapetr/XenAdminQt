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

#include "startvmcommand.h"
#include "vmoperationhelpers.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xen/network/connection.h"
#include "xen/vm.h"
#include "xen/actions/vm/vmstartaction.h"
#include "xen/failure.h"
#include <QMessageBox>
#include <QTimer>
#include <QMetaObject>
#include <QPointer>

StartVMCommand::StartVMCommand(MainWindow* mainWindow, QObject* parent)
    : VMCommand(mainWindow, parent)
{
}

bool StartVMCommand::CanRun() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    // Check VM is not running (Halted or Suspended) AND start is allowed (matches C# StartVMCommand.CanRun)
    QString powerState = vm->GetPowerState();

    if (powerState == "Running")
        return false;

    QVariantList allowedOperations = vm->GetData().value("allowed_operations").toList();
    return allowedOperations.contains("start");
}

void StartVMCommand::Run()
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return;

    runForVm(vm->OpaqueRef(), this->getSelectedVMName());
}

bool StartVMCommand::runForVm(const QString& vmRef, const QString& vmName)
{
    if (vmRef.isEmpty())
        return false;

    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    // Get XenConnection from VM
    XenConnection* conn = vm->GetConnection();
    if (!conn || !conn->IsConnected())
    {
        QMessageBox::warning(this->mainWindow(), "Not Connected",
                             "Not connected to XenServer");
        return false;
    }

    // Determine name if not provided
    QString displayName = vmName;
    if (displayName.isEmpty())
    {
        displayName = vm->GetName();
    }

    // Create VM object for action (action will own and delete it)
    QSharedPointer<VM> vmForAction = QSharedPointer<VM>(new VM(conn, vmRef));

    // Create VMStartAction with diagnosis callbacks (matches C# pattern)
    // NOTE: The callbacks are called by the action when failures occur
    QPointer<MainWindow> mainWindow = this->mainWindow();

    VMStartAction* action = new VMStartAction(
        vmForAction,
        nullptr,  // WarningDialogHAInvalidConfig callback (TODO: implement if needed)
        [conn, vmRef, displayName, mainWindow](VMStartAbstractAction* abstractAction, const Failure& failure) {
            Q_UNUSED(abstractAction)
            if (!mainWindow)
                return;

            Failure failureCopy = failure;
            QMetaObject::invokeMethod(mainWindow, [mainWindow, conn, vmRef, displayName, failureCopy]() {
                if (!mainWindow)
                    return;
                VMOperationHelpers::startDiagnosisForm(conn, vmRef, displayName, true, failureCopy, mainWindow);
            }, Qt::QueuedConnection);
        },
        this->mainWindow());

    // Register with OperationManager for history tracking (matches C# ConnectionsManager.History.Add)
    OperationManager::instance()->RegisterOperation(action);

    // Connect completion signal for cleanup
    connect(action, &AsyncOperation::completed, this, [action]() {
        // Auto-delete when complete (matches C# GC behavior)
        action->deleteLater();
    });

    // Run action asynchronously (matches C# pattern - no modal dialog)
    // Progress shown in status bar via OperationManager signals
    action->RunAsync();
    return true;
}

QString StartVMCommand::MenuText() const
{
    return "Start VM";
}
