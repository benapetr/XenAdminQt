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

#include "deactivatevbdcommand.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/session.h"
#include "xenlib/xen/apiversion.h"
#include "xenlib/xen/vbd.h"
#include "xenlib/xen/vdi.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/xenapi/xenapi_VBD.h"
#include "xenlib/xen/actions/delegatedasyncoperation.h"
#include "../../mainwindow.h"
#include <QMessageBox>
#include <QDebug>

DeactivateVBDCommand::DeactivateVBDCommand(MainWindow* mainWindow, QObject* parent) : VBDCommand(mainWindow, parent)
{
}

QString DeactivateVBDCommand::MenuText() const
{
    return "Deactivate Virtual Disk";
}

bool DeactivateVBDCommand::CanRun() const
{
    QSharedPointer<VBD> vbd = this->getVBD();
    if (!vbd || !vbd->IsValid())
        return false;

    return this->canRunVBD(vbd);
}

bool DeactivateVBDCommand::canRunVBD(QSharedPointer<VBD> vbd) const
{
    if (!vbd || !vbd->IsValid())
        return false;

    if (vbd->IsLocked())
        return false;

    QSharedPointer<VM> vm = vbd->GetVM();
    if (!vm || vm->IsTemplate())
        return false;

    QSharedPointer<VDI> vdi = vbd->GetVDI();
    if (!vdi)
        return false; // No VDI attached

    if (vdi->IsLocked())
        return false;

    // Check VM is running
    QString powerState = vm->GetPowerState();
    if (powerState != "Running")
        return false;

    // Check if system disk AND owner (boot disk cannot be unplugged)
    QString vdiType = vdi->GetType();
    if (vdiType == "system" && vbd->IsOwner())
        return false;

    // C# parity: require IO drivers only on pre-Ely hosts.
    if (this->areIODriversNeededAndMissing(vm))
        return false;

    if (!vbd->CurrentlyAttached())
        return false;

    // Check allowed operations
    QStringList allowedOps = vbd->AllowedOperations();
    if (!allowedOps.contains("unplug"))
        return false;

    return true;
}

bool DeactivateVBDCommand::areIODriversNeededAndMissing(const QSharedPointer<VM>& vm) const
{
    if (!vm || !vm->GetConnection())
        return false;

    XenAPI::Session* session = vm->GetConnection()->GetSession();
    if (session && session->ApiVersionMeets(APIVersion::API_2_6))
        return false;

    // C# equivalent:
    // !vm.GetVirtualizationStatus(out _).HasFlag(VM.VirtualizationStatus.IoDriversInstalled)
    constexpr int IoDriversInstalledFlag = 4;
    const int status = vm->GetVirtualizationStatus();
    return (status & IoDriversInstalledFlag) == 0;
}

void DeactivateVBDCommand::Run()
{
    QList<QSharedPointer<VBD>> candidates;
    const QList<QSharedPointer<XenObject>> selectedObjects = this->getSelectedObjects();
    for (const QSharedPointer<XenObject>& object : selectedObjects)
    {
        if (!object || object->GetObjectType() != XenObjectType::VBD)
            continue;

        QSharedPointer<VBD> vbd = qSharedPointerDynamicCast<VBD>(object);
        if (vbd && vbd->IsValid() && this->canRunVBD(vbd))
            candidates.append(vbd);
    }

    if (candidates.isEmpty())
    {
        QSharedPointer<VBD> vbd = this->getVBD();
        if (vbd && vbd->IsValid() && this->canRunVBD(vbd))
            candidates.append(vbd);
    }

    if (candidates.isEmpty())
        return;

    QList<AsyncOperation*> actions;
    actions.reserve(candidates.size());

    for (const QSharedPointer<VBD>& vbd : candidates)
    {
        QSharedPointer<VDI> vdi = vbd->GetVDI();
        QSharedPointer<VM> vm = vbd->GetVM();
        if (!vm || !vdi || !vbd->GetConnection())
            continue;

        const QString vdiName = vdi->GetName();
        const QString vmName = vm->GetName();

        auto* action = new DelegatedAsyncOperation(vbd->GetConnection(),
                                                   QString("Deactivating disk '%1' on VM '%2'").arg(vdiName, vmName),
                                                   QString("Deactivating virtual disk '%1'...").arg(vdiName),
                                                   [vbd](DelegatedAsyncOperation* self)
                                                   {
                                                       XenAPI::VBD::unplug(self->GetSession(), vbd->OpaqueRef());
                                                   },
                                                   this->mainWindow());
        action->SetVM(vm);
        actions.append(action);
    }

    if (actions.isEmpty())
        return;

    if (actions.size() == 1)
    {
        actions.first()->RunAsync();
        return;
    }

    this->RunMultipleActions(actions, QObject::tr("Deactivate Virtual Disks"), QObject::tr("Deactivating virtual disks..."), QObject::tr("Completed"), true, true);
}
