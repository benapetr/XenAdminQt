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
#include "../../operations/operationmanager.h"
#include "xenlib.h"
#include "xencache.h"
#include "xen/vdi.h"
#include "xen/actions/vdi/destroydiskaction.h"
#include <QMessageBox>
#include <QDebug>

DeleteVirtualDiskCommand::DeleteVirtualDiskCommand(MainWindow* mainWindow, QObject* parent)
    : VDICommand(mainWindow, parent)
{
}

bool DeleteVirtualDiskCommand::CanRun() const
{
    QSharedPointer<VDI> vdi = this->getVDI();
    if (!vdi || !vdi->isValid())
        return false;

    QString vdiRef = vdi->opaqueRef();
    QVariantMap vdiData = vdi->connection()->getCache()->ResolveObjectData("vdi", vdiRef);
    if (vdiData.isEmpty())
        return false;

    return this->canVDIBeDeleted(vdiData);
}

void DeleteVirtualDiskCommand::Run()
{
    QSharedPointer<VDI> vdi = this->getVDI();
    if (!vdi || !vdi->isValid())
        return;

    QString vdiRef = vdi->opaqueRef();
    QString vdiName = vdi->nameLabel();
    XenCache* cache = vdi->connection()->getCache();
    if (!cache)
        return;

    QVariantMap vdiData = cache->ResolveObjectData("vdi", vdiRef);
    if (vdiData.isEmpty())
        return;

    QString vdiType = this->getVDIType(vdiData);
    QString confirmTitle = this->getConfirmationTitle(vdiType);
    QString confirmText = this->getConfirmationText(vdiType, vdiName);

    // Show confirmation dialog
    QMessageBox msgBox(this->mainWindow());
    msgBox.setWindowTitle(confirmTitle);
    msgBox.setText(confirmText);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    int ret = msgBox.exec();

    if (ret != QMessageBox::Yes)
        return;

    qDebug() << "DeleteVirtualDiskCommand: Deleting VDI" << vdiName << "(" << vdiRef << ")";

    // Check if VDI is attached to running VMs
    QVariantList vbds = vdiData.value("VBDs", QVariantList()).toList();
    bool hasAttachedVBDs = false;

    for (const QVariant& vbdRefVar : vbds)
    {
        QString vbdRef = vbdRefVar.toString();
        QVariantMap vbdData = cache->ResolveObjectData("vbd", vbdRef);

        if (vbdData.value("currently_attached", false).toBool())
        {
            hasAttachedVBDs = true;
            break;
        }
    }

    // Create and run destroy action
    // allowRunningVMDelete = true if VDI is attached, action will handle detaching first
    DestroyDiskAction* action = new DestroyDiskAction(
        vdiRef,
        vdi->connection(),
        hasAttachedVBDs, // Allow deletion even if attached (action will detach first)
        this);

    // Register with OperationManager for history tracking
    OperationManager::instance()->registerOperation(action);

    // Connect completion signal for cleanup and status update
    connect(action, &AsyncOperation::completed, [this, vdiName, vdiType, action]() {
        if (action->state() == AsyncOperation::Completed && !action->isFailed())
        {
            mainWindow()->showStatusMessage(QString("Successfully deleted %1 '%2'").arg(vdiType, vdiName), 5000);
        } else
        {
            QMessageBox::warning(
                mainWindow(),
                QString("Delete %1 Failed").arg(vdiType),
                QString("Failed to delete %1 '%2'.\n\n%3")
                    .arg(vdiType, vdiName, action->errorMessage()));
        }
        // Auto-delete when complete
        action->deleteLater();
    });

    // Run action asynchronously
    action->runAsync();
}

QString DeleteVirtualDiskCommand::MenuText() const
{
    QSharedPointer<VDI> vdi = this->getVDI();
    if (!vdi || !vdi->isValid())
        return "Delete Virtual Disk";

    XenCache* cache = vdi->connection()->getCache();
    if (!cache)
        return "Delete Virtual Disk";

    QVariantMap vdiData = cache->ResolveObjectData("vdi", vdi->opaqueRef());
    if (vdiData.isEmpty())
        return "Delete Virtual Disk";

    QString vdiType = this->getVDIType(vdiData);
    return QString("Delete %1").arg(vdiType);
}

bool DeleteVirtualDiskCommand::canVDIBeDeleted(const QVariantMap& vdiData) const
{
    if (vdiData.isEmpty())
        return false;

    // Cannot delete locked VDI
    bool locked = vdiData.value("Locked", false).toBool();
    if (locked)
        return false;

    // Get SR data
    QString srRef = vdiData.value("SR").toString();
    if (srRef.isEmpty())
        return false;

    if (!mainWindow()->xenLib())
        return false;

    XenCache* cache = mainWindow()->xenLib()->getCache();
    if (!cache)
        return false;

    QVariantMap srData = cache->ResolveObjectData("sr", srRef);
    if (srData.isEmpty())
        return false;

    // Cannot delete VDI on physical SR
    QString srType = srData.value("type", "").toString();
    if (srType == "udev") // Physical device SR
        return false;

    // Cannot delete VDI on tools SR
    QString contentType = srData.value("content_type", "").toString();
    if (contentType == "tools")
        return false;

    // Check if VDI is used for HA (metadata VDI)
    QVariantMap otherConfig = vdiData.value("other_config", QVariantMap()).toMap();
    if (otherConfig.contains("ha_metadata"))
        return false;

    // Check allowed operations
    QVariantList allowedOps = vdiData.value("allowed_operations", QVariantList()).toList();
    bool destroyAllowed = false;
    for (const QVariant& op : allowedOps)
    {
        if (op.toString() == "destroy")
        {
            destroyAllowed = true;
            break;
        }
    }

    if (!destroyAllowed)
        return false;

    // Check VBDs - cannot delete if attached to running system disk
    QVariantList vbds = vdiData.value("VBDs", QVariantList()).toList();
    QString type = vdiData.value("type", "").toString();

    for (const QVariant& vbdRefVar : vbds)
    {
        QString vbdRef = vbdRefVar.toString();
        QVariantMap vbdData = cache->ResolveObjectData("vbd", vbdRef);

        if (vbdData.isEmpty())
            continue;

        // If system disk, check if VM is running
        if (type == "system")
        {
            QString vmRef = vbdData.value("VM").toString();
            QVariantMap vmData = cache->ResolveObjectData("vm", vmRef);

            QString powerState = vmData.value("power_state", "").toString();
            if (powerState == "Running")
            {
                // Cannot delete system disk of running VM
                return false;
            }
        }

        // Check if VBD is locked
        if (vbdData.value("Locked", false).toBool())
            return false;
    }

    return true;
}

QString DeleteVirtualDiskCommand::getVDIType(const QVariantMap& vdiData) const
{
    if (vdiData.isEmpty())
        return "Virtual Disk";

    // Check if snapshot
    bool isSnapshot = vdiData.value("is_a_snapshot", false).toBool();
    if (isSnapshot)
        return "Snapshot";

    // Check type
    QString type = vdiData.value("type", "").toString();
    if (type == "user")
        return "Virtual Disk";
    else if (type == "system")
        return "System Disk";

    // Check if ISO
    QString srRef = vdiData.value("SR").toString();
    if (!srRef.isEmpty() && mainWindow()->xenLib())
    {
        XenCache* cache = mainWindow()->xenLib()->getCache();
        if (cache)
        {
            QVariantMap srData = cache->ResolveObjectData("sr", srRef);
            QString contentType = srData.value("content_type", "").toString();
            if (contentType == "iso")
                return "ISO";
        }
    }

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
