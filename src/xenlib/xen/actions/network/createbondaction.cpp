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
#include "../../connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_Network.h"
#include "../../xenapi/xenapi_Bond.h"
#include "../../xenapi/xenapi_PIF.h"
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
    if (m_pifRefs.isEmpty())
        throw std::invalid_argument("PIF list cannot be empty");
}

void CreateBondAction::run()
{
    try
    {
        connection()->setExpectDisruption(true);

        // Create the network first
        QVariantMap networkRecord;
        networkRecord["name_label"] = m_networkName;
        networkRecord["name_description"] = "Bonded Network";
        networkRecord["MTU"] = QString::number(m_mtu);
        networkRecord["managed"] = true;

        QVariantMap otherConfig;
        otherConfig["automatic"] = m_autoplug ? "true" : "false";
        otherConfig[QString::fromUtf8("create_in_progress")] = "true";
        networkRecord["other_config"] = otherConfig;

        setDescription(QString("Creating network '%1'").arg(m_networkName));
        QString taskRef = XenAPI::Network::async_create(session(), networkRecord);
        pollToCompletion(taskRef, 0, 10);
        m_networkRef = result();

        // Remove create_in_progress flag
        XenAPI::Network::remove_from_other_config(session(), m_networkRef, "create_in_progress");

        // TODO: Set network.Locked = true (not yet implemented in Qt)

        try
        {
            // Get all hosts in pool
            QList<QVariantMap> hosts = connection()->getCache()->GetAll("host");

            int inc = hosts.count() > 0 ? 90 / (hosts.count() * 2) : 90;
            int progress = 10;

            // Create bond on each host (coordinator last for stability)
            QList<QVariantMap> orderedHosts = getHostsCoordinatorLast();

            for (const QVariantMap& hostData : orderedHosts)
            {
                QString hostRef = hostData.value("_ref").toString();
                QString hostName = hostData.value("name_label").toString();

                // Find physical PIFs on this host corresponding to the coordinator PIFs
                QStringList hostPifRefs = findMatchingPIFsOnHost(hostRef);

                if (hostPifRefs.isEmpty())
                {
                    qWarning() << "No matching PIFs found on host" << hostName;
                    continue;
                }

                setDescription(QString("Creating bond on host %1").arg(hostName));
                qDebug() << "Creating bond on" << hostName << "with" << hostPifRefs.count() << "PIFs";

                // Build bond properties
                QVariantMap bondProperties;
                if (m_bondMode == "lacp")
                {
                    bondProperties["hashing_algorithm"] = m_hashingAlgorithm;
                }

                QString bondTaskRef = XenAPI::Bond::async_create(session(), m_networkRef,
                                                                 hostPifRefs, "", m_bondMode,
                                                                 bondProperties);

                pollToCompletion(bondTaskRef, progress, progress + inc);
                QString bondRef = result();

                qDebug() << "Created bond on" << hostName << ":" << bondRef;

                // Get bond interface PIF
                QVariantMap bondData = connection()->getCache()->ResolveObjectData("bond", bondRef);
                QString bondInterfaceRef = bondData.value("master").toString();

                // Store bond info for management interface reconfiguration
                NewBond newBond;
                newBond.bondRef = bondRef;
                newBond.bondInterfaceRef = bondInterfaceRef;
                newBond.memberRefs = hostPifRefs;
                m_newBonds.append(newBond);

                // TODO: Set bond.Locked = true, bondInterface.Locked = true

                progress += inc;
            }

            // Reconfigure management interfaces on bond members
            for (const NewBond& newBond : m_newBonds)
            {
                for (const QString& memberRef : newBond.memberRefs)
                {
                    progress += inc;

                    // Move management interface name from member to bond interface
                    NetworkingActionHelpers::moveManagementInterfaceName(
                        this, memberRef, newBond.bondInterfaceRef);

                    setPercentComplete(progress);
                }
            }

            setDescription(QString("Bond '%1' created successfully").arg(m_networkName));
            connection()->setExpectDisruption(false);

        } catch (const std::exception& e)
        {
            qWarning() << "Bond creation failed, cleaning up:" << e.what();
            cleanupOnError();
            throw;
        }

    } catch (const std::exception& e)
    {
        connection()->setExpectDisruption(false);
        setError(QString("Failed to create bond: %1").arg(e.what()));
    }
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
        for (const NewBond& newBond : m_newBonds)
        {
            try
            {
                // Get bond data to find primary slave
                QVariantMap bondData = connection()->getCache()->ResolveObjectData("bond", newBond.bondRef);
                QString primarySlaveRef = bondData.value("primary_slave").toString();

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
        for (const NewBond& newBond : m_newBonds)
        {
            try
            {
                QString taskRef = XenAPI::Bond::async_destroy(session(), newBond.bondRef);
                pollToCompletion(taskRef, 0, 100, true); // Suppress failures
            } catch (const std::exception& e)
            {
                qWarning() << "Failed to destroy bond:" << e.what();
                // Continue cleanup
            }
        }

        // Destroy network
        if (!m_networkRef.isEmpty())
        {
            try
            {
                XenAPI::Network::destroy(session(), m_networkRef);
            } catch (const std::exception& e)
            {
                qWarning() << "Failed to destroy network:" << e.what();
            }
        }

    } catch (...)
    {
        // Nothrow guarantee - swallow all exceptions during cleanup
    }

    connection()->setExpectDisruption(false);
}

QList<QVariantMap> CreateBondAction::getHostsCoordinatorLast() const
{
    QList<QVariantMap> hosts = connection()->getCache()->GetAll("host");
    if (hosts.isEmpty())
    {
        return hosts;
    }

    // Get the pool to find the coordinator
    QList<QVariantMap> pools = connection()->getCache()->GetAll("pool");
    if (pools.isEmpty())
    {
        return hosts; // Not a pool, return as-is
    }

    QVariantMap pool = pools.first();
    QString coordinatorRef = pool.value("master").toString();

    // Separate coordinator from other hosts
    QList<QVariantMap> supporters;
    QVariantMap coordinatorHost;
    bool foundCoordinator = false;

    for (const QVariantMap& host : hosts)
    {
        if (host.value("_ref").toString() == coordinatorRef)
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

QStringList CreateBondAction::findMatchingPIFsOnHost(const QString& hostRef) const
{
    // Get device names from coordinator PIFs
    QStringList deviceNames;
    for (const QString& pifRef : m_pifRefs)
    {
        QVariantMap pifData = connection()->getCache()->ResolveObjectData("pif", pifRef);
        QString device = pifData.value("device").toString();
        if (!device.isEmpty() && !deviceNames.contains(device))
        {
            deviceNames.append(device);
        }
    }

    // Find PIFs on the target host with matching device names
    QStringList result;
    QList<QVariantMap> allPifs = connection()->getCache()->GetAll("pif");

    for (const QVariantMap& pif : allPifs)
    {
        if (pif.value("host").toString() != hostRef)
        {
            continue;
        }

        QString device = pif.value("device").toString();
        if (deviceNames.contains(device))
        {
            // Only include physical PIFs (not VLAN or bond interfaces)
            bool physical = pif.value("physical", false).toBool();
            if (physical)
            {
                result.append(pif.value("_ref").toString());
            }
        }
    }

    return result;
}
