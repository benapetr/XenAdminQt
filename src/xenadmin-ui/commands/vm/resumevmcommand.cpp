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

#include "resumevmcommand.h"
#include "vmoperationhelpers.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/actions/vm/vmresumeaction.h"
#include "xenlib/xen/failure.h"
#include <QMessageBox>
#include <QTimer>
#include <QMetaObject>
#include <QPointer>

namespace
{
    bool canResumeVm(const QSharedPointer<VM>& vm)
    {
        if (!vm)
            return false;

        if (vm->GetPowerState() != "Suspended")
            return false;

        return vm->GetAllowedOperations().contains("resume");
    }
} // namespace

ResumeVMCommand::ResumeVMCommand(MainWindow* mainWindow, QObject* parent) : VMCommand(mainWindow, parent)
{
}

bool ResumeVMCommand::CanRun() const
{
    const QList<QSharedPointer<VM>> vms = this->getVMs();
    if (!vms.isEmpty())
    {
        for (const QSharedPointer<VM>& vm : vms)
        {
            if (canResumeVm(vm))
                return true;
        }
        return false;
    }

    return canResumeVm(this->getVM());
}

void ResumeVMCommand::Run()
{
    const QList<QSharedPointer<VM>> vms = this->getVMs();
    if (vms.size() > 1)
    {
        int ret = QMessageBox::question(this->mainWindow(), tr("Resume VMs"), tr("Are you sure you want to resume the selected VMs?"), QMessageBox::Yes | QMessageBox::No);
        if (ret != QMessageBox::Yes)
            return;

        for (const QSharedPointer<VM>& vm : vms)
        {
            if (canResumeVm(vm))
                RunForVm(vm, vm->GetName(), false);
        }
        return;
    }
    QSharedPointer<VM> vm = vms.size() == 1 ? vms.first() : this->getVM();
    if (!vm || !canResumeVm(vm))
        return;

    RunForVm(vm, vm->GetName(), true);
}

bool ResumeVMCommand::RunForVm(const QSharedPointer<VM>& vm, const QString& vmName, bool promptUser)
{
    if (!vm)
        return false;

    QString vmRef = vm->OpaqueRef();

    QString displayName = vmName;
    if (displayName.isEmpty())
    {
        displayName = vm->GetName();
    }

    if (promptUser)
    {
        int ret = QMessageBox::question(this->mainWindow(), tr("Resume VM"), tr("Are you sure you want to resume VM '%1'?").arg(displayName), QMessageBox::Yes | QMessageBox::No);

        if (ret != QMessageBox::Yes)
            return false;
    }

    // Get XenConnection from VM
    XenConnection* conn = vm->GetConnection();
    if (!conn || !conn->IsConnected())
    {
        QMessageBox::warning(this->mainWindow(), "Not Connected", "Not connected to XenServer");
        return false;
    }

    // Create VMResumeAction with diagnosis callbacks (matches C# pattern)
    // Note: VMResumeAction is for resuming from Suspended state (from disk)
    // For unpausing from Paused state (in memory), use VMUnpause instead
    QPointer<MainWindow> mainWindow = this->mainWindow();

    VMResumeAction* action = new VMResumeAction(
        vm,
        nullptr,  // WarningDialogHAInvalidConfig callback (TODO: implement if needed)
        [conn, vmRef, displayName, mainWindow](VMStartAbstractAction* abstractAction, const Failure& failure) {
            Q_UNUSED(abstractAction)
            if (!mainWindow)
                return;

            Failure failureCopy = failure;
            QMetaObject::invokeMethod(mainWindow, [mainWindow, conn, vmRef, displayName, failureCopy]() {
                if (!mainWindow)
                    return;

                VMOperationHelpers::StartDiagnosisForm(conn, vmRef, displayName, false, failureCopy, mainWindow);
            }, Qt::QueuedConnection);
        },
        this->mainWindow());

    // Register with OperationManager for history tracking
    OperationManager::instance()->RegisterOperation(action);

    // Connect completion signal for cleanup
    connect(action, &AsyncOperation::completed, this, [action]() {
        // Auto-delete when complete
        action->deleteLater();
    });

    // Run action asynchronously
    action->RunAsync();
    return true;
}

QString ResumeVMCommand::MenuText() const
{
    return "Resume VM";
}

QIcon ResumeVMCommand::GetIcon() const
{
    return QIcon(":/icons/resume.png");
}
