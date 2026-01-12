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

#include "setssllegacyaction.h"
#include "../../xenapi/xenapi_Pool.h"
#include "../../session.h"
#include "xen/pool.h"
#include <QDebug>

SetSslLegacyAction::SetSslLegacyAction(QSharedPointer<Pool> pool, bool enableSslLegacy, QObject* parent)
    : AsyncOperation(enableSslLegacy ? "Enabling SSL legacy protocol" : "Enabling TLS verification",
                     enableSslLegacy ? "Enabling SSL legacy protocol..." : "Enabling TLS verification...",
                     parent)
    , m_pool(pool)
    , m_enableSslLegacy_(enableSslLegacy)
{
    if (!this->m_pool || !this->m_pool->IsValid())
        qWarning() << "SetSslLegacyAction: Invalid pool object";
    this->m_connection = pool->GetConnection();
}

void SetSslLegacyAction::run()
{
    try
    {
        if (!this->m_pool || !this->m_pool->IsValid())
        {
            setError(tr("Invalid pool object"));
            return;
        }

        XenAPI::Session* session = this->GetSession();
        if (!session || !session->IsLoggedIn())
        {
            setError(tr("Not connected to XenServer"));
            return;
        }

        // Set ssl-legacy in pool other_config
        XenAPI::Pool::set_ssl_legacy(session, this->m_pool->OpaqueRef(), this->m_enableSslLegacy_);

    }
    catch (const std::exception& e)
    {
        setError(QString("Failed to set SSL legacy mode: %1").arg(e.what()));
    }
}
