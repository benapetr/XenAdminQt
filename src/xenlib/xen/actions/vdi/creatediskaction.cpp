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

#include "creatediskaction.h"
#include "../../vm.h"
#include "../../vdi.h"
#include "../../vbd.h"
#include "../vbd/vbdcreateandplugaction.h"
#include "../../sr.h"
#include "../../session.h"
#include "../../xenapi/xenapi_VDI.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../../xencache.h"

CreateDiskAction::CreateDiskAction(const QVariantMap& vdiRecord,
                                   XenConnection* connection,
                                   QObject* parent)
    : AsyncOperation(connection,
                     tr("Creating disk '%1'").arg(vdiRecord.value("name_label").toString()),
                     tr("Creating virtual disk..."),
                     parent),
      m_vdiRecord(vdiRecord), m_vm(nullptr), m_attachToVM(false)
{
}

CreateDiskAction::CreateDiskAction(const QVariantMap& vdiRecord,
                                   const QVariantMap& vbdRecord,
                                   QSharedPointer<VM> vm,
                                   QObject* parent)
    : AsyncOperation(vm->GetConnection(),
                     tr("Creating disk '%1' on VM '%2'")
                         .arg(vdiRecord.value("name_label").toString())
                         .arg(vm->GetName()),
                     tr("Creating and attaching virtual disk..."),
                     parent),
      m_vdiRecord(vdiRecord), m_vbdRecord(vbdRecord), m_vm(vm), m_attachToVM(true)
{
    // Add RBAC method checks
    AddApiMethodToRoleCheck("VM.get_allowed_VBD_devices");
    AddApiMethodToRoleCheck("VDI.create");
    AddApiMethodToRoleCheck("VBD.create");
    this->AddApiMethodToRoleCheck("VBD.get_allowed_operations");
    this->AddApiMethodToRoleCheck("VBD.async_plug");
}

void CreateDiskAction::run()
{
    XenAPI::Session* session = this->GetSession();
    if (!session || !session->IsLoggedIn())
    {
        throw std::runtime_error("Not connected to XenServer");
    }

    if (!this->m_attachToVM)
    {
        // Simple VDI creation without VM attachment
        this->SetPercentComplete(0);
        this->SetDescription(tr("Creating VDI..."));

        QString vdiRef = XenAPI::VDI::create(session, this->m_vdiRecord);

        if (vdiRef.isEmpty())
        {
            throw std::runtime_error("Failed to create VDI - empty reference returned");
        }

        // Store the result so caller can get it via result()
        this->SetResult(vdiRef);

        this->SetPercentComplete(100);
        this->SetDescription(tr("Virtual disk created"));

    } else
    {
        // Create VDI and attach to VM
        if (!this->m_vm)
        {
            throw std::runtime_error("VM object is null");
        }

        // Step 1: Get allowed VBD device numbers
        this->SetPercentComplete(10);
        this->SetDescription(tr("Getting available device numbers..."));

        QVariant allowedDevicesVar = XenAPI::VM::get_allowed_VBD_devices(session, this->m_vm->OpaqueRef());
        QStringList allowedDevices;

        if (allowedDevicesVar.canConvert<QStringList>())
        {
            allowedDevices = allowedDevicesVar.toStringList();
        } else if (allowedDevicesVar.canConvert<QVariantList>())
        {
            for (const QVariant& dev : allowedDevicesVar.toList())
            {
                allowedDevices.append(dev.toString());
            }
        }

        if (allowedDevices.isEmpty())
        {
            throw std::runtime_error("Maximum number of VBDs already attached to VM");
        }

        QString userdevice = allowedDevices.first();

        // Step 2: Create VDI
        this->SetPercentComplete(30);
        this->SetDescription(tr("Creating VDI..."));

        QString vdiRef = XenAPI::VDI::create(session, this->m_vdiRecord);

        if (vdiRef.isEmpty())
        {
            throw std::runtime_error("Failed to create VDI - empty reference returned");
        }

        // Store the result so caller can get it via result()
        this->SetResult(vdiRef);

        // Step 3: Check if VM already has bootable disk
        this->SetPercentComplete(50);
        this->SetDescription(tr("Checking VM disk configuration..."));

        bool alreadyHasBootableDisk = this->hasBootableDisk();
        bool shouldBeBootable = (userdevice == "0") && !alreadyHasBootableDisk;

        // Step 4: Create VBD record
        this->SetPercentComplete(60);
        this->SetDescription(tr("Creating VBD..."));

        QVariantMap vbdRecord = this->m_vbdRecord;
        vbdRecord["VDI"] = vdiRef;
        vbdRecord["VM"] = this->m_vm->OpaqueRef();
        vbdRecord["userdevice"] = userdevice;
        vbdRecord["bootable"] = shouldBeBootable;

        // Set defaults if not specified
        if (!vbdRecord.contains("mode"))
        {
            vbdRecord["mode"] = "RW";
        }
        if (!vbdRecord.contains("type"))
        {
            vbdRecord["type"] = "Disk";
        }
        if (!vbdRecord.contains("unpluggable"))
        {
            vbdRecord["unpluggable"] = true;
        }
        if (!vbdRecord.contains("empty"))
        {
            vbdRecord["empty"] = false;
        }
        if (!vbdRecord.contains("qos_algorithm_type"))
        {
            vbdRecord["qos_algorithm_type"] = "";
        }
        if (!vbdRecord.contains("qos_algorithm_params"))
        {
            vbdRecord["qos_algorithm_params"] = QVariantMap();
        }

        VbdCreateAndPlugAction attachAction(this->m_vm, vbdRecord,
                                           this->m_vdiRecord.value("name_label").toString(), true);
        connect(&attachAction, &VbdCreateAndPlugAction::showUserInstruction,
                this, &CreateDiskAction::showUserInstruction, Qt::DirectConnection);
        attachAction.RunSync(session);
        if (attachAction.HasError())
        {
            this->setError(attachAction.GetErrorMessage(), attachAction.GetErrorDetails());
            return;
        }
        if (attachAction.IsCancelled())
        {
            this->setState(Cancelled);
            return;
        }

        this->SetPercentComplete(100);
        this->SetDescription(tr("Virtual disk created and attached to VM"));
    }
}
bool CreateDiskAction::hasBootableDisk()
{
    if (!this->m_vm)
    {
        return false;
    }

    // Match C#: inspect cached VBDs and ignore CD/floppy and tools media.
    for (const auto& vbd : this->m_vm->GetVBDs())
    {
        if (!vbd || vbd->IsCD() || vbd->IsFloppyDrive() || !vbd->IsBootable())
            continue;
        const auto vdi = vbd->GetVDI();
        const auto sr = vdi ? vdi->GetSR() : QSharedPointer<SR>();
        if (sr && sr->IsToolsSR())
            continue;
        return true;
    }
    return false;
}
