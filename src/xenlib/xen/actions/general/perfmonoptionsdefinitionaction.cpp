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

#include "perfmonoptionsdefinitionaction.h"
#include "../../network/connection.h"
#include "../../pool.h"
#include "../../xenapi/xenapi_Pool.h"
#include "../../../xencache.h"

PerfmonOptionsDefinitionAction::PerfmonOptionsDefinitionAction(XenConnection* connection,
                                                               const QString& mailDestination,
                                                               const QString& mailHub,
                                                               const QString& mailLanguageCode,
                                                               bool suppressHistory,
                                                               QObject* parent)
    : AsyncOperation(connection, tr("Change email notifications"), QString(), suppressHistory, parent),
      m_mailDestination(mailDestination),
      m_mailHub(mailHub),
      m_mailLanguageCode(mailLanguageCode)
{
    QSharedPointer<Pool> pool = connection && connection->GetCache() ? connection->GetCache()->GetPoolOfOne() : QSharedPointer<Pool>();
    if (pool)
    {
        this->m_poolRef = pool->OpaqueRef();
        this->SetPool(pool);
    }
    this->SetDescription(tr("Changing email notifications for '%1'...").arg(pool ? pool->GetName() : QString()));

    this->AddApiMethodToRoleCheck("pool.remove_from_other_config");
    if (!this->m_mailDestination.isEmpty() && !this->m_mailHub.isEmpty())
        this->AddApiMethodToRoleCheck("pool.add_to_other_config");
}

void PerfmonOptionsDefinitionAction::run()
{
    if (this->m_poolRef.isEmpty())
        return;

    XenAPI::Session* session = this->GetSession();
    const auto replace = [&](const char* key, const QString& value)
    {
        XenAPI::Pool::remove_from_other_config(session, this->m_poolRef, QString::fromLatin1(key));
        if (!value.isEmpty())
            XenAPI::Pool::add_to_other_config(session, this->m_poolRef, QString::fromLatin1(key), value);
    };

    try
    {
        replace(Pool::MAIL_DESTINATION_KEY_NAME, this->m_mailDestination);
        replace(Pool::SMTP_MAILHUB_KEY_NAME, this->m_mailHub);
        replace(Pool::MAIL_LANGUAGE_KEY_NAME, this->m_mailLanguageCode);
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}
