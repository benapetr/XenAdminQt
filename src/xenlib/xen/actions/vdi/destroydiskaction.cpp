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

#include "destroydiskaction.h"
#include "detachvirtualdiskaction.h"
#include "../../vdi.h"
#include "../../vbd.h"
#include "../../vm.h"
#include "../../sr.h"
#include "../../session.h"
#include "../../xenapi/VDI.h"
#include "../../xenapi/VBD.h"
#include "../../xenapi/xenapi_VM.h"

DestroyDiskAction::DestroyDiskAction(const QString& vdiRef,
                                     XenConnection* connection,
                                     bool allowRunningVMDelete,
                                     QObject* parent)
    : AsyncOperation(connection,
                     tr("Deleting virtual disk"),
                     tr("Deleting virtual disk..."),
                     parent),
      m_vdiRef(vdiRef), m_allowRunningVMDelete(allowRunningVMDelete)
{
    // Add RBAC method checks
    addApiMethodToRoleCheck("VBD.unplug");
    addApiMethodToRoleCheck("VBD.destroy");
    addApiMethodToRoleCheck("VDI.destroy");
}

void DestroyDiskAction::run()
{
    XenSession* session = this->session();
    if (!session || !session->isLoggedIn())
    {
        throw std::runtime_error("Not connected to XenServer");
    }

    // Get VDI record
    setPercentComplete(0);
    setDescription(tr("Getting VDI information..."));

    QVariantMap vdiRecord = XenAPI::VDI::get_record(session, m_vdiRef);
    QString vdiName = vdiRecord.value("name_label").toString();

    // Get list of VBDs attached to this VDI
    QVariant vbdsVar = vdiRecord.value("VBDs");
    QStringList vbdRefs;

    if (vbdsVar.canConvert<QStringList>())
    {
        vbdRefs = vbdsVar.toStringList();
    } else if (vbdsVar.canConvert<QVariantList>())
    {
        for (const QVariant& vbd : vbdsVar.toList())
        {
            vbdRefs.append(vbd.toString());
        }
    }

    // Detach from all VMs
    if (!vbdRefs.isEmpty())
    {
        setPercentComplete(10);
        setDescription(tr("Detaching disk from VMs..."));

        int vbdIndex = 0;
        int totalVBDs = vbdRefs.size();

        for (const QString& vbdRef : vbdRefs)
        {
            // Get VBD record
            QVariantMap vbdRecord = XenAPI::VBD::get_record(session, vbdRef);
            QString vmRef = vbdRecord.value("VM").toString();

            if (vmRef.isEmpty() || vmRef == "OpaqueRef:NULL")
            {
                continue;
            }

            // Get VM record
            QVariantMap vmRecord = XenAPI::VM::get_record(session, vmRef);
            QString vmName = vmRecord.value("name_label").toString();
            bool currentlyAttached = vbdRecord.value("currently_attached").toBool();

            // Check if VBD is attached to running VM
            if (currentlyAttached && !m_allowRunningVMDelete)
            {
                throw std::runtime_error(
                    QString("Cannot delete VDI - it is active on VM '%1'")
                        .arg(vmName)
                        .toStdString());
            }

            // Detach the VBD directly (we're already in an async context, so don't nest actions)
            int startPercent = 10 + (vbdIndex * 70 / totalVBDs);

            setPercentComplete(startPercent);
            setDescription(tr("Detaching from VM '%1'...").arg(vmName));

            // Unplug if currently attached
            if (currentlyAttached)
            {
                QVariantList allowedOps = XenAPI::VBD::get_allowed_operations(session, vbdRef);
                bool canUnplug = false;

                for (const QVariant& op : allowedOps)
                {
                    if (op.toString() == "unplug")
                    {
                        canUnplug = true;
                        break;
                    }
                }

                if (canUnplug)
                {
                    // Hot-unplug
                    QString taskRef = XenAPI::VBD::async_unplug(session, vbdRef);
                    pollToCompletion(taskRef, startPercent, startPercent + 30);
                } else
                {
                    throw std::runtime_error(
                        QString("Cannot unplug VBD from running VM '%1'")
                            .arg(vmName)
                            .toStdString());
                }
            }

            // Destroy the VBD
            XenAPI::VBD::destroy(session, vbdRef);

            vbdIndex++;
        }
    }

    // Destroy the VDI
    setPercentComplete(80);
    setDescription(tr("Destroying VDI..."));

    QString taskRef = XenAPI::VDI::async_destroy(session, m_vdiRef);
    pollToCompletion(taskRef, 80, 100);

    setPercentComplete(100);
    setDescription(tr("Virtual disk deleted"));
}
