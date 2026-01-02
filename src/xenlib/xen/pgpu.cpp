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

PGPU::PGPU(XenConnection* connection, const QString& opaqueRef, QObject* parent) : XenObject(connection, opaqueRef, parent)
{
}

QString PGPU::GetObjectType() const
{
    return "pgpu";
}

QString PGPU::Uuid() const
{
    return this->stringProperty("uuid");
}

QString PGPU::PCIRef() const
{
    return this->stringProperty("PCI");
}

QString PGPU::GPUGroupRef() const
{
    return this->stringProperty("GPU_group");
}

QString PGPU::HostRef() const
{
    return this->stringProperty("host");
}

QVariantMap PGPU::OtherConfig() const
{
    return this->property("other_config").toMap();
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
