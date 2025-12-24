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

#include "migratevirtualdiskcommand.h"
#include "mainwindow.h"
#include "xenlib.h"
#include "xencache.h"
#include "xen/vdi.h"
#include "dialogs/migratevirtualdiskdialog.h"
#include <QMessageBox>

MigrateVirtualDiskCommand::MigrateVirtualDiskCommand(MainWindow* mainWindow, QObject* parent)
    : VDICommand(mainWindow, parent)
{
}

bool MigrateVirtualDiskCommand::CanRun() const
{
    QSharedPointer<VDI> vdi = this->getVDI();
    if (!vdi || !vdi->IsValid())
        return false;

    XenCache* cache = vdi->GetConnection()->GetCache();
    QVariantMap vdiData = cache->ResolveObjectData("vdi", vdi->OpaqueRef());
    if (vdiData.isEmpty())
        return false;

    return this->canBeMigrated(vdiData);
}

void MigrateVirtualDiskCommand::Run()
{
    QSharedPointer<VDI> vdi = this->getVDI();
    if (!vdi || !vdi->IsValid())
        return;

    XenCache* cache = vdi->GetConnection()->GetCache();
    QString vdiRef = vdi->OpaqueRef();
    QVariantMap vdiData = cache->ResolveObjectData("vdi", vdiRef);
    if (vdiData.isEmpty())
    {
        QMessageBox::warning(this->mainWindow(), tr("Error"), tr("Unable to retrieve VDI information."));
        return;
    }

    // Double-check migration eligibility with detailed error messages
    if (!this->canBeMigrated(vdiData))
    {
        QString reason;

        if (vdiData.value("is_a_snapshot", false).toBool())
            reason = tr("Cannot migrate: VDI is a snapshot.");
        else if (vdiData.value("Locked", false).toBool())
            reason = tr("Cannot migrate: VDI is locked (in use).");
        else if (this->isHAType(vdiData))
            reason = tr("Cannot migrate: VDI is an HA type (statefile or redo log).");
        else if (vdiData.value("cbt_enabled", false).toBool())
            reason = tr("Cannot migrate: VDI has changed block tracking (CBT) enabled.");
        else if (this->isMetadataForDR(vdiData))
            reason = tr("Cannot migrate: VDI is metadata for disaster recovery.");
        else
        {
            // Check VBD count
            QList<QString> vbdRefs = vdiData.value("VBDs", QVariantList()).toStringList();
            if (vbdRefs.isEmpty())
            {
                reason = tr("Cannot migrate: VDI has no VBDs attached.");
            } else
            {
                // Check SR
                QString srRef = vdiData.value("SR", "").toString();
                if (srRef.isEmpty())
                {
                    reason = tr("Cannot migrate: VDI has no SR reference.");
                } else
                {
                    QVariantMap srData = this->xenLib()->getCache()->ResolveObjectData("sr", srRef);
                    if (srData.isEmpty())
                        reason = tr("Cannot migrate: Unable to retrieve SR information.");
                    else if (this->isHBALunPerVDI(srData))
                        reason = tr("Cannot migrate: Unsupported SR type (HBA LUN-per-VDI).");
                    else if (!this->supportsStorageMigration(srData))
                        reason = tr("Cannot migrate: SR does not support storage migration.");
                    else
                        reason = tr("Cannot migrate: Unknown reason.");
                }
            }
        }

        QMessageBox::information(this->mainWindow(), tr("Cannot Migrate"), reason);
        return;
    }

    // Open migrate dialog
    MigrateVirtualDiskDialog* dialog = new MigrateVirtualDiskDialog(
        this->xenLib(),
        vdiRef,
        this->mainWindow());

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

QString MigrateVirtualDiskCommand::MenuText() const
{
    return tr("&Migrate Virtual Disk...");
}

bool MigrateVirtualDiskCommand::canBeMigrated(const QVariantMap& vdiData) const
{
    // Check basic VDI properties
    if (vdiData.value("is_a_snapshot", false).toBool())
        return false;

    if (vdiData.value("Locked", false).toBool())
        return false;

    if (this->isHAType(vdiData))
        return false;

    if (vdiData.value("cbt_enabled", false).toBool())
        return false;

    if (this->isMetadataForDR(vdiData))
        return false;

    // Check that VDI has at least one VBD
    QList<QVariant> vbdList = vdiData.value("VBDs", QVariantList()).toList();
    if (vbdList.isEmpty())
        return false;

    // Check SR properties
    QString srRef = vdiData.value("SR", "").toString();
    if (srRef.isEmpty())
        return false;

    QVariantMap srData = this->xenLib()->getCache()->ResolveObjectData("sr", srRef);
    if (srData.isEmpty())
        return false;

    if (this->isHBALunPerVDI(srData))
        return false;

    if (!this->supportsStorageMigration(srData))
        return false;

    return true;
}

bool MigrateVirtualDiskCommand::isHAType(const QVariantMap& vdiData) const
{
    QString type = vdiData.value("type", "").toString();
    return (type == "ha_statefile" || type == "redo_log");
}

bool MigrateVirtualDiskCommand::isMetadataForDR(const QVariantMap& vdiData) const
{
    // Check if VDI has metadata_of_pool set (indicates DR metadata)
    QString metadataOfPool = vdiData.value("metadata_of_pool", "").toString();
    return !metadataOfPool.isEmpty() && metadataOfPool != "OpaqueRef:NULL";
}

bool MigrateVirtualDiskCommand::isHBALunPerVDI(const QVariantMap& srData) const
{
    // HBA SRs have is_tools_sr = false and type = "lvmohba"
    // Additionally check sm_config for "allocation" = "thick"
    QString srType = srData.value("type", "").toString();

    if (srType == "lvmohba" || srType == "lvmofc")
    {
        // These are HBA types - check if LUN-per-VDI
        QVariantMap smConfig = srData.value("sm_config", QVariantMap()).toMap();
        QString allocation = smConfig.value("allocation", "").toString();

        // LUN-per-VDI uses thick allocation
        return (allocation == "thick");
    }

    return false;
}

bool MigrateVirtualDiskCommand::supportsStorageMigration(const QVariantMap& srData) const
{
    // Check SR capabilities for storage migration support
    QVariantList capabilities = srData.value("capabilities", QVariantList()).toList();

    // Look for "VDI_MIRROR" capability which indicates migration support
    for (const QVariant& cap : capabilities)
    {
        if (cap.toString() == "VDI_MIRROR")
            return true;
    }

    // Also check if SR type is known to support migration
    QString srType = srData.value("type", "").toString();
    QStringList supportedTypes = {
        "lvm", "ext", "nfs", "lvmoiscsi", "lvmohba",
        "smb", "cifs", "cslg", "gfs2"};

    return supportedTypes.contains(srType);
}
