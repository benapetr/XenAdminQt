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
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/actions/vm/vmshutdownaction.h"
#include <QMessageBox>

namespace
{
    bool canForceShutdownVm(const QSharedPointer<VM>& vm)
    {
        if (!vm)
            return false;

        if (vm->IsTemplate() || vm->IsLocked())
            return false;

        if (vm->GetPowerState() == "Running" && !vm->CurrentOperations().isEmpty())
            return true;

        if (!vm->GetAllowedOperations().contains("hard_shutdown"))
            return false;

        return true;
    }
} // namespace

ForceShutdownVMCommand::ForceShutdownVMCommand(MainWindow* mainWindow, QObject* parent) : VMCommand(mainWindow, parent)
{
}

bool ForceShutdownVMCommand::CanRun() const
{
    const QList<QSharedPointer<VM>> vms = this->getVMs();
    if (!vms.isEmpty())
    {
        for (const QSharedPointer<VM>& vm : vms)
        {
            if (canForceShutdownVm(vm))
                return true;
        }
        return false;
    }

    return canForceShutdownVm(this->getVM());
}

void ForceShutdownVMCommand::Run()
{
    auto runForVm = [this](const QSharedPointer<VM>& vm)
    {
        if (!vm)
            return;

        XenConnection* conn = vm->GetConnection();
        if (!conn || !conn->IsConnected())
        {
            QMessageBox::warning(MainWindow::instance(), tr("Not Connected"), tr("Not connected to XenServer"));
            return;
        }

        VMHardShutdown* action = new VMHardShutdown(vm, MainWindow::instance());
        action->RunAsync(true);
    };

    const QList<QSharedPointer<VM>> vms = this->getVMs();
    if (vms.size() > 1)
    {
        bool anyRunningTasks = false;
        for (const QSharedPointer<VM>& vm : vms)
        {
            if (!vm->CurrentOperations().isEmpty())
            {
                anyRunningTasks = true;
                break;
            }
        }

        QString message = anyRunningTasks
                              ? "Some selected VMs have tasks in progress that will be cancelled. "
                                "Are you sure you want to force them to shut down?\n\n"
                                "This is equivalent to pulling the power cable out and may cause data loss."
                              : "Are you sure you want to force the selected VMs to shut down?\n\n"
                                "This is equivalent to pulling the power cable out and may cause data loss.";

        QMessageBox::StandardButton reply = QMessageBox::warning(
            MainWindow::instance(),
            "Force Shutdown VMs",
            message,
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (reply != QMessageBox::Yes)
            return;

        for (const QSharedPointer<VM>& vm : vms)
        {
            if (canForceShutdownVm(vm))
                runForVm(vm);
        }
        return;
    }

    QSharedPointer<VM> vm = vms.size() == 1 ? vms.first() : this->getVM();
    if (!vm || !canForceShutdownVm(vm))
        return;

    QString vmName = vm->GetName();
    if (vmName.isEmpty())
        return;

    QString message = !vm->CurrentOperations().isEmpty()
                          ? QString("'%1' has tasks in progress that will be cancelled. "
                                    "Are you sure you want to force it to shut down?\n\n"
                                    "This is equivalent to pulling the power cable out and may cause data loss.")
                                .arg(vmName)
                          : QString("Are you sure you want to force '%1' to shut down?\n\n"
                                    "This is equivalent to pulling the power cable out and may cause data loss.")
                                .arg(vmName);

    QMessageBox::StandardButton reply = QMessageBox::warning(
        MainWindow::instance(),
        "Force Shutdown VM",
        message,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    runForVm(vm);
}

QString ForceShutdownVMCommand::MenuText() const
{
    // Matches C# Messages.MAINWINDOW_FORCE_SHUTDOWN
    return "Force Shutdown";
}

QIcon ForceShutdownVMCommand::GetIcon() const
{
    return QIcon(":/icons/force_shutdown.png");
}
