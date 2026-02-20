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
#include "../../vbd.h"
#include "../../vdi.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../xenapi/xenapi_VDI.h"
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

    // RBAC dependencies (matches C# VMDestroyAction)
    this->AddApiMethodToRoleCheck("VM.destroy");
    this->AddApiMethodToRoleCheck("VDI.destroy");

    XenCache* cache = this->GetConnection() ? this->GetConnection()->GetCache() : nullptr;
    if (cache)
    {
        for (const QString& snapshotRef : this->m_snapshotsToDelete)
        {
            QSharedPointer<VM> snapshot = cache->ResolveObject<VM>(snapshotRef);
            if (snapshot && snapshot->GetPowerState() == "Suspended")
            {
                this->AddApiMethodToRoleCheck("VM.hard_shutdown");
                break;
            }
        }
    }
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

    // RBAC dependencies (matches C# VMDestroyAction)
    this->AddApiMethodToRoleCheck("VM.destroy");
    this->AddApiMethodToRoleCheck("VDI.destroy");

    // If deleteAllOwnerDisks is true, find all VBDs marked as owner
    if (deleteAllOwnerDisks)
    {
        const QList<QSharedPointer<VBD>> vbds = m_vm->GetVBDs();
        for (const QSharedPointer<VBD>& vbd : vbds)
        {
            // Check if this VBD owns its VDI
            if (vbd && vbd->IsOwner())
                m_vbdsToDelete.append(vbd->OpaqueRef());
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
            QSharedPointer<VM> snapshot = GetConnection()->GetCache()->ResolveObject<VM>(snapshotRef);
            QString powerState = snapshot ? snapshot->GetPowerState() : QString();

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
        QSharedPointer<VBD> vbd = GetConnection()->GetCache()->ResolveObject<VBD>(vbdRef);
        QString vdiRef = vbd ? vbd->GetVDIRef() : QString();

        if (!vdiRef.isEmpty() && vdiRef != XENOBJECT_NULL)
        {
            vdiRefsToDelete.append(vdiRef);
        }
    }

    // Add suspend VDI if present
    QSharedPointer<VM> vm = GetConnection()->GetCache()->ResolveObject<VM>(vmRef);
    QString suspendVdiRef = vm ? vm->GetSuspendVDIRef() : QString();
    if (!suspendVdiRef.isEmpty() && suspendVdiRef != XENOBJECT_NULL)
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
