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

#include "vgpu.h"
#include "network/connection.h"
#include "../xencache.h"
#include "vm.h"
#include "pci.h"
#include "gpugroup.h"

VGPU::VGPU(XenConnection* connection, const QString& opaqueRef, QObject* parent) : XenObject(connection, opaqueRef, parent)
{
}


QString VGPU::GetVMRef() const
{
    return this->stringProperty("VM");
}

QString VGPU::GetGPUGroupRef() const
{
    return this->stringProperty("GPU_group");
}

QString VGPU::GetDevice() const
{
    return this->stringProperty("device");
}

bool VGPU::CurrentlyAttached() const
{
    return this->boolProperty("currently_attached");
}

// GPU configuration
QString VGPU::TypeRef() const
{
    return this->stringProperty("type");
}

QString VGPU::ResidentOnRef() const
{
    return this->stringProperty("resident_on");
}

QString VGPU::ScheduledToBeResidentOnRef() const
{
    return this->stringProperty("scheduled_to_be_resident_on");
}

QVariantMap VGPU::CompatibilityMetadata() const
{
    return this->property("compatibility_metadata").toMap();
}

QString VGPU::ExtraArgs() const
{
    return this->stringProperty("extra_args");
}

QString VGPU::GetPCIRef() const
{
    return this->stringProperty("PCI");
}

// Helper methods
bool VGPU::IsAttached() const
{
    return this->CurrentlyAttached();
}

bool VGPU::IsResident() const
{
    QString resident = this->ResidentOnRef();
    return !resident.isEmpty() && resident != "OpaqueRef:NULL";
}

bool VGPU::HasScheduledLocation() const
{
    QString scheduled = this->ScheduledToBeResidentOnRef();
    return !scheduled.isEmpty() && scheduled != "OpaqueRef:NULL";
}

QSharedPointer<VM> VGPU::GetVM() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QSharedPointer<VM>();

    XenCache* cache = connection->GetCache();
    if (!cache)
        return QSharedPointer<VM>();

    QString vmRef = this->GetVMRef();
    if (vmRef.isEmpty() || vmRef == "OpaqueRef:NULL")
        return QSharedPointer<VM>();

    return cache->ResolveObject<VM>(XenObjectType::VM, vmRef);
}

QSharedPointer<GPUGroup> VGPU::GetGPUGroup() const
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

    return cache->ResolveObject<GPUGroup>(ref);
}

QSharedPointer<PCI> VGPU::GetPCI() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QSharedPointer<PCI>();

    XenCache* cache = connection->GetCache();
    if (!cache)
        return QSharedPointer<PCI>();

    QString pciRef = this->GetPCIRef();
    if (pciRef.isEmpty() || pciRef == "OpaqueRef:NULL")
        return QSharedPointer<PCI>();

    return cache->ResolveObject<PCI>(pciRef);
}
