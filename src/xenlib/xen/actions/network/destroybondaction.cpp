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

#include "destroybondaction.h"
#include "networkingactionhelpers.h"
#include "../../bond.h"
#include "../../host.h"
#include "../../network.h"
#include "../../network/connection.h"
#include "../../pif.h"
#include "../../pool.h"
#include "../../session.h"
#include "../../xenapi/xenapi_Bond.h"
#include "../../xenapi/xenapi_Network.h"
#include "../../../xencache.h"
#include <QDebug>

DestroyBondAction::DestroyBondAction(XenConnection* connection, const QString& bondRef, QObject* parent)
    : AsyncOperation(connection, "Destroying Bond", "Destroying network bond", parent), m_bondRef(bondRef)
{
    if (this->m_bondRef.isEmpty())
        throw std::invalid_argument("Bond reference cannot be empty");

    XenCache* cache = connection ? connection->GetCache() : nullptr;
    if (!cache)
        throw std::runtime_error("Cache is unavailable");

    QSharedPointer<Bond> bond = cache->ResolveObject<Bond>(this->m_bondRef);
    if (!bond || !bond->IsValid())
        throw std::runtime_error("Bond not found");

    QSharedPointer<PIF> masterPif = cache->ResolveObject<PIF>(bond->MasterRef());
    this->m_bondName = masterPif && masterPif->IsValid() ? masterPif->GetName() : QString();

    this->SetTitle(QString("Destroying bond %1").arg(this->m_bondName));
    this->SetDescription(QString("Destroying bond %1").arg(this->m_bondName));

    // RBAC dependencies (matches C# DestroyBondAction)
    this->AddApiMethodToRoleCheck("host.management_reconfigure");
    this->AddApiMethodToRoleCheck("network.destroy");
    this->AddApiMethodToRoleCheck("vif.plug");
    this->AddApiMethodToRoleCheck("vif.unplug");
    this->AddApiMethodToRoleCheck("pif.reconfigure_ip");
    this->AddApiMethodToRoleCheck("pif.plug");
    this->AddApiMethodToRoleCheck("bond.destroy");
}

void DestroyBondAction::run()
{
    try
    {
        this->GetConnection()->SetExpectDisruption(true);

        // Find all equivalent bonds across all hosts in the pool
        QList<BondInfo> bondsToDestroy = this->findAllEquivalentBonds();

        if (bondsToDestroy.isEmpty())
        {
            this->setError("No bonds found to destroy");
            this->GetConnection()->SetExpectDisruption(false);
            return;
        }

        this->lockObjectsForBonds(bondsToDestroy);
        this->SetPercentComplete(0);

        // Step 1: Move management interface names from bonds to primary members
        // This is done first to preserve management connectivity
        int incr = bondsToDestroy.count() > 0 ? 50 / bondsToDestroy.count() : 0;
        int progress = 0;

        try
        {
            for (const BondInfo& bondInfo : bondsToDestroy)
            {
                if (!bondInfo.primarySlaveRef.isEmpty())
                {
                    this->SetDescription(QString("Preparing to destroy bond on %1")
                                       .arg(bondInfo.hostName));

                    NetworkingActionHelpers::moveManagementInterfaceName(
                        this, bondInfo.bondInterfaceRef, bondInfo.primarySlaveRef);

                    progress += incr;
                    this->SetPercentComplete(progress);
                }
            }
        } catch (const std::exception& e)
        {
            QString error = e.what();
            // Ignore KeepAliveFailure - expected during disruption
            if (!error.contains("KeepAliveFailure"))
            {
                qWarning() << "Failed to move management interfaces:" << error;
                throw;
            }
        }

        this->SetPercentComplete(50);

        // Step 2: Destroy all bonds
        incr = bondsToDestroy.count() > 0 ? 40 / bondsToDestroy.count() : 0;
        progress = 50;

        QStringList errors;

        for (const BondInfo& bondInfo : bondsToDestroy)
        {
            try
            {
                this->SetDescription(QString("Destroying bond on %1").arg(bondInfo.hostName));

                QString taskRef = XenAPI::Bond::async_destroy(this->GetSession(), bondInfo.bondRef);
                this->pollToCompletion(taskRef, progress, progress + incr);

                progress += incr;

            } catch (const std::exception& e)
            {
                QString error = e.what();
                // Ignore KeepAliveFailure - expected during disruption
                if (!error.contains("KeepAliveFailure"))
                {
                    qWarning() << "Failed to destroy bond" << bondInfo.bondRef << ":" << error;
                    errors.append(error);
                }
            }
        }

        this->SetPercentComplete(90);

        // Step 3: Destroy the network (if it exists and is not used elsewhere)
        if (!bondsToDestroy.isEmpty())
        {
            QString networkRef = bondsToDestroy.first().networkRef;

            if (!networkRef.isEmpty() && networkRef != XENOBJECT_NULL)
            {
                try
                {
                    this->SetDescription("Destroying network");
                    XenAPI::Network::destroy(this->GetSession(), networkRef);

                } catch (const std::exception& e)
                {
                    QString error = e.what();
                    if (!error.contains("KeepAliveFailure"))
                    {
                        qWarning() << "Failed to destroy network:" << error;
                        errors.append(error);
                    }
                }
            }
        }

        this->SetPercentComplete(100);

        this->GetConnection()->SetExpectDisruption(false);
        this->unlockAllLockedObjects();

        if (!errors.isEmpty())
        {
            this->setError(QString("Bond destroyed with warnings: %1").arg(errors.join(", ")));
        } else
        {
            this->SetDescription(QString("Bond '%1' destroyed").arg(this->m_bondName));
        }

    } catch (const std::exception& e)
    {
        this->GetConnection()->SetExpectDisruption(false);
        this->unlockAllLockedObjects();
        this->setError(QString("Failed to destroy bond: %1").arg(e.what()));
    }
}

