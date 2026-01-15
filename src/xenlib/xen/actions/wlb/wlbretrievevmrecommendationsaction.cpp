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

#include "wlbretrievevmrecommendationsaction.h"
#include "../../network/connection.h"
#include "../../vm.h"
#include "../../host.h"
#include "../../pool.h"
#include "../../session.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../../xencache.h"
#include <QHash>
#include <QDebug>

WlbRetrieveVmRecommendationsAction::WlbRetrieveVmRecommendationsAction(XenConnection* connection, const QList<QSharedPointer<VM>>& vms, QObject* parent)
    : AsyncOperation(connection, "Retrieving WLB VM recommendations", "", parent), m_vms(vms)
{
    // Add required API role
    this->AddApiMethodToRoleCheck("vm.retrieve_wlb_recommendations");
}

QHash<QSharedPointer<VM>, QHash<QSharedPointer<Host>, QStringList>> WlbRetrieveVmRecommendationsAction::GetRecommendations() const
{
    return this->m_recommendations;
}

void WlbRetrieveVmRecommendationsAction::run()
{
    XenAPI::Session* session = GetSession();
    if (!session || !session->IsLoggedIn())
    {
        this->setError("Not connected to XenServer");
        return;
    }

    // Check if WLB is enabled on the connection
    // TODO: Implement Helpers::WlbEnabled() check
    // For now we'll attempt the call and handle errors
    bool wlbEnabled = false;
    
    // Get pool to check WLB configuration
    QSharedPointer<Pool> pool = GetConnection()->GetCache()->GetPool();
    if (pool && pool->IsValid())
    {
        QString wlbUrl = pool->WLBUrl();
        wlbEnabled = pool->IsWLBEnabled() && !wlbUrl.isEmpty();
    }

    if (!wlbEnabled)
    {
        qDebug() << "WLB is not enabled on this connection";
        return;
    }

    try
    {
        for (const QSharedPointer<VM>& vm : this->m_vms)
        {
            if (!vm || !vm->IsValid())
                continue;

            QString vmRef = vm->OpaqueRef();
            if (vmRef.isEmpty())
                continue;

            SetDescription(QString("Retrieving WLB recommendations for VM '%1'").arg(vm->GetName()));

            // Call XenAPI to retrieve recommendations
            QHash<QString, QStringList> hostRecommendations = XenAPI::VM::retrieve_wlb_recommendations(session, vmRef);

            // Convert host refs to Host* objects
            QHash<QSharedPointer<Host>, QStringList> recommendations;
            QHashIterator<QString, QStringList> it(hostRecommendations);
            while (it.hasNext())
            {
                it.next();
                QSharedPointer<Host> host = GetConnection()->GetCache()->ResolveObject<Host>("host", it.key());
                if (host && host->IsValid())
                {
                    recommendations[host] = it.value();
                }
            }

            this->m_recommendations[vm] = recommendations;
        }
    }
    catch (const std::exception& e)
    {
        setError(QString("Failed to retrieve WLB recommendations: %1").arg(e.what()));
    }
}
