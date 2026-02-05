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

#include "hostpoweronaction.h"
#include "../../pool.h"
#include "../../xenapi/xenapi_Host.h"
#include "../../xenapi/xenapi_Pool.h"
#include "../../failure.h"
#include <QDebug>

namespace
{
    bool isWlbEnabled(const QSharedPointer<Pool>& pool)
    {
        return pool && pool->IsWLBEnabled() && !pool->WLBUrl().isEmpty();
    }

    QVariantMap buildWlbHostConfig(const QString& hostUuid, bool lastPowerOnSucceeded, bool disablePowerManagement)
    {
        QVariantMap config;
        const QString base = QString("host_%1_").arg(hostUuid);
        config.insert(base + "LastPowerOnSucceeded", lastPowerOnSucceeded ? "true" : "false");
        if (disablePowerManagement)
            config.insert(base + "ParticipatesInPowerManagement", "false");
        return config;
    }
}

HostPowerOnAction::HostPowerOnAction(QSharedPointer<Host> host, QObject* parent)
    : AsyncOperation(host->GetConnection(), QObject::tr("Power on host"), QObject::tr("Powering on host..."), parent),
      m_host(host)
{
    if (!this->m_host)
        throw std::invalid_argument("Host cannot be null");

    this->AddApiMethodToRoleCheck("host.power_on");
    this->AddApiMethodToRoleCheck("pool.send_wlb_configuration");
}

void HostPowerOnAction::run()
{
    bool succeeded = false;

    try
    {
        this->SetDescription(QObject::tr("Powering on '%1'").arg(this->m_host->GetName()));
        XenAPI::Host::power_on(this->GetSession(), this->m_host->OpaqueRef());
        succeeded = true;
        this->SetDescription(QObject::tr("Powered on '%1'").arg(this->m_host->GetName()));
    } catch (const Failure& failure)
    {
        const QStringList description = failure.errorDescription();
        QString errorMessage = failure.message();
        if (description.size() > 2)
        {
            const QString code = description[2];
            if (code == "DRAC_NO_SUPP_PACK")
                errorMessage = "DRAC_NO_SUPP_PACK";
            else if (code == "DRAC_POWERON_FAILED")
                errorMessage = "DRAC_POWERON_FAILED";
            else if (code == "ILO_CONNECTION_ERROR")
                errorMessage = "ILO_CONNECTION_ERROR";
            else if (code == "ILO_POWERON_FAILED")
                errorMessage = "ILO_POWERON_FAILED";
        }
        this->setError(QString("Failed to power on host: %1").arg(errorMessage));
    } catch (const std::exception& e)
    {
        this->setError(QString("Failed to power on host: %1").arg(e.what()));
    }

    QSharedPointer<Pool> pool = this->m_host->GetPoolOfOne();
    if (isWlbEnabled(pool))
    {
        const QVariantMap config = buildWlbHostConfig(this->m_host->GetUUID(), succeeded, !succeeded);
        try
        {
            XenAPI::Pool::send_wlb_configuration(this->GetSession(), config);
        } catch (const std::exception& e)
        {
            qWarning() << "HostPowerOnAction: Failed to update WLB configuration:" << e.what();
        }
    }
}
