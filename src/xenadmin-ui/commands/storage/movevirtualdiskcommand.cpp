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

#include "movevirtualdiskcommand.h"
#include "../../mainwindow.h"
#include "../../dialogs/movevirtualdiskdialog.h"
#include "xenlib.h"
#include "xencache.h"
#include "xen/vdi.h"
#include <QMessageBox>

MoveVirtualDiskCommand::MoveVirtualDiskCommand(MainWindow* mainWindow, QObject* parent)
    : VDICommand(mainWindow, parent)
{
}

bool MoveVirtualDiskCommand::CanRun() const
{
    QSharedPointer<VDI> vdi = this->getVDI();
    if (!vdi || !vdi->isValid())
        return false;

    XenCache* cache = vdi->connection()->getCache();
    QVariantMap vdiData = cache->ResolveObjectData("vdi", vdi->opaqueRef());
    if (vdiData.isEmpty())
        return false;

    return this->canBeMoved(vdiData);
}

void MoveVirtualDiskCommand::Run()
{
    QSharedPointer<VDI> vdi = this->getVDI();
    if (!vdi || !vdi->isValid())
        return;

    // Open the move virtual disk dialog
    MoveVirtualDiskDialog* dialog = new MoveVirtualDiskDialog(
        this->xenLib(),
        vdi->opaqueRef(),
        this->mainWindow());

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

QString MoveVirtualDiskCommand::MenuText() const
{
    return tr("Move...");
}

bool MoveVirtualDiskCommand::canBeMoved(const QVariantMap& vdiData) const
{
    // Check if VDI is a snapshot
    if (vdiData.value("is_a_snapshot", false).toBool())
    {
        return false;
    }

    // Check if VDI is locked (in use)
    if (vdiData.value("locked", false).toBool() ||
        vdiData.value("currently_attached", false).toBool())
    {
        return false;
    }

    // Check if VDI is HA type
    if (this->isHAType(vdiData))
    {
        return false;
    }

    // Check if VDI has CBT enabled
    if (vdiData.value("cbt_enabled", false).toBool())
    {
        return false;
    }

    // Check if VDI is DR metadata
    if (this->isMetadataForDR(vdiData))
    {
        return false;
    }

    // Get SR
    QString srRef = vdiData.value("SR").toString();
    if (srRef.isEmpty())
    {
        return false;
    }

    QVariantMap srData = this->xenLib()->getCache()->ResolveObjectData("sr", srRef);
    if (srData.isEmpty())
    {
        return false;
    }

    // Check if SR is HBA LUN-per-VDI type
    QString srType = srData.value("type").toString();

    // HBA type SRs (lvmohba, lvmoiscsi with LUN-per-VDI) don't support move
    if (srType == "lvmohba")
    {
        return false;
    }

    // Check if any VMs using this VDI are running
    QString vdiUuid = vdiData.value("uuid").toString();
    if (!vdiUuid.isEmpty() && this->isVDIInUseByRunningVM(vdiUuid))
    {
        return false;
    }

    return true;
}

bool MoveVirtualDiskCommand::isVDIInUseByRunningVM(const QString& vdiRef) const
{
    // Get all VBDs
    QList<QVariantMap> vbds = this->xenLib()->getCache()->GetAllData("vbd");

    for (const QVariantMap& vbdData : vbds)
    {
        // Check if this VBD references our VDI
        QString vbdVDIRef = vbdData.value("VDI").toString();
        if (vbdVDIRef != vdiRef)
        {
            continue;
        }

        // Get the VM
        QString vmRef = vbdData.value("VM").toString();
        if (vmRef.isEmpty())
        {
            continue;
        }

        QVariantMap vmData = this->xenLib()->getCache()->ResolveObjectData("vm", vmRef);
        if (vmData.isEmpty())
        {
            continue;
        }

        // Check VM power state
        QString powerState = vmData.value("power_state").toString();
        if (powerState != "Halted")
        {
            return true; // VM is running
        }
    }

    return false;
}

bool MoveVirtualDiskCommand::isHAType(const QVariantMap& vdiData) const
{
    QString type = vdiData.value("type").toString();
    return type == "ha";
}

bool MoveVirtualDiskCommand::isMetadataForDR(const QVariantMap& vdiData) const
{
    // Check if VDI is DR metadata
    // DR metadata VDIs have specific tags or sm_config entries
    QVariantMap smConfig = vdiData.value("sm_config").toMap();

    // Check for DR-related tags
    QVariantList tags = vdiData.value("tags").toList();
    for (const QVariant& tag : tags)
    {
        QString tagStr = tag.toString();
        if (tagStr.contains("disaster_recovery", Qt::CaseInsensitive))
        {
            return true;
        }
    }

    // Check sm_config for DR markers
    if (smConfig.contains("dr_metadata") ||
        smConfig.value("disaster_recovery", false).toBool())
    {
        return true;
    }

    return false;
}
