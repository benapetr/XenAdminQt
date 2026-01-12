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

#include "savepoweronsettingsaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../api.h"
#include "../../../utils/misc.h"
#include <QDebug>

using namespace XenAPI;

// PowerOnMode toString implementation
QString PowerOnMode::toString() const
{
    switch (this->type) {
        case Disabled: return "";
        case WakeOnLAN: return "wake-on-lan";
        case iLO: return "iLO";
        case DRAC: return "DRAC";
        case Custom: return this->customMode;
    }
    return "";
}

SavePowerOnSettingsAction::SavePowerOnSettingsAction(XenConnection* connection,
                                                     const QList<QPair<QString, PowerOnMode>>& hostModes,
                                                     QObject* parent)
    : AsyncOperation(connection, QObject::tr("Change Power-On Mode"),
                     hostModes.size() == 1
                         ? QObject::tr("Changing power-on mode")
                         : QObject::tr("Changing power-on mode for %1 hosts").arg(hostModes.size()),
                     parent)
    , m_hostModes(hostModes)
{
}

void SavePowerOnSettingsAction::run()
{
    XenConnection* conn = this->GetConnection();
    if (!conn || !conn->GetSession()) {
        this->setError(tr("Not connected to server"));
        return;
    }
    
    int current = 0;
    int total = this->m_hostModes.size();
    
    for (const QPair<QString, PowerOnMode>& pair : this->m_hostModes)
    {
        if (this->IsCancelled())
        {
            this->setError(tr("Cancelled"));
            return;
        }
        
        QString hostRef = pair.first;
        PowerOnMode mode = pair.second;
        
        try
        {
            this->saveHostConfig(hostRef, mode);
        } catch (const std::exception& e) {
            this->setError(tr("Failed to set power-on mode: %1").arg(e.what()));
            return;
        }
        
        current++;
        this->SetPercentComplete(current * 100 / total);
    }
}

void SavePowerOnSettingsAction::saveHostConfig(const QString& hostRef, const PowerOnMode& mode)
{
    XenConnection* conn = this->GetConnection();
    Session* session = conn->GetSession();
    XenRpcAPI api(session);
    
    QString modeString = mode.toString();
    QMap<QString, QString> config;
    
    // Build configuration based on mode type
    if (mode.type == PowerOnMode::iLO || mode.type == PowerOnMode::DRAC)
    {
        config["power_on_ip"] = mode.ipAddress;
        config["power_on_user"] = mode.username;
        
        if (!mode.password.isEmpty())
        {
            // Create secret for password
            QString secretUuid = this->createSecret(mode.password);
            config["power_on_password_secret"] = secretUuid;
        }
    } else if (mode.type == PowerOnMode::Custom)
    {
        // Copy custom config
        for (auto it = mode.customConfig.begin(); it != mode.customConfig.end(); ++it)
        {
            config[it.key()] = it.value();
        }
        
        // Check if password secret is needed
        if (config.contains("power_on_password_secret"))
        {
            QString password = config["power_on_password_secret"];
            QString secretUuid = this->createSecret(password);
            config["power_on_password_secret"] = secretUuid;
        }
    }
    
    // Convert QMap to QVariantMap for API call
    QVariantMap configMap;
    for (auto it = config.begin(); it != config.end(); ++it)
    {
        configMap[it.key()] = it.value();
    }
    
    // Call Host.set_power_on_mode
    QVariantList params;
    params << session->getSessionId();
    params << hostRef;
    params << modeString;
    params << configMap;
    
    QByteArray request = api.BuildJsonRpcCall("host.set_power_on_mode", params);
    QByteArray response = conn->SendRequest(request);
    
    QVariant result = api.ParseJsonRpcResponse(response);
    if (Misc::QVariantIsMap(result))
    {
        QVariantMap resultMap = result.toMap();
        if (resultMap.value("Status").toString() != "Success")
        {
            QString error = resultMap.value("ErrorDescription").toStringList().join(": ");
            throw std::runtime_error(error.toStdString());
        }
    }
}

QString SavePowerOnSettingsAction::createSecret(const QString& value)
{
    XenConnection* conn = this->GetConnection();
    Session* session = conn->GetSession();
    XenRpcAPI api(session);
    
    // Create a secret to store the password
    QVariantMap secretRecord;
    secretRecord["value"] = value;
    secretRecord["other_config"] = QVariantMap();
    
    QVariantList params;
    params << session->getSessionId();
    params << secretRecord;
    
    QByteArray request = api.BuildJsonRpcCall("secret.create", params);
    QByteArray response = conn->SendRequest(request);
    
    QVariant result = api.ParseJsonRpcResponse(response);
    if (Misc::QVariantIsMap(result))
    {
        QVariantMap resultMap = result.toMap();
        if (resultMap.value("Status").toString() == "Success")
        {
            QString secretRef = resultMap.value("Value").toString();
            
            // Get the UUID of the secret
            params.clear();
            params << session->getSessionId();
            params << secretRef;
            
            request = api.BuildJsonRpcCall("secret.get_uuid", params);
            response = conn->SendRequest(request);
            
            result = api.ParseJsonRpcResponse(response);
            if (Misc::QVariantIsMap(result))
            {
                resultMap = result.toMap();
                if (resultMap.value("Status").toString() == "Success")
                    return resultMap.value("Value").toString();
            }
        }
    }
    
    throw std::runtime_error("Failed to create secret");
}

void SavePowerOnSettingsAction::destroySecret(const QString& secretRef)
{
    XenConnection* conn = this->GetConnection();
    Session* session = conn->GetSession();
    XenRpcAPI api(session);
    
    QVariantList params;
    params << session->getSessionId();
    params << secretRef;
    
    QByteArray request = api.BuildJsonRpcCall("secret.destroy", params);
    conn->SendRequest(request);
    // Ignore errors when destroying secrets
}
