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

#include "deletevmsandtemplatescommand.h"
#include "../../mainwindow.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xencache.h"

DeleteVMsAndTemplatesCommand::DeleteVMsAndTemplatesCommand(MainWindow* mainWindow, QObject* parent) : DeleteVMCommand(mainWindow, parent)
{
}

bool DeleteVMsAndTemplatesCommand::CanRun() const
{
    const QList<QSharedPointer<VM>> vms = this->collectSelectedVMs(true);
    if (vms.isEmpty())
        return false;

    for (const QSharedPointer<VM>& vm : vms)
    {
        if (this->canDeleteVm(vm, true))
            return true;
    }

    return false;
}

bool DeleteVMsAndTemplatesCommand::canRunForVM(const QString& vmRef) const
{
    if (vmRef.isEmpty())
        return false;

    QSharedPointer<XenObject> ob = this->GetObject();

    if (!ob)
        return false;

    QSharedPointer<VM> vm = ob->GetConnection()->GetCache()->ResolveObject<VM>(XenObjectType::VM, vmRef);
    if (!vm)
        return false;

    // Check if VM/template is locked
    if (vm->IsLocked())
        return false;

    // Don't allow deletion of snapshots
    if (vm->IsSnapshot())
        return false;

    // Check allowed operations
    return vm->GetAllowedOperations().contains("destroy");
}

void DeleteVMsAndTemplatesCommand::Run()
{
    const QList<QSharedPointer<VM>> vms = this->collectSelectedVMs(true);
    this->runDeleteFlow(vms, true, tr("Delete Items"), tr("Some VMs or templates cannot be deleted."));
}

QString DeleteVMsAndTemplatesCommand::MenuText() const
{
    return tr("&Delete");
}
