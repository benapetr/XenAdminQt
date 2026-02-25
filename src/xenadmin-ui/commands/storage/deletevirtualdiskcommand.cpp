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

#include "deletevirtualdiskcommand.h"
#include "../../mainwindow.h"
#include "xenlib/xen/vdi.h"
#include "xenlib/xen/vbd.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/actions/vdi/destroydiskaction.h"
#include "deactivatevbdcommand.h"
#include <QPointer>
#include <QMessageBox>

DeleteVirtualDiskCommand::DeleteVirtualDiskCommand(MainWindow* mainWindow, QObject* parent) : VDICommand(mainWindow, parent)
{
}

QList<QSharedPointer<VDI>> DeleteVirtualDiskCommand::getSelectedVDIs() const
{
    QList<QSharedPointer<VDI>> vdis;
    const QList<QSharedPointer<XenObject>> selectedObjects = this->getSelectedObjects();
    vdis.reserve(selectedObjects.size());
    for (const QSharedPointer<XenObject>& object : selectedObjects)
    {
        QSharedPointer<VDI> vdi = qSharedPointerDynamicCast<VDI>(object);
        if (!vdi || !vdi->IsValid())
            return {};
        vdis.append(vdi);
    }
    return vdis;
}

bool DeleteVirtualDiskCommand::CanRun() const
{
    QList<QSharedPointer<VDI>> vdis = this->getSelectedVDIs();
    if (vdis.isEmpty())
    {
        QSharedPointer<VDI> vdi = this->getVDI();
        if (!vdi || !vdi->IsValid())
            return false;
        vdis.append(vdi);
    }

    for (const QSharedPointer<VDI>& vdi : vdis)
    {
        if (!this->canVDIBeDeleted(vdi))
            return false;
    }

    return true;
}

void DeleteVirtualDiskCommand::Run()
{
    QList<QSharedPointer<VDI>> vdis = this->getSelectedVDIs();
    if (vdis.isEmpty())
    {
        QSharedPointer<VDI> vdi = this->getVDI();
        if (!vdi || !vdi->IsValid())
            return;
        vdis.append(vdi);
    }
    if (vdis.isEmpty())
        return;

    for (const QSharedPointer<VDI>& vdi : vdis)
    {
        if (!this->canVDIBeDeleted(vdi))
            return;
    }

    QString confirmTitle;
    QString confirmText;
    if (vdis.size() == 1)
    {
        QString vdiName = vdis.first()->GetName();
        QString vdiType = this->getVDIType(vdis.first());
        confirmTitle = this->getConfirmationTitle(vdiType);
        confirmText = this->getConfirmationText(vdiType, vdiName);
    }
    else
    {
        confirmTitle = tr("Delete Virtual Disks");
        confirmText = tr("Are you sure you want to permanently delete the selected virtual disks?\n\nThis operation cannot be undone.");
    }

    QMessageBox msgBox(MainWindow::instance());
    msgBox.setWindowTitle(confirmTitle);
    msgBox.setText(confirmText);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    int ret = msgBox.exec();

    if (ret != QMessageBox::Yes)
        return;

    QList<AsyncOperation*> actions;
    actions.reserve(vdis.size());
    for (const QSharedPointer<VDI>& vdi : vdis)
    {
        bool hasAttachedVBDs = false;
        const QList<QSharedPointer<VBD>> vbds = vdi->GetVBDs();
        for (const QSharedPointer<VBD>& vbd : vbds)
        {
            if (vbd && vbd->CurrentlyAttached())
            {
                hasAttachedVBDs = true;
                break;
            }
        }

        auto* action = new DestroyDiskAction(vdi->OpaqueRef(), vdi->GetConnection(),
                                             this->m_allowRunningVMDelete && hasAttachedVBDs,
                                             this->mainWindow());
        actions.append(action);
    }

    if (actions.isEmpty())
        return;

    if (actions.size() == 1)
    {
        actions.first()->RunAsync();
        return;
    }
    this->RunMultipleActions(actions, tr("Delete Virtual Disks"), tr("Deleting virtual disks..."), tr("Completed"), true, true);
}

