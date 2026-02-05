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

#include "createbondaction.h"
#include "networkingactionhelpers.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../bond.h"
#include "../../host.h"
#include "../../network.h"
#include "../../pif.h"
#include "../../pool.h"
#include "../../xenapi/xenapi_Network.h"
#include "../../xenapi/xenapi_Bond.h"
#include "../../failure.h"
#include "../../../xencache.h"
#include <QDebug>

CreateBondAction::CreateBondAction(XenConnection* connection,
                                   const QString& networkName,
                                   const QStringList& pifRefs,
                                   bool autoplug,
                                   qint64 mtu,
                                   const QString& bondMode,
                                   const QString& hashingAlgorithm,
                                   QObject* parent)
    : AsyncOperation(connection,
                     "Creating Bond",
                     QString("Creating bond '%1'").arg(networkName),
                     parent),
      m_networkName(networkName),
      m_pifRefs(pifRefs),
      m_autoplug(autoplug),
      m_mtu(mtu),
      m_bondMode(bondMode),
      m_hashingAlgorithm(hashingAlgorithm),
      m_networkRef()
{
    if (this->m_pifRefs.isEmpty())
        throw std::invalid_argument("PIF list cannot be empty");
}

void CreateBondAction::run()
{
    bool completed = false;
    try
    {
        this->GetConnection()->SetExpectDisruption(true);

        QSharedPointer<Pool> pool = this->GetConnection()->GetCache()->GetPoolOfOne();
        if (!pool || !pool->IsValid())
            throw Failure(Failure::INTERNAL_ERROR, "Pool not found for bond creation");

        QSharedPointer<Host> coordinator = pool->GetMasterHost();
        if (!coordinator || !coordinator->IsValid())
            throw Failure(Failure::INTERNAL_ERROR, "Pool coordinator not found for bond creation");

        const QList<QSharedPointer<Host>> poolHosts = pool->GetHosts();
        if (poolHosts.isEmpty())
            throw Failure(Failure::INTERNAL_ERROR, "No hosts available for bond creation");

        QHash<QString, QStringList> pifsByHost = this->getPifsOnAllHosts(poolHosts);
        XenCache* cache = this->GetConnection()->GetCache();
        if (!cache)
            throw Failure(Failure::INTERNAL_ERROR, "Cache unavailable for bond creation");

        for (auto it = pifsByHost.constBegin(); it != pifsByHost.constEnd(); ++it)
        {
            for (const QString& pifRef : it.value())
            {
                if (pifRef.isEmpty())
                    continue;
                QSharedPointer<PIF> pif = cache->ResolveObject<PIF>(pifRef);
                if (pif && pif->IsValid())
                {
                    pif->Lock();
                    this->m_lockedPifRefs.insert(pifRef);
                }
            }
        }

        // Create the network first
        QVariantMap networkRecord;
        networkRecord["name_label"] = this->m_networkName;
        networkRecord["name_description"] = "Bonded Network";
        networkRecord["MTU"] = QString::number(this->m_mtu);
        networkRecord["managed"] = true;

        QVariantMap otherConfig;
        otherConfig["automatic"] = this->m_autoplug ? "true" : "false";
        otherConfig[QString::fromUtf8("create_in_progress")] = "true";
        networkRecord["other_config"] = otherConfig;

        this->SetDescription(QString("Creating network '%1'").arg(this->m_networkName));
        QString taskRef = XenAPI::Network::async_create(this->GetSession(), networkRecord);
        this->pollToCompletion(taskRef, 0, 10);
        this->m_networkRef = this->GetResult();

        QSharedPointer<Network> network = this->GetConnection()->WaitForCacheObject<Network>(
            "network", this->m_networkRef);
        if (!network)
            throw std::runtime_error("Network not found in cache after creation");
        network->Lock();
        this->m_lockedNetworkRef = this->m_networkRef;

        // Remove create_in_progress flag
        XenAPI::Network::remove_from_other_config(this->GetSession(), this->m_networkRef, "create_in_progress");

        // TODO: Set network.Locked = true (not yet implemented in Qt)

        try
        {
            int inc = poolHosts.count() > 0 ? 90 / (poolHosts.count() * 2) : 90;
            int progress = 10;

            // Create bond on each host (coordinator last for stability)
            QList<QSharedPointer<Host>> orderedHosts = this->getHostsCoordinatorLast();

            for (const QSharedPointer<Host>& host : orderedHosts)
            {
                if (!host || !host->IsValid())
                    continue;

                QString hostRef = host->OpaqueRef();
                QString hostName = host->GetName();

                // Find PIFs on this host corresponding to the coordinator PIFs
                QStringList hostPifRefs = pifsByHost.value(hostRef);

                if (hostPifRefs.isEmpty())
                {
                    qWarning() << "No matching PIFs found on host" << hostName;
                    continue;
                }

                this->SetDescription(QString("Creating bond on host %1").arg(hostName));
                qDebug() << "Creating bond on" << hostName << "with" << hostPifRefs.count() << "PIFs";

                // Build bond properties
                QVariantMap bondProperties;
                if (this->m_bondMode == "lacp")
                {
                    bondProperties["hashing_algorithm"] = this->m_hashingAlgorithm;
                }

                // Filter to physical PIFs (matches C# behavior)
                QStringList physicalPifRefs;
                for (const QString& pifRef : hostPifRefs)
                {
                    QSharedPointer<PIF> pif = this->GetConnection()->GetCache()->ResolveObject<PIF>(pifRef);
                    if (pif && pif->IsValid() && pif->IsPhysical())
                        physicalPifRefs.append(pifRef);
                }

                QString bondTaskRef = XenAPI::Bond::async_create(this->GetSession(), this->m_networkRef,
                                                                 physicalPifRefs, "", this->m_bondMode,
                                                                 bondProperties);

                this->pollToCompletion(bondTaskRef, progress, progress + inc);
                QString bondRef = this->GetResult();

                qDebug() << "Created bond on" << hostName << ":" << bondRef;

                QSharedPointer<Bond> bond = this->GetConnection()->WaitForCacheObject<Bond>("bond", bondRef);
                if (!bond)
                    throw std::runtime_error("Bond not found in cache after creation");

                QString bondInterfaceRef = bond->MasterRef();
                if (bondInterfaceRef.isEmpty() || bondInterfaceRef == XENOBJECT_NULL)
                    throw std::runtime_error("Bond master interface not found in cache after creation");
                QSharedPointer<PIF> bondInterface = this->GetConnection()->GetCache()->ResolveObject<PIF>(bondInterfaceRef);
                if (!bondInterface)
                    throw std::runtime_error("Bond master interface not found in cache after creation");

                bond->Lock();
                bondInterface->Lock();
                this->m_lockedBondRefs.insert(bondRef);
                this->m_lockedBondInterfaceRefs.insert(bondInterfaceRef);

                // Store bond info for management interface reconfiguration
                NewBond newBond;
                newBond.bondRef = bondRef;
                newBond.bondInterfaceRef = bondInterfaceRef;
                newBond.memberRefs = hostPifRefs;
                this->m_newBonds.append(newBond);

                // TODO: Set bond.Locked = true, bondInterface.Locked = true

                progress += inc;
            }

            // Reconfigure management interfaces on bond members
            for (const NewBond& newBond : this->m_newBonds)
            {
                for (const QString& memberRef : newBond.memberRefs)
                {
                    progress += inc;

                    // Move management interface name from member to bond interface
                    NetworkingActionHelpers::moveManagementInterfaceName(
                        this, memberRef, newBond.bondInterfaceRef);

                    this->SetPercentComplete(progress);
                }
            }

            this->SetDescription(QString("Bond '%1' created successfully").arg(this->m_networkName));
            this->GetConnection()->SetExpectDisruption(false);
            completed = true;

        } catch (const std::exception& e)
        {
            qWarning() << "Bond creation failed, cleaning up:" << e.what();
            this->cleanupOnError();
            throw;
        }

    } catch (const std::exception& e)
    {
        this->GetConnection()->SetExpectDisruption(false);
        this->setError(QString("Failed to create bond: %1").arg(e.what()));
    }

    if (!completed)
        this->GetConnection()->SetExpectDisruption(false);
    this->unlockAllLockedObjects();
}

