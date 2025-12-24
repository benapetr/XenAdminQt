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
#include "../../sr.h"
#include "../../session.h"
#include "../../xenapi/xenapi_VDI.h"
#include "../../xenapi/xenapi_VBD.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../xenapi/xenapi_SR.h"
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
                                   VM* vm,
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
    addApiMethodToRoleCheck("VM.get_allowed_VBD_devices");
    addApiMethodToRoleCheck("VDI.create");
    addApiMethodToRoleCheck("VBD.create");
}

void CreateDiskAction::run()
{
    XenSession* session = this->session();
    if (!session || !session->IsLoggedIn())
    {
        throw std::runtime_error("Not connected to XenServer");
    }

    if (!m_attachToVM)
    {
        // Simple VDI creation without VM attachment
        setPercentComplete(0);
        setDescription(tr("Creating VDI..."));

        QString vdiRef = XenAPI::VDI::create(session, m_vdiRecord);

        if (vdiRef.isEmpty())
        {
            throw std::runtime_error("Failed to create VDI - empty reference returned");
        }

        // Store the result so caller can get it via result()
        setResult(vdiRef);

        setPercentComplete(100);
        setDescription(tr("Virtual disk created"));

    } else
    {
        // Create VDI and attach to VM
        if (!m_vm)
        {
            throw std::runtime_error("VM object is null");
        }

        // Step 1: Get allowed VBD device numbers
        setPercentComplete(10);
        setDescription(tr("Getting available device numbers..."));

        QVariant allowedDevicesVar = XenAPI::VM::get_allowed_VBD_devices(session, m_vm->OpaqueRef());
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
        setPercentComplete(30);
        setDescription(tr("Creating VDI..."));

        QString vdiRef = XenAPI::VDI::create(session, m_vdiRecord);

        if (vdiRef.isEmpty())
        {
            throw std::runtime_error("Failed to create VDI - empty reference returned");
        }

        // Store the result so caller can get it via result()
        setResult(vdiRef);

        // Step 3: Check if VM already has bootable disk
        setPercentComplete(50);
        setDescription(tr("Checking VM disk configuration..."));

        bool alreadyHasBootableDisk = hasBootableDisk();
        bool shouldBeBootable = (userdevice == "0") && !alreadyHasBootableDisk;

        // Step 4: Create VBD record
        setPercentComplete(60);
        setDescription(tr("Creating VBD..."));

        QVariantMap vbdRecord = m_vbdRecord;
        vbdRecord["VDI"] = vdiRef;
        vbdRecord["VM"] = m_vm->OpaqueRef();
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

        QString vbdRef = XenAPI::VBD::create(session, vbdRecord);

        setPercentComplete(100);
        setDescription(tr("Virtual disk created and attached to VM"));
    }
}
bool CreateDiskAction::hasBootableDisk()
{
    if (!m_vm)
    {
        return false;
    }

    try
    {
        XenSession* session = this->session();
        if (!session || !session->IsLoggedIn())
        {
            return false;
        }

        // Get all VBDs for this VM
        QStringList vbdRefs = m_vm->VBDRefs();

        for (const QString& vbdRef : vbdRefs)
        {
            // Get VBD record
            QVariantMap vbdRecord = XenAPI::VBD::get_record(session, vbdRef);

            // Skip CD and floppy drives
            QString type = vbdRecord.value("type").toString();
            if (type == "CD")
            {
                continue;
            }

            // Check if bootable
            bool bootable = vbdRecord.value("bootable").toBool();
            if (!bootable)
            {
                continue;
            }

            // Get VDI reference
            QString vdiRef = vbdRecord.value("VDI").toString();
            if (vdiRef.isEmpty() || vdiRef == "OpaqueRef:NULL")
            {
                continue;
            }

            // Get VDI record to check SR
            QVariantMap vdiRecord = XenAPI::VDI::get_record(session, vdiRef);
            QString srRef = vdiRecord.value("SR").toString();

            if (!srRef.isEmpty() && srRef != "OpaqueRef:NULL")
            {
                // Get SR record to check if it's a tools SR
                QVariantMap srRecord = XenAPI::SR::get_record(session, srRef);
                QVariantMap otherConfig = srRecord.value("other_config").toMap();

                // Skip tools SR (special SR for XenServer Tools ISOs)
                if (otherConfig.value("xenserver_tools_sr").toBool())
                {
                    continue;
                }

                // Found a bootable disk that's not in tools SR
                return true;
            }
        }

        return false;

    } catch (...)
    {
        // If we can't check, assume no bootable disk
        return false;
    }
}
