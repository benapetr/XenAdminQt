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
#include <QMessageBox>
#include "../../mainwindow.h"
#include "../../dialogs/attachvirtualdiskdialog.h"
#include "../../dialogs/actionprogressdialog.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/vbd.h"
#include "xenlib/xen/vdi.h"
#include "xenlib/xen/actions/vbd/vbdcreateandplugaction.h"

AttachVirtualDiskCommand::AttachVirtualDiskCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

bool AttachVirtualDiskCommand::CanRun() const
{
    // Can attach virtual disk if VM is selected and not a snapshot
    if (!this->isVMSelected())
        return false;

    QSharedPointer<VM> vm = qSharedPointerDynamicCast<VM>(this->GetObject());

    if (!vm)
        return false;

    // Cannot attach to snapshot or locked VM
    if (vm->IsSnapshot())
        return false;

    return vm->CurrentOperations().isEmpty();
}

void AttachVirtualDiskCommand::Run()
{
    if (!this->isVMSelected())
        return;

    QSharedPointer<VM> vm = qSharedPointerDynamicCast<VM>(this->GetObject());

    if (!vm)
        return;

    // Check VBD limit
    int maxVBDs = this->getMaxVBDsAllowed(vm);
    int currentVBDs = this->getCurrentVBDCount(vm);

    if (currentVBDs >= maxVBDs)
    {
        QMessageBox::warning(mainWindow(), "Maximum VBDs Reached",
                             QString("The maximum number of virtual disks (%1) has been reached for this VM.\n\n"
                                     "Please detach a disk before attaching a new one.")
                                 .arg(maxVBDs));
        return;
    }

    // Launch attach dialog
    AttachVirtualDiskDialog dialog(vm, mainWindow());

    qDebug() << "[AttachVirtualDiskCommand] Showing AttachVirtualDiskDialog modally...";
    if (dialog.exec() != QDialog::Accepted)
    {
        qDebug() << "[AttachVirtualDiskCommand] Dialog cancelled by user";
        return;
    }

    qDebug() << "[AttachVirtualDiskCommand] Dialog accepted, proceeding with attachment";
    this->performAttachment(&dialog, vm);
}

void AttachVirtualDiskCommand::performAttachment(AttachVirtualDiskDialog* dialog, QSharedPointer<VM> vm)
{
    if (!vm->IsConnected())
    {
        qWarning() << "[AttachVirtualDiskCommand] No connection available, aborting";
        QMessageBox::warning(mainWindow(), "Error", "No connection available");
        return;
    }

    qDebug() << "[AttachVirtualDiskCommand] Starting attachment process for VM:" << vm->OpaqueRef();

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

    // Get VDI name for UI feedback
    QSharedPointer<VDI> vdi = vm->GetCache()->ResolveObject<VDI>(vdiRef);
    QString vdiName = vdi ? vdi->GetName() : QString();
    if (vdiName.isEmpty())
        vdiName = "Virtual Disk";
    qDebug() << "[AttachVirtualDiskCommand] VDI name:" << vdiName;

    // Build VBD record (matches C# AttachDiskDialog.cs lines 206-216)
    QVariantMap vbdRecord;
    vbdRecord["VDI"] = vdiRef;
    vbdRecord["VM"] = vm->OpaqueRef();
    vbdRecord["bootable"] = bootable;
    vbdRecord["device"] = QString(""); // Will be filled by XenAPI
    vbdRecord["empty"] = false;
    vbdRecord["userdevice"] = devicePosition;
    vbdRecord["type"] = "Disk";
    vbdRecord["mode"] = (mode == "RO") ? "RO" : "RW";
    vbdRecord["unpluggable"] = true;

    // Check if this is the first VBD for this VDI (owner flag)
    QList<QSharedPointer<VBD>> allVbds = vm->GetCache()->GetAll<VBD>();
    bool isOwner = true;
    for (const QSharedPointer<VBD>& vbd : allVbds)
    {
        if (vbd && vbd->GetVDIRef() == vdiRef)
        {
            isOwner = false;
            break;
        }
    }
    vbdRecord["owner"] = isOwner;
    qDebug() << "[AttachVirtualDiskCommand] VBD owner flag:" << isOwner;

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
    ActionProgressDialog* progressDialog = new ActionProgressDialog(action, mainWindow());
    progressDialog->setAttribute(Qt::WA_DeleteOnClose);

    qDebug() << "[AttachVirtualDiskCommand] Executing progress dialog...";
    int dialogResult = progressDialog->exec();
    qDebug() << "[AttachVirtualDiskCommand] Progress dialog result:" << dialogResult
             << "(Accepted=" << QDialog::Accepted << ")";
    qDebug() << "[AttachVirtualDiskCommand] Action state:"
             << "hasError=" << action->HasError()
             << "isCancelled=" << action->IsCancelled()
             << "errorMessage=" << action->GetErrorMessage();

    if (dialogResult == QDialog::Accepted)
    {
        qDebug() << "[AttachVirtualDiskCommand] Attachment succeeded";
        mainWindow()->ShowStatusMessage(QString("Virtual disk '%1' attached successfully").arg(vdiName), 5000);
    } else
    {
        qWarning() << "[AttachVirtualDiskCommand] Attachment failed or cancelled";
        if (!action->GetErrorMessage().isEmpty())
        {
            qWarning() << "[AttachVirtualDiskCommand] Error message:" << action->GetErrorMessage();
            QMessageBox::warning(mainWindow(), "Attach Disk Failed",
                                 QString("Failed to attach virtual disk:\n\n%1").arg(action->GetErrorMessage()));
        } else
        {
            qWarning() << "[AttachVirtualDiskCommand] Dialog rejected but no error message";
        }
    }
}

QString AttachVirtualDiskCommand::MenuText() const
{
    return "Attach Virtual Disk...";
}

bool AttachVirtualDiskCommand::isVMSelected() const
{
    return this->getSelectedObjectType() == XenObjectType::VM;
}

QString AttachVirtualDiskCommand::getSelectedVMRef() const
{
    if (!this->isVMSelected())
        return QString();
    return this->getSelectedObjectRef();
}

int AttachVirtualDiskCommand::getMaxVBDsAllowed(const QSharedPointer<VM>& vm) const
{
    if (!vm)
        return 0;

    // Check allowed_VBD_devices property
    QVariantList allowedDevices = vm->GetData().value("allowed_VBD_devices").toList();
    if (!allowedDevices.isEmpty())
        return allowedDevices.size();

    // Default max VBDs if property not present
    return 16;
}

int AttachVirtualDiskCommand::getCurrentVBDCount(const QSharedPointer<VM>& vm) const
{
    if (!vm)
        return 0;

    return vm->GetVBDs().size();
}
