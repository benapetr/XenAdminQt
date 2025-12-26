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
#include "../../network/connection.h"
#include "../../session.h"
#include "../../api.h"
#include <QDebug>

using namespace XenAPI;

ChangeHostAutostartAction::ChangeHostAutostartAction(XenConnection* connection,
                                                     const QString& hostRef,
                                                     bool enable,
                                                     bool suppressHistory,
                                                     QObject* parent)
    : AsyncOperation(connection,
                     QObject::tr("Change VM Autostart"),
                     QObject::tr("Changing VM autostart setting..."),
                     parent),
      m_hostRef(hostRef), m_enableAutostart(enable)
{
    setSuppressHistory(suppressHistory);
}

void ChangeHostAutostartAction::run()
{
    if (!connection() || !session())
    {
        qWarning() << "ChangeHostAutostartAction: No connection or session";
        return;
    }

    setPercentComplete(0);
    setDescription(tr("Updating VM autostart setting..."));

    try
    {
        // Get pool reference for this host
        // In C#: Pool p = Helpers.GetPoolOfOne(Connection);
        XenRpcAPI api(session());

        // First, get the host's pool reference
        QVariantList hostPoolParams;
        hostPoolParams << session()->getSessionId() << m_hostRef;

        QByteArray hostPoolRequest = api.buildJsonRpcCall("session.get_pool", hostPoolParams);
        QByteArray hostPoolResponse = connection()->SendRequest(hostPoolRequest);
        QVariant poolRefVariant = api.parseJsonRpcResponse(hostPoolResponse);
        QString poolRef = poolRefVariant.toString();

        if (poolRef.isEmpty())
        {
            qWarning() << "ChangeHostAutostartAction: Failed to get pool reference";
            return;
        }

        setPercentComplete(30);

        // Get current other_config
        QVariantList getConfigParams;
        getConfigParams << session()->getSessionId() << poolRef;

        QByteArray getConfigRequest = api.buildJsonRpcCall("pool.get_other_config", getConfigParams);
        QByteArray getConfigResponse = connection()->SendRequest(getConfigRequest);
        QVariant otherConfigVariant = api.parseJsonRpcResponse(getConfigResponse);
        QVariantMap otherConfig = otherConfigVariant.toMap();

        setPercentComplete(50);

        // Update auto_poweron value
        otherConfig["auto_poweron"] = m_enableAutostart ? "true" : "false";

        // Set the modified other_config back
        QVariantList setConfigParams;
        setConfigParams << session()->getSessionId() << poolRef << otherConfig;

        QByteArray setConfigRequest = api.buildJsonRpcCall("pool.set_other_config", setConfigParams);
        QByteArray setConfigResponse = connection()->SendRequest(setConfigRequest);

        // Parse response to check for errors
        api.parseJsonRpcResponse(setConfigResponse);

        setPercentComplete(100);
        setDescription(tr("VM autostart setting updated successfully"));

    } catch (const std::exception& e)
    {
        qWarning() << "ChangeHostAutostartAction failed:" << e.what();
        setDescription(tr("Failed to change VM autostart: %1").arg(e.what()));
    }
}
