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

#include "suspendvmcommand.h"
#include "../../mainwindow.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/actions/vm/vmshutdownaction.h"
#include "../../operations/operationmanager.h"
#include <QMessageBox>

namespace
{
    bool canSuspendVm(const QSharedPointer<VM>& vm)
    {
        if (!vm)
            return false;

        if (vm->GetPowerState() != "Running")
            return false;

        return vm->GetAllowedOperations().contains("suspend");
    }
} // namespace

SuspendVMCommand::SuspendVMCommand(MainWindow* mainWindow, QObject* parent) : VMCommand(mainWindow, parent)
{
}

bool SuspendVMCommand::CanRun() const
{
    const QList<QSharedPointer<VM>> vms = this->getVMs();
    if (!vms.isEmpty())
    {
        for (const QSharedPointer<VM>& vm : vms)
        {
            if (canSuspendVm(vm))
                return true;
        }
        return false;
    }

    return canSuspendVm(this->getVM());
}

void SuspendVMCommand::Run()
{
    auto runForVm = [this](const QSharedPointer<VM>& vm)
    {
        if (!vm)
            return;

        // Get XenConnection from VM
        XenConnection* conn = vm->GetConnection();
        if (!conn || !conn->IsConnected())
        {
            QMessageBox::warning(MainWindow::instance(), "Not Connected", "Not connected to XenServer");
            return;
        }

        // Create VMSuspendAction (parent is MainWindow to prevent premature deletion)
        VMSuspendAction* action = new VMSuspendAction(vm, MainWindow::instance());

        // Register with OperationManager for history tracking (matches C# ConnectionsManager.History.Add)
        OperationManager::instance()->RegisterOperation(action);

        // Run action asynchronously (matches C# pattern - no modal dialog)
        // Progress shown in status bar via OperationManager signals
        action->RunAsync(true);
    };

    const QList<QSharedPointer<VM>> vms = this->getVMs();
    if (vms.size() > 1)
    {
        int ret = QMessageBox::question(MainWindow::instance(), "Suspend VMs",
                                        "Are you sure you want to suspend the selected VMs?",
                                        QMessageBox::Yes | QMessageBox::No);
        if (ret != QMessageBox::Yes)
            return;

        for (const QSharedPointer<VM>& vm : vms)
        {
            if (canSuspendVm(vm))
                runForVm(vm);
        }
        return;
    }
    QSharedPointer<VM> vm = vms.size() == 1 ? vms.first() : this->getVM();
    if (!vm || !canSuspendVm(vm))
        return;

    QString vmName = vm->GetName();
    if (vmName.isEmpty())
        return;

    int ret = QMessageBox::question(MainWindow::instance(), "Suspend VM",
                                    QString("Are you sure you want to suspend VM '%1'?").arg(vmName),
                                    QMessageBox::Yes | QMessageBox::No);

    if (ret != QMessageBox::Yes)
        return;

    runForVm(vm);
}

QString SuspendVMCommand::MenuText() const
{
    return "Suspend VM";
}

QIcon SuspendVMCommand::GetIcon() const
{
    return QIcon(":/icons/suspend.png");
}
