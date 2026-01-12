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

#include "createcddriveaction.h"
#include "xen/vm.h"
#include "xen/vbd.h"
#include "xencache.h"
#include "xen/network/connection.h"
#include "xen/session.h"
#include "xen/actions/vbd/vbdcreateandplugaction.h"
#include "xen/xenapi/xenapi_VM.h"
#include <QObject>

CreateCdDriveAction::CreateCdDriveAction(QSharedPointer<VM> vm, QObject* parent)
    : AsyncOperation(vm ? vm->GetConnection() : nullptr,
                     vm ? tr("Creating DVD drive for '%1'").arg(vm->GetName()) : tr("Creating DVD drive"),
                     QString(),
                     parent)
    , vm_(vm)
{
}

void CreateCdDriveAction::run()
{
    if (!this->vm_)
    {
        this->setError(tr("No VM specified"));
        return;
    }

    XenConnection* connection = this->vm_->GetConnection();
    if (!connection)
    {
        this->setError(tr("VM has no connection"));
        return;
    }

    XenAPI::Session* session = connection->GetSession();
    if (!session || !session->IsLoggedIn())
    {
        this->setError(tr("Not connected to XenServer"));
        return;
    }

    XenCache* cache = connection->GetCache();
    if (!cache)
    {
        this->setError(tr("Cache not available"));
        return;
    }

    // Check if VM already has a CD-ROM drive (matches C# VM.FindVMCDROM())
    QSharedPointer<VBD> existingCdrom = this->vm_->FindVMCDROM();

    if (existingCdrom)
    {
        // CD-ROM already exists, nothing to do
        this->SetDescription(tr("DVD drive already exists"));
        return;
    }

    // CD-ROM doesn't exist, create it
    this->SetDescription(tr("Creating DVD drive..."));

    // Check max VBDs allowed (matches C# logic)
    QStringList vbdRefs = this->vm_->GetVBDRefs();
    int currentVbdCount = vbdRefs.count();
    int maxVbds = this->vm_->MaxVBDsAllowed();
    
    if (currentVbdCount >= maxVbds)
    {
        this->setError(tr("Maximum number of VBDs (%1) has been reached. Cannot create a new CD drive.").arg(maxVbds));
        return;
    }

    // Get allowed VBD devices from XenAPI
    QString vmRef = this->vm_->OpaqueRef();
    QStringList allowedDevices;
    
    try
    {
        QVariant devicesVariant = XenAPI::VM::get_allowed_VBD_devices(session, vmRef);
        if (devicesVariant.canConvert<QStringList>())
        {
            allowedDevices = devicesVariant.toStringList();
        }
        else if (devicesVariant.canConvert<QVariantList>())
        {
            const QVariantList deviceList = devicesVariant.toList();
            for (const QVariant& device : deviceList)
                allowedDevices.append(device.toString());
        }
        else if (devicesVariant.typeId() == QMetaType::QString)
        {
            allowedDevices.append(devicesVariant.toString());
        }
    }
    catch (const std::exception& e)
    {
        this->setError(tr("Failed to get allowed VBD devices: %1").arg(e.what()));
        return;
    }

    if (allowedDevices.isEmpty())
    {
        this->setError(tr("Maximum number of VBDs has been reached. No device slots available."));
        return;
    }

    // Prefer device "3" if available (standard CD-ROM position), otherwise use first available
    QString userdevice = allowedDevices.contains("3") ? "3" : allowedDevices.first();

    // Build VBD record for new CD drive
    QVariantMap vbdRecord;
    vbdRecord["VM"] = vmRef;
    vbdRecord["VDI"] = "OpaqueRef:NULL";  // Empty drive
    vbdRecord["bootable"] = false;
    vbdRecord["device"] = "";  // Auto-assign device name
    vbdRecord["userdevice"] = userdevice;
    vbdRecord["empty"] = true;
    vbdRecord["type"] = "CD";
    vbdRecord["mode"] = "RO";

    // Use VbdCreateAndPlugAction to create and plug the drive
    VbdCreateAndPlugAction* createAction = new VbdCreateAndPlugAction(
        this->vm_,
        vbdRecord,
        tr("DVD Drive"),
        true,  // suppress progress notifications (we handle them)
        this
    );

    // Forward showUserInstruction signal
    connect(createAction, &VbdCreateAndPlugAction::showUserInstruction,
            this, &CreateCdDriveAction::showUserInstruction);

    // Handle completion
    connect(createAction, &AsyncOperation::completed, this, [this]() {
        this->SetDescription(tr("DVD drive created successfully"));
    });

    connect(createAction, &AsyncOperation::failed, this, [this](const QString& error) {
        this->setError(error);
    });

    // Run synchronously (blocking this thread, but we're on a worker thread)
    createAction->RunSync();
}
