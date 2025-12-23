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

#include "attachvirtualdiskcommand.h"
#include <QDebug>
#include "../../mainwindow.h"
#include "xenlib.h"
#include "xencache.h"
#include "xen/network/connection.h"
#include "xen/vm.h"
#include "xen/actions/vbd/vbdcreateandplugaction.h"
#include "../../dialogs/attachvirtualdiskdialog.h"
#include "../../dialogs/operationprogressdialog.h"
#include <QMessageBox>

AttachVirtualDiskCommand::AttachVirtualDiskCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool AttachVirtualDiskCommand::canRun() const
{
    // Can attach virtual disk if VM is selected and not a snapshot
    if (!isVMSelected())
        return false;

    QString vmRef = getSelectedVMRef();
    if (vmRef.isEmpty())
        return false;

    if (!mainWindow()->xenLib())
        return false;

    XenCache* cache = mainWindow()->xenLib()->getCache();
    if (!cache)
        return false;

    QVariantMap vmData = cache->ResolveObjectData("vm", vmRef);

    // Cannot attach to snapshot or locked VM
    if (vmData.value("is_a_snapshot", false).toBool())
        return false;

    QVariantMap currentOps = vmData.value("current_operations", QVariantMap()).toMap();
    return currentOps.isEmpty();
}

void AttachVirtualDiskCommand::run()
{
    QString vmRef = getSelectedVMRef();
    if (vmRef.isEmpty())
        return;

    XenLib* xenLib = mainWindow()->xenLib();
    if (!xenLib)
        return;

    XenCache* cache = xenLib->getCache();
    if (!cache)
        return;

    // Check VBD limit
    QVariantMap vmData = cache->ResolveObjectData("vm", vmRef);
    int maxVBDs = getMaxVBDsAllowed(vmData);
    int currentVBDs = getCurrentVBDCount(vmRef);

    if (currentVBDs >= maxVBDs)
    {
        QMessageBox::warning(mainWindow(), "Maximum VBDs Reached",
                             QString("The maximum number of virtual disks (%1) has been reached for this VM.\n\n"
                                     "Please detach a disk before attaching a new one.")
                                 .arg(maxVBDs));
        return;
    }

    // Launch attach dialog
    AttachVirtualDiskDialog dialog(xenLib, vmRef, mainWindow());

    qDebug() << "[AttachVirtualDiskCommand] Showing AttachVirtualDiskDialog modally...";
    if (dialog.exec() != QDialog::Accepted)
    {
        qDebug() << "[AttachVirtualDiskCommand] Dialog cancelled by user";
        return;
    }

    qDebug() << "[AttachVirtualDiskCommand] Dialog accepted, proceeding with attachment";
    performAttachment(&dialog, xenLib, vmRef);
}

