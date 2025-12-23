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
#include "../../../xenlib/xencache.h"
#include "../../../xenlib/xenlib.h"
#include "../../../xenlib/xen/network/connection.h"
#include "../../../xenlib/xen/vm.h"
#include "../../../xenlib/xen/actions/vdi/detachvirtualdiskaction.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include <QMessageBox>
#include <QDebug>

DetachVirtualDiskCommand::DetachVirtualDiskCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

QString DetachVirtualDiskCommand::menuText() const
{
    return "Detach Virtual Disk";
}

bool DetachVirtualDiskCommand::canRun() const
{
    if (this->getSelectedObjectType() != "vdi")
    {
        return false;
    }

    QString vdiRef = this->getSelectedObjectRef();
    if (vdiRef.isEmpty())
    {
        return false;
    }

    return this->canRunVDI(vdiRef);
}

bool DetachVirtualDiskCommand::canRunVDI(const QString& vdiRef) const
{
    XenCache* cache = this->mainWindow()->xenLib()->getCache();
    if (!cache)
    {
        return false;
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

    // Get all VBDs attached to this VDI
    QVariantList vbds = vdiData.value("VBDs").toList();
    if (vbds.isEmpty())
    {
        return false; // No VBDs - nothing to detach
    }

    // Check each VBD - at least one must be detachable
    bool hasDetachableVBD = false;

    for (const QVariant& vbdVar : vbds)
    {
        QString vbdRef = vbdVar.toString();
        QVariantMap vbdData = cache->ResolveObjectData("vbd", vbdRef);

        if (vbdData.isEmpty())
        {
            continue;
        }

        // If VBD is currently attached, check if we can deactivate it
        bool currentlyAttached = vbdData.value("currently_attached").toBool();
        if (currentlyAttached)
        {
            // Check if this VBD can be deactivated (using DeactivateVBDCommand logic)
            // For simplicity, we'll check basic conditions here

            // Get VM
            QString vmRef = vbdData.value("VM").toString();
            QVariantMap vmData = cache->ResolveObjectData("vm", vmRef);

            if (vmData.isEmpty() || vmData.value("is_a_template").toBool())
            {
                continue;
            }

            // Check VM is running
            QString powerState = vmData.value("power_state").toString();
            if (powerState != "Running")
            {
                continue; // Can't hot-unplug if VM is not running
            }

            // Check if VBD or VDI is locked
            if (vbdData.value("Locked", false).toBool())
            {
                continue;
            }

            // Check if system boot disk
            QString vdiType = vdiData.value("type").toString();
            if (vdiType == "system")
            {
                bool isOwner = vbdData.value("device", "").toString() == "0" ||
                               vbdData.value("bootable", false).toBool();
                if (isOwner)
                {
                    continue; // Can't detach system boot disk
                }
            }

            // Check allowed operations
            QVariantList allowedOps = vbdData.value("allowed_operations").toList();
            if (!allowedOps.contains("unplug"))
            {
                continue;
            }
        }

        // If we reach here, this VBD can be detached
        hasDetachableVBD = true;
    }

    return hasDetachableVBD;
}

QString DetachVirtualDiskCommand::getCantRunReasonVDI(const QString& vdiRef) const
{
    XenCache* cache = this->mainWindow()->xenLib()->getCache();
    if (!cache)
    {
        return "Cache not available";
    }

    QVariantMap vdiData = cache->ResolveObjectData("vdi", vdiRef);
    if (vdiData.isEmpty())
    {
        return "VDI not found";
    }

    if (vdiData.value("Locked", false).toBool())
    {
        return "Virtual disk is in use";
    }

    QVariantList vbds = vdiData.value("VBDs").toList();
    if (vbds.isEmpty())
    {
        return "Virtual disk is not attached to any VM";
    }

    // Check each VBD for detailed reason
    for (const QVariant& vbdVar : vbds)
    {
        QString vbdRef = vbdVar.toString();
        QVariantMap vbdData = cache->ResolveObjectData("vbd", vbdRef);

        if (vbdData.isEmpty())
        {
            continue;
        }

        bool currentlyAttached = vbdData.value("currently_attached").toBool();
        if (currentlyAttached)
        {
            QString vmRef = vbdData.value("VM").toString();
            QVariantMap vmData = cache->ResolveObjectData("vm", vmRef);
            QString vmName = vmData.value("name_label").toString();

            if (vmData.value("is_a_template").toBool())
            {
                return "Cannot detach disk from template";
            }

            QString powerState = vmData.value("power_state").toString();
            if (powerState != "Running")
            {
                return QString("Cannot hot-detach from halted VM '%1'").arg(vmName);
            }

            if (vbdData.value("Locked", false).toBool())
            {
                return "Virtual disk is locked";
            }

            // Check if system boot disk
            QString vdiType = vdiData.value("type").toString();
            if (vdiType == "system")
            {
                bool isOwner = vbdData.value("device", "").toString() == "0" ||
                               vbdData.value("bootable", false).toBool();
                if (isOwner)
                {
                    return QString("Cannot detach system boot disk from '%1'").arg(vmName);
                }
            }
        }
    }

    return "Unknown reason";
}

void DetachVirtualDiskCommand::run()
{
    QString vdiRef = this->getSelectedObjectRef();
    if (vdiRef.isEmpty())
    {
        return;
    }

    XenCache* cache = this->mainWindow()->xenLib()->getCache();
    if (!cache)
    {
        return;
    }

    QVariantMap vdiData = cache->ResolveObjectData("vdi", vdiRef);
    if (vdiData.isEmpty())
    {
        return;
    }

    QString vdiName = vdiData.value("name_label", "disk").toString();
    QString vdiType = vdiData.value("type").toString();

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
    QVariantList vbds = vdiData.value("VBDs").toList();

    QList<AsyncOperation*> actions;

    for (const QVariant& vbdVar : vbds)
    {
        QString vbdRef = vbdVar.toString();
        QVariantMap vbdData = cache->ResolveObjectData("vbd", vbdRef);

        if (vbdData.isEmpty())
        {
            continue;
        }

        // Get VM
        QString vmRef = vbdData.value("VM").toString();
        QVariantMap vmData = cache->ResolveObjectData("vm", vmRef);
        QString vmName = vmData.value("name_label", "VM").toString();

        // Create VM object for the action
        VM* vm = new VM(this->mainWindow()->xenLib()->getConnection(), vmRef, this);

        // Create detach action
        DetachVirtualDiskAction* action = new DetachVirtualDiskAction(
            vdiRef,
            vm,
            this);

        // Register with OperationManager for history tracking
        OperationManager::instance()->registerOperation(action);

        // Connect completion signal
        connect(action, &AsyncOperation::completed, [this, vdiName, vmName, action]() {
            if (action->state() == AsyncOperation::Completed && !action->isFailed())
            {
                this->mainWindow()->showStatusMessage(
                    QString("Successfully detached virtual disk '%1' from VM '%2'").arg(vdiName, vmName),
                    5000);
            } else
            {
                this->mainWindow()->showStatusMessage(
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
        action->runAsync();
    }
}
