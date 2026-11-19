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

#include "vtpmcommand.h"
#include "../../mainwindow.h"
#include "../../dialogs/vtpmmanagementdialog.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/vm.h"

VtpmCommand::VtpmCommand(MainWindow* mainWindow, QObject* parent) : VMCommand(mainWindow, parent)
{
}

bool VtpmCommand::CanRun() const
{
    // C# also requires the feature not to be licence restricted; not ported (XCP-ng)
    const QList<QSharedPointer<VM>> vms = this->getVMs();
    if (vms.size() > 1)
        return false;

    const QSharedPointer<VM> vm = this->getVM();
    return vm && vm->IsRealVM()
        && Host::XapiEqualOrGreater(vm->GetConnection(), "22.26.0")
        && vm->IsHVM() && vm->IsDefaultBootModeUefi();
}

QString VtpmCommand::CantRunReason() const
{
    const QSharedPointer<VM> vm = this->getVM();
    if (vm && Host::XapiEqualOrGreater(vm->GetConnection(), "22.26.0") && !(vm->IsHVM() && vm->IsDefaultBootModeUefi()))
        return tr("TPM is supported only for VMs using the UEFI or UEFI secure boot modes.");
    return QString();
}

void VtpmCommand::Run()
{
    if (!this->CanRun())
        return;
    VtpmManagementDialog::ShowForVM(this->getVM(), this->mainWindow());
}

QString VtpmCommand::MenuText() const
{
    return tr("Mana&ge vTPMs...");
}
