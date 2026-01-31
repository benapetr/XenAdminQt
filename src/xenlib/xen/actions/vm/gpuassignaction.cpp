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

#include "gpuassignaction.h"
#include "../../../xen/network/connection.h"
#include "../../xenapi/xenapi_VGPU.h"
#include "../../../xencache.h"
#include "../../vm.h"
#include <stdexcept>
#include <QSet>

GpuAssignAction::GpuAssignAction(QSharedPointer<VM> vm,
                                 const QVariantList& vgpuData,
                                 QObject* parent)
    : AsyncOperation(QString("Set GPU"),
                     QString("Configuring GPU assignments for '%1'").arg(vm ? vm->GetName() : ""),
                     parent),
      m_vm(vm),
      m_vgpuData(vgpuData)
{
    if (!vm)
        throw std::invalid_argument("VM cannot be null");
    this->m_connection = vm->GetConnection();
}

void GpuAssignAction::run()
{
    try
    {
        SetPercentComplete(0);
        SetDescription("Retrieving VM configuration...");

        // Check if VM is still valid
        if (!this->m_vm || !this->m_vm->IsValid())
        {
            throw std::runtime_error("VM is no longer valid");
        }

        // Get current VGPUs for the VM
        QSet<QString> vgpusToRemove;
        const QStringList currentVGPUrefs = this->m_vm->VGPURefs();
        for (const QString& ref : currentVGPUrefs)
            vgpusToRemove.insert(ref);

        // Identify VGPUs to keep (those with existing opaque_ref)
        QSet<QString> vgpusToKeep;
        for (const QVariant& vgpuVar : m_vgpuData)
        {
            QVariantMap vgpuMap = vgpuVar.toMap();
            QString opaqueRef = vgpuMap.value("opaque_ref").toString();
            if (!opaqueRef.isEmpty() && opaqueRef != "OpaqueRef:NULL")
            {
                vgpusToKeep.insert(opaqueRef);
            }
        }

        // Remove VGPUs that are no longer needed
        vgpusToRemove.subtract(vgpusToKeep);

        SetPercentComplete(20);

        int i = 0;
        int totalToRemove = vgpusToRemove.size();

        for (const QString& vgpuRef : vgpusToRemove)
        {
            SetDescription(QString("Removing VGPU %1 of %2...").arg(i + 1).arg(totalToRemove));
            XenAPI::VGPU::destroy(GetSession(), vgpuRef);
            i++;
            SetPercentComplete(20 + (30 * i / qMax(1, totalToRemove)));
        }

        SetPercentComplete(50);

        // Add new VGPUs (those without opaque_ref)
        QList<QVariantMap> vgpusToAdd;
        for (const QVariant& vgpuVar : m_vgpuData)
        {
            QVariantMap vgpuMap = vgpuVar.toMap();
            QString opaqueRef = vgpuMap.value("opaque_ref").toString();
            if (opaqueRef.isEmpty() || opaqueRef == "OpaqueRef:NULL")
            {
                vgpusToAdd.append(vgpuMap);
            }
        }

        int totalToAdd = vgpusToAdd.size();
        i = 0;

        for (const QVariantMap& vgpuMap : vgpusToAdd)
        {
            SetDescription(QString("Adding VGPU %1 of %2...").arg(i + 1).arg(totalToAdd));

            QString gpuGroupRef = vgpuMap.value("GPU_group").toString();
            QString vgpuTypeRef = vgpuMap.value("type").toString();
            QString device = vgpuMap.value("device", "0").toString();

            addGpu(gpuGroupRef, vgpuTypeRef, device);

            i++;
            SetPercentComplete(50 + (50 * i / qMax(1, totalToAdd)));
        }

        SetPercentComplete(100);
        SetDescription("GPU configuration completed successfully");

    } catch (const std::exception& e)
    {
        setError(QString("Failed to configure GPU: %1").arg(e.what()));
    }
}

void GpuAssignAction::addGpu(const QString& gpuGroupRef, const QString& vgpuTypeRef, const QString& device)
{
    if (gpuGroupRef.isEmpty() || gpuGroupRef == "OpaqueRef:NULL")
    {
        return; // No GPU group specified
    }

    QVariantMap otherConfig; // Empty for now

    QString taskRef;
    if (vgpuTypeRef.isEmpty() || vgpuTypeRef == "OpaqueRef:NULL")
    {
        // Create without type (basic VGPU)
        taskRef = XenAPI::VGPU::async_create(GetSession(), this->m_vm->OpaqueRef(), gpuGroupRef, device, otherConfig);
    } else
    {
        // Create with specific VGPU type
        taskRef = XenAPI::VGPU::async_create(GetSession(), this->m_vm->OpaqueRef(), gpuGroupRef, device, otherConfig, vgpuTypeRef);
    }

    // Poll the task to completion
    pollToCompletion(taskRef, GetPercentComplete(), GetPercentComplete());
}
