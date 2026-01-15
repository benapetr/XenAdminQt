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
#include "xenlib/xencache.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/session.h"
#include "xenlib/xen/vbd.h"
#include "xenlib/xen/vdi.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/xenapi/xenapi_VBD.h"
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

    if (!vbd->CurrentlyAttached())
        return false;

    // Check allowed operations
    QStringList allowedOps = vbd->AllowedOperations();
    if (!allowedOps.contains("unplug"))
        return false;

    return true;
}

bool DeactivateVBDCommand::areIODriversNeededAndMissing(const QVariantMap& vmData) const
{
    // Simplified check - C# has complex API version checking (Ely or greater)
    // For modern XenServer, IO drivers are not strictly required for hot-unplug
    // TODO: Implement full virtualization status checking
    Q_UNUSED(vmData);
    return false;
}

void DeactivateVBDCommand::Run()
{
    QSharedPointer<VBD> vbd = this->getVBD();
    if (!vbd || !vbd->IsValid())
        return;

    QSharedPointer<VDI> vdi = vbd->GetVDI();
    QSharedPointer<VM> vm = vbd->GetVM();

    if (!vm || !vdi)
        return;

    QString vdiName = vdi->GetName();
    QString vmName = vm->GetName();

    // Execute unplug operation directly (matches C# DelegatedAsyncAction pattern)
    try
    {
        XenAPI::VBD::unplug(vbd->GetConnection()->GetSession(), vbd->OpaqueRef());

        this->mainWindow()->ShowStatusMessage(QString("Successfully deactivated virtual disk '%1' from VM '%2'").arg(vdiName, vmName), 5000);
    } catch (const std::exception& e)
    {
        QMessageBox::warning(
            this->mainWindow(),
            "Deactivate Virtual Disk Failed",
            QString("Failed to deactivate virtual disk '%1' from VM '%2': %3").arg(vdiName, vmName, e.what()));
    }
}
