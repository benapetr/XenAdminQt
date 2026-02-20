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

#include "setsecondarymanagementpurposeaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../pif.h"
#include "../../host.h"
#include "../../pool.h"
#include "../../network.h"
#include "../../xenapi/xenapi_PIF.h"
#include <QDebug>

SetSecondaryManagementPurposeAction::SetSecondaryManagementPurposeAction(XenConnection* connection,
                                                                         const QSharedPointer<Pool>& pool,
                                                                         const QList<QSharedPointer<PIF>>& pifs,
                                                                         QObject* parent)
    : AsyncOperation(connection,
                     "Set Secondary Management Purpose",
                     "Updating secondary management interface purpose",
                     parent),
      m_pool(pool),
      m_pifs(pifs)
{
    // RBAC dependencies (matches C# SetSecondaryManagementPurposeAction)
    this->AddApiMethodToRoleCheck("pif.set_other_config");
}

void SetSecondaryManagementPurposeAction::run()
{
    try
    {
        XenAPI::Session* session = this->GetSession();
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("No active session");

        for (const QSharedPointer<PIF>& pif : this->m_pifs)
        {
            if (!pif || !pif->IsValid())
                continue;

            QSharedPointer<Network> network = pif->GetNetwork();
            if (!network || !network->IsValid())
            {
                qWarning() << "Network has gone away for PIF" << pif->OpaqueRef();
                return;
            }

            QList<QSharedPointer<PIF>> allPifs = network->GetPIFs();
            QList<QSharedPointer<PIF>> targets;
            if (this->m_pool)
            {
                targets = allPifs;
            }
            else
            {
                for (const QSharedPointer<PIF>& candidate : allPifs)
                {
                    if (!candidate || !candidate->IsValid())
                        continue;
                    QSharedPointer<Host> host = candidate->GetHost();
                    QSharedPointer<Host> sourceHost = pif->GetHost();
                    if (host && sourceHost && host->OpaqueRef() == sourceHost->OpaqueRef())
                        targets.append(candidate);
                }
            }

            if (targets.isEmpty())
                return;

            const QVariantMap otherConfig = pif->GetOtherConfig();
            const QString purpose = otherConfig.value("management_purpose").toString();

            for (const QSharedPointer<PIF>& target : targets)
            {
                if (!target || !target->IsValid())
                    continue;
                if (purpose.isEmpty())
                    XenAPI::PIF::remove_from_other_config(session, target->OpaqueRef(), "management_purpose");
                else
                    XenAPI::PIF::add_to_other_config(session, target->OpaqueRef(), "management_purpose", purpose);
            }
        }

        this->SetDescription("Secondary management purpose updated");
    }
    catch (const std::exception& e)
    {
        this->setError(QString("Failed to update management purpose: %1").arg(e.what()));
    }
}
