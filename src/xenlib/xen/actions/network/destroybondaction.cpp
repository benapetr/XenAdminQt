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
#include "../../connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_Bond.h"
#include "../../xenapi/xenapi_Network.h"
#include "../../xenapi/xenapi_PIF.h"
#include "../../../xencache.h"
#include <QDebug>

DestroyBondAction::DestroyBondAction(XenConnection* connection,
                                     const QString& bondRef,
                                     QObject* parent)
    : AsyncOperation(connection,
                     "Destroying Bond",
                     "Destroying network bond",
                     parent),
      m_bondRef(bondRef)
{
    if (m_bondRef.isEmpty())
        throw std::invalid_argument("Bond reference cannot be empty");

    // Get bond name for display
    QVariantMap bondData = connection->getCache()->ResolveObjectData("bond", m_bondRef);
    QVariantMap masterPifData = connection->getCache()->ResolveObjectData("pif",
                                                                bondData.value("master").toString());
    m_bondName = masterPifData.value("device").toString();

    setTitle(QString("Destroying bond %1").arg(m_bondName));
    setDescription(QString("Destroying bond %1").arg(m_bondName));
}

void DestroyBondAction::run()
{
    try
    {
        connection()->setExpectDisruption(true);

        // Find all equivalent bonds across all hosts in the pool
        QList<BondInfo> bondsToDestroy = findAllEquivalentBonds();

        if (bondsToDestroy.isEmpty())
        {
            setError("No bonds found to destroy");
            connection()->setExpectDisruption(false);
            return;
        }

        setPercentComplete(0);

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
                    setDescription(QString("Preparing to destroy bond on %1")
                                       .arg(bondInfo.hostName));

                    NetworkingActionHelpers::moveManagementInterfaceName(
                        this, bondInfo.bondInterfaceRef, bondInfo.primarySlaveRef);

                    progress += incr;
                    setPercentComplete(progress);
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

        setPercentComplete(50);

        // Step 2: Destroy all bonds
        incr = bondsToDestroy.count() > 0 ? 40 / bondsToDestroy.count() : 0;
        progress = 50;

        QStringList errors;

        for (const BondInfo& bondInfo : bondsToDestroy)
        {
            try
            {
                setDescription(QString("Destroying bond on %1").arg(bondInfo.hostName));

                QString taskRef = XenAPI::Bond::async_destroy(session(), bondInfo.bondRef);
                pollToCompletion(taskRef, progress, progress + incr);

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

        setPercentComplete(90);

        // Step 3: Destroy the network (if it exists and is not used elsewhere)
        if (!bondsToDestroy.isEmpty())
        {
            QString networkRef = bondsToDestroy.first().networkRef;

            if (!networkRef.isEmpty() && networkRef != "OpaqueRef:NULL")
            {
                try
                {
                    setDescription("Destroying network");
                    XenAPI::Network::destroy(session(), networkRef);

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

        setPercentComplete(100);

        connection()->setExpectDisruption(false);

        if (!errors.isEmpty())
        {
            setError(QString("Bond destroyed with warnings: %1").arg(errors.join(", ")));
        } else
        {
            setDescription(QString("Bond '%1' destroyed").arg(m_bondName));
        }

    } catch (const std::exception& e)
    {
        connection()->setExpectDisruption(false);
        setError(QString("Failed to destroy bond: %1").arg(e.what()));
    }
}

QList<DestroyBondAction::BondInfo> DestroyBondAction::findAllEquivalentBonds() const
{
    QList<BondInfo> result;

    // Get the coordinator bond data to use as reference
    QVariantMap refBondData = connection()->getCache()->ResolveObjectData("bond", m_bondRef);
    QString refMasterPifRef = refBondData.value("master").toString();
    QVariantMap refMasterPifData = connection()->getCache()->ResolveObjectData("pif", refMasterPifRef);
    QString refDevice = refMasterPifData.value("device").toString();

    // Get all hosts in the pool
    QList<QVariantMap> hosts = connection()->getCache()->GetAll("host");

    for (const QVariantMap& host : hosts)
    {
        QString hostRef = host.value("_ref").toString();
        QString hostName = host.value("name_label").toString();

        // Find bond on this host with matching device name
        QList<QVariantMap> allBonds = connection()->getCache()->GetAll("bond");
        for (const QVariantMap& bond : allBonds)
        {
            QString bondRef = bond.value("_ref").toString();
            QString masterPifRef = bond.value("master").toString();
            QString primarySlaveRef = bond.value("primary_slave").toString();

            QVariantMap masterPifData = connection()->getCache()->ResolveObjectData("pif", masterPifRef);
            QString device = masterPifData.value("device").toString();
            QString bondHost = masterPifData.value("host").toString();
            QString networkRef = masterPifData.value("network").toString();

            // Match by device name and host
            if (device == refDevice && bondHost == hostRef)
            {
                BondInfo info;
                info.bondRef = bondRef;
                info.bondInterfaceRef = masterPifRef;
                info.primarySlaveRef = primarySlaveRef;
                info.networkRef = networkRef;
                info.hostRef = hostRef;
                info.hostName = hostName;

                result.append(info);
                break; // Only one bond per host with this device name
            }
        }
    }

    return result;
}
