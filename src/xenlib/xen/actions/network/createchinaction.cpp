/*
 * Copyright (c) 2025, Petr Bena <petrbena@bena.rocks>
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

#include "createchinaction.h"
#include "../../network/connection.h"
#include "../../xenapi/xenapi_Network.h"
#include "../../xenapi/xenapi_Tunnel.h"
#include "../../pif.h"
#include "../../network.h"
#include "../../../xencache.h"

CreateChinAction::CreateChinAction(XenConnection* connection,
                                   const QSharedPointer<Network>& newNetwork,
                                   const QSharedPointer<Network>& transportNetwork,
                                   QObject* parent)
    : AsyncOperation(connection,
                     "Creating Network",
                     newNetwork ? QString("Creating network '%1'").arg(newNetwork->GetName())
                                : QStringLiteral("Creating network"),
                     parent),
      m_newNetwork(newNetwork),
      m_transportNetwork(transportNetwork)
{
    if (!this->m_newNetwork)
        throw std::invalid_argument("New network cannot be null");
    if (!this->m_transportNetwork)
        throw std::invalid_argument("Transport network cannot be null");

    // RBAC dependencies (matches C# CreateChinAction)
    this->AddApiMethodToRoleCheck("Network.create");

    XenCache* cache = this->GetConnection() ? this->GetConnection()->GetCache() : nullptr;
    if (cache)
    {
        const QStringList pifRefs = this->m_transportNetwork->GetPIFRefs();
        for (const QString& pifRef : pifRefs)
        {
            QSharedPointer<PIF> pif = cache->ResolveObject<PIF>(pifRef);
            if (pif && pif->IsValid() && pif->IsManagementInterface())
            {
                this->AddApiMethodToRoleCheck("Tunnel.create");
                break;
            }
        }
    }
}

void CreateChinAction::run()
{
    if (!this->m_newNetwork || !this->m_transportNetwork)
        throw std::runtime_error("Network parameters are missing");

    // Create the network
    QVariantMap networkRecord;
    networkRecord["name_label"] = this->m_newNetwork->GetName();
    networkRecord["name_description"] = this->m_newNetwork->GetDescription();
    networkRecord["other_config"] = this->m_newNetwork->GetOtherConfig();
    networkRecord["tags"] = this->m_newNetwork->GetTags();
    QVariantMap data = this->m_newNetwork->GetData();
    networkRecord["managed"] = data.contains("managed") ? data.value("managed") : true;
    if (data.contains("MTU"))
        networkRecord["MTU"] = data.value("MTU");

    const QString networkRef = XenAPI::Network::create(this->GetSession(), networkRecord);

    // Build tunnels using management PIFs from the selected transport network
    XenCache* cache = this->GetConnection() ? this->GetConnection()->GetCache() : nullptr;
    if (!cache)
        return;

    const QStringList pifRefs = this->m_transportNetwork->GetPIFRefs();
    for (const QString& pifRef : pifRefs)
    {
        QSharedPointer<PIF> pif = cache->ResolveObject<PIF>(pifRef);
        if (!pif || !pif->IsValid())
            continue;
        if (!pif->IsManagementInterface())
            continue;

        XenAPI::Tunnel::create(this->GetSession(), pif->OpaqueRef(), networkRef);
    }
}
