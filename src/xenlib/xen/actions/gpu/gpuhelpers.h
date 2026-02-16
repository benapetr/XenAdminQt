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

#ifndef GPUHELPERS_H
#define GPUHELPERS_H

#include "../../network/connection.h"
#include "../../xenobject.h"
#include "../../host.h"
#include "../../pool.h"
#include "../../gpugroup.h"
#include "../../../xencache.h"

namespace GpuHelpers
{
    inline bool FeatureForbidden(XenConnection* connection, bool (Host::*restrictionTest)() const)
    {
        if (!connection || !connection->GetCache() || !restrictionTest)
            return false;

        const QList<QSharedPointer<Host>> hosts = connection->GetCache()->GetAll<Host>(XenObjectType::Host);
        for (const QSharedPointer<Host>& host : hosts)
        {
            if (host && host->IsValid() && (host.data()->*restrictionTest)())
                return true;
        }

        return false;
    }

    inline bool FeatureForbidden(const XenObject* object, bool (Host::*restrictionTest)() const)
    {
        return object ? FeatureForbidden(object->GetConnection(), restrictionTest) : false;
    }

    inline bool GpuCapability(XenConnection* connection)
    {
        if (!connection || !connection->GetCache())
            return false;

        if (FeatureForbidden(connection, &Host::RestrictGpu))
            return false;

        QSharedPointer<Pool> pool = connection->GetCache()->GetPoolOfOne();
        return pool && pool->IsValid() && pool->HasGpu();
    }

    inline bool VGpuCapability(XenConnection* connection)
    {
        if (!connection || !connection->GetCache())
            return false;

        if (FeatureForbidden(connection, &Host::RestrictVgpu))
            return false;

        QSharedPointer<Pool> pool = connection->GetCache()->GetPoolOfOne();
        return pool && pool->IsValid() && pool->HasVGpu();
    }

    inline bool GpusAvailable(XenConnection* connection)
    {
        if (!connection || !connection->GetCache())
            return false;

        const QList<QSharedPointer<GPUGroup>> groups = connection->GetCache()->GetAll<GPUGroup>(XenObjectType::GPUGroup);
        for (const QSharedPointer<GPUGroup>& group : groups)
        {
            if (!group || !group->IsValid())
                continue;

            if (!group->GetPGPURefs().isEmpty() && !group->SupportedVGPUTypeRefs().isEmpty())
                return true;
        }

        return false;
    }
}

#endif // GPUHELPERS_H