static QStringList bondSlaveDevices(XenCache* cache, const QStringList& slaveRefs)
{
    QStringList devices;
    if (!cache)
        return devices;

    devices.reserve(slaveRefs.size());
    for (const QString& slaveRef : slaveRefs)
    {
        QSharedPointer<PIF> pif = cache->ResolveObject<PIF>(slaveRef);
        if (!pif || !pif->IsValid())
            continue;
        const QString device = pif->GetDevice();
        if (!device.isEmpty())
            devices.append(device);
    }

    devices.sort();
    return devices;
}

QList<DestroyBondAction::BondInfo> DestroyBondAction::findAllEquivalentBonds() const
{
    QList<BondInfo> result;

    XenCache* cache = this->GetConnection() ? this->GetConnection()->GetCache() : nullptr;
    if (!cache)
        return result;

    QSharedPointer<Bond> refBond = cache->ResolveObject<Bond>(this->m_bondRef);
    if (!refBond || !refBond->IsValid())
        return result;

    QSharedPointer<PIF> refMasterPif = cache->ResolveObject<PIF>(refBond->MasterRef());
    if (!refMasterPif || !refMasterPif->IsValid())
        return result;

    const QStringList refDevices = bondSlaveDevices(cache, refBond->SlaveRefs());
    const QString refHostRef = refMasterPif->GetHostRef();

    QList<QSharedPointer<Host>> hosts;
    QSharedPointer<Pool> pool = cache->GetPoolOfOne();
    if (pool && pool->IsValid())
        hosts = pool->GetHosts();
    else
        hosts = QList<QSharedPointer<Host>> { refMasterPif->GetHost() };

    if (hosts.isEmpty())
        return result;

    QList<QSharedPointer<Bond>> allBonds = cache->GetAll<Bond>();

    for (const QSharedPointer<Host>& host : hosts)
    {
        if (!host || !host->IsValid())
            continue;

        const QString hostRef = host->OpaqueRef();

        for (const QSharedPointer<Bond>& bond : allBonds)
        {
            if (!bond || !bond->IsValid())
                continue;

            QSharedPointer<PIF> masterPif = cache->ResolveObject<PIF>(bond->MasterRef());
            if (!masterPif || !masterPif->IsValid())
                continue;

            if (masterPif->GetHostRef() != hostRef)
                continue;

            const QStringList bondDevices = bondSlaveDevices(cache, bond->SlaveRefs());
            if (bondDevices != refDevices)
                continue;

            BondInfo info;
            info.bondRef = bond->OpaqueRef();
            info.bondInterfaceRef = bond->MasterRef();
            info.primarySlaveRef = bond->PrimarySlaveRef();
            info.slaveRefs = bond->SlaveRefs();
            info.networkRef = masterPif->GetNetworkRef();
            info.hostRef = hostRef;
            info.hostName = host->GetName();

            result.append(info);
            break;
        }
    }

    if (result.isEmpty() && refBond && refBond->IsValid())
    {
        BondInfo info;
        info.bondRef = refBond->OpaqueRef();
        info.bondInterfaceRef = refBond->MasterRef();
        info.primarySlaveRef = refBond->PrimarySlaveRef();
        info.slaveRefs = refBond->SlaveRefs();
        info.networkRef = refMasterPif->GetNetworkRef();
        info.hostRef = refHostRef;
        info.hostName = refMasterPif->GetHost() ? refMasterPif->GetHost()->GetName() : QString();
        result.append(info);
    }

    return result;
}

