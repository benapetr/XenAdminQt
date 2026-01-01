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

    return this->canRunVBD(vbd->OpaqueRef());
}

bool ActivateVBDCommand::canRunVBD(const QString& vbdRef) const
{
    QSharedPointer<VBD> vbd = this->getVBD();
    if (!vbd || !vbd->IsValid())
        return false;

    XenCache* cache = vbd->GetConnection()->GetCache();

    QVariantMap vbdData = cache->ResolveObjectData("vbd", vbdRef);
    if (vbdData.isEmpty())
        return false;

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

    // Check VM is running
    QString powerState = vmData.value("power_state").toString();
    if (powerState != "Running")
    {
        return false;
    }

    // Check if system disk
    QString vdiType = vdiData.value("type").toString();
    if (vdiType == "system")
    {
        return false;
    }

    // Check for IO drivers (simplified - C# has complex version checking)
    // For now, we'll assume IO drivers are available

    // Check if already attached
    bool currentlyAttached = vbdData.value("currently_attached").toBool();
    if (currentlyAttached)
    {
        return false;
    }

    // Check allowed operations
    QVariantList allowedOps = vbdData.value("allowed_operations").toList();
    if (!allowedOps.contains("plug"))
    {
        return false;
    }

    return true;
}

QString ActivateVBDCommand::getCantRunReasonVBD(QSharedPointer<VBD> vbd) const
{
    if (!vbd || !vbd->IsConnected())
    {
        return "VBD not found";
    }

    XenCache* cache = vbd->GetConnection()->GetCache();
    QVariantMap vbdData = vbd->GetData();

    // Get VM
    QString vmRef = vbdData.value("VM").toString();
    QVariantMap vmData = cache->ResolveObjectData("vm", vmRef);
    if (vmData.isEmpty())
    {
        return "VM not found";
    }

    if (vmData.value("is_a_template").toBool())
    {
        return "Cannot activate disk on template";
    }

    // Get VDI
    QString vdiRef = vbdData.value("VDI").toString();
    if (vdiRef.isEmpty() || vdiRef == "OpaqueRef:NULL")
    {
        return "No VDI attached to this VBD";
    }

    QVariantMap vdiData = cache->ResolveObjectData("vdi", vdiRef);
    if (vdiData.isEmpty())
    {
        return "VDI not found";
    }

    // Get SR to check if contactable
    QString srRef = vdiData.value("SR").toString();
    QVariantMap srData = cache->ResolveObjectData("sr", srRef);
    if (srData.isEmpty())
    {
        return "SR could not be contacted";
    }

    // Check if VDI is locked
    if (vdiData.value("Locked", false).toBool())
    {
        return "Virtual disk is in use";
    }

    // Check VM power state
    QString powerState = vmData.value("power_state").toString();
    if (powerState != "Running")
    {
        QString vmName = vmData.value("name_label").toString();
        return QString("VM '%1' is not running").arg(vmName);
    }

    // Check if system disk
    QString vdiType = vdiData.value("type").toString();
    if (vdiType == "system")
    {
        return "Cannot hot-plug system disk";
    }

    // Check if already attached
    bool currentlyAttached = vbdData.value("currently_attached").toBool();
    if (currentlyAttached)
    {
        QString vmName = vmData.value("name_label").toString();
        return QString("Virtual disk is already active on %1").arg(vmName);
    }

    return "Unknown reason";
}

bool ActivateVBDCommand::areIODriversNeededAndMissing(const QVariantMap& vmData) const
{
    // Simplified check - C# has complex API version checking (Ely or greater)
    // For modern XenServer, IO drivers are not strictly required for hot-plug
    // TODO: Implement full virtualization status checking
    Q_UNUSED(vmData);
    return false;
}

void ActivateVBDCommand::Run()
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

    // Execute plug operation directly (matches C# DelegatedAsyncAction pattern)
    try
    {
        XenAPI::VBD::plug(vbd->GetConnection()->GetSession(), vbdRef);

        this->mainWindow()->showStatusMessage(
            QString("Successfully activated virtual disk '%1' on VM '%2'").arg(vdiName, vmName),
            5000);
    } catch (const std::exception& e)
    {
        QMessageBox::warning(
            this->mainWindow(),
            "Activate Virtual Disk Failed",
            QString("Failed to activate virtual disk '%1' on VM '%2': %3")
                .arg(vdiName, vmName, e.what()));
    }
}
