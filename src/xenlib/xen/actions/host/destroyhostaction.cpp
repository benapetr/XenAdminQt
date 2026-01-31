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
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/session.h"
#include "xenlib/xen/pbd.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/xenapi/xenapi_Host.h"
#include "xenlib/xen/xenapi/xenapi_SR.h"
#include "xenlib/xencache.h"
#include <QDebug>
#include <QThread>

DestroyHostAction::DestroyHostAction(QSharedPointer<Host> host, QObject* parent)
    : AsyncOperation(host->GetConnection(),
                     QString("Removing host '%1'").arg(host ? host->GetName() : ""),
                     "Removing host from pool",
                     parent),
      m_host(host)
{
    if (!this->m_host)
        throw std::invalid_argument("Host cannot be null");
    this->m_pool = this->m_host->GetPool();
    if (!this->m_pool)
        throw std::invalid_argument("Pool cannot be null");
    this->AddApiMethodToRoleCheck("host.destroy");
    this->AddApiMethodToRoleCheck("sr.forget");
}

void DestroyHostAction::run()
{
    try
    {
        this->SetDescription("Removing host from pool");

        // Get all local SRs belonging to this host
        QList<QSharedPointer<SR>> allSRs = this->m_host->GetCache()->GetAll<SR>();
        QStringList localSRRefs;

        for (const QSharedPointer<SR>& sr : allSRs)
        {
            if (!sr || !sr->IsValid())
                continue;

            QString srRef = sr->OpaqueRef();

            // Check if SR is local and belongs to this host
            // C# checks: sr.GetStorageHost() == host && sr.IsLocalSR()
            bool isLocal = sr->IsLocal();
            // Check if SR's PBDs are only connected to this host
            QList<QSharedPointer<PBD>> pbds = sr->GetPBDs();
            bool belongsToThisHost = false;

            for (const QSharedPointer<PBD>& pbd : pbds)
            {
                if (!pbd || !pbd->IsValid())
                    continue;

                QString pbdHost = pbd->GetHostRef();

                if (pbdHost == this->m_host->OpaqueRef())
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
        QString taskRef = XenAPI::Host::async_destroy(GetSession(), m_host->OpaqueRef());
        pollToCompletion(taskRef, 0, (int) p);
        i++;

        if (!localSRRefs.isEmpty())
        {
            SetDescription("Removing storage repositories");

            // Forget each local SR
            for (const QString& srRef : localSRRefs)
            {
                // Wait for SR to be detached (up to 2 minutes)
                if (!isSRDetached(srRef))
                {
                    SetDescription("Completed - some storage repositories could not be removed");
                    return;
                }

                int lo = (int) (i * p);
                int hi = (int) ((i + 1) * p);

                QString taskRef = XenAPI::SR::async_forget(GetSession(), srRef);
                pollToCompletion(taskRef, lo, hi);
                i++;
            }
        }

        SetDescription("Completed");

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
        QSharedPointer<SR> sr = GetConnection()->GetCache()->ResolveObject<SR>(srRef);
        QList<QSharedPointer<PBD>> pbds = sr ? sr->GetPBDs() : QList<QSharedPointer<PBD>>();

        if (pbds.isEmpty())
        {
            return true; // All PBDs detached
        }

        // Check if any PBDs are still attached
        bool anyAttached = false;
        for (const QSharedPointer<PBD>& pbd : pbds)
        {
            if (!pbd || !pbd->IsValid())
                continue;

            bool currentlyAttached = pbd->IsCurrentlyAttached();

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
    QSharedPointer<SR> sr = GetConnection()->GetCache()->ResolveObject<SR>(srRef);
    QList<QSharedPointer<PBD>> pbds = sr ? sr->GetPBDs() : QList<QSharedPointer<PBD>>();
    return pbds.isEmpty();
}
