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

#include "enablehaaction.h"
#include "../../../xen/connection.h"
#include "../../../xen/session.h"
#include "../../xenapi/xenapi_Pool.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../../xencache.h"
#include <stdexcept>

EnableHAAction::EnableHAAction(XenConnection* connection,
                               const QString& poolRef,
                               const QStringList& heartbeatSRRefs,
                               qint64 failuresToTolerate,
                               const QMap<QString, QVariantMap>& vmStartupOptions,
                               QObject* parent)
    : AsyncOperation(connection,
                     QString("Enabling HA on pool"),
                     "Enabling HA",
                     parent),
      m_poolRef(poolRef),
      m_heartbeatSRRefs(heartbeatSRRefs),
      m_failuresToTolerate(failuresToTolerate),
      m_vmStartupOptions(vmStartupOptions)
{
    if (m_heartbeatSRRefs.isEmpty())
    {
        throw std::invalid_argument("You must specify at least 1 heartbeat SR");
    }
}

void EnableHAAction::run()
{
    try
    {
        setPercentComplete(0);
        setDescription("Configuring HA settings...");

        // Step 1: Set VM restart priorities if provided (matches C# pattern)
        if (!m_vmStartupOptions.isEmpty())
        {
            double increment = 10.0 / qMax(m_vmStartupOptions.size(), 1);
            int i = 0;

            QMapIterator<QString, QVariantMap> it(m_vmStartupOptions);
            while (it.hasNext())
            {
                it.next();
                const QString& vmRef = it.key();
                const QVariantMap& options = it.value();

                // Set HA restart priority
                if (options.contains("ha_restart_priority"))
                {
                    QString priority = options["ha_restart_priority"].toString();
                    XenAPI::VM::set_ha_restart_priority(session(), vmRef, priority);
                }

                // Set start order
                if (options.contains("order"))
                {
                    qint64 order = options["order"].toLongLong();
                    XenAPI::VM::set_order(session(), vmRef, order);
                }

                // Set start delay
                if (options.contains("start_delay"))
                {
                    qint64 delay = options["start_delay"].toLongLong();
                    XenAPI::VM::set_start_delay(session(), vmRef, delay);
                }

                setPercentComplete(static_cast<int>(++i * increment));
            }
        }

        setPercentComplete(10);
        setDescription("Setting host failure tolerance...");

        // Step 2: Set ha_host_failures_to_tolerate
        XenAPI::Pool::set_ha_host_failures_to_tolerate(session(), m_poolRef, m_failuresToTolerate);

        setPercentComplete(15);
        setDescription("Enabling HA...");

        // Step 3: Call async_enable_ha with heartbeat SRs
        // Empty configuration map (matches C# pattern - Dictionary<string, string>())
        QVariantMap configuration;
        QString taskRef = XenAPI::Pool::async_enable_ha(session(), m_heartbeatSRRefs, configuration);

        // Poll to completion (15% -> 100%)
        pollToCompletion(taskRef, 15, 100);

        setDescription("HA enabled successfully");

    } catch (const std::exception& e)
    {
        if (isCancelled())
        {
            setDescription("HA enable cancelled");
        } else
        {
            // TODO: In C# there's special handling for VDI_NOT_AVAILABLE errors
            // to show VDI UUID in friendly format
            setError(QString("Failed to enable HA: %1").arg(e.what()));
        }
    }
}
