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

#include "activatevbdcommand.h"
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

ActivateVBDCommand::ActivateVBDCommand(MainWindow* mainWindow, QObject* parent) : VBDCommand(mainWindow, parent)
{
}

QString ActivateVBDCommand::MenuText() const
{
    return "Activate Virtual Disk";
}

bool ActivateVBDCommand::CanRun() const
{
    QSharedPointer<VBD> vbd = this->getVBD();
    if (!vbd || !vbd->IsValid())
        return false;

    return this->canRunVBD(vbd);
}

bool ActivateVBDCommand::canRunVBD(const QSharedPointer<VBD> &vbd) const
{
    if (!vbd || !vbd->IsValid())
        return false;

    QSharedPointer<VM> vm = vbd->GetVM();
    if (!vm || !vm->IsRealVM() || vm->GetPowerState() != "Running")
        return false;

    QSharedPointer<VDI> vdi = vbd->GetVDI();
    // Check if system disk
    if (!vdi || vdi->GetType() == "system")
        return false;

    // Check for IO drivers (simplified - C# has complex version checking)
    // For now, we'll assume IO drivers are available
    if (areIODriversNeededAndMissing(vm))
        return false;

    // Check if already attached
    if (vbd->CurrentlyAttached())
        return false;

    // Check allowed operations
    if (!vbd->AllowedOperations().contains("plug"))
        return false;

    return true;
}

QString ActivateVBDCommand::getCantRunReasonVBD(const QSharedPointer<VBD> &vbd) const
{
    if (!vbd || !vbd->IsConnected())
        return "VBD not found";

    QSharedPointer<VM> vm = vbd->GetVM();
    if (!vm)
        return "VM not found";

    if (vm->IsTemplate())
        return "Cannot activate disk on template";

    QSharedPointer<VDI> vdi = vbd->GetVDI();
    if (!vdi)
        return "VDI not found";

    // Get SR to check if contactable
    if (!vdi->GetSR())
        return "SR could not be contacted";

    // Check if VDI is locked
    if (vdi->IsLocked())
        return "Virtual disk is in use";

    // Check VM power state
    if (vm->GetPowerState() != "Running")
        return QString("VM '%1' is not running").arg(vm->GetName());

    // Check if system disk
    if (vdi->GetType() == "system")
        return "Cannot hot-plug system disk";

    // Check if already attached
    if (vbd->CurrentlyAttached())
        return QString("Virtual disk is already active on %1").arg(vm->GetName());

    return "Unknown reason";
}

bool ActivateVBDCommand::areIODriversNeededAndMissing(const QSharedPointer<VM> &vm) const
{
    if (!vm || !vm->GetConnection())
        return false;

    XenAPI::Session* session = vm->GetConnection()->GetSession();
    if (session && session->ApiVersionMeets(APIVersion::API_2_6))
        return false;

    constexpr int IoDriversInstalledFlag = 4;
    const int status = vm->GetVirtualizationStatus();
    return (status & IoDriversInstalledFlag) == 0;
}

void ActivateVBDCommand::Run()
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
                                                   QString("Activating disk '%1' on VM '%2'").arg(vdiName, vmName),
                                                   QString("Activating virtual disk '%1'...").arg(vdiName),
                                                   [vbd](DelegatedAsyncOperation* self)
                                                   {
                                                       XenAPI::VBD::plug(self->GetSession(), vbd->OpaqueRef());
                                                   },
                                                   this->mainWindow());
        action->SetVM(vm);
        actions.append(action);
    }

    if (actions.isEmpty())
        return;

    if (actions.size() == 1)
    {
        actions.first()->RunAsync(true);
        return;
    }

    this->RunMultipleActions(actions, QObject::tr("Activate Virtual Disks"), QObject::tr("Activating virtual disks..."), QObject::tr("Completed"), true, true);
}
