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

#include "createsriovaction.h"
#include "../../connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_Network.h"
#include "../../xenapi/xenapi_Network_sriov.h"
#include "../../xenapi/xenapi_PIF.h"
#include "../../../xencache.h"
#include <QDebug>

CreateSriovAction::CreateSriovAction(XenConnection* connection,
                                     const QString& networkName,
                                     const QStringList& pifRefs,
                                     QObject* parent)
    : AsyncOperation(connection,
                     "Creating SR-IOV Network",
                     QString("Creating SR-IOV network '%1'").arg(networkName),
                     parent),
      m_networkName(networkName),
      m_pifRefs(pifRefs)
{
    if (m_pifRefs.isEmpty())
        throw std::invalid_argument("PIF list cannot be empty");
}

void CreateSriovAction::run()
{
    try
    {
        if (m_pifRefs.isEmpty())
        {
            setError("No PIFs selected for SR-IOV");
            return;
        }

        // Find coordinator PIF and put it first (if present)
        QString coordinatorPifRef;
        for (const QString& pifRef : m_pifRefs)
        {
            QVariantMap pifData = connection()->getCache()->resolve("pif", pifRef);
            QString hostRef = pifData.value("host").toString();
            QVariantMap hostData = connection()->getCache()->resolve("host", hostRef);

            // Check if this host is the pool coordinator
            // In C# this is done via host.IsCoordinator()
            // We can check if this host is the master in the pool
            QList<QVariantMap> pools = connection()->getCache()->getAll("pool");
            if (!pools.isEmpty())
            {
                QString masterRef = pools.first().value("master").toString();

                if (hostRef == masterRef)
                {
                    coordinatorPifRef = pifRef;
                    break;
                }
            }
        }

        // Reorder PIFs to enable coordinator first
        QStringList orderedPifs = m_pifRefs;
        if (!coordinatorPifRef.isEmpty())
        {
            orderedPifs.removeOne(coordinatorPifRef);
            orderedPifs.prepend(coordinatorPifRef);
        }

        int inc = 100 / orderedPifs.count();
        int progress = 0;

        // Create the network
        QVariantMap networkRecord;
        networkRecord["name_label"] = m_networkName;
        networkRecord["name_description"] = "SR-IOV Network";
        networkRecord["managed"] = true;

        setDescription(QString("Creating network '%1'").arg(m_networkName));
        QString networkRef = XenAPI::Network::async_create(session(), networkRecord);
        pollToCompletion(networkRef, 0, 10);
        networkRef = result();

        setDescription("Enabling SR-IOV on network interfaces");

        // Enable SR-IOV on each PIF
        for (int i = 0; i < orderedPifs.count(); ++i)
        {
            const QString& pifRef = orderedPifs[i];

            QVariantMap pifData = connection()->getCache()->resolve("pif", pifRef);
            QString hostRef = pifData.value("host").toString();
            QVariantMap hostData = connection()->getCache()->resolve("host", hostRef);
            QString hostName = hostData.value("name_label").toString();

            setDescription(QString("Enabling SR-IOV on %1").arg(hostName));

            try
            {
                QString taskRef = XenAPI::Network_sriov::async_create(session(), pifRef, networkRef);
                pollToCompletion(taskRef, 10 + progress, 10 + progress + inc);
                progress += inc;

            } catch (const std::exception& e)
            {
                // If this is the first PIF (coordinator), destroy the network and fail
                if (i == 0)
                {
                    qWarning() << "Failed to enable SR-IOV on coordinator, destroying network:" << e.what();
                    try
                    {
                        XenAPI::Network::destroy(session(), networkRef);
                    } catch (...)
                    {
                        // Ignore cleanup errors
                    }
                    throw;
                }
                // For other PIFs, log but continue
                qWarning() << "Failed to enable SR-IOV on" << hostName << ":" << e.what();
            }
        }

        setDescription(QString("SR-IOV network '%1' created").arg(m_networkName));

    } catch (const std::exception& e)
    {
        setError(QString("Failed to create SR-IOV network: %1").arg(e.what()));
    }
}
