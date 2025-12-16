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

#include "sethaprioritiesaction.h"
#include "../../../xen/connection.h"
#include "../../../xen/session.h"
#include "../../xenapi/xenapi_Pool.h"
#include "../../xenapi/xenapi_VM.h"
#include <stdexcept>

SetHaPrioritiesAction::SetHaPrioritiesAction(XenConnection* connection,
                                             const QString& poolRef,
                                             const QMap<QString, QVariantMap>& vmStartupOptions,
                                             qint64 ntol,
                                             QObject* parent)
    : AsyncOperation(connection,
                     QString("Setting HA priorities"),
                     "Configuring HA",
                     parent),
      m_poolRef(poolRef),
      m_vmStartupOptions(vmStartupOptions),
      m_ntol(ntol)
{
}

bool SetHaPrioritiesAction::isRestartPriority(const QString& priority) const
{
    return priority == "restart" ||
           priority == "always_restart" ||
           priority == "always_restart_high_priority";
}

void SetHaPrioritiesAction::run()
{
    try
    {
        setPercentComplete(0);
        setDescription("Configuring HA priorities...");

        int totalVMs = m_vmStartupOptions.size();
        int processedVMs = 0;

        // First pass: Move VMs from protected -> unprotected
        // This prevents overcommitment during transition
        QMapIterator<QString, QVariantMap> it1(m_vmStartupOptions);
        while (it1.hasNext())
        {
            it1.next();
            const QString& vmRef = it1.key();
            const QVariantMap& options = it1.value();

            QString priority = options.value("ha_restart_priority", "").toString();

            // Skip VMs that are being set to restart priority (handle in second pass)
            if (isRestartPriority(priority))
                continue;

            setDescription(QString("Setting priority for VM..."));

            // Set HA restart priority
            XenAPI::VM::set_ha_restart_priority(session(), vmRef, priority);

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

            processedVMs++;
            setPercentComplete(static_cast<int>(processedVMs * 30.0 / qMax(totalVMs, 1)));

            if (isCancelled())
            {
                setDescription("Cancelled");
                return;
            }
        }

        setPercentComplete(30);
        setDescription("Setting failure tolerance...");

        // Set NTOL
        XenAPI::Pool::set_ha_host_failures_to_tolerate(session(), m_poolRef, m_ntol);

        setPercentComplete(40);

        // Second pass: Move VMs from unprotected -> protected
        QMapIterator<QString, QVariantMap> it2(m_vmStartupOptions);
        while (it2.hasNext())
        {
            it2.next();
            const QString& vmRef = it2.key();
            const QVariantMap& options = it2.value();

            QString priority = options.value("ha_restart_priority", "").toString();

            // Only process VMs being set to restart priority
            if (!isRestartPriority(priority))
                continue;

            setDescription(QString("Setting priority for VM..."));

            // Set HA restart priority
            XenAPI::VM::set_ha_restart_priority(session(), vmRef, priority);

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

            processedVMs++;
            setPercentComplete(40 + static_cast<int>((processedVMs - (totalVMs / 2)) * 30.0 / qMax(totalVMs, 1)));

            if (isCancelled())
            {
                setDescription("Cancelled");
                return;
            }
        }

        setPercentComplete(70);
        setDescription("Synchronizing pool database...");

        // Sync database to ensure settings propagate to all hosts
        QString taskRef = XenAPI::Pool::async_sync_database(session());
        pollToCompletion(taskRef, 70, 100);

        setDescription("HA priorities updated successfully");

    } catch (const std::exception& e)
    {
        if (isCancelled())
        {
            setDescription("HA priority update cancelled");
        } else
        {
            setError(QString("Failed to update HA priorities: %1").arg(e.what()));
        }
    }
}