void AttachVirtualDiskCommand::performAttachment(AttachVirtualDiskDialog* dialog, XenLib* xenLib, const QString& vmRef)
{
    qDebug() << "[AttachVirtualDiskCommand] Starting attachment process for VM:" << vmRef;

    QString vdiRef = dialog->getSelectedVDIRef();
    if (vdiRef.isEmpty())
    {
        qWarning() << "[AttachVirtualDiskCommand] No VDI selected, aborting";
        return;
    }

    qDebug() << "[AttachVirtualDiskCommand] Selected VDI:" << vdiRef;

    QString devicePosition = dialog->getDevicePosition();
    QString mode = dialog->getMode();
    bool bootable = dialog->isBootable();

    qDebug() << "[AttachVirtualDiskCommand] Device position:" << devicePosition
             << "Mode:" << mode << "Bootable:" << bootable;

    XenCache* cache = xenLib->getCache();
    if (!cache)
    {
        qWarning() << "[AttachVirtualDiskCommand] No cache available, aborting";
        return;
    }

    // Get VDI name for UI feedback
    QVariantMap vdiData = cache->ResolveObjectData("vdi", vdiRef);
    QString vdiName = vdiData.value("name_label", "Virtual Disk").toString();
    qDebug() << "[AttachVirtualDiskCommand] VDI name:" << vdiName;

    // Build VBD record (matches C# AttachDiskDialog.cs lines 206-216)
    QVariantMap vbdRecord;
    vbdRecord["VDI"] = vdiRef;
    vbdRecord["VM"] = vmRef;
    vbdRecord["bootable"] = bootable;
    vbdRecord["device"] = QString(""); // Will be filled by XenAPI
    vbdRecord["empty"] = false;
    vbdRecord["userdevice"] = devicePosition;
    vbdRecord["type"] = "Disk";
    vbdRecord["mode"] = (mode == "RO") ? "RO" : "RW";
    vbdRecord["unpluggable"] = true;

    // Check if this is the first VBD for this VDI (owner flag)
    QList<QVariantMap> allVbds = cache->GetAllData("vbd");
    bool isOwner = true;
    for (const QVariantMap& vbd : allVbds)
    {
        if (vbd.value("VDI").toString() == vdiRef)
        {
            isOwner = false;
            break;
        }
    }
    vbdRecord["owner"] = isOwner;
    qDebug() << "[AttachVirtualDiskCommand] VBD owner flag:" << isOwner;

    // Get VM object for action (same pattern as StorageTabPage line 1241)
    XenConnection* connection = xenLib->getConnection();
    if (!connection)
    {
        qWarning() << "[AttachVirtualDiskCommand] No connection available, aborting";
        QMessageBox::warning(mainWindow(), "Error", "No connection available");
        return;
    }

    qDebug() << "[AttachVirtualDiskCommand] Creating VM object for" << vmRef;
    VM* vm = new VM(connection, vmRef, this);

    // Create and execute the action (matches C# AttachDiskDialog.cs lines 218-221)
    qDebug() << "[AttachVirtualDiskCommand] Creating VbdCreateAndPlugAction";
    VbdCreateAndPlugAction* action = new VbdCreateAndPlugAction(vm, vbdRecord, vdiName, false, this);

    // Show user instruction dialog if reboot is needed
    connect(action, &VbdCreateAndPlugAction::showUserInstruction,
            mainWindow(), [this](const QString& instruction) {
                qDebug() << "[AttachVirtualDiskCommand] Reboot instruction received:" << instruction;
                QMessageBox::information(mainWindow(), "Reboot Required", instruction);
            });

    // Show progress dialog
    qDebug() << "[AttachVirtualDiskCommand] Creating OperationProgressDialog";
    OperationProgressDialog* progressDialog = new OperationProgressDialog(action, mainWindow());
    progressDialog->setAttribute(Qt::WA_DeleteOnClose);

    qDebug() << "[AttachVirtualDiskCommand] Executing progress dialog...";
    int dialogResult = progressDialog->exec();
    qDebug() << "[AttachVirtualDiskCommand] Progress dialog result:" << dialogResult
             << "(Accepted=" << QDialog::Accepted << ")";
    qDebug() << "[AttachVirtualDiskCommand] Action state:"
             << "hasError=" << action->hasError()
             << "isCancelled=" << action->isCancelled()
             << "errorMessage=" << action->errorMessage();

    if (dialogResult == QDialog::Accepted)
    {
        qDebug() << "[AttachVirtualDiskCommand] Attachment succeeded";
        mainWindow()->showStatusMessage(QString("Virtual disk '%1' attached successfully").arg(vdiName), 5000);
    } else
    {
        qWarning() << "[AttachVirtualDiskCommand] Attachment failed or cancelled";
        if (!action->errorMessage().isEmpty())
        {
            qWarning() << "[AttachVirtualDiskCommand] Error message:" << action->errorMessage();
            QMessageBox::warning(mainWindow(), "Attach Disk Failed",
                                 QString("Failed to attach virtual disk:\n\n%1").arg(action->errorMessage()));
        } else
        {
            qWarning() << "[AttachVirtualDiskCommand] Dialog rejected but no error message";
        }
    }
}

QString AttachVirtualDiskCommand::menuText() const
{
    return "Attach Virtual Disk...";
}

bool AttachVirtualDiskCommand::isVMSelected() const
{
    return getSelectedObjectType() == "vm";
}

QString AttachVirtualDiskCommand::getSelectedVMRef() const
{
    if (!isVMSelected())
        return QString();
    return getSelectedObjectRef();
}

int AttachVirtualDiskCommand::getMaxVBDsAllowed(const QVariantMap& vmData) const
{
    // Check allowed_VBD_devices property
    QVariantList allowedDevices = vmData.value("allowed_VBD_devices").toList();
    if (!allowedDevices.isEmpty())
        return allowedDevices.size();

    // Default max VBDs if property not present
    return 16;
}

int AttachVirtualDiskCommand::getCurrentVBDCount(const QString& vmRef) const
{
    XenLib* xenLib = mainWindow()->xenLib();
    if (!xenLib)
        return 0;

    XenCache* cache = xenLib->getCache();
    if (!cache)
        return 0;

    // Get all VBDs and count those belonging to this VM
    QList<QVariantMap> vbds = cache->GetAllData("vbd");
    int count = 0;

    for (const QVariantMap& vbd : vbds)
    {
        if (vbd.value("VM").toString() == vmRef)
            count++;
    }

    return count;
}
