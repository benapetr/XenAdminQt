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

#include "destroyhostaction.h"
#include "../../connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_Host.h"
#include "../../xenapi/xenapi_SR.h"
#include "../../../xencache.h"
#include <QDebug>
#include <QThread>

DestroyHostAction::DestroyHostAction(XenConnection* connection,
                                     Pool* pool,
                                     Host* host,
                                     QObject* parent)
    : AsyncOperation(connection,
                     QString("Removing host '%1'").arg(host ? host->nameLabel() : ""),
                     "Removing host from pool",
                     parent),
      m_pool(pool),
      m_host(host)
{
    if (!m_host)
        throw std::invalid_argument("Host cannot be null");
    if (!m_pool)
        throw std::invalid_argument("Pool cannot be null");
}

void DestroyHostAction::run()
{
    try
    {
        setDescription("Removing host from pool");

        // Get all local SRs belonging to this host
        QList<QVariantMap> allSRs = connection()->getCache()->GetAllData("sr");
        QStringList localSRRefs;

        for (const QVariantMap& srData : allSRs)
        {
            QString srRef = srData.value("_ref").toString();

            // Check if SR is local and belongs to this host
            // C# checks: sr.GetStorageHost() == host && sr.IsLocalSR()
            bool isLocal = srData.value("shared", false).toBool() == false;
            QString srType = srData.value("type").toString();

            // Check if SR's PBDs are only connected to this host
            QVariantList pbdRefs = srData.value("PBDs").toList();
            bool belongsToThisHost = false;

            for (const QVariant& pbdVar : pbdRefs)
            {
                QString pbdRef = pbdVar.toString();
                QVariantMap pbdData = connection()->getCache()->ResolveObjectData("pbd", pbdRef);
                QString pbdHost = pbdData.value("host").toString();

                if (pbdHost == m_host->opaqueRef())
                {
                    belongsToThisHost = true;
                    break;
                }
            }

            if (isLocal && belongsToThisHost)
            {
                localSRRefs.append(srRef);
            }
        }

        // Number of operations: 1 (destroy host) + N (forget SRs)
        int n = 1 + localSRRefs.size();
        double p = 100.0 / n;
        int i = 0;

        // Destroy the host
        QString taskRef = XenAPI::Host::async_destroy(session(), m_host->opaqueRef());
        pollToCompletion(taskRef, 0, (int) p);
        i++;

        if (!localSRRefs.isEmpty())
        {
            setDescription("Removing storage repositories");

            // Forget each local SR
            for (const QString& srRef : localSRRefs)
            {
                // Wait for SR to be detached (up to 2 minutes)
                if (!isSRDetached(srRef))
                {
                    setDescription("Completed - some storage repositories could not be removed");
                    return;
                }

                int lo = (int) (i * p);
                int hi = (int) ((i + 1) * p);

                QString taskRef = XenAPI::SR::async_forget(session(), srRef);
                pollToCompletion(taskRef, lo, hi);
                i++;
            }
        }

        setDescription("Completed");

    } catch (const std::exception& e)
    {
        setError(QString("Failed to destroy host: %1").arg(e.what()));
    }
}

bool DestroyHostAction::isSRDetached(const QString& srRef)
{
    // Wait up to 2 minutes for all SR's PBDs to detach
    const int maxSeconds = 2 * 60;

    for (int i = 0; i < maxSeconds; i++)
    {
        QVariantMap srData = connection()->getCache()->ResolveObjectData("sr", srRef);
        QVariantList pbdRefs = srData.value("PBDs").toList();

        if (pbdRefs.isEmpty())
        {
            return true; // All PBDs detached
        }

        // Check if any PBDs are still attached
        bool anyAttached = false;
        for (const QVariant& pbdVar : pbdRefs)
        {
            QString pbdRef = pbdVar.toString();
            QVariantMap pbdData = connection()->getCache()->ResolveObjectData("pbd", pbdRef);
            bool currentlyAttached = pbdData.value("currently_attached", false).toBool();

            if (currentlyAttached)
            {
                anyAttached = true;
                break;
            }
        }

        if (!anyAttached)
        {
            return true; // All PBDs detached
        }

        // Wait 1 second before checking again
        QThread::msleep(1000);
    }

    // Timeout - check one last time
    QVariantMap srData = connection()->getCache()->ResolveObjectData("sr", srRef);
    QVariantList pbdRefs = srData.value("PBDs").toList();
    return pbdRefs.isEmpty();
}
