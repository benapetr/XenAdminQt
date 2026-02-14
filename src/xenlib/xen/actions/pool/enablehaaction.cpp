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
#include "../../xenapi/xenapi_Pool.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../../xencache.h"
#include "../../failure.h"
#include "../../pool.h"
#include <stdexcept>

EnableHAAction::EnableHAAction(QSharedPointer<Pool> pool, const QStringList& heartbeatSRRefs, qint64 failuresToTolerate, const QMap<QString, QVariantMap>& vmStartupOptions, QObject* parent)
    : AsyncOperation(pool ? pool->GetConnection() : nullptr, QString("Enabling HA on pool"), "Enabling HA", parent),
      m_pool(pool),
      m_heartbeatSRRefs(heartbeatSRRefs),
      m_failuresToTolerate(failuresToTolerate),
      m_vmStartupOptions(vmStartupOptions)
{
    if (!this->m_pool || !this->m_pool->IsValid())
    {
        throw std::invalid_argument("Invalid pool object");
    }
    if (this->m_heartbeatSRRefs.isEmpty())
    {
        throw std::invalid_argument("You must specify at least 1 heartbeat SR");
    }

    this->SetPool(pool);

    this->AddApiMethodToRoleCheck("pool.set_ha_host_failures_to_tolerate");
    this->AddApiMethodToRoleCheck("pool.async_enable_ha");
    if (!this->m_vmStartupOptions.isEmpty())
    {
        this->AddApiMethodToRoleCheck("vm.set_ha_restart_priority");
        this->AddApiMethodToRoleCheck("vm.set_order");
        this->AddApiMethodToRoleCheck("vm.set_start_delay");
    }
}

void EnableHAAction::run()
{
    try
    {
        this->SetPercentComplete(0);
        this->SetDescription("Configuring HA settings...");

        // Step 1: Set VM restart priorities if provided (matches C# pattern)
        if (!this->m_vmStartupOptions.isEmpty())
        {
            double increment = 10.0 / qMax(this->m_vmStartupOptions.size(), 1);
            int i = 0;

            QMapIterator<QString, QVariantMap> it(this->m_vmStartupOptions);
            while (it.hasNext())
            {
                it.next();
                const QString& vmRef = it.key();
                const QVariantMap& options = it.value();

                // Set HA restart priority
                if (options.contains("ha_restart_priority"))
                {
                    QString priority = options["ha_restart_priority"].toString();
                    XenAPI::VM::set_ha_restart_priority(this->GetSession(), vmRef, priority);
                }

                // Set start order
                if (options.contains("order"))
                {
                    qint64 order = options["order"].toLongLong();
                    XenAPI::VM::set_order(this->GetSession(), vmRef, order);
                }

                // Set start delay
                if (options.contains("start_delay"))
                {
                    qint64 delay = options["start_delay"].toLongLong();
                    XenAPI::VM::set_start_delay(this->GetSession(), vmRef, delay);
                }

                SetPercentComplete(static_cast<int>(++i * increment));
            }
        }

        SetPercentComplete(10);
        SetDescription("Setting host failure tolerance...");

        // Step 2: Set ha_host_failures_to_tolerate
        XenAPI::Pool::set_ha_host_failures_to_tolerate(GetSession(), this->m_pool->OpaqueRef(), this->m_failuresToTolerate);

        SetPercentComplete(15);
        SetDescription("Enabling HA...");

        // Step 3: Call async_enable_ha with heartbeat SRs
        // Empty configuration map (matches C# pattern - Dictionary<string, string>())
        QVariantMap configuration;
        QString taskRef = XenAPI::Pool::async_enable_ha(GetSession(), m_heartbeatSRRefs, configuration);

        // Poll to completion (15% -> 100%)
        pollToCompletion(taskRef, 15, 100);

        SetDescription("HA enabled successfully");

    } catch (const Failure& f)
    {
        if (IsCancelled())
        {
            SetDescription("HA enable cancelled");
            return;
        }

        const QStringList description = f.errorDescription();
        if (!description.isEmpty() && description.first() == "VDI_NOT_AVAILABLE")
        {
            QString vdiRef = description.value(1);
            QString vdiUuid;

            XenCache* cache = this->m_pool && this->m_pool->GetConnection() ? this->m_pool->GetConnection()->GetCache() : nullptr;
            if (cache && !vdiRef.isEmpty())
            {
                const QVariantMap vdiData = cache->ResolveObjectData(XenObjectType::VDI, vdiRef);
                vdiUuid = vdiData.value("uuid").toString();
            }

            if (!vdiUuid.isEmpty())
                setError(QString("Failed to enable HA: VDI not available: %1").arg(vdiUuid));
            else
                setError("Failed to enable HA: One or more HA statefile VDIs are not available.");
            return;
        }

        setError(QString("Failed to enable HA: %1").arg(f.message()));
    } catch (const std::exception& e)
    {
        if (IsCancelled())
        {
            SetDescription("HA enable cancelled");
        } else
        {
            setError(QString("Failed to enable HA: %1").arg(QString::fromUtf8(e.what())));
        }
    }
}