void CreateBondAction::cleanupOnError()
{
    // Cleanup order (nothrow guarantee):
    // 1. Revert management interfaces on all bonds
    // 2. Destroy all bonds
    // 3. Destroy network

    try
    {
        // Revert management interfaces
        for (const NewBond& newBond : this->m_newBonds)
        {
            try
            {
                // Get bond data to find primary slave
                QSharedPointer<Bond> bond = this->GetConnection()->GetCache()->ResolveObject<Bond>(newBond.bondRef);
                QString primarySlaveRef = bond ? bond->PrimarySlaveRef() : QString();

                if (!primarySlaveRef.isEmpty())
                {
                    // Move management interface name back from bond to primary member
                    NetworkingActionHelpers::moveManagementInterfaceName(
                        this, newBond.bondInterfaceRef, primarySlaveRef);
                }
            } catch (const std::exception& e)
            {
                qWarning() << "Failed to revert management interface:" << e.what();
                // Continue cleanup
            }
        }

        // Destroy bonds
        for (const NewBond& newBond : this->m_newBonds)
        {
            try
            {
                QString taskRef = XenAPI::Bond::async_destroy(this->GetSession(), newBond.bondRef);
                this->pollToCompletion(taskRef, 0, 100, true); // Suppress failures
            } catch (const std::exception& e)
            {
                qWarning() << "Failed to destroy bond:" << e.what();
                // Continue cleanup
            }
        }

        // Destroy network
        if (!this->m_networkRef.isEmpty())
        {
            try
            {
                XenAPI::Network::destroy(this->GetSession(), this->m_networkRef);
            } catch (const std::exception& e)
            {
                qWarning() << "Failed to destroy network:" << e.what();
            }
        }

    } catch (...)
    {
        // Nothrow guarantee - swallow all exceptions during cleanup
    }

    this->GetConnection()->SetExpectDisruption(false);
}

