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

#include "nrpeactions.h"
#include "../../host.h"
#include "../../pool.h"
#include "../../network/connection.h"
#include "../../xenapi/xenapi_Host.h"
#include "../../../xencache.h"
#include <QDebug>

NRPERetrieveAction::NRPERetrieveAction(QSharedPointer<XenObject> hostOrPool, bool suppressHistory, QObject* parent)
    : AsyncOperation(hostOrPool->GetConnection(), tr("Retrieving NRPE configuration"), tr("Retrieving NRPE configuration"),
                     suppressHistory, parent)
{
    // C#: a pool's configuration is read from its coordinator
    if (hostOrPool->GetObjectType() == XenObjectType::Pool)
    {
        QSharedPointer<Host> coordinator = Host::GetCoordinator(hostOrPool->GetConnection());
        this->m_hostRef = coordinator ? coordinator->OpaqueRef() : QString();
    } else
    {
        this->m_hostRef = hostOrPool->OpaqueRef();
    }
    this->AddApiMethodToRoleCheck("Host.call_plugin");
}

void NRPERetrieveAction::run()
{
    try
    {
        XenAPI::Session* session = this->GetSession();
        const QString status = XenAPI::Host::call_plugin(session, this->m_hostRef, NRPEHostConfiguration::PLUGIN_NAME, "status", QVariantMap());
        this->m_configuration.enableNRPE = status.trimmed() == "active enabled";

        this->m_configuration.ParseGeneralConfiguration(
            XenAPI::Host::call_plugin(session, this->m_hostRef, NRPEHostConfiguration::PLUGIN_NAME, "get-config", QVariantMap()));
        this->m_configuration.ParseThresholds(
            XenAPI::Host::call_plugin(session, this->m_hostRef, NRPEHostConfiguration::PLUGIN_NAME, "get-threshold", QVariantMap()));
        this->m_succeeded = true;
    } catch (const std::exception& e)
    {
        qWarning() << "Run NRPE plugin failed, failed reason:" << e.what();
        this->m_succeeded = false;
    }
}

NRPEUpdateAction::NRPEUpdateAction(QSharedPointer<XenObject> hostOrPool, const NRPEHostConfiguration& configuration,
                                   const NRPEHostConfiguration& original, bool suppressHistory, QObject* parent)
    : AsyncOperation(hostOrPool->GetConnection(), tr("Changing NRPE configuration"), tr("Changing NRPE configuration"),
                     suppressHistory, parent),
      m_configuration(configuration),
      m_original(original)
{
    // C#: a pool's settings go to each of its hosts
    if (hostOrPool->GetObjectType() == XenObjectType::Pool)
    {
        XenCache* cache = hostOrPool->GetCache();
        for (const QSharedPointer<Host>& host : cache ? cache->GetAll<Host>(XenObjectType::Host) : QList<QSharedPointer<Host>>())
        {
            if (host)
                this->m_hostRefs << host->OpaqueRef();
        }
    } else
    {
        this->m_hostRefs << hostOrPool->OpaqueRef();
    }
    this->AddApiMethodToRoleCheck("Host.call_plugin");
}

QString NRPEUpdateAction::callPlugin(const QString& hostRef, const QString& function, const QVariantMap& args)
{
    const QString result = XenAPI::Host::call_plugin(this->GetSession(), hostRef, NRPEHostConfiguration::PLUGIN_NAME, function, args);
    qInfo() << "Run NRPE" << function << "return:" << result;
    return result;
}

void NRPEUpdateAction::run()
{
    try
    {
        for (const QString& hostRef : this->m_hostRefs)
            this->configureHost(hostRef);
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}

void NRPEUpdateAction::configureHost(const QString& hostRef)
{
    const NRPEHostConfiguration& config = this->m_configuration;
    const NRPEHostConfiguration& original = this->m_original;

    if (config.enableNRPE != original.enableNRPE)
    {
        if (config.enableNRPE)
        {
            // C# workaround for CA-384579: "control" fixes the permissions of certificates made by
            // older versions, which otherwise make enable fail; its own failure is only logged
            try
            {
                this->callPlugin(hostRef, "control", QVariantMap{{"enable", "true"}});
            } catch (const std::exception& e)
            {
                qWarning() << "Run NRPE control failed, error:" << e.what();
            }
        }
        this->callPlugin(hostRef, config.enableNRPE ? "enable" : "disable");
    }

    if (!config.enableNRPE)
        return;

    if (config.allowHosts != original.allowHosts || config.debug != original.debug || config.sslLogging != original.sslLogging)
    {
        this->callPlugin(hostRef, "set-config", QVariantMap{
            {"allowed_hosts", "127.0.0.1,::1," + config.allowHosts},
            {"debug", config.debug ? NRPEHostConfiguration::DEBUG_ENABLE : NRPEHostConfiguration::DEBUG_DISABLE},
            {"ssl_logging", config.sslLogging ? NRPEHostConfiguration::SSL_LOGGING_ENABLE : NRPEHostConfiguration::SSL_LOGGING_DISABLE}});
    }

    for (auto it = config.checks.constBegin(); it != config.checks.constEnd(); ++it)
    {
        auto originalCheck = original.checks.constFind(it.key());
        if (originalCheck == original.checks.constEnd() || *originalCheck == it.value())
            continue;

        // C# passes the check name as a key with a null value; xapi's string map needs a string
        this->callPlugin(hostRef, "set-threshold", QVariantMap{
            {it.key(), QString()},
            {"w", it.value().warningThreshold},
            {"c", it.value().criticalThreshold}});
    }

    // The new settings take effect after a restart
    this->callPlugin(hostRef, "restart");
}
