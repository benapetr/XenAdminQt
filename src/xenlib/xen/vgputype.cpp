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

#include "vgputype.h"
#include "network/connection.h"
#include "../xencache.h"
#include "pgpu.h"

VGPUType::VGPUType(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

QString VGPUType::VendorName() const
{
    return this->stringProperty("vendor_name");
}

QString VGPUType::ModelName() const
{
    return this->stringProperty("model_name");
}

qint64 VGPUType::FramebufferSize() const
{
    return this->intProperty("framebuffer_size", 0);
}

qint64 VGPUType::MaxHeads() const
{
    return this->intProperty("max_heads", 0);
}

qint64 VGPUType::MaxResolutionX() const
{
    return this->intProperty("max_resolution_x", 0);
}

qint64 VGPUType::MaxResolutionY() const
{
    return this->intProperty("max_resolution_y", 0);
}

QString VGPUType::Implementation() const
{
    return this->stringProperty("implementation");
}

QString VGPUType::Identifier() const
{
    return this->stringProperty("identifier");
}

bool VGPUType::IsExperimental() const
{
    return this->boolProperty("experimental", false);
}

QStringList VGPUType::SupportedOnPGPURefs() const
{
    return this->stringListProperty("supported_on_PGPUs");
}

QStringList VGPUType::EnabledOnPGPURefs() const
{
    return this->stringListProperty("enabled_on_PGPUs");
}

QStringList VGPUType::SupportedOnGPUGroupRefs() const
{
    return this->stringListProperty("supported_on_GPU_groups");
}

QStringList VGPUType::EnabledOnGPUGroupRefs() const
{
    return this->stringListProperty("enabled_on_GPU_groups");
}

QStringList VGPUType::CompatibleTypesInVmRefs() const
{
    return this->stringListProperty("compatible_types_in_vm");
}

bool VGPUType::IsPassthrough() const
{
    return this->ModelName() == QStringLiteral("passthrough");
}

qint64 VGPUType::Capacity() const
{
    XenConnection* connection = this->GetConnection();
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    if (!cache)
        return 0;

    const QStringList pgpuRefs = this->SupportedOnPGPURefs();
    if (pgpuRefs.isEmpty())
        return 0;

    QSharedPointer<PGPU> pgpu = cache->ResolveObject<PGPU>(pgpuRefs.first());
    if (!pgpu || !pgpu->IsValid())
        return 0;

    const QVariantMap capacities = pgpu->SupportedVGPUMaxCapacities();
    if (!capacities.contains(this->OpaqueRef()))
        return 0;

    return capacities.value(this->OpaqueRef()).toLongLong();
}

QString VGPUType::DisplayName() const
{
    if (this->IsPassthrough())
        return QStringLiteral("Pass-through");

    const qint64 capacity = this->Capacity();
    if (capacity > 0)
        return QStringLiteral("%1 (%2 vGPUs/GPU)").arg(this->ModelName()).arg(capacity);

    return this->ModelName();
}

QString VGPUType::DisplayDescription() const
{
    if (this->IsPassthrough())
        return QStringLiteral("Pass-through");

    const QStringList compatibility = this->CompatibleTypesInVmRefs();
    if (compatibility.isEmpty())
        return this->DisplayName();

    return QStringLiteral("%1 - multiple vGPU support").arg(this->DisplayName());
}
