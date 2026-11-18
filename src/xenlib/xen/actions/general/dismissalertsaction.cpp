/*
 * Copyright (c) 2026, Petr Bena <petr@bena.rocks>
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

#include "dismissalertsaction.h"
#include "../../network/connection.h"
#include "../../../xencache.h"
#include "../../../utils/misc.h"
#include "../../failure.h"
#include "../../host.h"
#include "../../pool.h"
#include "../../xenapi/xenapi_Message.h"

DismissAlertsAction::DismissAlertsAction(XenConnection* connection, const QStringList& messageRefs, QObject* parent)
    : AsyncOperation(connection, actionTitle(connection, messageRefs.size()), tr("Removing alerts..."), parent),
      m_messageRefs(messageRefs)
{
    this->AddApiMethodToRoleCheck("message.destroy");

    XenCache* cache = connection ? connection->GetCache() : nullptr;
    QSharedPointer<Pool> pool = cache ? cache->GetPoolOfOne() : QSharedPointer<Pool>();
    QSharedPointer<Host> coordinator = pool ? pool->GetMasterHost() : QSharedPointer<Host>();
    if (pool)
        this->SetPool(pool);
    if (coordinator)
        this->SetHost(coordinator);

    // C# Reference: Helpers.XapiEqualOrGreater_22_19_0 (true when the coordinator is unknown)
    this->m_canDestroyMany = !coordinator || !coordinator->IsValid()
        || Misc::ProductVersionCompare(coordinator->GetXapiVersion(), QStringLiteral("22.19.0")) >= 0;
}

QString DismissAlertsAction::actionTitle(XenConnection* connection, int alertCount)
{
    QString name;
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    QSharedPointer<Pool> pool = cache ? cache->GetPoolOfOne() : QSharedPointer<Pool>();
    if (pool && pool->IsValid())
        name = pool->GetName();
    if (name.isEmpty() && connection)
        name = connection->GetHostname();

    return alertCount == 1
        ? tr("Removing alert on '%1'").arg(name)
        : tr("Removing %1 alerts on '%2'").arg(alertCount).arg(name);
}

void DismissAlertsAction::run()
{
    XenAPI::Session* session = this->GetSession();
    const int count = this->m_messageRefs.size();

    if (count > 0)
    {
        if (this->m_canDestroyMany)
        {
            const QString taskRef = XenAPI::Message::async_destroy_many(session, this->m_messageRefs);
            this->pollToCompletion(taskRef, 0, 100);
        } else
        {
            for (int i = 0; i < count; ++i)
            {
                this->SetDescription(tr("Removing alert %1 of %2...").arg(i).arg(count));
                try
                {
                    XenAPI::Message::destroy(session, this->m_messageRefs.at(i));
                } catch (const Failure& failure)
                {
                    // C# Reference: MessageAlert.Dismiss() - the message is already gone
                    if (failure.errorCode() != Failure::HANDLE_INVALID)
                        throw;
                }
                this->SetPercentComplete(i * 100 / count);
            }
        }
    }

    this->SetDescription(count == 1 ? tr("Removed 1 alert") : tr("Removed %1 alerts").arg(count));
}
