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
#include "xenlib/vmhelpers.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/vbd.h"
#include "xenlib/xen/vdi.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/actions/vm/vmstartaction.h"
#include "xenlib/xen/actions/vm/changevmisoaction.h"
#include "xenlib/xen/failure.h"
#include "xenlib/operations/multipleaction.h"
#include "xenlib/xencache.h"
#include <QMessageBox>
#include <QTimer>
#include <QMetaObject>
#include <QPointer>
#include <QHash>
#include <QPushButton>

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

    bool enabledTargetExistsForVm(const QSharedPointer<VM>& vm)
    {
        if (!vm)
            return false;

        XenConnection* connection = vm->GetConnection();
        if (!connection)
            return false;

        QSharedPointer<Host> host;
        if (vm->GetPowerState() == "Running")
        {
            host = vm->GetResidentOnHost();
        }
        else
        {
            QString hostRef = VMHelpers::getVMStorageHost(connection, vm->GetData(), false);
            if (!hostRef.isEmpty() && hostRef != XENOBJECT_NULL)
                host = connection->GetCache()->ResolveObject<Host>(hostRef);
        }

        return enabledTargetExists(host, connection);
    }

    bool canStartVm(const QSharedPointer<VM>& vm)
    {
        if (!vm)
            return false;

        if (vm->IsTemplate() || vm->IsSnapshot() || vm->IsLocked())
            return false;

        if (vm->GetPowerState() != "Halted")
            return false;

        if (!vm->GetAllowedOperations().contains("start"))
            return false;

        return enabledTargetExistsForVm(vm);
    }

    bool hasBrokenCd(const QSharedPointer<VM>& vm, QList<QSharedPointer<VBD>>& brokenCdVbds)
    {
        if (!vm)
            return false;

        const QList<QSharedPointer<VBD>> vbds = vm->GetVBDs();
        for (const QSharedPointer<VBD>& vbd : vbds)
        {
            if (!vbd || !vbd->IsValid())
                continue;
            if (!vbd->IsCD() || vbd->Empty())
                continue;

            QSharedPointer<VDI> vdi = vbd->GetVDI();
            if (!vdi || !vdi->IsValid())
            {
                brokenCdVbds.append(vbd);
                continue;
            }

            QSharedPointer<SR> sr = vdi->GetSR();
            if (!sr || sr->IsBroken() || sr->IsDetached())
            {
                brokenCdVbds.append(vbd);
            }
        }

        return !brokenCdVbds.isEmpty();
    }

    VMStartAction* createStartAction(MainWindow* mainWindow, const QSharedPointer<VM>& vm)
    {
        XenConnection* conn = vm ? vm->GetConnection() : nullptr;
        const QString displayName = vm ? vm->GetName() : QString();
        QPointer<MainWindow> windowPtr = mainWindow;

        return new VMStartAction(
            vm,
            nullptr,
            [conn, displayName, windowPtr, vm](VMStartAbstractAction* abstractAction, const Failure& failure) {
                Q_UNUSED(abstractAction)
                if (!windowPtr)
                    return;
                Failure failureCopy = failure;
                QMetaObject::invokeMethod(windowPtr, [windowPtr, conn, vm, displayName, failureCopy]() {
                    if (!windowPtr)
                        return;
                    VMOperationHelpers::StartDiagnosisForm(conn, vm->OpaqueRef(), displayName, true, failureCopy, windowPtr);
                }, Qt::QueuedConnection);
            },
            mainWindow);
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
        if (canStartVm(vm))
            runnable.append(vm);
    }

    if (runnable.isEmpty())
        return;

    QHash<QSharedPointer<VM>, QList<QSharedPointer<VBD>>> brokenCds;
    for (const QSharedPointer<VM>& vm : runnable)
    {
        QList<QSharedPointer<VBD>> vbds;
        if (hasBrokenCd(vm, vbds))
            brokenCds.insert(vm, vbds);
    }

    if (runnable.size() == 1 && brokenCds.isEmpty())
    {
        VMStartAction* action = createStartAction(MainWindow::instance(), runnable.first());
        action->RunAsync(true);
        return;
    }

    if (!brokenCds.isEmpty())
    {
        QMessageBox prompt(MainWindow::instance());
        prompt.setWindowTitle(runnable.size() > 1 ? tr("Starting VMs") : tr("Starting VM"));
        prompt.setText(tr("It may not be possible to start the selected VMs as they are using ISOs from an SR which is unavailable.\n\n"
                          "Would you like to eject these ISOs before continuing?"));
        QPushButton* ejectButton = prompt.addButton(tr("&Eject"), QMessageBox::AcceptRole);
        QPushButton* ignoreButton = prompt.addButton(tr("&Ignore"), QMessageBox::DestructiveRole);
        prompt.addButton(QMessageBox::Cancel);
        prompt.exec();

        if (prompt.clickedButton() == ignoreButton)
        {
            brokenCds.clear();
        }
        else if (prompt.clickedButton() != ejectButton)
        {
            return;
        }
    }

    QList<AsyncOperation*> actions;
    for (const QSharedPointer<VM>& vm : runnable)
    {
        if (!brokenCds.contains(vm))
        {
            actions.append(createStartAction(MainWindow::instance(), vm));
            continue;
        }

        QList<AsyncOperation*> subActions;
        const QList<QSharedPointer<VBD>>& vbds = brokenCds[vm];
        for (const QSharedPointer<VBD>& vbd : vbds)
        {
            subActions.append(new ChangeVMISOAction(vm, QString(), vbd->OpaqueRef(), MainWindow::instance()));
        }
        subActions.append(createStartAction(MainWindow::instance(), vm));

        XenConnection* conn = vm->GetConnection();
        MultipleAction* multi = new MultipleAction(
            conn,
            tr("Starting VMs"),
            tr("Starting VMs"),
            tr("Started"),
            subActions,
            false,
            false,
            false,
            MainWindow::instance());
        actions.append(multi);
    }

    this->RunMultipleActions(actions, tr("Starting VMs"), tr("Starting VMs"), tr("Started"), true);
}

