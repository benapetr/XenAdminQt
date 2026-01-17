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

#include "stopvmcommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/actions/vm/vmshutdownaction.h"
#include <QMessageBox>
#include <QTimer>

namespace
{
    bool canShutdownVm(const QSharedPointer<VM>& vm)
    {
        if (!vm)
            return false;

        if (vm->GetPowerState() != "Running")
            return false;

        return vm->GetAllowedOperations().contains("clean_shutdown");
    }
} // namespace

StopVMCommand::StopVMCommand(MainWindow* mainWindow, QObject* parent) : VMCommand(mainWindow, parent)
{
}

bool StopVMCommand::CanRun() const
{
    const QList<QSharedPointer<VM>> vms = this->getVMs();
    if (!vms.isEmpty())
    {
        for (const QSharedPointer<VM>& vm : vms)
        {
            if (canShutdownVm(vm))
                return true;
        }
        return false;
    }

    return canShutdownVm(this->getVM());
}

void StopVMCommand::Run()
{
    auto runForVm = [this](const QSharedPointer<VM>& vm) {
        if (!vm)
            return;

        // Get XenConnection from VM
        XenConnection* conn = vm->GetConnection();
        if (!conn || !conn->IsConnected())
        {
            QMessageBox::warning(this->mainWindow(), "Not Connected",
                                 "Not connected to XenServer");
            return;
        }

        // Create VMCleanShutdown action (parent is MainWindow to prevent premature deletion)
        VMCleanShutdown* action = new VMCleanShutdown(vm, this->mainWindow());

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
    };

    const QList<QSharedPointer<VM>> vms = this->getVMs();
    if (vms.size() > 1)
    {
        int ret = QMessageBox::question(this->mainWindow(), "Shutdown VMs",
                                        "Are you sure you want to shutdown the selected VMs?",
                                        QMessageBox::Yes | QMessageBox::No);
        if (ret != QMessageBox::Yes)
            return;

        for (const QSharedPointer<VM>& vm : vms)
        {
            if (canShutdownVm(vm))
                runForVm(vm);
        }
        return;
    }
    QSharedPointer<VM> vm = vms.size() == 1 ? vms.first() : this->getVM();
    if (!vm || !canShutdownVm(vm))
        return;

    QString vmName = vm->GetName();
    if (vmName.isEmpty())
        return;

    int ret = QMessageBox::question(this->mainWindow(), "Shutdown VM",
                                    QString("Are you sure you want to shutdown VM '%1'?").arg(vmName),
                                    QMessageBox::Yes | QMessageBox::No);

    if (ret != QMessageBox::Yes)
        return;

    runForVm(vm);
}

QString StopVMCommand::MenuText() const
{
    return "Shutdown VM";
}

QIcon StopVMCommand::GetIcon() const
{
    return QIcon(":/icons/shutdown.png");
}
