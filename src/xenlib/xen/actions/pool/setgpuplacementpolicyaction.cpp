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

#include "setgpuplacementpolicyaction.h"
#include "../../pool.h"
#include "../../gpugroup.h"
#include "../../../xencache.h"
#include <stdexcept>

SetGpuPlacementPolicyAction::SetGpuPlacementPolicyAction(QSharedPointer<Pool> pool,
                                                         XenAPI::AllocationAlgorithm allocationAlgorithm,
                                                         QObject* parent)
    : AsyncOperation(pool ? pool->GetConnection() : nullptr,
                     QStringLiteral("Set GPU placement policy"),
                     QStringLiteral("Updating GPU placement policy..."),
                     parent),
      m_pool(pool),
      m_allocationAlgorithm(allocationAlgorithm)
{
    if (!this->m_pool || !this->m_pool->IsValid())
        throw std::invalid_argument("Invalid pool object");

    this->SetPool(pool);
    this->AddApiMethodToRoleCheck("GPU_group.set_allocation_algorithm");
}

void SetGpuPlacementPolicyAction::run()
{
    if (this->m_allocationAlgorithm == XenAPI::AllocationAlgorithm::Unknown)
    {
        this->setError(QStringLiteral("Unknown GPU placement policy"));
        return;
    }

    XenCache* cache = this->GetConnection() ? this->GetConnection()->GetCache() : nullptr;
    if (!cache)
    {
        this->setError(QStringLiteral("GPU cache is not available"));
        return;
    }

    const QList<QSharedPointer<GPUGroup>> gpuGroups = cache->GetAll<GPUGroup>(XenObjectType::GPUGroup);
    if (gpuGroups.isEmpty())
    {
        this->SetPercentComplete(100);
        this->SetDescription(QStringLiteral("No GPU groups found"));
        return;
    }

    int processed = 0;
    for (const QSharedPointer<GPUGroup>& group : gpuGroups)
    {
        if (!group || !group->IsValid())
            continue;

        XenAPI::GPU_group::set_allocation_algorithm(this->GetSession(), group->OpaqueRef(), this->m_allocationAlgorithm);

        ++processed;
        this->SetPercentComplete((processed * 100) / gpuGroups.size());
    }

    this->SetPercentComplete(100);
    this->SetDescription(QStringLiteral("GPU placement policy updated"));
}
