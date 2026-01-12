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

#include "vmmoveaction.h"
#include "../../vm.h"
#include "../../sr.h"
#include "../../host.h"
#include "../../vbd.h"
#include "../../vdi.h"
#include "../../network/connection.h"
#include "../../../xencache.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../xenapi/xenapi_VDI.h"
#include "../../xenapi/xenapi_VBD.h"
#include <QDebug>

VMMoveAction::VMMoveAction(QSharedPointer<VM> vm,
                           const QMap<QString, QSharedPointer<SR>>& storageMapping,
                           QSharedPointer<Host> host,
                           QObject* parent)
    : AsyncOperation(QString("Moving VM '%1'").arg(vm ? vm->GetName() : ""),
                     QString(""),
                     parent)
    , m_vm(vm)
    , m_host(host)
    , m_sr(nullptr)
    , m_storageMapping(storageMapping)
{
    if (!this->m_vm)
        throw std::invalid_argument("VM cannot be null");

    this->m_connection = vm->GetConnection();

    if (!storageMapping.isEmpty())
        this->m_sr = storageMapping.first();

    // Set context objects
    SetVM(this->m_vm);
    if (this->m_host)
        SetHost(this->m_host);
    if (this->m_sr)
        SetSR(this->m_sr);
}

VMMoveAction::VMMoveAction(QSharedPointer<VM> vm,
                           QSharedPointer<SR> sr,
                           QSharedPointer<Host> host,
                           QObject* parent)
    : VMMoveAction(vm, getStorageMapping(vm, sr), host, parent)
{
}

QMap<QString, QSharedPointer<SR>> VMMoveAction::getStorageMapping(QSharedPointer<VM> vm, QSharedPointer<SR> sr)
{
    QMap<QString, QSharedPointer<SR>> storageMapping;
    
    if (!vm || !sr)
        return storageMapping;

    XenConnection* connection = vm->GetConnection();
    if (!connection)
        return storageMapping;

    XenCache* cache = connection->GetCache();
    if (!cache)
        return storageMapping;

    // Get all VBDs for this VM
    QStringList vbdRefs = vm->GetVBDRefs();
    for (const QString& vbdRef : vbdRefs)
    {
        QSharedPointer<VBD> vbd = cache->ResolveObject<VBD>("vbd", vbdRef);
        if (!vbd || !vbd->IsValid())
            continue;

        QString vdiRef = vbd->GetVDIRef();
        if (!vdiRef.isEmpty())
            storageMapping[vdiRef] = sr;
    }

    return storageMapping;
}

