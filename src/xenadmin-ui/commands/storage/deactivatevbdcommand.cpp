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

DeactivateVBDCommand::DeactivateVBDCommand(MainWindow* mainWindow, QObject* parent)
    : VBDCommand(mainWindow, parent)
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

    return this->canRunVBD(vbd->OpaqueRef());
}

bool DeactivateVBDCommand::canRunVBD(const QString& vbdRef) const
{
    QSharedPointer<VBD> vbd = this->getVBD();
    if (!vbd || !vbd->IsValid())
        return false;

    XenCache* cache = vbd->GetConnection()->GetCache();

    QVariantMap vbdData = cache->ResolveObjectData("vbd", vbdRef);
    if (vbdData.isEmpty())
        return false;

    // Check if VBD is locked
    if (vbdData.value("Locked", false).toBool())
    {
        return false;
    }

    // Get VM
    QString vmRef = vbdData.value("VM").toString();
    QVariantMap vmData = cache->ResolveObjectData("vm", vmRef);
    if (vmData.isEmpty() || vmData.value("is_a_template").toBool())
    {
        return false;
    }

    // Get VDI
    QString vdiRef = vbdData.value("VDI").toString();
    if (vdiRef.isEmpty() || vdiRef == "OpaqueRef:NULL")
    {
        return false; // No VDI attached
    }

    QVariantMap vdiData = cache->ResolveObjectData("vdi", vdiRef);
    if (vdiData.isEmpty())
    {
        return false;
    }

    // Check if VDI is locked
    if (vdiData.value("Locked", false).toBool())
    {
        return false;
    }

    // Check VM is running
    QString powerState = vmData.value("power_state").toString();
    if (powerState != "Running")
    {
        return false;
    }

    // Check if system disk AND owner (boot disk cannot be unplugged)
    QString vdiType = vdiData.value("type").toString();
    if (vdiType == "system")
    {
        // C# checks GetIsOwner() - for now we'll block all system disks
        // TODO: Implement proper VBD.GetIsOwner() check
        bool isOwner = vbdData.value("device", "").toString() == "0" ||
                       vbdData.value("bootable", false).toBool();
        if (isOwner)
        {
            return false;
        }
    }

    // Check if not currently attached
    bool currentlyAttached = vbdData.value("currently_attached").toBool();
    if (!currentlyAttached)
    {
        return false;
    }

    // Check allowed operations
    QVariantList allowedOps = vbdData.value("allowed_operations").toList();
    if (!allowedOps.contains("unplug"))
    {
        return false;
    }

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

    QString vbdRef = vbd->OpaqueRef();
    XenCache* cache = vbd->GetConnection()->GetCache();

    QVariantMap vbdData = cache->ResolveObjectData("vbd", vbdRef);
    if (vbdData.isEmpty())
        return;

    // Get VDI and VM names for status message
    QString vdiRef = vbdData.value("VDI").toString();
    QString vmRef = vbdData.value("VM").toString();

    QVariantMap vdiData = cache->ResolveObjectData("vdi", vdiRef);
    QVariantMap vmData = cache->ResolveObjectData("vm", vmRef);

    QString vdiName = vdiData.value("name_label", "disk").toString();
    QString vmName = vmData.value("name_label", "VM").toString();

    // Execute unplug operation directly (matches C# DelegatedAsyncAction pattern)
    try
    {
        XenAPI::VBD::unplug(vbd->GetConnection()->GetSession(), vbdRef);

        this->mainWindow()->ShowStatusMessage(
            QString("Successfully deactivated virtual disk '%1' from VM '%2'").arg(vdiName, vmName),
            5000);
    } catch (const std::exception& e)
    {
        QMessageBox::warning(
            this->mainWindow(),
            "Deactivate Virtual Disk Failed",
            QString("Failed to deactivate virtual disk '%1' from VM '%2': %3")
                .arg(vdiName, vmName, e.what()));
    }
}
