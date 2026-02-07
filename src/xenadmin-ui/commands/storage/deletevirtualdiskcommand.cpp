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
#include <QMessageBox>

DeleteVirtualDiskCommand::DeleteVirtualDiskCommand(MainWindow* mainWindow, QObject* parent) : VDICommand(mainWindow, parent)
{
}

bool DeleteVirtualDiskCommand::CanRun() const
{
    QSharedPointer<VDI> vdi = this->getVDI();
    if (!vdi || !vdi->IsValid())
        return false;

    return this->canVDIBeDeleted(vdi);
}

void DeleteVirtualDiskCommand::Run()
{
    QSharedPointer<VDI> vdi = this->getVDI();
    if (!vdi || !vdi->IsValid())
        return;

    QString vdiName = vdi->GetName();
    QString vdiType = this->getVDIType(vdi);
    QString confirmTitle = this->getConfirmationTitle(vdiType);
    QString confirmText = this->getConfirmationText(vdiType, vdiName);

    // Show confirmation dialog
    QMessageBox msgBox(MainWindow::instance());
    msgBox.setWindowTitle(confirmTitle);
    msgBox.setText(confirmText);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    int ret = msgBox.exec();

    if (ret != QMessageBox::Yes)
        return;

    // Check if VDI is attached to running VMs
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

    // Create and run destroy action
    // allowRunningVMDelete = true if VDI is attached, action will handle detaching first
    DestroyDiskAction* action = new DestroyDiskAction(vdi->OpaqueRef(), vdi->GetConnection(),
        hasAttachedVBDs, // Allow deletion even if attached (action will detach first)
        nullptr);

    // Connect completion signal for cleanup and status update
    connect(action, &AsyncOperation::completed, [vdiName, vdiType, action]() 
    {
        MainWindow* mainWindow = MainWindow::instance();
        if (action->GetState() == AsyncOperation::Completed && !action->IsFailed())
        {
            if (mainWindow)
                mainWindow->ShowStatusMessage(QString("Successfully deleted %1 '%2'").arg(vdiType, vdiName), 5000);
        } else
        {
            QMessageBox::warning(mainWindow, QString("Delete %1 Failed").arg(vdiType), QString("Failed to delete %1 '%2'.\n\n%3").arg(vdiType, vdiName, action->GetErrorMessage()));
        }
        // Auto-delete when complete
        action->deleteLater();
    });

    // Run action asynchronously
    action->RunAsync();
}

QString DeleteVirtualDiskCommand::MenuText() const
{
    QSharedPointer<VDI> vdi = this->getVDI();
    if (!vdi || !vdi->IsValid())
        return "Delete Virtual Disk";

    QString vdiType = this->getVDIType(vdi);
    return QString("Delete %1").arg(vdiType);
}

bool DeleteVirtualDiskCommand::canVDIBeDeleted(QSharedPointer<VDI> vdi) const
{
    // Cannot delete locked VDI
    if (vdi->IsLocked())
        return false;

    QSharedPointer<SR> sr = vdi->GetSR();
    if (!sr)
        return false;

    // Cannot delete VDI on physical SR
    if (sr->GetType() == "udev") // Physical device SR
        return false;

    // Cannot delete VDI on tools SR
    if (sr->ContentType() == "tools")
        return false;

    // Check if VDI is used for HA (metadata VDI)
    if (vdi->GetOtherConfig().contains("ha_metadata"))
        return false;

    // Check allowed operations
    if (!vdi->AllowedOperations().contains("destroy"))
        return false;

    // Check VBDs - cannot delete if attached to running system disk
    QString type = vdi->GetType();
    const QList<QSharedPointer<VBD>> vbds = vdi->GetVBDs();
    for (const QSharedPointer<VBD>& vbd : vbds)
    {
        if (!vbd || !vbd->IsValid())
            continue;

        // If system disk, check if VM is running
        if (type == "system")
        {
            QSharedPointer<VM> vm = vbd->GetVM();
            if (vm && vm->GetPowerState() == "Running")
            {
                // Cannot delete system disk of running VM
                return false;
            }
        }

        // Check if VBD is locked
        if (vbd->IsLocked())
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
