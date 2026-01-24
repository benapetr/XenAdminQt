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

#include "detachvirtualdiskcommand.h"
#include "deactivatevbdcommand.h"
#include "xen/vbd.h"
#include "xen/vdi.h"
#include "xen/vm.h"
#include "xen/actions/vdi/detachvirtualdiskaction.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include <QMessageBox>

DetachVirtualDiskCommand::DetachVirtualDiskCommand(MainWindow* mainWindow, QObject* parent)
    : VDICommand(mainWindow, parent)
{
}

QString DetachVirtualDiskCommand::MenuText() const
{
    return "Detach Virtual Disk";
}

bool DetachVirtualDiskCommand::CanRun() const
{
    QSharedPointer<VDI> vdi = this->getVDI();
    if (!vdi || !vdi->IsValid())
        return false;

    return this->canRunVDI(vdi);
}

bool DetachVirtualDiskCommand::canRunVDI(QSharedPointer<VDI> vdi) const
{
    if (!vdi || !vdi->IsValid())
        return false;

    // Check if VDI is locked
    if (vdi->IsLocked())
        return false;

    // Get all VBDs attached to this VDI
    QList<QSharedPointer<VBD>> vbds = vdi->GetVBDs();
    if (vbds.isEmpty())
        return false; // No VBDs - nothing to detach

    // Check each VBD - at least one must be detachable
    bool hasDetachableVBD = false;

    for (const QSharedPointer<VBD>& vbd : vbds)
    {
        if (!vbd || !vbd->IsValid())
            continue;

        // If VBD is currently attached, check if we can deactivate it
        if (vbd->CurrentlyAttached())
        {
            // Check if this VBD can be deactivated (using DeactivateVBDCommand logic)
            // For simplicity, we'll check basic conditions here

            // Get VM
            QSharedPointer<VM> vm = vbd->GetVM();
            if (!vm || vm->IsTemplate())
                continue;

            // Check VM is running
            if (vm->GetPowerState() != "Running")
                continue; // Can't hot-unplug if VM is not running

            // Check if VBD or VDI is locked
            if (vbd->IsLocked())
                continue;

            // Check if system boot disk
            if (vdi->GetType() == "system")
            {
                if (vbd->IsOwner())
                    continue; // Can't detach system boot disk
            }

            // Check allowed operations
            if (!vbd->AllowedOperations().contains("unplug"))
                continue;
        }

        // If we reach here, this VBD can be detached
        hasDetachableVBD = true;
    }

    return hasDetachableVBD;
}

QString DetachVirtualDiskCommand::getCantRunReasonVDI(QSharedPointer<VDI> vdi) const
{
    if (!vdi || !vdi->IsValid())
        return "VDI not found";

    if (vdi->IsLocked())
        return "Virtual disk is in use";

    QList<QSharedPointer<VBD>> vbds = vdi->GetVBDs();
    if (vbds.isEmpty())
    {
        return "Virtual disk is not attached to any VM";
    }

    // Check each VBD for detailed reason
    for (const QSharedPointer<VBD>& vbd : vbds)
    {
        if (!vbd || !vbd->IsValid())
            continue;

        if (vbd->CurrentlyAttached())
        {
            QSharedPointer<VM> vm = vbd->GetVM();
            QString vmName = vm ? vm->GetName() : QString("VM");

            if (vm && vm->IsTemplate())
            {
                return "Cannot detach disk from template";
            }

            if (vm && vm->GetPowerState() != "Running")
                return QString("Cannot hot-detach from halted VM '%1'").arg(vmName);

            if (vbd->IsLocked())
                return "Virtual disk is locked";

            // Check if system boot disk
            if (vdi->GetType() == "system")
            {
                if (vbd->IsOwner())
                    return QString("Cannot detach system boot disk from '%1'").arg(vmName);
            }
        }
    }

    return "Unknown reason";
}

void DetachVirtualDiskCommand::Run()
{
    QSharedPointer<VDI> vdi = this->getVDI();
    if (!vdi || !vdi->IsValid())
        return;

    QString vdiName = vdi->GetName();
    QString vdiType = vdi->GetType();

    // Show confirmation dialog
    QString confirmText;
    QString confirmTitle;

    if (vdiType == "system")
    {
        confirmTitle = "Detach System Disk";
        confirmText = QString("Are you sure you want to detach the system disk '%1'?\n\n"
                              "Warning: Detaching a system disk may prevent the VM from booting.")
                          .arg(vdiName);
    } else
    {
        confirmTitle = "Detach Virtual Disk";
        confirmText = QString("Are you sure you want to detach virtual disk '%1'?").arg(vdiName);
    }

    QMessageBox msgBox(this->mainWindow());
    msgBox.setWindowTitle(confirmTitle);
    msgBox.setText(confirmText);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    int ret = msgBox.exec();
    if (ret != QMessageBox::Yes)
    {
        return;
    }

    // Get all VBDs and create detach actions
    QList<QSharedPointer<VBD>> vbds = vdi->GetVBDs();

    QList<AsyncOperation*> actions;

    for (const QSharedPointer<VBD>& vbd : vbds)
    {
        if (!vbd || !vbd->IsValid())
            continue;

        QSharedPointer<VM> vm = vbd->GetVM();
        if (!vm)
            continue;

        // Create detach action
        DetachVirtualDiskAction* action = new DetachVirtualDiskAction(vdi->OpaqueRef(), vm.data(), nullptr);

        // Register with OperationManager for history tracking
        OperationManager::instance()->RegisterOperation(action);

        // Connect completion signal
        QString vmName = vm->GetName();
        connect(action, &AsyncOperation::completed, [vdiName, vmName, action]() {
            MainWindow* mainWindow = MainWindow::instance();
            if (action->GetState() == AsyncOperation::Completed && !action->IsFailed())
            {
                if (mainWindow)
                    mainWindow->ShowStatusMessage(
                    QString("Successfully detached virtual disk '%1' from VM '%2'").arg(vdiName, vmName),
                    5000);
            } else
            {
                if (mainWindow)
                    mainWindow->ShowStatusMessage(
                    QString("Failed to detach virtual disk '%1' from VM '%2'").arg(vdiName, vmName),
                    5000);
            }
            // Auto-delete when complete
            action->deleteLater();
        });

        actions.append(action);
    }

    // Run all actions
    if (actions.isEmpty())
    {
        QMessageBox::warning(
            this->mainWindow(),
            "Detach Virtual Disk",
            QString("No VBDs found to detach for virtual disk '%1'").arg(vdiName));
        return;
    }

    // Run actions asynchronously
    for (AsyncOperation* action : actions)
    {
        action->RunAsync();
    }
}
