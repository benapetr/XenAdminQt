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
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/actions/vm/vmstartaction.h"
#include "xenlib/xen/failure.h"
#include <QMessageBox>
#include <QTimer>
#include <QMetaObject>
#include <QPointer>

namespace
{
    bool canStartVm(const QSharedPointer<VM>& vm)
    {
        if (!vm)
            return false;

        // Check VM is not running (Halted or Suspended) AND start is allowed (matches C# StartVMCommand.CanRun)
        QString powerState = vm->GetPowerState();

        if (powerState == "Running")
            return false;

        return vm->GetAllowedOperations().contains("start");
    }
} // namespace

StartVMCommand::StartVMCommand(MainWindow* mainWindow, QObject* parent) : VMCommand(mainWindow, parent)
{
}

bool StartVMCommand::CanRun() const
{
    const QList<QSharedPointer<VM>> vms = this->getVMs();
    if (!vms.isEmpty())
    {
        for (const QSharedPointer<VM>& vm : vms)
        {
            if (canStartVm(vm))
                return true;
        }
        return false;
    }

    return canStartVm(this->getVM());
}

void StartVMCommand::Run()
{
    const QList<QSharedPointer<VM>> vms = this->getVMs();
    if (!vms.isEmpty())
    {
        for (const QSharedPointer<VM>& vm : vms)
        {
            if (canStartVm(vm))
                this->RunForVm(vm);
        }
        return;
    }

    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return;

    this->RunForVm(vm);
}

bool StartVMCommand::RunForVm(QSharedPointer<VM> vm)
{
    // Get XenConnection from VM
    XenConnection* conn = vm->GetConnection();
    if (!conn || !conn->IsConnected())
    {
        QMessageBox::warning(this->mainWindow(), "Not Connected",  "Not connected to XenServer");
        return false;
    }

    // Determine name if not provided
    QString displayName = vm->GetName();

    // Create VMStartAction with diagnosis callbacks (matches C# pattern)
    // NOTE: The callbacks are called by the action when failures occur
    QPointer<MainWindow> mainWindow = this->mainWindow();

    VMStartAction* action = new VMStartAction(
        vm,
        nullptr,  // WarningDialogHAInvalidConfig callback (TODO: implement if needed)
        [conn, displayName, mainWindow, vm](VMStartAbstractAction* abstractAction, const Failure& failure) {
            Q_UNUSED(abstractAction)
            if (!mainWindow)
                return;

            Failure failureCopy = failure;
            QMetaObject::invokeMethod(mainWindow, [mainWindow, conn, vm, displayName, failureCopy]() {
                if (!mainWindow)
                    return;
                VMOperationHelpers::StartDiagnosisForm(conn, vm->OpaqueRef(), displayName, true, failureCopy, mainWindow);
            }, Qt::QueuedConnection);
        },
        this->mainWindow());

    // Register with OperationManager for history tracking (matches C# ConnectionsManager.History.Add)
    OperationManager::instance()->RegisterOperation(action);

    // Run action asynchronously (matches C# pattern - no modal dialog)
    // Progress shown in status bar via OperationManager signals
    action->RunAsync(true);
    return true;
}

QString StartVMCommand::MenuText() const
{
    return "Start VM";
}

QIcon StartVMCommand::GetIcon() const
{
    return QIcon(":/icons/start_vm.png");
}