void DestroyBondAction::lockObjectsForBonds(const QList<BondInfo>& bonds)
{
    XenCache* cache = this->GetConnection() ? this->GetConnection()->GetCache() : nullptr;
    if (!cache)
        return;

    for (const BondInfo& bondInfo : bonds)
    {
        QSharedPointer<Bond> bond = cache->ResolveObject<Bond>(bondInfo.bondRef);
        if (bond && bond->IsValid())
        {
            bond->Lock();
            this->m_lockedBondRefs.insert(bondInfo.bondRef);
        }

        if (!bondInfo.bondInterfaceRef.isEmpty())
        {
            QSharedPointer<PIF> masterPif = cache->ResolveObject<PIF>(bondInfo.bondInterfaceRef);
            if (masterPif && masterPif->IsValid())
            {
                masterPif->Lock();
                this->m_lockedPifRefs.insert(bondInfo.bondInterfaceRef);
            }
        }

        for (const QString& slaveRef : bondInfo.slaveRefs)
        {
            QSharedPointer<PIF> slavePif = cache->ResolveObject<PIF>(slaveRef);
            if (slavePif && slavePif->IsValid())
            {
                slavePif->Lock();
                this->m_lockedPifRefs.insert(slaveRef);
            }
        }

        if (this->m_lockedNetworkRef.isEmpty())
            this->m_lockedNetworkRef = bondInfo.networkRef;
    }

    if (!this->m_lockedNetworkRef.isEmpty())
    {
        QSharedPointer<Network> network = cache->ResolveObject<Network>(this->m_lockedNetworkRef);
        if (network && network->IsValid())
            network->Lock();
    }
}

void DestroyBondAction::unlockAllLockedObjects()
{
    XenCache* cache = this->GetConnection() ? this->GetConnection()->GetCache() : nullptr;
    if (!cache)
        return;

    for (const QString& pifRef : this->m_lockedPifRefs)
    {
        QSharedPointer<PIF> pif = cache->ResolveObject<PIF>(pifRef);
        if (pif && pif->IsValid())
            pif->Unlock();
    }

    for (const QString& bondRef : this->m_lockedBondRefs)
    {
        QSharedPointer<Bond> bond = cache->ResolveObject<Bond>(bondRef);
        if (bond && bond->IsValid())
            bond->Unlock();
    }

    if (!this->m_lockedNetworkRef.isEmpty())
    {
        QSharedPointer<Network> network = cache->ResolveObject<Network>(this->m_lockedNetworkRef);
        if (network && network->IsValid())
            network->Unlock();
    }

    this->m_lockedPifRefs.clear();
    this->m_lockedBondRefs.clear();
    this->m_lockedNetworkRef.clear();
}
