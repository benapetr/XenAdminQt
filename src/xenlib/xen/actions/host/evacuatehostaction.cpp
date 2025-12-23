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

#include "evacuatehostaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_Host.h"
#include "../../xenapi/xenapi_Pool.h"
#include "../../../xencache.h"
#include <QDebug>

EvacuateHostAction::EvacuateHostAction(XenConnection* connection,
                                       Host* host,
                                       Host* newCoordinator,
                                       QObject* parent)
    : AsyncOperation(connection,
                     "Evacuating host",
                     QString("Evacuating '%1'").arg(host ? host->nameLabel() : ""),
                     parent),
      m_host(host),
      m_newCoordinator(newCoordinator)
{
    if (!m_host)
        throw std::invalid_argument("Host cannot be null");
}

void EvacuateHostAction::run()
{
    bool coordinator = isCoordinator();

    try
    {
        setDescription(QString("Evacuating '%1'").arg(m_host->nameLabel()));

        // Disable host (0-20%)
        disable(0, 20);

        // WLB evacuation is not yet implemented in Qt version
        // For now, use simple Host.async_evacuate
        // TODO: Implement WLB evacuation recommendations when WLB is ported

        qDebug() << "EvacuateHostAction: Using non-WLB evacuation";

        // Evacuate all VMs from the host (20-80% or 20-90% depending on coordinator)
        QString taskRef = XenAPI::Host::async_evacuate(session(), m_host->opaqueRef());
        pollToCompletion(taskRef, 20, coordinator ? 80 : 90);

        setDescription(QString("Evacuated '%1'").arg(m_host->nameLabel()));

        // If this is the coordinator and we have a new coordinator, transition
        if (coordinator && m_newCoordinator)
        {
            setDescription(QString("Transitioning to new coordinator '%1'")
                               .arg(m_newCoordinator->nameLabel()));

            // Signal to connection that coordinator is changing
            // C# sets Connection.CoordinatorMayChange = true
            // TODO: Add this flag to XenConnection when needed

            try
            {
                QString taskRef = XenAPI::Pool::async_designate_new_master(session(),
                                                                           m_newCoordinator->opaqueRef());
                pollToCompletion(taskRef, 80, 90);
            } catch (...)
            {
                // If designate new master fails, clear the flag
                // TODO: Clear CoordinatorMayChange flag
                throw;
            }

            setDescription(QString("Transitioned to new coordinator '%1'")
                               .arg(m_newCoordinator->nameLabel()));
        }

        setPercentComplete(100);

    } catch (const std::exception& e)
    {
        qDebug() << "EvacuateHostAction: Exception during evacuation, removing MAINTENANCE_MODE flag";

        // On error, re-enable the host
        try
        {
            enable(coordinator ? 80 : 90, 100, false);
        } catch (...)
        {
            qDebug() << "EvacuateHostAction: Failed to re-enable host during error recovery";
        }

        setError(QString("Failed to evacuate host: %1").arg(e.what()));
    }
}

void EvacuateHostAction::disable(int start, int finish)
{
    // TODO: maybeReduceNtolBeforeOp() - HA ntol reduction
    // This asks users whether to decrease ntol since disable will fail if it would cause HA overcommit
    // For now, skip this check (HA not fully implemented)
    qDebug() << "EvacuateHostAction: TODO - HA ntol reduction check not yet implemented";

    // Disable the host
    QString taskRef = XenAPI::Host::async_disable(session(), m_host->opaqueRef());
    pollToCompletion(taskRef, start, finish);

    // Remove and then re-add MAINTENANCE_MODE flag
    XenAPI::Host::remove_from_other_config(session(), m_host->opaqueRef(), "MAINTENANCE_MODE");
    XenAPI::Host::add_to_other_config(session(), m_host->opaqueRef(), "MAINTENANCE_MODE", "true");
}

void EvacuateHostAction::enable(int start, int finish, bool queryNtolIncrease)
{
    Q_UNUSED(queryNtolIncrease);

    // Remove MAINTENANCE_MODE flag
    XenAPI::Host::remove_from_other_config(session(), m_host->opaqueRef(), "MAINTENANCE_MODE");

    // Enable the host
    QString taskRef = XenAPI::Host::async_enable(session(), m_host->opaqueRef());
    pollToCompletion(taskRef, start, finish);
}

bool EvacuateHostAction::isCoordinator() const
{
    // Check if this host is the pool coordinator
    // Get pool from cache
    QList<QVariantMap> pools = connection()->getCache()->GetAllData("pool");
    if (pools.isEmpty())
        return false;

    QVariantMap pool = pools.first();
    QString masterRef = pool.value("master").toString();

    return masterRef == m_host->opaqueRef();
}