bool StartVMCommand::RunForVm(QSharedPointer<VM> vm)
{
    if (!vm || !canStartVm(vm))
        return false;

    QList<QSharedPointer<VBD>> brokenVbds;
    if (hasBrokenCd(vm, brokenVbds))
    {
        QMessageBox prompt(MainWindow::instance());
        prompt.setWindowTitle(tr("Starting VM"));
        prompt.setText(tr("It may not be possible to start the selected VMs as they are using ISOs from an SR which is unavailable.\n\n"
                          "Would you like to eject these ISOs before continuing?"));
        QPushButton* ejectButton = prompt.addButton(tr("&Eject"), QMessageBox::AcceptRole);
        QPushButton* ignoreButton = prompt.addButton(tr("&Ignore"), QMessageBox::DestructiveRole);
        prompt.addButton(QMessageBox::Cancel);
        prompt.exec();

        if (prompt.clickedButton() == ignoreButton)
        {
            brokenVbds.clear();
        }
        else if (prompt.clickedButton() != ejectButton)
        {
            return false;
        }
    }

    if (!brokenVbds.isEmpty())
    {
        QList<AsyncOperation*> subActions;
        for (const QSharedPointer<VBD>& vbd : brokenVbds)
        {
            subActions.append(new ChangeVMISOAction(vm, QString(), vbd->OpaqueRef(), MainWindow::instance()));
        }
        subActions.append(createStartAction(MainWindow::instance(), vm));

        MultipleAction* multi = new MultipleAction(
            vm->GetConnection(),
            tr("Starting VMs"),
            tr("Starting VMs"),
            tr("Started"),
            subActions,
            false,
            false,
            false,
            MainWindow::instance());
        multi->RunAsync();
        return true;
    }

    VMStartAction* action = createStartAction(MainWindow::instance(), vm);
    action->RunAsync(true);
    return true;
}

QString StartVMCommand::MenuText() const
{
    return tr("Start");
}

QIcon StartVMCommand::GetIcon() const
{
    return QIcon(":/icons/start_vm.png");
}
