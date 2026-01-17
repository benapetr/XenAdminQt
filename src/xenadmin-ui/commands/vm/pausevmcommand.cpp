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

#include "pausevmcommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/actions/vm/vmpauseaction.h"
#include <QMessageBox>
#include <QTimer>

namespace
{
    bool canPauseVm(const QSharedPointer<VM>& vm)
    {
        if (!vm)
            return false;

        if (vm->GetPowerState() != "Running")
            return false;

        return vm->GetAllowedOperations().contains("pause");
    }
} // namespace

PauseVMCommand::PauseVMCommand(MainWindow* mainWindow, QObject* parent) : VMCommand(mainWindow, parent)
{
}

bool PauseVMCommand::CanRun() const
{
    const QList<QSharedPointer<VM>> vms = this->getVMs();
    if (!vms.isEmpty())
    {
        for (const QSharedPointer<VM>& vm : vms)
        {
            if (canPauseVm(vm))
                return true;
        }
        return false;
    }

    return canPauseVm(this->getVM());
}

void PauseVMCommand::Run()
{
    auto runForVm = [this](const QSharedPointer<VM>& vm)
    {
        if (!vm)
            return;

        // Get XenConnection from VM
        XenConnection* conn = vm->GetConnection();
        if (!conn || !conn->IsConnected())
        {
            QMessageBox::warning(this->mainWindow(), "Not Connected", "Not connected to XenServer");
            return;
        }

        // Create VMPause action (parent is MainWindow to prevent premature deletion)
        VMPause* action = new VMPause(vm, this->mainWindow());

        // Register with OperationManager for history tracking
        OperationManager::instance()->RegisterOperation(action);

        // Connect completion signal for cleanup
        connect(action, &AsyncOperation::completed, this, [action]() {
            // Auto-delete when complete
            action->deleteLater();
        });

        // Run action asynchronously (no modal dialog for pause)
        action->RunAsync();
    };

    const QList<QSharedPointer<VM>> vms = this->getVMs();
    if (!vms.isEmpty())
    {
        for (const QSharedPointer<VM>& vm : vms)
        {
            if (canPauseVm(vm))
                runForVm(vm);
        }
        return;
    }

    QSharedPointer<VM> vm = this->getVM();
    if (!vm || !canPauseVm(vm))
        return;

    runForVm(vm);
}

QString PauseVMCommand::MenuText() const
{
    return "Pause VM";
}

QIcon PauseVMCommand::GetIcon() const
{
    return QIcon(":/icons/pause.png");
}
