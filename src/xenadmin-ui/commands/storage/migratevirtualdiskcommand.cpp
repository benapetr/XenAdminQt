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
#include "xen/vdi.h"
#include "xen/vbd.h"
#include "xen/sr.h"
#include "dialogs/migratevirtualdiskdialog.h"
#include <QMessageBox>

MigrateVirtualDiskCommand::MigrateVirtualDiskCommand(MainWindow* mainWindow, QObject* parent) : VDICommand(mainWindow, parent)
{
}

bool MigrateVirtualDiskCommand::CanRun() const
{
    QSharedPointer<VDI> vdi = this->getVDI();
    if (!vdi || !vdi->IsValid())
        return false;

    QString reason;
    return this->canBeMigrated(vdi, reason);
}

void MigrateVirtualDiskCommand::Run()
{
    QSharedPointer<VDI> vdi = this->getVDI();
    if (!vdi || !vdi->IsValid())
        return;

    // Double-check migration eligibility with detailed error messages
    QString reason;
    if (!this->canBeMigrated(vdi, reason))
    {
        QMessageBox::information(this->mainWindow(), tr("Cannot Migrate"), reason);
        return;
    }

    // Open migrate dialog
    MigrateVirtualDiskDialog* dialog = new MigrateVirtualDiskDialog(vdi, this->mainWindow());

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

QString MigrateVirtualDiskCommand::MenuText() const
{
    return tr("&Migrate Virtual Disk...");
}

bool MigrateVirtualDiskCommand::canBeMigrated(const QSharedPointer<VDI>& vdi, QString& reason) const
{
    // Check basic VDI properties
    if (!vdi || !vdi->IsValid())
    {
        reason = tr("Cannot migrate: VDI is missing or invalid.");
        return false;
    }

    if (vdi->IsSnapshot())
    {
        reason = tr("Cannot migrate: VDI is a snapshot.");
        return false;
    }

    if (vdi->IsLocked())
    {
        reason = tr("Cannot migrate: VDI is locked (in use).");
        return false;
    }

    if (this->isHAType(vdi))
    {
        reason = tr("Cannot migrate: VDI is an HA type (statefile or redo log).");
        return false;
    }

    if (vdi->IsCBTEnabled())
    {
        reason = tr("Cannot migrate: VDI has changed block tracking (CBT) enabled.");
        return false;
    }

    if (this->isMetadataForDR(vdi))
    {
        reason = tr("Cannot migrate: VDI is metadata for disaster recovery.");
        return false;
    }

    // Check that VDI has at least one VBD
    if (vdi->GetVBDs().isEmpty())
    {
        reason = tr("Cannot migrate: VDI has no VBDs attached.");
        return false;
    }

    // Check SR properties
    QSharedPointer<SR> sr = vdi->GetSR();
    if (!sr)
    {
        reason = tr("Cannot migrate: VDI has no SR reference.");
        return false;
    }

    if (this->isHBALunPerVDI(sr))
    {
        reason = tr("Cannot migrate: Unsupported SR type (HBA LUN-per-VDI).");
        return false;
    }

    if (!this->supportsStorageMigration(sr))
    {
        reason = tr("Cannot migrate: SR does not support storage migration.");
        return false;
    }

    reason.clear();
    return true;
}

bool MigrateVirtualDiskCommand::isHAType(const QSharedPointer<VDI> &vdi) const
{
    if (!vdi)
        return false;

    QString type = vdi->GetType();
    return (type == "ha_statefile" || type == "redo_log");
}

bool MigrateVirtualDiskCommand::isMetadataForDR(const QSharedPointer<VDI> &vdi) const
{
    // Check if VDI has metadata_of_pool set (indicates DR metadata)
    if (!vdi)
        return false;

    QString metadataOfPool = vdi->MetadataOfPoolRef();
    return !metadataOfPool.isEmpty() && metadataOfPool != XENOBJECT_NULL;
}

bool MigrateVirtualDiskCommand::isHBALunPerVDI(const QSharedPointer<SR> &sr) const
{
    // HBA SRs have is_tools_sr = false and type = "lvmohba"
    // Additionally check sm_config for "allocation" = "thick"
    if (!sr)
        return false;

    QString srType = sr->GetType();

    if (srType == "lvmohba" || srType == "lvmofc")
    {
        // These are HBA types - check if LUN-per-VDI
        QVariantMap smConfig = sr->SMConfig();
        QString allocation = smConfig.value("allocation", "").toString();

        // LUN-per-VDI uses thick allocation
        return (allocation == "thick");
    }

    return false;
}

bool MigrateVirtualDiskCommand::supportsStorageMigration(const QSharedPointer<SR> &sr) const
{
    if (!sr)
        return false;

    // Look for "VDI_MIRROR" capability which indicates migration support
    if (sr->GetCapabilities().contains("VDI_MIRROR"))
        return true;

    // Also check if SR type is known to support migration
    QString srType = sr->GetType();
    QStringList supportedTypes = {
        "lvm", "ext", "nfs", "lvmoiscsi", "lvmohba",
        "smb", "cifs", "cslg", "gfs2"};

    return supportedTypes.contains(srType);
}
