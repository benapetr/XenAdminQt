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
#include "../../xenapi/xenapi_VBD.h"
#include "../../failure.h"
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
        // The disks to delete were picked from the cache by the caller, as VBDs
        QStringList vdiRefs;
        XenCache* cache = this->GetConnection() ? this->GetConnection()->GetCache() : nullptr;
        for (const QString& vbdRef : this->m_vbdsToDelete)
        {
            QSharedPointer<VBD> vbd = cache ? cache->ResolveObject<VBD>(vbdRef) : QSharedPointer<VBD>();
            const QString vdiRef = vbd ? vbd->GetVDIRef() : QString();
            if (!vdiRef.isEmpty() && vdiRef != XENOBJECT_NULL)
                vdiRefs.append(vdiRef);
        }

        destroyVM(this->GetSession(), this->m_vm->OpaqueRef(), vdiRefs, this->m_snapshotsToDelete);
        this->SetDescription(tr("VM destroyed"));
    } catch (const std::exception& e)
    {
        this->setError(QString("Failed to destroy VM: %1").arg(e.what()));
    }
}

void VMDestroyAction::DestroyVM(XenAPI::Session* session, const QString& vmRef, bool deleteAllOwnerDisks)
{
    destroyVM(session, vmRef, deleteAllOwnerDisks ? ownerVdiRefs(session, vmRef) : QStringList(), QStringList());
}

QStringList VMDestroyAction::ownerVdiRefs(XenAPI::Session* session, const QString& vmRef)
{
    QStringList vdiRefs;
    const QVariantList vbdRefs = XenAPI::VM::get_record(session, vmRef).value("VBDs").toList();
    for (const QVariant& vbdRefVar : vbdRefs)
    {
        const QVariantMap vbdRecord = XenAPI::VBD::get_record(session, vbdRefVar.toString());
        // C# VBD.GetIsOwner
        if (!vbdRecord.value("other_config").toMap().contains("owner"))
            continue;
        const QString vdiRef = vbdRecord.value("VDI").toString();
        if (!vdiRef.isEmpty() && vdiRef != XENOBJECT_NULL)
            vdiRefs.append(vdiRef);
    }
    return vdiRefs;
}

void VMDestroyAction::destroyVM(XenAPI::Session* session,
                                const QString& vmRef,
                                const QStringList& vdiRefsToDelete,
                                const QStringList& snapshotRefsToDelete)
{
    QStringList errors;

    for (const QString& snapshotRef : snapshotRefsToDelete)
    {
        try
        {
            const QVariantMap snapshotRecord = XenAPI::VM::get_record(session, snapshotRef);
            if (snapshotRecord.value("power_state").toString() == "Suspended")
                XenAPI::VM::hard_shutdown(session, snapshotRef);

            // C# deletes the snapshot together with the disks it owns
            DestroyVM(session, snapshotRef, true);
        } catch (const std::exception& e)
        {
            qWarning() << "Failed to delete snapshot" << snapshotRef << ":" << e.what();
            errors.append(QString("Failed to delete snapshot: %1").arg(e.what()));
        }
    }

    QStringList vdiRefs = vdiRefsToDelete;

    // CA-91072: delete the suspend image VDI too
    const QString suspendVdiRef = XenAPI::VM::get_record(session, vmRef).value("suspend_VDI").toString();
    if (!suspendVdiRef.isEmpty() && suspendVdiRef != XENOBJECT_NULL && !vdiRefs.contains(suspendVdiRef))
        vdiRefs.append(suspendVdiRef);

    XenAPI::VM::destroy(session, vmRef);

    for (const QString& vdiRef : vdiRefs)
    {
        try
        {
            XenAPI::VDI::destroy(session, vdiRef);
        } catch (const std::exception& e)
        {
            // CA-115249: XenAPI could have already deleted the VDI
            const Failure* failure = dynamic_cast<const Failure*>(&e);
            const bool handleInvalid = failure
                ? (!failure->errorDescription().isEmpty() && failure->errorDescription().first() == Failure::HANDLE_INVALID)
                : QString::fromUtf8(e.what()).contains(Failure::HANDLE_INVALID);
            if (handleInvalid)
            {
                qDebug() << "VDI" << vdiRef << "has already been deleted; ignoring";
            } else
            {
                qWarning() << "Failed to delete VDI" << vdiRef << ":" << e.what();
                errors.append(QString("Failed to delete VDI: %1").arg(e.what()));
            }
        }
    }

    if (!errors.isEmpty())
        throw std::runtime_error(errors.first().toStdString());
}
