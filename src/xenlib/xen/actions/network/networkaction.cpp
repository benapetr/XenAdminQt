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

#include "networkaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../host.h"
#include "../../pool.h"
#include "../../xenapi/xenapi_Network.h"
#include "../../xenapi/xenapi_VLAN.h"
#include "../../xenapi/xenapi_Tunnel.h"
#include "../../xenapi/xenapi_Pool.h"
#include "../../xenapi/xenapi_PIF.h"
#include "../../jsonrpcclient.h"
#include "../../../xencache.h"

namespace
{
    void throwIfJsonError(const char* context)
    {
        const QString jsonErr = Xen::JsonRpcClient::lastError();
        if (!jsonErr.isEmpty())
            throw std::runtime_error(QString("%1 failed: %2").arg(context, jsonErr).toStdString());
    }
}

// Constructor: Create external (VLAN) network
NetworkAction::NetworkAction(QSharedPointer<Network> network,
                               QSharedPointer<PIF> basePif,
                               qint64 vlan,
                               QObject* parent)
    : AsyncOperation(network ? network->GetConnection() : nullptr,
                     "Creating Network",
                     network ? QString("Creating network '%1'").arg(network->GetName())
                             : "Creating network",
                     parent),
      m_network(network),
      m_basePif(basePif),
      m_actionType(ActionType::Create),
      m_vlan(vlan),
      m_external(true),
      m_changePIFs(false)
{
    if (!network)
        throw std::invalid_argument("Network cannot be null");
    if (!basePif)
        throw std::invalid_argument("Base PIF cannot be null for VLAN network");
}

// Constructor: Create or destroy internal network
NetworkAction::NetworkAction(QSharedPointer<Network> network,
                               bool create,
                               QObject* parent)
    : AsyncOperation(network ? network->GetConnection() : nullptr,
                     create ? "Creating Network" : "Removing Network",
                     network ? QString(create ? "Creating network '%1'" : "Removing network '%1'")
                                   .arg(network->GetName())
                             : (create ? "Creating network" : "Removing network"),
                     parent),
      m_network(network),
      m_actionType(create ? ActionType::Create : ActionType::Destroy),
      m_vlan(0),
      m_external(false),
      m_changePIFs(false)
{
    if (!network)
        throw std::invalid_argument("Network cannot be null");

    // For destroy, get the PIFs
    if (!create && network->IsValid())
    {
        QStringList pifRefs = network->GetPIFRefs();
        for (const QString& pifRef : pifRefs)
        {
            QSharedPointer<PIF> pif = network->GetConnection()->GetCache()->ResolveObject<PIF>(pifRef);
            if (pif && pif->IsValid())
                this->m_pifs.append(pif);
        }
    }
}

// Constructor: Update network
NetworkAction::NetworkAction(QSharedPointer<Network> network,
                               bool changePIFs,
                               bool external,
                               QSharedPointer<PIF> basePif,
                               qint64 vlan,
                               bool suppressHistory,
                               QObject* parent)
    : AsyncOperation(network ? network->GetConnection() : nullptr,
                     "Updating Network",
                     network ? QString("Updating network '%1'").arg(network->GetName())
                             : "Updating network",
                     parent),
      m_network(network),
      m_basePif(basePif),
      m_actionType(ActionType::Update),
      m_vlan(vlan),
      m_external(external),
      m_changePIFs(changePIFs)
{
    Q_UNUSED(suppressHistory); // TODO: Implement history suppression if needed

    if (!network)
        throw std::invalid_argument("Network cannot be null");

    // If changing PIFs, get the current PIFs
    if (changePIFs && network->IsValid())
    {
        QStringList pifRefs = network->GetPIFRefs();
        for (const QString& pifRef : pifRefs)
        {
            QSharedPointer<PIF> pif = network->GetConnection()->GetCache()->ResolveObject<PIF>(pifRef);
            if (pif && pif->IsValid())
                this->m_pifs.append(pif);
        }
    }
}

void NetworkAction::destroyPIFs()
{
    if (this->m_pifs.isEmpty())
        return;

    for (QSharedPointer<PIF> pif : this->m_pifs)
    {
        if (!pif || !pif->IsValid())
            continue;

        // Check if this is a tunnel access PIF
        QStringList tunnelRefs = pif->TunnelAccessPIFOfRefs();
        if (!tunnelRefs.isEmpty())
        {
            // Destroy associated tunnels
            for (const QString& tunnelRef : tunnelRefs)
            {
                XenAPI::Tunnel::destroy(this->GetSession(), tunnelRef);
                throwIfJsonError("Tunnel.destroy");
            }
        }
        else
        {
            // Not a tunnel access PIF
            if (!pif->IsPhysical())
            {
                // Virtual PIF - destroy GetVLAN or SR-IOV
                qint64 vlanTag = pif->GetVLAN();
                if (vlanTag != -1)
                {
                    // VLAN PIF - destroy via VLAN.destroy
                    QString vlanMasterRef = pif->VLANMasterOfRef();
                    if (!vlanMasterRef.isEmpty())
                    {
                        XenAPI::VLAN::destroy(this->GetSession(), vlanMasterRef);
                        throwIfJsonError("VLAN.destroy");
                    }
                }

                // Check for SR-IOV logical PIF
                QStringList sriovRefs = pif->SriovLogicalPIFOfRefs();
                if (!sriovRefs.isEmpty())
                {
                    // TODO: This works because xenapi_Network_sriov.h exists
                    // But keep the pattern consistent
                    for (const QString& sriovRef : sriovRefs)
                    {
                        qWarning() << "SR-IOV PIF cleanup not yet tested";
                        qWarning() << "SR-IOV ref:" << sriovRef;
                        // C# code: Network_sriov.destroy(Session, sriovRef);
                        // Implementation would go here when tested
                    }
                }
            }
            else
            {
                // Physical PIF - forget it
                XenAPI::PIF::forget(this->GetSession(), pif->OpaqueRef());
                throwIfJsonError("PIF.forget");
            }
        }
    }

    this->m_pifs.clear();
}

