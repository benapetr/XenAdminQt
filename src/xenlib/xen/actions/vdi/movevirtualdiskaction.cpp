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

#include "movevirtualdiskaction.h"
#include "xen/network/connection.h"
#include "xen/xenapi/xenapi_VDI.h"
#include "xen/xenapi/xenapi_VM.h"
#include "xen/xenapi/xenapi_VBD.h"
#include "xen/xenapi/xenapi_SR.h"
#include <QDebug>

using namespace XenAPI;

MoveVirtualDiskAction::MoveVirtualDiskAction(XenConnection* connection,
                                             const QString& vdiRef,
                                             const QString& srRef,
                                             QObject* parent)
    : AsyncOperation(connection, QString("Moving Virtual Disk"), QString(), parent), m_vdiRef(vdiRef), m_srRef(srRef)
{
    QString vdiName = this->getVDIName();
    QString oldSR = this->getSRName(QString()); // Get current SR from VDI
    QString newSR = this->getSRName(srRef);

    this->SetTitle(QString("Moving Virtual Disk"));
    this->SetDescription(QString("Moving '%1' from '%2' to '%3'...")
                       .arg(vdiName)
                       .arg(oldSR)
                       .arg(newSR));

    // RBAC dependencies (matches C# MoveVirtualDiskAction)
    this->AddApiMethodToRoleCheck("vdi.destroy");
    this->AddApiMethodToRoleCheck("vdi.copy");

    XenAPI::Session* session = this->GetConnection() ? this->GetConnection()->GetSession() : nullptr;
    if (session)
    {
        try
        {
            QVariantMap vdiRecord = XenAPI::VDI::get_record(session, this->m_vdiRef);
            if (vdiRecord.value("type").toString() == "suspend")
                this->AddApiMethodToRoleCheck("vm.set_suspend_VDI");
        } catch (...)
        {
            // Best-effort RBAC enrichment.
        }
    }
}

