/*
 * Copyright (c) 2026, Petr Bena <petr@bena.rocks>
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

#include "clusteringactions.h"
#include "../../cluster.h"
#include "../../clusterhost.h"
#include "../../network.h"
#include "../../pool.h"
#include "../../network/connection.h"
#include "../../xenapi/xenapi_Cluster.h"
#include "../../xenapi/xenapi_PIF.h"
#include "../../../xencache.h"

namespace
{
    // C# XenAPI.Cluster's defaults, which EnableClusteringAction passes on
    const double DEFAULT_TOKEN_TIMEOUT = 20.0;
    const double DEFAULT_TOKEN_TIMEOUT_COEFFICIENT = 1.0;
}

EnableClusteringAction::EnableClusteringAction(QSharedPointer<Pool> pool, QSharedPointer<Network> network, QObject* parent)
    : AsyncOperation(pool->GetConnection(), tr("Enable clustering."), tr("Enabling clustering on %1.").arg(pool->GetName()), true, parent),
      m_networkRef(network->OpaqueRef()),
      m_pifRefs(network->GetPIFRefs()),
      m_poolName(pool->GetName())
{
    this->SetPool(pool);
    this->AddApiMethodToRoleCheck("cluster.pool_create");
    this->AddApiMethodToRoleCheck("pif.set_disallow_unplug");
}

void EnableClusteringAction::run()
{
    try
    {
        // C#: the cluster network's PIFs must stay plugged
        for (const QString& pifRef : this->m_pifRefs)
            XenAPI::PIF::set_disallow_unplug(this->GetSession(), pifRef, true);

        XenAPI::Cluster::pool_create(this->GetSession(), this->m_networkRef, "corosync",
                                     DEFAULT_TOKEN_TIMEOUT, DEFAULT_TOKEN_TIMEOUT_COEFFICIENT);
        this->SetDescription(tr("Enabled clustering on %1.").arg(this->m_poolName));
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}

DisableClusteringAction::DisableClusteringAction(QSharedPointer<Pool> pool, QObject* parent)
    : AsyncOperation(pool->GetConnection(), tr("Disable clustering."), tr("Disabling clustering on %1.").arg(pool->GetName()), true, parent),
      m_poolName(pool->GetName())
{
    this->SetPool(pool);
    this->AddApiMethodToRoleCheck("cluster.pool_destroy");
    this->AddApiMethodToRoleCheck("pif.set_disallow_unplug");
}

void DisableClusteringAction::run()
{
    try
    {
        XenCache* cache = this->GetConnection() ? this->GetConnection()->GetCache() : nullptr;
        const QList<QSharedPointer<Cluster>> clusters = cache ? cache->GetAll<Cluster>(XenObjectType::Cluster) : QList<QSharedPointer<Cluster>>();
        if (!clusters.isEmpty() && clusters.first())
        {
            const QSharedPointer<Cluster> cluster = clusters.first();
            // C# reads the cluster hosts after destroying; take their PIFs first, before the cache drops them
            QStringList pifRefs;
            for (const QSharedPointer<ClusterHost>& clusterHost : cluster->GetClusterHosts())
            {
                if (clusterHost)
                    pifRefs << clusterHost->GetPIFRef();
            }

            XenAPI::Cluster::pool_destroy(this->GetSession(), cluster->OpaqueRef());

            for (const QString& pifRef : pifRefs)
                XenAPI::PIF::set_disallow_unplug(this->GetSession(), pifRef, false);
        }
        this->SetDescription(tr("Disabled clustering on %1.").arg(this->m_poolName));
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}
