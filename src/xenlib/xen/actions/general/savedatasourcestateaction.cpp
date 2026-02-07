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

#include "savedatasourcestateaction.h"
#include "../../network/connection.h"
#include "../../../xencache.h"
#include "../../pool.h"
#include "../../xenapi/xenapi_Host.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../xenapi/xenapi_Pool.h"

SaveDataSourceStateAction::SaveDataSourceStateAction(XenConnection* connection,
                                                     XenObjectType objectType,
                                                     const QString& objectRef,
                                                     const QList<QVariantMap>& dataSourceStates,
                                                     const QVariantMap& newGuiConfig,
                                                     QObject* parent)
    : AsyncOperation(connection, "Save data source state", "Saving performance graph settings...", true, parent),
      m_objectType(objectType),
      m_objectRef(objectRef),
      m_dataSourceStates(dataSourceStates),
      m_newGuiConfig(newGuiConfig)
{
    if (this->m_objectType == XenObjectType::Host)
    {
        this->AddApiMethodToRoleCheck("host.record_data_source");
        this->AddApiMethodToRoleCheck("host.forget_data_source_archives");
    } else if (this->m_objectType == XenObjectType::VM)
    {
        this->AddApiMethodToRoleCheck("VM.record_data_source");
        this->AddApiMethodToRoleCheck("VM.forget_data_source_archives");
    }
    this->AddApiMethodToRoleCheck("pool.set_gui_config");
}

void SaveDataSourceStateAction::run()
{
    for (const QVariantMap& state : this->m_dataSourceStates)
    {
        const QString nameLabel = state.value(QStringLiteral("name_label")).toString();
        const bool currentEnabled = state.value(QStringLiteral("current_enabled")).toBool();
        const bool desiredEnabled = state.value(QStringLiteral("desired_enabled")).toBool();

        if (nameLabel.isEmpty() || currentEnabled == desiredEnabled)
            continue;

        if (this->m_objectType == XenObjectType::Host)
        {
            if (desiredEnabled)
                XenAPI::Host::record_data_source(this->GetSession(), this->m_objectRef, nameLabel);
            else
                XenAPI::Host::forget_data_source_archives(this->GetSession(), this->m_objectRef, nameLabel);
        } else if (this->m_objectType == XenObjectType::VM)
        {
            if (desiredEnabled)
                XenAPI::VM::record_data_source(this->GetSession(), this->m_objectRef, nameLabel);
            else
                XenAPI::VM::forget_data_source_archives(this->GetSession(), this->m_objectRef, nameLabel);
        }
    }

    XenConnection* connection = this->GetConnection();
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    QSharedPointer<Pool> pool = cache ? cache->GetPoolOfOne() : QSharedPointer<Pool>();
    if (!pool)
        return;

    XenAPI::Pool::set_gui_config(this->GetSession(), pool->OpaqueRef(), this->m_newGuiConfig);
}
