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
#include "../../../xenlib/xencache.h"
#include "../../../xenlib/xenlib.h"
#include "../../../xenlib/xen/connection.h"
#include "../../../xenlib/xen/session.h"
#include "../../../xenlib/xen/xenapi/xenapi_VBD.h"
#include "../../mainwindow.h"
#include <QMessageBox>
#include <QDebug>

DeactivateVBDCommand::DeactivateVBDCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

QString DeactivateVBDCommand::menuText() const
{
    return "Deactivate Virtual Disk";
}

bool DeactivateVBDCommand::canRun() const
{
    if (getSelectedObjectType() != "vbd")
    {
        return false;
    }

    QString vbdRef = getSelectedObjectRef();
    if (vbdRef.isEmpty())
    {
        return false;
    }

    return canRunVBD(vbdRef);
}

bool DeactivateVBDCommand::canRunVBD(const QString& vbdRef) const
{
    XenCache* cache = mainWindow()->xenLib()->getCache();
    if (!cache)
    {
        return false;
    }

    QVariantMap vbdData = cache->ResolveObjectData("vbd", vbdRef);
    if (vbdData.isEmpty())
    {
        return false;
    }

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

QString DeactivateVBDCommand::getCantRunReasonVBD(const QString& vbdRef) const
{
    XenCache* cache = mainWindow()->xenLib()->getCache();
    if (!cache)
    {
        return "Cache not available";
    }

    QVariantMap vbdData = cache->ResolveObjectData("vbd", vbdRef);
    if (vbdData.isEmpty())
    {
        return "VBD not found";
    }

    // Get VM
    QString vmRef = vbdData.value("VM").toString();
    QVariantMap vmData = cache->ResolveObjectData("vm", vmRef);
    if (vmData.isEmpty())
    {
        return "VM not found";
    }

    if (vmData.value("is_a_template").toBool())
    {
        return "Cannot deactivate disk on template";
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

    // Check if VBD is locked
    if (vbdData.value("Locked", false).toBool())
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
        bool isOwner = vbdData.value("device", "").toString() == "0" ||
                       vbdData.value("bootable", false).toBool();
        if (isOwner)
        {
            return "Cannot hot-unplug system boot disk";
        }
    }

    // Check if not currently attached
    bool currentlyAttached = vbdData.value("currently_attached").toBool();
    if (!currentlyAttached)
    {
        QString vmName = vmData.value("name_label").toString();
        return QString("Virtual disk is not active on %1").arg(vmName);
    }

    return "Unknown reason";
}

bool DeactivateVBDCommand::areIODriversNeededAndMissing(const QVariantMap& vmData) const
{
    // Simplified check - C# has complex API version checking (Ely or greater)
    // For modern XenServer, IO drivers are not strictly required for hot-unplug
    // TODO: Implement full virtualization status checking
    Q_UNUSED(vmData);
    return false;
}

void DeactivateVBDCommand::run()
{
    QString vbdRef = getSelectedObjectRef();
    if (vbdRef.isEmpty())
    {
        return;
    }

    XenCache* cache = mainWindow()->xenLib()->getCache();
    if (!cache)
    {
        return;
    }

    QVariantMap vbdData = cache->ResolveObjectData("vbd", vbdRef);
    if (vbdData.isEmpty())
    {
        return;
    }

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
        XenAPI::VBD::unplug(mainWindow()->xenLib()->getConnection()->getSession(), vbdRef);

        mainWindow()->showStatusMessage(
            QString("Successfully deactivated virtual disk '%1' from VM '%2'").arg(vdiName, vmName),
            5000);
    } catch (const std::exception& e)
    {
        QMessageBox::warning(
            mainWindow(),
            "Deactivate Virtual Disk Failed",
            QString("Failed to deactivate virtual disk '%1' from VM '%2': %3")
                .arg(vdiName, vmName, e.what()));
    }
}