void VMMoveAction::run()
{
    if (!this->m_vm || !this->m_connection)
    {
        setError("Invalid VM or connection");
        return;
    }

    XenCache* cache = this->m_connection->GetCache();
    if (!cache)
    {
        setError("No cache available");
        return;
    }

    // Move the progress bar above 0, it's more reassuring to see than a blank bar
    this->SetPercentComplete(this->GetPercentComplete() + 10);

    QStringList vbdRefs = this->m_vm->GetVBDRefs();
    if (vbdRefs.isEmpty())
    {
        this->SetPercentComplete(100);
        this->SetDescription("No disks to move");
        return;
    }

    int halfstep = 90 / (vbdRefs.count() * 2);
    QStringList failedVDIDestroys;

    for (const QString& vbdRef : vbdRefs)
    {
        if (this->IsCancelled())
        {
            this->setError("Operation cancelled");
            return;
        }

        QSharedPointer<VBD> oldVBD = cache->ResolveObject<VBD>("vbd", vbdRef);
        if (!oldVBD || !oldVBD->IsValid())
            continue;

        // Only move VBDs that are owned by this VM (check other_config["owner"])
        QVariantMap otherConfig = oldVBD->OtherConfig();
        bool isOwner = otherConfig.contains("owner");
        if (!isOwner)
            continue;

        QString vdiRef = oldVBD->GetVDIRef();
        if (vdiRef.isEmpty())
            continue;

        // Check if we have a target SR for this VDI
        QSharedPointer<SR> targetSR = this->m_storageMapping.value(vdiRef, nullptr);
        if (!targetSR)
            continue;

        QSharedPointer<VDI> curVdi = cache->ResolveObject<VDI>("vdi", vdiRef);
        if (!curVdi || !curVdi->IsValid())
            continue;

        // Skip if VDI is already on the target SR
        QString currentSRRef = curVdi->SRRef();
        if (currentSRRef == targetSR->OpaqueRef())
            continue;

        // Get SR names for user feedback
        QSharedPointer<SR> currentSR = cache->ResolveObject<SR>("sr", currentSRRef);
        QString currentSRName = currentSR && currentSR->IsValid() ? currentSR->GetName() : "Unknown";
        QString targetSRName = targetSR->GetName();
        QString vdiName = curVdi->GetName();

        this->SetDescription(QString("Moving VDI '%1' from '%2' to '%3'")
                         .arg(vdiName)
                         .arg(currentSRName)
                         .arg(targetSRName));

        // Copy VDI to new SR
        QString taskRef = XenAPI::VDI::async_copy(GetSession(), vdiRef, targetSR->OpaqueRef());
        
        if (taskRef.isEmpty())
        {
            setError(QString("Failed to copy VDI '%1'").arg(vdiName));
            return;
        }

        this->pollToCompletion(taskRef, this->GetPercentComplete(), this->GetPercentComplete() + halfstep);

        // Get the new VDI ref from the task GetResult
        QString newVDIRef = GetResult();
        if (newVDIRef.isEmpty())
        {
            setError("Failed to get new VDI reference");
            return;
        }

        // Get the new VDI from cache (it should be updated after task completes)
        QSharedPointer<VDI> newVDI = cache->ResolveObject<VDI>("vdi", newVDIRef);
        if (!newVDI || !newVDI->IsValid())
        {
            this->setError("Failed to retrieve new VDI from cache");
            return;
        }

        // Create a new VBD for the new VDI
        QVariantMap newVBDRecord;
        newVBDRecord["userdevice"] = oldVBD->GetUserdevice();
        newVBDRecord["bootable"] = oldVBD->IsBootable();
        newVBDRecord["mode"] = oldVBD->GetMode();
        newVBDRecord["type"] = oldVBD->GetType();
        newVBDRecord["unpluggable"] = oldVBD->Unpluggable();
        newVBDRecord["other_config"] = oldVBD->OtherConfig();
        newVBDRecord["VDI"] = newVDIRef;
        newVBDRecord["VM"] = this->m_vm->OpaqueRef();

        // Create the new VBD
        QString newVBDRef = XenAPI::VBD::create(GetSession(), newVBDRecord);
        if (newVBDRef.isEmpty())
        {
            setError("Failed to create new VBD");
            return;
        }

        // New VBD created (cache will update automatically)

        // Try to destroy the old VDI
        try
        {
            XenAPI::VDI::destroy(GetSession(), vdiRef);
        }
        catch (...)
        {
            qWarning() << "Failed to destroy old VDI" << vdiRef;
            failedVDIDestroys.append(vdiRef);
        }

        this->SetPercentComplete(this->GetPercentComplete() + halfstep);
    }

    this->SetDescription(QString());

    // Set suspend SR if specified
    if (this->m_sr)
    {
        try
        {
            XenAPI::VM::set_suspend_SR(GetSession(), this->m_vm->OpaqueRef(), this->m_sr->OpaqueRef());
        }
        catch (...)
        {
            qWarning() << "Failed to set suspend SR for VM" << this->m_vm->GetName();
        }
    }

    if (!failedVDIDestroys.isEmpty())
    {
        this->setError(QString("Failed to destroy old VDIs for VM '%1'").arg(this->m_vm->GetName()));
        return;
    }

    this->SetPercentComplete(100);
    this->SetDescription("Moved");
}
