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
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/actions/vm/vmrebootaction.h"
#include "xenlib/xencache.h"
#include <QMessageBox>
#include <QTimer>

namespace
{
    bool enabledTargetExists(const QSharedPointer<Host>& host, XenConnection* connection)
    {
        if (host)
            return host->IsEnabled();

        if (!connection)
            return false;

        XenCache* cache = connection->GetCache();
        if (!cache)
            return false;

        const QList<QSharedPointer<Host>> hosts = cache->GetAll<Host>();
        for (const QSharedPointer<Host>& item : hosts)
        {
            if (item && item->IsEnabled())
                return true;
        }

        return false;
    }

    bool canRestartVm(const QSharedPointer<VM>& vm)
    {
        if (!vm)
            return false;

        if (vm->IsTemplate() || vm->IsSnapshot() || vm->IsLocked())
            return false;

        if (!vm->GetAllowedOperations().contains("clean_reboot"))
            return false;

        XenConnection* connection = vm->GetConnection();
        if (!connection)
            return false;

        return enabledTargetExists(vm->GetResidentOnHost(), connection);
    }
} // namespace

RestartVMCommand::RestartVMCommand(MainWindow* mainWindow, QObject* parent) : VMCommand(mainWindow, parent)
{
}

bool RestartVMCommand::CanRun() const
{
    const QList<QSharedPointer<VM>> vms = this->getVMs();
    if (!vms.isEmpty())
    {
        for (const QSharedPointer<VM>& vm : vms)
        {
            if (canRestartVm(vm))
                return true;
        }
        return false;
    }

    return canRestartVm(this->getVM());
}

void RestartVMCommand::Run()
{
    auto runForVm = [](const QSharedPointer<VM>& vm)
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

        // Create VMCleanReboot action (parent is MainWindow to prevent premature deletion)
        VMCleanReboot* action = new VMCleanReboot(vm, MainWindow::instance());

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
        if (canRestartVm(vm))
            runnable.append(vm);
    }

    if (runnable.size() > 1)
    {
        int ret = QMessageBox::question(MainWindow::instance(), tr("Reboot Multiple VMs"),
                                        tr("Are you sure you want to reboot the selected VMs?"),
                                        QMessageBox::Yes | QMessageBox::No);
        if (ret != QMessageBox::Yes)
            return;

        QList<AsyncOperation*> actions;
        for (const QSharedPointer<VM>& vm : runnable)
            actions.append(new VMCleanReboot(vm, MainWindow::instance()));

        this->RunMultipleActions(actions, tr("Rebooting VMs"), tr("Rebooting VMs"), tr("Rebooted"), true);
        return;
    }

    QSharedPointer<VM> vm = runnable.size() == 1 ? runnable.first() : QSharedPointer<VM>();
    if (!vm || !canRestartVm(vm))
        return;

    int ret = QMessageBox::question(MainWindow::instance(), tr("Reboot VM"),
                                    tr("Are you sure you want to reboot the selected VM?"),
                                    QMessageBox::Yes | QMessageBox::No);

    if (ret != QMessageBox::Yes)
        return;

    runForVm(vm);
}

QString RestartVMCommand::MenuText() const
{
    return "Reboot";
}

QIcon RestartVMCommand::GetIcon() const
{
    return QIcon(":/icons/reboot.png");
}
