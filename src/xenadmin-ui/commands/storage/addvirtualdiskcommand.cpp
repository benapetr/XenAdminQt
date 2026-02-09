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

#include <QMessageBox>
#include <QDebug>
#include "addvirtualdiskcommand.h"
#include "../../mainwindow.h"
#include "../../dialogs/newvirtualdiskdialog.h"
#include "../../dialogs/actionprogressdialog.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/actions/vdi/creatediskaction.h"
#include "xenlib/xen/actions/vbd/vbdcreateandplugaction.h"
#include "xenlib/xen/sr.h"

AddVirtualDiskCommand::AddVirtualDiskCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

bool AddVirtualDiskCommand::CanRun() const
{
    // Can add virtual disk if SR or VM is selected
    return (this->isSRSelected() || this->isVMSelected()) && this->canAddDisk();
}

void AddVirtualDiskCommand::Run()
{
    XenObjectType objectType = this->getSelectedObjectType();
    QString objectRef = this->getSelectedRef();
    QSharedPointer<XenObject> object = this->GetObject();
    if (!object || !object->GetConnection())
        return;
    XenCache *cache = object->GetCache();

    if (objectType == XenObjectType::VM)
    {
        QSharedPointer<VM> vm = qSharedPointerDynamicCast<VM>(object);

        if (!vm)
            return;

        // Check VBD limit
        QVariantMap vmData = vm->GetData();
        int maxVBDs = this->getMaxVBDsAllowed(vmData);
        int currentVBDs = this->getCurrentVBDCount(cache, objectRef);

        if (currentVBDs >= maxVBDs)
        {
            QMessageBox::warning(MainWindow::instance(), tr("Cannot Add Disk"), tr("The maximum number of virtual disks (%1) has been reached for this VM.").arg(maxVBDs));
            return;
        }

        // Open NewVirtualDiskDialog for VM (modal)
        qDebug() << "[AddVirtualDiskCommand] Opening NewVirtualDiskDialog for VM:" << objectRef;
        NewVirtualDiskDialog dialog(vm, MainWindow::instance());
        if (dialog.exec() != QDialog::Accepted)
        {
            qDebug() << "[AddVirtualDiskCommand] Dialog cancelled by user";
            return;
        }

        // Get parameters from dialog
        QString srRef = dialog.getSelectedSR();
        QString name = dialog.getVDIName();
        QString description = dialog.getVDIDescription();
        qint64 size = dialog.getSize();
        QString devicePosition = dialog.getDevicePosition();
        QString mode = dialog.getMode();
        bool bootable = dialog.isBootable();

        qDebug() << "[AddVirtualDiskCommand] VDI parameters:";
        qDebug() << "  SR:" << srRef;
        qDebug() << "  Name:" << name;
        qDebug() << "  Size:" << size;
        qDebug() << "  Device position:" << devicePosition;
        qDebug() << "  Mode:" << mode;
        qDebug() << "  Bootable:" << bootable;

        // Build VDI record
        QVariantMap vdiRecord;
        vdiRecord["name_label"] = name;
        vdiRecord["name_description"] = description;
        vdiRecord["SR"] = srRef;
        vdiRecord["virtual_size"] = QString::number(size);
        vdiRecord["type"] = "user";
        vdiRecord["sharable"] = false;
        vdiRecord["read_only"] = false;
        vdiRecord["other_config"] = QVariantMap();

        // Create VDI using CreateDiskAction
        qDebug() << "[AddVirtualDiskCommand] Creating VDI with CreateDiskAction...";
        CreateDiskAction* createAction = new CreateDiskAction(vdiRecord, vm->GetConnection(), this);

        ActionProgressDialog* createDialog = new ActionProgressDialog(createAction, MainWindow::instance());
        qDebug() << "[AddVirtualDiskCommand] Executing create dialog...";
        int createResult = createDialog->exec();

        if (createResult != QDialog::Accepted)
        {
            qWarning() << "[AddVirtualDiskCommand] VDI creation failed or cancelled";
            QMessageBox::warning(MainWindow::instance(), tr("Failed"), tr("Failed to create virtual disk."));
            delete createDialog;
            return;
        }

        QString vdiRef = createAction->GetResult();
        qDebug() << "[AddVirtualDiskCommand] VDI created successfully, ref:" << vdiRef;
        delete createDialog;

        if (vdiRef.isEmpty())
        {
            qWarning() << "[AddVirtualDiskCommand] VDI ref is empty despite success";
            QMessageBox::warning(MainWindow::instance(), tr("Failed"), tr("Failed to create virtual disk."));
            return;
        }

        QVariantMap vbdRecord;
        vbdRecord["VM"] = objectRef;
        vbdRecord["VDI"] = vdiRef;
        vbdRecord["userdevice"] = devicePosition;
        vbdRecord["bootable"] = bootable;
        vbdRecord["mode"] = mode;
        vbdRecord["type"] = "Disk";
        vbdRecord["unpluggable"] = true;
        vbdRecord["empty"] = false;
        vbdRecord["other_config"] = QVariantMap();
        vbdRecord["qos_algorithm_type"] = "";
        vbdRecord["qos_algorithm_params"] = QVariantMap();

        qDebug() << "[AddVirtualDiskCommand] Creating VbdCreateAndPlugAction to attach VDI to VM...";
        VbdCreateAndPlugAction* attachAction = new VbdCreateAndPlugAction(vm, vbdRecord, name, false, this);

        ActionProgressDialog* attachDialog = new ActionProgressDialog(attachAction, MainWindow::instance());
        qDebug() << "[AddVirtualDiskCommand] Executing attach dialog...";
        int attachResult = attachDialog->exec();
        qDebug() << "[AddVirtualDiskCommand] Attach dialog result:" << attachResult
                 << "(Accepted=" << QDialog::Accepted << ")";
        qDebug() << "[AddVirtualDiskCommand] VbdCreateAndPlugAction state:"
                 << "hasError=" << attachAction->HasError()
                 << "isCancelled=" << attachAction->IsCancelled()
                 << "errorMessage=" << attachAction->GetErrorMessage();

        if (attachResult != QDialog::Accepted)
        {
            qWarning() << "[AddVirtualDiskCommand] VBD attachment failed or cancelled";
            QMessageBox::warning(MainWindow::instance(), tr("Warning"), tr("Virtual disk created but failed to attach to VM.\nYou can attach it manually from the Attach menu."));
            delete attachDialog;
            return;
        }

        qDebug() << "[AddVirtualDiskCommand] VBD attached successfully";
        delete attachDialog;

        MainWindow::instance()->ShowStatusMessage(tr("Virtual disk created and attached successfully"), 5000);
    } else if (objectType == XenObjectType::SR)
    {
        QSharedPointer<SR> sr = qSharedPointerDynamicCast<SR>(object);
        if (!sr)
            return;

        qDebug() << "[AddVirtualDiskCommand] Opening NewVirtualDiskDialog for SR:" << objectRef;
        NewVirtualDiskDialog dialog(sr, MainWindow::instance());
        if (dialog.exec() != QDialog::Accepted)
        {
            qDebug() << "[AddVirtualDiskCommand] Dialog cancelled by user";
            return;
        }

        QString srRef = dialog.getSelectedSR();
        QString name = dialog.getVDIName();
        QString description = dialog.getVDIDescription();
        qint64 size = dialog.getSize();

        qDebug() << "[AddVirtualDiskCommand] VDI parameters:";
        qDebug() << "  SR:" << srRef;
        qDebug() << "  Name:" << name;
        qDebug() << "  Size:" << size;

        QVariantMap vdiRecord;
        vdiRecord["name_label"] = name;
        vdiRecord["name_description"] = description;
        vdiRecord["SR"] = srRef;
        vdiRecord["virtual_size"] = QString::number(size);
        vdiRecord["type"] = "user";
        vdiRecord["sharable"] = false;
        vdiRecord["read_only"] = false;
        vdiRecord["other_config"] = QVariantMap();

        qDebug() << "[AddVirtualDiskCommand] Creating VDI with CreateDiskAction...";
        CreateDiskAction* createAction = new CreateDiskAction(vdiRecord, sr->GetConnection(), this);

        ActionProgressDialog* createDialog = new ActionProgressDialog(createAction, MainWindow::instance());
        qDebug() << "[AddVirtualDiskCommand] Executing create dialog...";
        int createResult = createDialog->exec();

        if (createResult != QDialog::Accepted)
        {
            qWarning() << "[AddVirtualDiskCommand] VDI creation failed or cancelled";
            QMessageBox::warning(MainWindow::instance(), tr("Failed"), tr("Failed to create virtual disk."));
            delete createDialog;
            return;
        }

        QString vdiRef = createAction->GetResult();
        qDebug() << "[AddVirtualDiskCommand] VDI created successfully, ref:" << vdiRef;
        delete createDialog;

        if (vdiRef.isEmpty())
        {
            qWarning() << "[AddVirtualDiskCommand] VDI ref is empty despite success";
            QMessageBox::warning(MainWindow::instance(), tr("Failed"), tr("Failed to create virtual disk."));
            return;
        }

        MainWindow::instance()->ShowStatusMessage(tr("Virtual disk created successfully"), 5000);
    }
}

