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
#include "xencache.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/actions/vm/vmshutdownaction.h"
#include <QMessageBox>
#include <QTimer>

namespace
{
    bool canShutdownVm(const QSharedPointer<VM>& vm)
    {
        if (!vm)
            return false;

        if (vm->IsTemplate() || vm->IsSnapshot() || vm->IsLocked())
            return false;

        return vm->GetAllowedOperations().contains("clean_shutdown");
    }

    bool isHaProtected(const QSharedPointer<VM>& vm)
    {
        if (!vm)
            return false;

        QSharedPointer<Pool> pool = vm->GetCache()->GetPoolOfOne();
        if (!pool)
            return false;

        return pool->HAEnabled() && vm->HARestartPriority() != "do_not_restart";
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
            QMessageBox::warning(MainWindow::instance(), "Not Connected", "Not connected to XenServer");
            return;
        }

        // Create VMCleanShutdown action (parent is MainWindow to prevent premature deletion)
        VMCleanShutdown* action = new VMCleanShutdown(vm, MainWindow::instance());

        // Run action asynchronously (matches C# pattern - no modal dialog)
        // Progress shown in status bar via OperationManager signals
        action->RunAsync(true);
    };

    QList<QSharedPointer<VM>> vms = this->getVMs();
    if (vms.isEmpty())
    {
        QSharedPointer<VM> vm = this->getVM();
        if (vm)
            vms.append(vm);
    }

    QList<QSharedPointer<VM>> runnable;
    for (const QSharedPointer<VM>& vm : vms)
    {
        if (canShutdownVm(vm))
            runnable.append(vm);
    }

    if (runnable.size() > 1)
    {
        int ret = QMessageBox::question(MainWindow::instance(), tr("Shut Down Multiple VMs"),
                                        tr("Are you sure you want to shut down the selected VMs?"),
                                        QMessageBox::Yes | QMessageBox::No);
        if (ret != QMessageBox::Yes)
            return;

        QList<AsyncOperation*> actions;
        for (const QSharedPointer<VM>& vm : runnable)
            actions.append(new VMCleanShutdown(vm, MainWindow::instance()));

        RunMultipleActions(actions, tr("Shutting Down VMs"), tr("Shutting Down VMs"), tr("Shut down"), true);
        return;
    }

    QSharedPointer<VM> vm = runnable.size() == 1 ? runnable.first() : QSharedPointer<VM>();
    if (!vm || !canShutdownVm(vm))
        return;

    const QString text = isHaProtected(vm)
        ? tr("The selected VM is currently protected by HA. Are you sure you want to shut it down?")
        : tr("Are you sure you want to shut down the selected VM?");

    int ret = QMessageBox::question(MainWindow::instance(), tr("Shut Down VM"), text, QMessageBox::Yes | QMessageBox::No);

    if (ret != QMessageBox::Yes)
        return;

    runForVm(vm);
}

QString StopVMCommand::MenuText() const
{
    return tr("Shut Down");
}

QIcon StopVMCommand::GetIcon() const
{
    return QIcon(":/icons/shutdown.png");
}
