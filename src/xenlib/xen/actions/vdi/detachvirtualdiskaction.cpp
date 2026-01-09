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

#include "detachvirtualdiskaction.h"
#include "../../vm.h"
#include "../../vdi.h"
#include "../../vbd.h"
#include "../../session.h"
#include "../../xenapi/xenapi_VDI.h"
#include "../../xenapi/xenapi_VBD.h"

DetachVirtualDiskAction::DetachVirtualDiskAction(const QString& vdiRef,
                                                 VM* vm,
                                                 QObject* parent)
    : AsyncOperation(vm->GetConnection(),
                     tr("Detaching disk from VM '%1'").arg(vm->GetName()),
                     tr("Detaching virtual disk..."),
                     parent),
      m_vdiRef(vdiRef), m_vm(vm)
{
    // Find the VBD connecting this VDI to the VM
    QStringList vbdRefs = vm->GetVBDRefs();
    for (const QString& vbdRef : vbdRefs)
    {
        // We'll check VDI reference in run() when we have session
        m_vbdRef = vbdRef; // Store for later check
    }

    // Add RBAC method checks
    AddApiMethodToRoleCheck("VBD.get_allowed_operations");
    AddApiMethodToRoleCheck("VBD.async_unplug");
    AddApiMethodToRoleCheck("VBD.async_destroy");
}

void DetachVirtualDiskAction::run()
{
    XenAPI::Session* session = this->GetSession();
    if (!session || !session->IsLoggedIn())
    {
        throw std::runtime_error("Not connected to XenServer");
    }

    if (!m_vm)
    {
        throw std::runtime_error("VM object is null");
    }

    // Find the VBD that connects this VDI to the VM
    SetPercentComplete(0);
    SetDescription(tr("Finding VBD..."));

    QString vbdRef;
    QStringList vbdRefs = m_vm->GetVBDRefs();

    for (const QString& ref : vbdRefs)
    {
        QVariantMap vbdRecord = XenAPI::VBD::get_record(session, ref);
        QString vdiRefFromVBD = vbdRecord.value("VDI").toString();

        if (vdiRefFromVBD == m_vdiRef)
        {
            vbdRef = ref;
            m_vbdRef = ref;
            break;
        }
    }

    if (vbdRef.isEmpty())
    {
        throw std::runtime_error("VBD not found for this VDI and VM combination");
    }

    // Get VBD details
    QVariantMap vbdRecord = XenAPI::VBD::get_record(session, vbdRef);
    bool currentlyAttached = vbdRecord.value("currently_attached").toBool();

    // Step 1: Unplug VBD if currently attached
    if (currentlyAttached)
    {
        SetPercentComplete(10);
        SetDescription(tr("Checking if VBD can be unplugged..."));

        // Check if unplug is allowed
        QVariant allowedOpsVar = XenAPI::VBD::get_allowed_operations(session, vbdRef);
        QStringList allowedOps;

        if (allowedOpsVar.canConvert<QStringList>())
        {
            allowedOps = allowedOpsVar.toStringList();
        } else if (allowedOpsVar.canConvert<QVariantList>())
        {
            for (const QVariant& op : allowedOpsVar.toList())
            {
                allowedOps.append(op.toString());
            }
        }

        if (allowedOps.contains("unplug"))
        {
            SetPercentComplete(20);
            SetDescription(tr("Unplugging VBD..."));

            QString taskRef = XenAPI::VBD::async_unplug(session, vbdRef);
            pollToCompletion(taskRef, 20, 50);
        }
    }

    // Step 2: Destroy VBD
    SetPercentComplete(50);
    SetDescription(tr("Destroying VBD..."));

    QString taskRef = XenAPI::VBD::async_destroy(session, vbdRef);
    pollToCompletion(taskRef, 50, 100);

    SetPercentComplete(100);
    SetDescription(tr("Virtual disk detached"));
}