QString AddVirtualDiskCommand::MenuText() const
{
    return "Add Virtual Disk...";
}

bool AddVirtualDiskCommand::isSRSelected() const
{
    return this->getSelectedObjectType() == XenObjectType::SR;
}

bool AddVirtualDiskCommand::isVMSelected() const
{
    return this->getSelectedObjectType() == XenObjectType::VM;
}

QString AddVirtualDiskCommand::getSelectedRef() const
{
    return this->getSelectedObjectRef();
}

bool AddVirtualDiskCommand::canAddDisk() const
{
    XenObjectType objectType = this->getSelectedObjectType();
    QString objectRef = this->getSelectedRef();
    QSharedPointer<XenObject> object = this->GetObject();
    if (!object || !object->GetConnection() || !object->GetConnection()->GetCache())
        return false;

    XenCache* cache = object->GetConnection()->GetCache();

    if (objectType == XenObjectType::SR)
    {
        QSharedPointer<SR> sr = cache->ResolveObject<SR>(XenObjectType::SR, objectRef);
        if (!sr)
            return false;

        // Check if SR is locked
        return sr->CurrentOperations().isEmpty();
    } else if (objectType == XenObjectType::VM)
    {
        QSharedPointer<VM> vm = cache->ResolveObject<VM>(XenObjectType::VM, objectRef);
        if (!vm)
            return false;

        // Cannot add disk to snapshot or locked VM
        if (vm->IsSnapshot())
            return false;
        return vm->CurrentOperations().isEmpty();
    }

    return false;
}

int AddVirtualDiskCommand::getMaxVBDsAllowed(const QVariantMap& vmData) const
{
    // Default max is 16 VBDs for most VMs
    // Check allowed_VBD_devices to get actual limit
    QVariantList allowedDevices = vmData.value("allowed_VBD_devices").toList();
    if (!allowedDevices.isEmpty())
    {
        return allowedDevices.size();
    }

    // Fallback: standard limit is 16 (0-15 device positions)
    return 16;
}

int AddVirtualDiskCommand::getCurrentVBDCount(const QString& vmRef) const
{
    return this->getCurrentVBDCount(nullptr, vmRef);
}

int AddVirtualDiskCommand::getCurrentVBDCount(XenCache* cache, const QString& vmRef) const
{
    if (!cache)
        return 0;

    // Get all VBDs and count those attached to this VM
    QList<QVariantMap> allVBDs = cache->GetAllData(XenObjectType::VBD);

    int count = 0;
    for (const QVariantMap& vbdData : allVBDs)
    {
        QString vbdVMRef = vbdData.value("VM").toString();
        if (vbdVMRef == vmRef)
        {
            count++;
        }
    }

    return count;
}
