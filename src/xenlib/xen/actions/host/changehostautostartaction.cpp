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

#include "changehostautostartaction.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/session.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/api.h"
#include "xenlib/xencache.h"
#include <QDebug>

using namespace XenAPI;

ChangeHostAutostartAction::ChangeHostAutostartAction(QSharedPointer<Host> host, bool enable, bool suppressHistory, QObject* parent)
    : AsyncOperation(QObject::tr("Change VM Autostart"), QObject::tr("Changing VM autostart setting..."), suppressHistory, parent),
      m_host(host), m_enableAutostart_(enable)
{
    if (!this->m_host || !this->m_host->IsValid())
    {
        qWarning() << "ChangeHostAutostartAction: Invalid host object";
        this->m_connection = nullptr;
    } else
    {
        this->m_connection = host->GetConnection();
    }
    this->AddApiMethodToRoleCheck("pool.get_all");
    this->AddApiMethodToRoleCheck("pool.set_other_config");
}

void ChangeHostAutostartAction::run()
{
    if (!this->GetConnection() || !this->GetSession())
    {
        qWarning() << "ChangeHostAutostartAction: No connection or session";
        return;
    }

    this->SetPercentComplete(0);
    this->SetDescription(this->tr("Updating VM autostart setting..."));

    try
    {
        QSharedPointer<Pool> pool = this->GetConnection()->GetCache()
            ? this->GetConnection()->GetCache()->GetPoolOfOne()
            : QSharedPointer<Pool>();
        if (pool.isNull())
        {
            this->setError(this->tr("Failed to locate pool configuration."));
            return;
        }

        this->SetPercentComplete(30);
        QVariantMap otherConfig = pool->GetOtherConfig();
        otherConfig["auto_poweron"] = this->m_enableAutostart_ ? "true" : "false";

        XenRpcAPI api(this->GetSession());
        QVariantList setConfigParams;
        setConfigParams << GetSession()->GetSessionID() << pool->OpaqueRef() << otherConfig;

        QByteArray setConfigRequest = api.BuildJsonRpcCall("pool.set_other_config", setConfigParams);
        QByteArray setConfigResponse = this->GetConnection()->SendRequest(setConfigRequest);
        api.ParseJsonRpcResponse(setConfigResponse);

        QVariantMap poolData = pool->GetData();
        poolData["other_config"] = otherConfig;
        pool->SetLocalData(poolData);
        pool->Refresh();

        this->SetPercentComplete(100);
        this->SetDescription(this->tr("VM autostart setting updated successfully"));

    } catch (const std::exception& e)
    {
        qWarning() << "ChangeHostAutostartAction failed:" << e.what();
        this->setError(this->tr("Failed to change VM autostart: %1").arg(e.what()));
    }
}