QString DeleteVirtualDiskCommand::MenuText() const
{
    QList<QSharedPointer<VDI>> vdis = this->getSelectedVDIs();
    if (vdis.size() > 1)
        return tr("Delete Virtual Disks");

    QSharedPointer<VDI> vdi = vdis.isEmpty() ? this->getVDI() : vdis.first();
    if (!vdi || !vdi->IsValid())
        return "Delete Virtual Disk";

    QString vdiType = this->getVDIType(vdi);
    return QString("Delete %1").arg(vdiType);
}

bool DeleteVirtualDiskCommand::canVDIBeDeleted(QSharedPointer<VDI> vdi) const
{
    if (vdi->IsLocked())
        return false;

    QSharedPointer<SR> sr = vdi->GetSR();
    if (!sr)
        return false;

    if (sr->GetType() == "udev")
        return false;

    if (sr->ContentType() == "tools")
        return false;

    if (vdi->GetOtherConfig().contains("ha_metadata"))
        return false;

    const QList<QSharedPointer<VBD>> vbds = vdi->GetVBDs();
    if (vbds.size() > 1 && !this->m_allowMultipleVBDDelete)
        return false;

    QString type = vdi->GetType();
    for (const QSharedPointer<VBD>& vbd : vbds)
    {
        if (!vbd || !vbd->IsValid())
            continue;

        if (type == "system")
        {
            QSharedPointer<VM> vm = vbd->GetVM();
            if (vm && vm->GetPowerState() == "Running")
            {
                // Cannot delete system disk of running VM
                return false;
            }
        }

        if (vbd->IsLocked())
            return false;

        if (vbd->CurrentlyAttached())
        {
            if (!this->m_allowRunningVMDelete)
                return false;

            DeactivateVBDCommand cmd(this->mainWindow(), nullptr);
            cmd.SetSelectionOverride(QList<QSharedPointer<XenObject>>{vbd});
            if (!cmd.CanRun())
                return false;
        }
    }

    if (sr->HBALunPerVDI())
        return true;

    if (!vdi->AllowedOperations().contains("destroy"))
    {
        if (this->m_allowRunningVMDelete)
            return true;
        return false;
    }

    return true;
}

QString DeleteVirtualDiskCommand::getVDIType(QSharedPointer<VDI> vdi) const
{
    // Check if snapshot
    if (vdi->IsSnapshot())
        return "Snapshot";

    // Check type
    QString type = vdi->GetType();
    if (type == "user")
        return "Virtual Disk";
    else if (type == "system")
        return "System Disk";

    // Check if ISO
    QSharedPointer<SR> sr = vdi->GetSR();
    if (sr && sr->ContentType() == "iso")
        return "ISO";

    return "Virtual Disk";
}

QString DeleteVirtualDiskCommand::getConfirmationText(const QString& vdiType, const QString& vdiName) const
{
    if (vdiType == "Snapshot")
    {
        return QString("Are you sure you want to delete snapshot '%1'?\n\n"
                       "This will permanently delete the snapshot and cannot be undone.")
            .arg(vdiName);
    } else if (vdiType == "ISO")
    {
        return QString("Are you sure you want to remove ISO '%1' from the SR?\n\n"
                       "Note: This will remove the ISO from the storage repository.")
            .arg(vdiName);
    } else if (vdiType == "System Disk")
    {
        return QString("WARNING: You are about to delete a system disk '%1'!\n\n"
                       "This is the boot disk for a virtual machine. Deleting it will make "
                       "the VM unbootable and the data will be permanently lost.\n\n"
                       "Are you absolutely sure you want to continue?")
            .arg(vdiName);
    } else
    {
        return QString("Are you sure you want to delete virtual disk '%1'?\n\n"
                       "This will permanently delete the disk and all data on it. "
                       "This action cannot be undone.")
            .arg(vdiName);
    }
}

QString DeleteVirtualDiskCommand::getConfirmationTitle(const QString& vdiType) const
{
    if (vdiType == "Snapshot")
        return "Delete Snapshot";
    else if (vdiType == "ISO")
        return "Remove ISO";
    else if (vdiType == "System Disk")
        return "Delete System Disk";
    else
        return "Delete Virtual Disk";
}
