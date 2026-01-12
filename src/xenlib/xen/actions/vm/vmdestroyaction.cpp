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

#include "vmdestroyaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../xenapi/xenapi_VDI.h"
#include "../../xenapi/xenapi_VBD.h"
#include "../../../xencache.h"
#include <QDebug>

VMDestroyAction::VMDestroyAction(QSharedPointer<VM> vm,
                                 const QStringList& vbdsToDelete,
                                 const QStringList& snapshotsToDelete,
                                 QObject* parent)
    : AsyncOperation("Destroying VM",
                     QString("Destroying '%1'").arg(vm ? vm->GetName() : ""),
                     parent),
      m_vm(vm),
      m_vbdsToDelete(vbdsToDelete),
      m_snapshotsToDelete(snapshotsToDelete)
{
    if (!vm)
        throw std::invalid_argument("VM cannot be null");
    this->m_connection = vm->GetConnection();
}

VMDestroyAction::VMDestroyAction(QSharedPointer<VM> vm,
                                 bool deleteAllOwnerDisks,
                                 QObject* parent)
    : AsyncOperation("Destroying VM",
                     QString("Destroying '%1'").arg(vm ? vm->GetName() : ""),
                     parent),
      m_vm(vm)
{
    if (!m_vm)
        throw std::invalid_argument("VM cannot be null");

    this->m_connection = vm->GetConnection();

    // If deleteAllOwnerDisks is true, find all VBDs marked as owner
    if (deleteAllOwnerDisks)
    {
        QStringList vbdRefs = m_vm->GetVBDRefs();
        for (const QString& vbdRef : vbdRefs)
        {
            QVariantMap vbdData = this->m_connection->GetCache()->ResolveObjectData("vbd", vbdRef);

            // Check if this VBD owns its VDI
            bool isOwner = vbdData.value("owner", false).toBool();
            if (isOwner)
            {
                m_vbdsToDelete.append(vbdRef);
            }
        }
    }
}

void VMDestroyAction::run()
{
    try
    {
        destroyVM(m_vm->OpaqueRef(), m_vbdsToDelete, m_snapshotsToDelete);
        SetDescription("VM destroyed");

    } catch (const std::exception& e)
    {
        setError(QString("Failed to destroy VM: %1").arg(e.what()));
    }
}

void VMDestroyAction::destroyVM(const QString& vmRef,
                                const QStringList& vbdRefsToDelete,
                                const QStringList& snapshotRefsToDelete)
{
    QStringList errors;

    // Step 1: Destroy snapshots
    for (const QString& snapshotRef : snapshotRefsToDelete)
    {
        try
        {
            QVariantMap snapshotData = GetConnection()->GetCache()->ResolveObjectData("vm", snapshotRef);
            QString powerState = snapshotData.value("power_state").toString();

            // If snapshot is suspended, hard shutdown first
            if (powerState == "Suspended")
            {
                XenAPI::VM::hard_shutdown(GetSession(), snapshotRef);
            }

            // Recursively destroy snapshot (with all its disks)
            destroyVM(snapshotRef, QStringList(), QStringList());

        } catch (const std::exception& e)
        {
            qWarning() << "Failed to delete snapshot" << snapshotRef << ":" << e.what();
            errors.append(QString("Failed to delete snapshot: %1").arg(e.what()));
        }
    }

    // Step 2: Collect VDIs to delete
    QStringList vdiRefsToDelete;

    // Add VDIs from specified VBDs
    for (const QString& vbdRef : vbdRefsToDelete)
    {
        QVariantMap vbdData = GetConnection()->GetCache()->ResolveObjectData("vbd", vbdRef);
        QString vdiRef = vbdData.value("VDI").toString();

        if (!vdiRef.isEmpty() && vdiRef != "OpaqueRef:NULL")
        {
            vdiRefsToDelete.append(vdiRef);
        }
    }

    // Add suspend VDI if present
    QVariantMap vmData = GetConnection()->GetCache()->ResolveObjectData("vm", vmRef);
    QString suspendVdiRef = vmData.value("suspend_VDI").toString();
    if (!suspendVdiRef.isEmpty() && suspendVdiRef != "OpaqueRef:NULL")
    {
        vdiRefsToDelete.append(suspendVdiRef);
    }

    // Step 3: Destroy the VM itself
    XenAPI::VM::destroy(GetSession(), vmRef);

    // Step 4: Destroy VDIs
    for (const QString& vdiRef : vdiRefsToDelete)
    {
        try
        {
            XenAPI::VDI::destroy(GetSession(), vdiRef);

        } catch (const std::exception& e)
        {
            QString error = e.what();

            // CA-115249: XenAPI could have already deleted the VDI
            // Ignore HANDLE_INVALID errors
            if (error.contains("HANDLE_INVALID"))
            {
                qDebug() << "VDI" << vdiRef << "has already been deleted; ignoring";
            } else
            {
                qWarning() << "Failed to delete VDI" << vdiRef << ":" << error;
                errors.append(QString("Failed to delete VDI: %1").arg(error));
            }
        }
    }

    // If there were any errors, throw the first one
    if (!errors.isEmpty())
    {
        throw std::runtime_error(errors.first().toStdString());
    }
}
