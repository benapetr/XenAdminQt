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
#include "xen/vdi.h"
#include "xen/vbd.h"
#include "xen/vm.h"
#include "xen/sr.h"
#include <QMessageBox>

MoveVirtualDiskCommand::MoveVirtualDiskCommand(MainWindow* mainWindow, QObject* parent) : VDICommand(mainWindow, parent)
{
}

bool MoveVirtualDiskCommand::CanRun() const
{
    QSharedPointer<VDI> vdi = this->getVDI();
    if (!vdi || !vdi->IsValid())
        return false;

    return this->canBeMoved(vdi);
}

void MoveVirtualDiskCommand::Run()
{
    QSharedPointer<VDI> vdi = this->getVDI();
    if (!vdi || !vdi->IsValid())
        return;

    // Open the move virtual disk dialog
    MoveVirtualDiskDialog* dialog = new MoveVirtualDiskDialog(vdi->GetConnection(), vdi->OpaqueRef(), this->mainWindow());

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

QString MoveVirtualDiskCommand::MenuText() const
{
    return tr("Move...");
}

bool MoveVirtualDiskCommand::canBeMoved(QSharedPointer<VDI> vdi) const
{
    // Check if VDI is a snapshot
    if (!vdi || !vdi->IsValid())
        return false;

    if (vdi->IsSnapshot())
    {
        return false;
    }

    // Check if VDI is locked (in use)
    if (vdi->IsLocked())
        return false;

    for (const QSharedPointer<VBD>& vbd : vdi->GetVBDs())
        if (vbd && vbd->CurrentlyAttached())
            return false;

    // Check if VDI is HA type
    if (this->isHAType(vdi))
    {
        return false;
    }

    // Check if VDI has CBT enabled
    if (vdi->IsCBTEnabled())
    {
        return false;
    }

    // Check if VDI is DR metadata
    if (this->isMetadataForDR(vdi))
    {
        return false;
    }

    // Get SR
    QSharedPointer<SR> sr = vdi->GetSR();
    if (!sr)
        return false;

    // Check if SR is HBA LUN-per-VDI type
    QString srType = sr->GetType();

    // HBA type SRs (lvmohba, lvmoiscsi with LUN-per-VDI) don't support move
    if (srType == "lvmohba")
    {
        return false;
    }

    // Check if any VMs using this VDI are running
    if (this->isVDIInUseByRunningVM(vdi))
    {
        return false;
    }

    return true;
}

bool MoveVirtualDiskCommand::isVDIInUseByRunningVM(QSharedPointer<VDI> vdi) const
{
    if (!vdi)
        return false;

    QList<QSharedPointer<VBD>> vbds = vdi->GetVBDs();
    for (const QSharedPointer<VBD>& vbd : vbds)
    {
        if (!vbd || !vbd->IsValid())
            continue;

        QSharedPointer<VM> vm = vbd->GetVM();
        if (!vm)
            continue;

        // Check VM power state
        if (vm->GetPowerState() != "Halted")
            return true; // VM is running
    }

    return false;
}

bool MoveVirtualDiskCommand::isHAType(QSharedPointer<VDI> vdi) const
{
    if (!vdi)
        return false;

    QString type = vdi->GetType();
    return type == "ha";
}

bool MoveVirtualDiskCommand::isMetadataForDR(QSharedPointer<VDI> vdi) const
{
    // Check if VDI is DR metadata
    // DR metadata VDIs have specific tags or sm_config entries
    if (!vdi)
        return false;

    QVariantMap smConfig = vdi->SMConfig();

    // Check for DR-related tags
    QStringList tags = vdi->GetTags();
    for (const QString& tag : tags)
        if (tag.contains("disaster_recovery", Qt::CaseInsensitive))
            return true;

    // Check sm_config for DR markers
    if (smConfig.contains("dr_metadata") ||
        smConfig.value("disaster_recovery", false).toBool())
    {
        return true;
    }

    return false;
}
