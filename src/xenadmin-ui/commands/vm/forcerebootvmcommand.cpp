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
#include "xenlib/xen/host.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/actions/vm/vmrebootaction.h"
#include "xenlib/xencache.h"
#include <QMessageBox>

namespace
{
    // Matches C# EnabledTargetExists(host, connection) logic:
    // If the vm has a home server check it's enabled, otherwise check if any host is enabled
    bool enabledTargetExistsForVm(const QSharedPointer<VM>& vm)
    {
        QVariantMap vmData = vm->GetData();
        if (vmData.isEmpty())
            return false;

        QSharedPointer<Host> host = vm->GetResidentOnHost();
        if (!host.isNull())
            return host->IsEnabled();

        XenCache* cache = vm->GetConnection()->GetCache();
        QList<QSharedPointer<Host>> hosts = cache->GetAll<Host>(XenObjectType::Host);
        foreach (QSharedPointer host, hosts)
        {
            if (host->IsEnabled())
                return true;
        }

        return false;
    }

    // Matches C# ForceVMRebootCommand.CanRun() logic:  // if (vm != null && !vm.is_a_template && !vm.Locked)
    // {
    //     if (vm.power_state == vm_power_state.Running && HelpersGUI.HasRunningTasks(vm))
    //         return true;
    //     else
    //         return vm.allowed_operations != null && vm.allowed_operations.Contains(vm_operations.hard_reboot)
    //                && EnabledTargetExists(vm.Home(), vm.Connection);
    // }
    bool canForceRebootVm(const QSharedPointer<VM>& vm)
    {
        if (!vm)
            return false;

        if (vm->IsTemplate() || vm->IsLocked())
            return false;

        if (vm->GetPowerState() == "Running" && !vm->CurrentOperations().isEmpty())
            return true;

        if (!vm->GetAllowedOperations().contains("hard_reboot"))
            return false;

        return enabledTargetExistsForVm(vm);
    }
} // namespace

ForceRebootVMCommand::ForceRebootVMCommand(MainWindow* mainWindow, QObject* parent) : VMCommand(mainWindow, parent)
{
}

bool ForceRebootVMCommand::CanRun() const
{
    const QList<QSharedPointer<VM>> vms = this->getVMs();
    if (!vms.isEmpty())
    {
        for (const QSharedPointer<VM>& vm : vms)
        {
            if (canForceRebootVm(vm))
                return true;
        }
        return false;
    }

    return canForceRebootVm(this->getVM());
}

void ForceRebootVMCommand::Run()
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

        VMHardReboot* action = new VMHardReboot(vm, MainWindow::instance());
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
                                "Are you sure you want to force them to reboot?\n\n"
                                "This is equivalent to pressing the reset button and may cause data loss."
                              : "Are you sure you want to force the selected VMs to reboot?\n\n"
                                "This is equivalent to pressing the reset button and may cause data loss.";

        QMessageBox::StandardButton reply = QMessageBox::warning(
            MainWindow::instance(),
            "Force Reboot VMs",
            message,
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (reply != QMessageBox::Yes)
            return;

        for (const QSharedPointer<VM>& vm : vms)
        {
            if (canForceRebootVm(vm))
                runForVm(vm);
        }
        return;
    }

    QSharedPointer<VM> vm = vms.size() == 1 ? vms.first() : this->getVM();
    if (!vm || !canForceRebootVm(vm))
        return;

    QString vmName = vm->GetName();
    if (vmName.isEmpty())
        return;

    QString message = !vm->CurrentOperations().isEmpty()
                          ? QString("'%1' has tasks in progress that will be cancelled. "
                                    "Are you sure you want to force it to reboot?\n\n"
                                    "This is equivalent to pressing the reset button and may cause data loss.")
                                .arg(vmName)
                          : QString("Are you sure you want to force '%1' to reboot?\n\n"
                                    "This is equivalent to pressing the reset button and may cause data loss.")
                                .arg(vmName);

    QMessageBox::StandardButton reply = QMessageBox::warning(
        MainWindow::instance(),
        "Force Reboot VM",
        message,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    runForVm(vm);
}

QString ForceRebootVMCommand::MenuText() const
{
    // Matches C# Messages.MAINWINDOW_FORCE_REBOOT
    return "Force Reboot";
}

QIcon ForceRebootVMCommand::GetIcon() const
{
    return QIcon(":/icons/force_reboot.png");
}
