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

#include "setpoolpropertyaction.h"
#include "../../xenapi/xenapi_Pool.h"
#include "../../session.h"
#include "xen/network/connection.h"
#include "xen/pool.h"
#include <QDebug>

SetPoolPropertyAction::SetPoolPropertyAction(QSharedPointer<Pool> pool,
                                             const QString& propertyName,
                                             const QVariant& value,
                                             const QString& description,
                                             QObject* parent)
    : AsyncOperation(description, description, parent)
    , m_pool(pool)
    , m_propertyName(propertyName)
    , m_value(value)
{
    if (!this->m_pool || !this->m_pool->IsValid())
    {
        qWarning() << "SetPoolPropertyAction: Invalid pool object";
    }

    this->m_connection = pool->GetConnection();
}

void SetPoolPropertyAction::run()
{
    try
    {
        XenAPI::Session* session = this->GetSession();
        if (!session || !session->IsLoggedIn())
        {
            setError(tr("Not connected to XenServer"));
            return;
        }
        if (!this->m_pool || !this->m_pool->IsValid())
        {
            setError(tr("Invalid pool object"));
            return;
        }

        QString poolRef = this->m_pool->OpaqueRef();
        qDebug() << "SetPoolPropertyAction: Setting" << this->m_propertyName
                 << "poolRef=" << poolRef
                 << "value=" << this->m_value;
        if (!this->m_connection || !this->m_connection->IsConnected())
        {
            qWarning() << "SetPoolPropertyAction: Connection not ready for" << this->m_propertyName
                       << "poolRef=" << poolRef
                       << "connected=" << (this->m_connection ? this->m_connection->IsConnected() : false);
        }

        // Call the appropriate Pool.set_* method based on property name
        if (this->m_propertyName == "migration_compression")
        {
            XenAPI::Pool::set_migration_compression(session, poolRef, this->m_value.toBool());
        }
        else if (this->m_propertyName == "live_patching_disabled")
        {
            XenAPI::Pool::set_live_patching_disabled(session, poolRef, this->m_value.toBool());
        }
        else if (this->m_propertyName == "igmp_snooping_enabled")
        {
            XenAPI::Pool::set_igmp_snooping_enabled(session, poolRef, this->m_value.toBool());
        }
        else
        {
            setError(tr("Unknown pool property: %1").arg(this->m_propertyName));
            return;
        }

    }
    catch (const std::exception& e)
    {
        setError(QString("Failed to set pool property: %1").arg(e.what()));
    }
}