void NetworkAction::createVLAN(const QString& networkRef)
{
    if (!this->m_basePif || !this->m_basePif->IsValid())
    {
        throw std::runtime_error("Base PIF is not valid for VLAN creation");
    }

    // Get the pool coordinator
    XenCache* cache = this->GetConnection()->GetCache();
    QList<QSharedPointer<Pool>> pools = cache->GetAll<Pool>(XenObjectType::Pool);

    if (pools.isEmpty())
    {
        throw std::runtime_error("No pool found - cannot create VLAN");
    }

    QSharedPointer<Pool> pool = pools.first();
    QString masterRef = pool->GetMasterHostRef();

    QSharedPointer<Host> coordinator = cache->ResolveObject<Host>(XenObjectType::Host, masterRef);
    if (!coordinator || !coordinator->IsValid())
    {
        throw std::runtime_error("Pool coordinator not found");
    }

    // Create VLAN from PIF
    XenAPI::Pool::create_VLAN_from_PIF(this->GetSession(),
                                       this->m_basePif->OpaqueRef(),
                                       networkRef,
                                       this->m_vlan);
    throwIfJsonError("Pool.create_VLAN_from_PIF");
}

void NetworkAction::run()
{
    try
    {
        switch (this->m_actionType)
        {
            case ActionType::Destroy:
            {
                this->SetDescription("Removing network");

                // Destroy PIFs first
                this->destroyPIFs();

                // Destroy the network
                if (this->m_network && this->m_network->IsValid())
                {
                    const QString ref = this->m_network->OpaqueRef();
                    XenAPI::Network::destroy(this->GetSession(), ref);
                    throwIfJsonError("Network.destroy");
                    XenCache* cache = this->GetConnection() ? this->GetConnection()->GetCache() : nullptr;
                    if (cache)
                        cache->Remove(XenObjectType::Network, ref);
                }

                this->SetDescription(QString("Network '%1' removed").arg(this->m_network->GetName()));
                break;
            }

            case ActionType::Update:
            {
                this->SetDescription("Updating network");

                if (this->m_changePIFs)
                {
                    if (this->m_external)
                    {
                        // Check if VLAN tag already exists on this device
                        QList<QSharedPointer<PIF>> allPifs = this->GetConnection()->GetCache()->GetAll<PIF>(XenObjectType::PIF);
                        for (const QSharedPointer<PIF>& pif : allPifs)
                        {
                            if (pif && pif->IsValid() &&
                                pif->GetVLAN() == this->m_vlan &&
                                pif->GetDevice() == this->m_basePif->GetDevice())
                            {
                                throw std::runtime_error("VLAN tag already exists on this device");
                            }
                        }
                    }

                    // Destroy old PIFs
                    this->destroyPIFs();

                    // Create new VLAN if external
                    if (this->m_external && this->m_network && this->m_network->IsValid())
                    {
                        this->createVLAN(this->m_network->OpaqueRef());
                    }
                }

                this->SetDescription(QString("Network '%1' updated").arg(this->m_network->GetName()));
                break;
            }

            case ActionType::Create:
            {
                this->SetDescription("Creating network");

                // Create the network
                if (!this->m_network)
                {
                    throw std::runtime_error("Network object is null");
                }

                // Build network record for creation
                QVariantMap networkRecord;
                networkRecord["name_label"] = this->m_network->GetName();
                networkRecord["name_description"] = this->m_network->GetDescription();
                networkRecord["other_config"] = this->m_network->GetOtherConfig();
                networkRecord["tags"] = this->m_network->GetTags();
                QVariantMap networkData = this->m_network->GetData();
                if (networkData.contains("MTU"))
                    networkRecord["MTU"] = networkData.value("MTU");
                if (networkData.contains("managed"))
                    networkRecord["managed"] = networkData.value("managed");

                // Create the network
                QString networkRef = XenAPI::Network::create(this->GetSession(), networkRecord);
                throwIfJsonError("Network.create");

                // Create VLAN if external
                if (this->m_external)
                {
                    this->createVLAN(networkRef);
                }

                this->SetDescription(QString("Network '%1' created").arg(this->m_network->GetName()));
                break;
            }
        }

    } catch (const std::exception& e)
    {
        this->setError(QString("Network operation failed: %1").arg(e.what()));
        throw;
    }
}