void MoveVirtualDiskAction::run()
{
    try
    {
        Session* session = this->GetConnection()->GetSession();
        if (!session)
        {
            throw std::runtime_error("No valid session");
        }

        // Get VDI information
        QVariantMap vdiRecord = XenAPI::VDI::get_record(session, this->m_vdiRef);
        QString vdiType = vdiRecord.value("type").toString();
        QString vdiUuid = vdiRecord.value("uuid").toString();
        QVariantList vbdRefs = vdiRecord.value("VBDs").toList();

        this->SetPercentComplete(10);
        qDebug() << "Moving VDI" << this->m_vdiRef << "to SR" << this->m_srRef;

        // Step 1: Copy VDI to target SR
        this->SetDescription(QString("Copying '%1' to new storage...").arg(this->getVDIName()));
        QString taskRef = XenAPI::VDI::async_copy(session, this->m_vdiRef, this->m_srRef);
        if (taskRef.isEmpty())
        {
            throw std::runtime_error("Failed to start VDI copy task");
        }

        // Poll copy task to completion (10% to 60%)
        this->pollToCompletion(taskRef, 10, 60);

        // Get the new VDI reference from task GetResult
        QString newVdiRef = this->GetResult();
        if (newVdiRef.isEmpty())
        {
            throw std::runtime_error("Failed to get new VDI reference from task");
        }

        qDebug() << "VDI copied successfully, new ref:" << newVdiRef;

        // Step 2: If this is a suspend VDI, update the VM's suspend_VDI reference
        if (vdiType == "suspend")
        {
            this->SetDescription(QString("Updating suspend VDI reference..."));

            // Get all VMs and find the one with this VDI as suspend_VDI
            QVariantMap allVms = XenAPI::VM::get_all_records(session);

            for (auto it = allVms.begin(); it != allVms.end(); ++it)
            {
                QString vmRef = it.key();
                QVariantMap vmData = it.value().toMap();
                QString suspendVdiRef = vmData.value("suspend_VDI").toString();

                if (!suspendVdiRef.isEmpty() && suspendVdiRef == this->m_vdiRef)
                {
                    XenAPI::VM::set_suspend_VDI(session, vmRef, newVdiRef);
                    qDebug() << "Updated suspend_VDI for VM" << vmRef;
                    break;
                }
            }
        }

        this->SetPercentComplete(60);

        // Step 3: Recreate all VBDs pointing to the new VDI
        this->SetDescription(QString("Updating disk attachments..."));
        QList<QVariantMap> newVbds;

        for (const QVariant& vbdRefVar : vbdRefs)
        {
            QString vbdRef = vbdRefVar.toString();
            QVariantMap oldVbd = XenAPI::VBD::get_record(session, vbdRef);

            if (oldVbd.isEmpty())
            {
                qWarning() << "Could not resolve VBD:" << vbdRef;
                continue;
            }

            // Create new VBD record pointing to new VDI
            QVariantMap newVbd;
            newVbd["userdevice"] = oldVbd.value("userdevice");
            newVbd["bootable"] = oldVbd.value("bootable");
            newVbd["mode"] = oldVbd.value("mode");
            newVbd["type"] = oldVbd.value("type");
            newVbd["unpluggable"] = oldVbd.value("unpluggable");
            newVbd["other_config"] = oldVbd.value("other_config");
            newVbd["VDI"] = newVdiRef;
            newVbd["VM"] = oldVbd.value("VM");

            // Check if owner flag is set
            QVariantMap otherConfig = oldVbd.value("other_config").toMap();
            if (otherConfig.value("owner", false).toBool())
            {
                QVariantMap newOtherConfig = newVbd.value("other_config").toMap();
                newOtherConfig["owner"] = true;
                newVbd["other_config"] = newOtherConfig;
            }

            newVbds.append(newVbd);

            // Unplug and destroy old VBD
            try
            {
                bool currentlyAttached = oldVbd.value("currently_attached", false).toBool();
                QVariantList allowedOps = oldVbd.value("allowed_operations").toList();
                bool canUnplug = false;

                for (const QVariant& op : allowedOps)
                {
                    if (op.toString() == "unplug")
                    {
                        canUnplug = true;
                        break;
                    }
                }

                if (currentlyAttached && canUnplug)
                {
                    XenAPI::VBD::unplug(session, vbdRef);
                }
            } catch (const std::exception& e)
            {
                qWarning() << "Failed to unplug VBD" << vbdRef << ":" << e.what();
            }

            // Always destroy the old VBD if it's not attached
            bool currentlyAttached = oldVbd.value("currently_attached", false).toBool();
            if (!currentlyAttached)
            {
                XenAPI::VBD::destroy(session, vbdRef);
            }
        }

        this->SetPercentComplete(80);

        // Step 4: Destroy original VDI
        this->SetDescription(QString("Removing old disk..."));
        XenAPI::VDI::destroy(session, this->m_vdiRef);
        qDebug() << "Original VDI destroyed:" << this->m_vdiRef;

        // Step 5: Create new VBDs
        this->SetDescription(QString("Creating new disk attachments..."));
        for (const QVariantMap& newVbd : newVbds)
        {
            QString createdVbdRef = XenAPI::VBD::create(session, newVbd);
            if (createdVbdRef.isEmpty())
            {
                qWarning() << "Failed to create VBD";
            } else
            {
                qDebug() << "Created new VBD:" << createdVbdRef;
            }
        }

        this->SetDescription(QString("'%1' moved successfully").arg(this->getVDIName()));
        this->SetPercentComplete(100);

        qDebug() << "VDI moved successfully";

    } catch (const std::exception& e)
    {
        qWarning() << "MoveVirtualDiskAction failed:" << e.what();
        throw;
    }
}

QString MoveVirtualDiskAction::getVDIName() const
{
    try
    {
        Session* session = this->GetConnection()->GetSession();
        QString name = XenAPI::VDI::get_name_label(session, this->m_vdiRef);
        return name.isEmpty() ? this->m_vdiRef : name;
    } catch (...)
    {
        return this->m_vdiRef;
    }
}

QString MoveVirtualDiskAction::getSRName(const QString& srRef) const
{
    try
    {
        Session* session = this->GetConnection()->GetSession();
        QString actualSrRef = srRef;

        // If no SR ref provided, get it from the VDI
        if (actualSrRef.isEmpty())
        {
            actualSrRef = XenAPI::VDI::get_SR(session, this->m_vdiRef);
        }

        QString name = XenAPI::SR::get_name_label(session, actualSrRef);
        return name.isEmpty() ? actualSrRef : name;
    } catch (...)
    {
        return srRef.isEmpty() ? "Unknown" : srRef;
    }
}
