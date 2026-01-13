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

#include "pgpu.h"
#include "network/connection.h"
#include "../xencache.h"
#include "pci.h"
#include "gpugroup.h"
#include "host.h"
#include "vgpu.h"

PGPU::PGPU(XenConnection* connection, const QString& opaqueRef, QObject* parent) : XenObject(connection, opaqueRef, parent)
{
}

QString PGPU::GetObjectType() const
{
    return "pgpu";
}

QString PGPU::GetPCIRef() const
{
    return this->stringProperty("PCI");
}

QString PGPU::GetGPUGroupRef() const
{
    return this->stringProperty("GPU_group");
}

QString PGPU::GetHostRef() const
{
    return this->stringProperty("host");
}

// VGPU type support
QStringList PGPU::SupportedVGPUTypeRefs() const
{
    return this->property("supported_VGPU_types").toStringList();
}

QStringList PGPU::EnabledVGPUTypeRefs() const
{
    return this->property("enabled_VGPU_types").toStringList();
}

QStringList PGPU::ResidentVGPURefs() const
{
    return this->property("resident_VGPUs").toStringList();
}

QVariantMap PGPU::SupportedVGPUMaxCapacities() const
{
    return this->property("supported_VGPU_max_capacities").toMap();
}

// Device status
QString PGPU::Dom0Access() const
{
    return this->stringProperty("dom0_access");
}

bool PGPU::IsSystemDisplayDevice() const
{
    return this->boolProperty("is_system_display_device");
}

QVariantMap PGPU::CompatibilityMetadata() const
{
    return this->property("compatibility_metadata").toMap();
}

// Helper methods
bool PGPU::SupportsVGPUs() const
{
    return !this->SupportedVGPUTypeRefs().isEmpty();
}

bool PGPU::HasResidentVGPUs() const
{
    return !this->ResidentVGPURefs().isEmpty();
}

int PGPU::ResidentVGPUCount() const
{
    return this->ResidentVGPURefs().count();
}

bool PGPU::IsAccessibleFromDom0() const
{
    QString access = this->Dom0Access();
    return access == "enabled" || access == "enabled_on_reboot";
}

QSharedPointer<PCI> PGPU::GetPCI() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QSharedPointer<PCI>();
    
    XenCache* cache = connection->GetCache();
    if (!cache)
        return QSharedPointer<PCI>();
    
    QString ref = this->GetPCIRef();
    if (ref.isEmpty() || ref == "OpaqueRef:NULL")
        return QSharedPointer<PCI>();
    
    return cache->ResolveObject<PCI>("pci", ref);
}

QSharedPointer<GPUGroup> PGPU::GetGPUGroup() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QSharedPointer<GPUGroup>();
    
    XenCache* cache = connection->GetCache();
    if (!cache)
        return QSharedPointer<GPUGroup>();
    
    QString ref = this->GetGPUGroupRef();
    if (ref.isEmpty() || ref == "OpaqueRef:NULL")
        return QSharedPointer<GPUGroup>();
    
    return cache->ResolveObject<GPUGroup>("gpu_group", ref);
}

QSharedPointer<Host> PGPU::GetHost() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QSharedPointer<Host>();
    
    XenCache* cache = connection->GetCache();
    if (!cache)
        return QSharedPointer<Host>();
    
    QString ref = this->GetHostRef();
    if (ref.isEmpty() || ref == "OpaqueRef:NULL")
        return QSharedPointer<Host>();
    
    return cache->ResolveObject<Host>("host", ref);
}

QList<QSharedPointer<VGPU>> PGPU::GetResidentVGPUs() const
{
    QList<QSharedPointer<VGPU>> result;
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return result;
    
    XenCache* cache = connection->GetCache();
    if (!cache)
        return result;
    
    QStringList refs = this->ResidentVGPURefs();
    for (const QString& ref : refs)
    {
        if (!ref.isEmpty() && ref != "OpaqueRef:NULL")
        {
            QSharedPointer<VGPU> obj = cache->ResolveObject<VGPU>("vgpu", ref);
            if (obj)
                result.append(obj);
        }
    }
    return result;
}