void CreateBondAction::unlockAllLockedObjects()
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

    for (const QString& pifRef : this->m_lockedBondInterfaceRefs)
    {
        QSharedPointer<PIF> pif = cache->ResolveObject<PIF>(pifRef);
        if (pif && pif->IsValid())
            pif->Unlock();
    }

    if (!this->m_lockedNetworkRef.isEmpty())
    {
        QSharedPointer<Network> network = cache->ResolveObject<Network>(this->m_lockedNetworkRef);
        if (network && network->IsValid())
            network->Unlock();
    }
}

QList<QSharedPointer<Host>> CreateBondAction::getHostsCoordinatorLast() const
{
    QSharedPointer<Pool> pool = this->GetConnection()->GetCache()->GetPoolOfOne();
    if (!pool || !pool->IsValid())
        return QList<QSharedPointer<Host>>();

    QList<QSharedPointer<Host>> hosts = pool->GetHosts();
    if (hosts.isEmpty())
    {
        return hosts;
    }

    QString coordinatorRef = pool->GetMasterHostRef();

    // Separate coordinator from other hosts
    QList<QSharedPointer<Host>> supporters;
    QSharedPointer<Host> coordinatorHost;
    bool foundCoordinator = false;

    for (const QSharedPointer<Host>& host : hosts)
    {
        if (!host || !host->IsValid())
            continue;

        if (host->OpaqueRef() == coordinatorRef)
        {
            coordinatorHost = host;
            foundCoordinator = true;
        } else
        {
            supporters.append(host);
        }
    }

    // Append coordinator last
    if (foundCoordinator)
    {
        supporters.append(coordinatorHost);
    }

    return supporters;
}

QStringList CreateBondAction::coordinatorDeviceNames() const
{
    QStringList deviceNames;
    for (const QString& pifRef : this->m_pifRefs)
    {
        QSharedPointer<PIF> pif = this->GetConnection()->GetCache()->ResolveObject<PIF>(pifRef);
        if (!pif || !pif->IsValid())
            continue;
        const QString device = pif->GetDevice();
        if (!device.isEmpty() && !deviceNames.contains(device))
            deviceNames.append(device);
    }
    return deviceNames;
}

QHash<QString, QStringList> CreateBondAction::getPifsOnAllHosts(const QList<QSharedPointer<Host>>& hosts) const
{
    QHash<QString, QStringList> result;
    const QStringList devices = this->coordinatorDeviceNames();
    if (devices.isEmpty())
        return result;

    for (const QSharedPointer<Host>& host : hosts)
    {
        if (!host || !host->IsValid())
            continue;

        QStringList pifRefs;
        QList<QSharedPointer<PIF>> pifs = host->GetPIFs();
        for (const QSharedPointer<PIF>& pif : pifs)
        {
            if (!pif || !pif->IsValid())
                continue;
            if (devices.contains(pif->GetDevice()))
                pifRefs.append(pif->OpaqueRef());
        }

        result.insert(host->OpaqueRef(), pifRefs);
    }

    return result;
}
