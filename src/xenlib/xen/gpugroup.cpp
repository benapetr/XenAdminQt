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

#include "gpugroup.h"
#include "network/connection.h"
#include "../xencache.h"
#include "pgpu.h"
#include "vgpu.h"
#include "vgputype.h"
#include <QMap>

GPUGroup::GPUGroup(XenConnection* connection, const QString& opaqueRef, QObject* parent) : XenObject(connection, opaqueRef, parent)
{
}

GPUGroup::~GPUGroup()
{
}

QStringList GPUGroup::GetPGPURefs() const
{
    return this->property("PGPUs").toStringList();
}

QStringList GPUGroup::GetVGPURefs() const
{
    return this->property("VGPUs").toStringList();
}

QStringList GPUGroup::GPUTypes() const
{
    return this->property("GPU_types").toStringList();
}

QString GPUGroup::AllocationAlgorithm() const
{
    return this->stringProperty("allocation_algorithm");
}

QStringList GPUGroup::SupportedVGPUTypeRefs() const
{
    return this->property("supported_VGPU_types").toStringList();
}

QStringList GPUGroup::EnabledVGPUTypeRefs() const
{
    return this->property("enabled_VGPU_types").toStringList();
}

QString GPUGroup::Name() const
{
    QString name = this->GetName();
    if (name.startsWith(QStringLiteral("Group of ")))
        name = name.mid(9);
    return name;
}

bool GPUGroup::HasVGpu() const
{
    const QList<QSharedPointer<PGPU>> pgpus = this->GetPGPUs();
    for (const QSharedPointer<PGPU>& pgpu : pgpus)
    {
        if (pgpu && pgpu->HasVGpu())
            return true;
    }
    return false;
}

bool GPUGroup::HasPassthrough() const
{
    XenConnection* connection = this->GetConnection();
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    if (!cache)
        return false;

    const QStringList refs = this->SupportedVGPUTypeRefs();
    for (const QString& ref : refs)
    {
        QSharedPointer<VGPUType> type = cache->ResolveObject<VGPUType>(ref);
        if (type && type->IsValid() && type->IsPassthrough())
            return true;
    }
    return false;
}

QList<QSharedPointer<PGPU>> GPUGroup::GetPGPUs() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QList<QSharedPointer<PGPU>>();
    
    XenCache* cache = connection->GetCache();
    if (!cache)
        return QList<QSharedPointer<PGPU>>();
    
    QStringList refs = this->GetPGPURefs();
    QList<QSharedPointer<PGPU>> result;
    for (const QString& ref : refs)
    {
        if (ref.isEmpty() || ref == XENOBJECT_NULL)
            continue;
        QSharedPointer<PGPU> obj = cache->ResolveObject<PGPU>(ref);
        if (obj)
            result.append(obj);
    }
    return result;
}

QList<QSharedPointer<VGPU>> GPUGroup::GetVGPUs() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QList<QSharedPointer<VGPU>>();
    
    XenCache* cache = connection->GetCache();
    if (!cache)
        return QList<QSharedPointer<VGPU>>();
    
    QStringList refs = this->GetVGPURefs();
    QList<QSharedPointer<VGPU>> result;
    for (const QString& ref : refs)
    {
        if (ref.isEmpty() || ref == XENOBJECT_NULL)
            continue;
        QSharedPointer<VGPU> obj = cache->ResolveObject<VGPU>(ref);
        if (obj)
            result.append(obj);
    }
    return result;
}
