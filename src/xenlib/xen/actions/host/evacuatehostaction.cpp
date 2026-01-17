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
#include "../../pool.h"
#include "../../vm.h"
#include "../../xenapi/xenapi_Host.h"
#include "../../xenapi/xenapi_Pool.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../../xencache.h"
#include "../../failure.h"
#include "../host/hahelpers.h"
#include "hostpoweronaction.h"
#include <QDebug>
#include <QThread>

namespace
{
    constexpr int kRecToHost = 1;

    bool isWlbEnabled(const QSharedPointer<Pool>& pool)
    {
        return pool && pool->IsWLBEnabled() && !pool->WLBUrl().isEmpty();
    }

    QSharedPointer<Host> findHostByUuid(XenConnection* connection, const QString& uuid)
    {
        if (!connection || !connection->GetCache())
            return QSharedPointer<Host>();

        const QList<QSharedPointer<Host>> hosts = connection->GetCache()->GetAll<Host>("host");
        for (const QSharedPointer<Host>& host : hosts)
        {
            if (host && host->GetUUID() == uuid)
                return host;
        }

        return QSharedPointer<Host>();
    }

    bool noRecommendationError(const QHash<QString, QStringList>& recommendations, QStringList& error)
    {
        for (auto it = recommendations.constBegin(); it != recommendations.constEnd(); ++it)
        {
            const QStringList rec = it.value();
            if (!rec.isEmpty() && rec.first().trimmed().compare("wlb", Qt::CaseInsensitive) != 0)
            {
                error = rec;
                return false;
            }
        }

        return true;
    }
}

EvacuateHostAction::EvacuateHostAction(QSharedPointer<Host> host, QSharedPointer<Host> newCoordinator, QObject* parent)
    : EvacuateHostAction(host, newCoordinator, nullptr, nullptr, parent)
{
}

EvacuateHostAction::EvacuateHostAction(QSharedPointer<Host> host,
                                       QSharedPointer<Host> newCoordinator,
                                       std::function<bool(QSharedPointer<Pool>, qint64, qint64)> acceptNtolChanges,
                                       std::function<bool(QSharedPointer<Pool>, QSharedPointer<Host>, qint64, qint64)> acceptNtolChangesOnEnable,
                                       QObject* parent)
    : AsyncOperation(host->GetConnection(), "Evacuating host", QString("Evacuating '%1'").arg(host ? host->GetName() : ""), parent),
      m_host(host),
      m_newCoordinator(newCoordinator),
      m_acceptNtolChanges(std::move(acceptNtolChanges)),
      m_acceptNtolChangesOnEnable(std::move(acceptNtolChangesOnEnable))
{
    if (!m_host)
        throw std::invalid_argument("Host cannot be null");
    this->AddApiMethodToRoleCheck("host.disable");
    this->AddApiMethodToRoleCheck("host.evacuate");
    this->AddApiMethodToRoleCheck("host.remove_from_other_config");
    this->AddApiMethodToRoleCheck("host.add_to_other_config");
    this->AddApiMethodToRoleCheck("host.enable");
    this->AddApiMethodToRoleCheck("pool.designate_new_master");
    this->AddApiMethodToRoleCheck("pool.ha_compute_hypothetical_max_host_failures_to_tolerate");
    this->AddApiMethodToRoleCheck("pool.set_ha_host_failures_to_tolerate");
}

void EvacuateHostAction::run()
{
    bool coordinator = this->isCoordinator();

    try
    {
        this->SetDescription(QString("Evacuating '%1'").arg(this->m_host->GetName()));

        // Disable host (0-20%)
        this->disable(0, 20);

        bool tryAgain = false;
        const QSharedPointer<Pool> pool = this->m_host->GetPool();

        if (isWlbEnabled(pool))
        {
            QHash<QString, QStringList> hostRecommendations =
                XenAPI::Host::retrieve_wlb_evacuate_recommendations(this->GetSession(), this->m_host->OpaqueRef());

            if (!hostRecommendations.isEmpty())
            {
                QStringList error;
                if (!noRecommendationError(hostRecommendations, error))
                {
                    throw Failure(error.join(": "));
                }

                struct RecItem
                {
                    QString vmRef;
                    QStringList rec;
                };

                QList<RecItem> hostPowerOnRecs;
                QList<RecItem> vmMoveRecs;

                for (auto it = hostRecommendations.constBegin(); it != hostRecommendations.constEnd(); ++it)
                {
                    const QString vmRef = it.key();
                    const QStringList rec = it.value();
                    if (rec.size() <= kRecToHost)
                        continue;

                    QSharedPointer<VM> vm = this->GetConnection()->GetCache()->ResolveObject<VM>("vm", vmRef);
                    if (!vm)
                        continue;

                    const QString toHostUuid = rec[kRecToHost];
                    QSharedPointer<Host> toHost = findHostByUuid(this->GetConnection(), toHostUuid);
                    if (!toHost)
                        continue;

                    if (vm->IsControlDomain() && !toHost->IsLive())
                        hostPowerOnRecs.append({vmRef, rec});
                    else
                        vmMoveRecs.append({vmRef, rec});
                }

                const int total = hostPowerOnRecs.size() + vmMoveRecs.size();
                if (total > 0)
                {
                    int start = 20;
                    int each = (coordinator ? 80 : 90) / total;

                    for (const RecItem& rec : hostPowerOnRecs)
                    {
                        const QString toHostUuid = rec.rec.value(kRecToHost);
                        QSharedPointer<Host> toHost = findHostByUuid(this->GetConnection(), toHostUuid);
                        if (!toHost)
                            continue;

                        if (!toHost->IsLive())
                        {
                            HostPowerOnAction powerOnAction(toHost, nullptr);
                            powerOnAction.RunSync(this->GetSession());
                        }

                        if (!toHost->IsEnabled())
                        {
                            QString enableTaskRef = XenAPI::Host::async_enable(this->GetSession(), toHost->OpaqueRef());
                            this->pollToCompletion(enableTaskRef, start, start);
                        }
                    }

                    for (const RecItem& rec : vmMoveRecs)
                    {
                        const QString vmRef = rec.vmRef;
                        const QString toHostUuid = rec.rec.value(kRecToHost);
                        QSharedPointer<Host> toHost = findHostByUuid(this->GetConnection(), toHostUuid);
                        if (!toHost)
                            continue;

                        int retry = 3;
                        while (retry > 0)
                        {
                            try
                            {
                                QVariantMap options;
                                options["live"] = "true";
                                QString taskRef = XenAPI::VM::async_pool_migrate(this->GetSession(), vmRef,
                                                                                 toHost->OpaqueRef(), options);
                                this->pollToCompletion(taskRef, start, start + each);
                                start += each;
                                break;
                            } catch (...)
                            {
                                QThread::sleep(10);
                                retry--;
                            }
                        }
                    }
                }
                else
                {
                    tryAgain = true;
                }
            }
            else
            {
                tryAgain = true;
            }
        }

        if (!isWlbEnabled(pool) || tryAgain)
        {
            QString taskRef = XenAPI::Host::async_evacuate(this->GetSession(), this->m_host->OpaqueRef());
            this->pollToCompletion(taskRef, 20, coordinator ? 80 : 90);
        }

        this->SetDescription(QString("Evacuated '%1'").arg(this->m_host->GetName()));

        // If this is the coordinator and we have a new coordinator, transition
        if (coordinator && this->m_newCoordinator)
        {
            this->SetDescription(QString("Transitioning to new coordinator '%1'")
                               .arg(this->m_newCoordinator->GetName()));

            // Signal to connection that coordinator is changing
            this->GetConnection()->SetCoordinatorMayChange(true);

            try
            {
                QString taskRef = XenAPI::Pool::async_designate_new_master(this->GetSession(),
                                                                           this->m_newCoordinator->OpaqueRef());
                this->pollToCompletion(taskRef, 80, 90);
            } catch (...)
            {
                // If designate new master fails, clear the flag
                this->GetConnection()->SetCoordinatorMayChange(false);
                throw;
            }

            this->SetDescription(QString("Transitioned to new coordinator '%1'")
                               .arg(this->m_newCoordinator->GetName()));
        }

        this->SetPercentComplete(100);

    } catch (const std::exception& e)
    {
        qDebug() << "EvacuateHostAction: Exception during evacuation, removing MAINTENANCE_MODE flag";

        // On error, re-enable the host
        try
        {
            this->enable(coordinator ? 80 : 90, 100, false);
        } catch (...)
        {
            qDebug() << "EvacuateHostAction: Failed to re-enable host during error recovery";
        }

        this->setError(QString("Failed to evacuate host: %1").arg(e.what()));
    }
}

void EvacuateHostAction::disable(int start, int finish)
{
    QSharedPointer<Pool> pool = this->m_host ? this->m_host->GetPool() : QSharedPointer<Pool>();
    if (pool && pool->HAEnabled())
    {
        const QVariantMap configuration = HostHaHelpers::BuildHaConfiguration(this->GetConnection());
        const qint64 maxFailures =
            XenAPI::Pool::ha_compute_hypothetical_max_host_failures_to_tolerate(this->GetSession(), configuration);
        const qint64 currentNtol = pool->HAHostFailuresToTolerate();
        const qint64 targetNtol = qMax<qint64>(0, maxFailures - 1);

        if (currentNtol > targetNtol)
        {
            bool cancel = false;
            if (this->m_acceptNtolChanges)
                cancel = this->m_acceptNtolChanges(pool, currentNtol, targetNtol);

            if (cancel)
            {
                this->setError("Cancelled");
                this->setState(Cancelled);
                return;
            }

            XenAPI::Pool::set_ha_host_failures_to_tolerate(this->GetSession(), pool->OpaqueRef(), targetNtol);
        }
    }

    // Disable the host
    QString taskRef = XenAPI::Host::async_disable(this->GetSession(), this->m_host->OpaqueRef());
    this->pollToCompletion(taskRef, start, finish);

    // Remove and then re-add MAINTENANCE_MODE flag
    XenAPI::Host::remove_from_other_config(this->GetSession(), this->m_host->OpaqueRef(), "MAINTENANCE_MODE");
    XenAPI::Host::add_to_other_config(this->GetSession(), this->m_host->OpaqueRef(), "MAINTENANCE_MODE", "true");
}

void EvacuateHostAction::enable(int start, int finish, bool queryNtolIncrease)
{
    // Remove MAINTENANCE_MODE flag
    XenAPI::Host::remove_from_other_config(this->GetSession(), this->m_host->OpaqueRef(), "MAINTENANCE_MODE");

    // Enable the host
    QString taskRef = XenAPI::Host::async_enable(this->GetSession(), this->m_host->OpaqueRef());
    this->pollToCompletion(taskRef, start, finish);

    if (queryNtolIncrease)
    {
        QSharedPointer<Pool> pool = this->m_host ? this->m_host->GetPool() : QSharedPointer<Pool>();
        if (pool && pool->HAEnabled() && this->m_acceptNtolChangesOnEnable)
        {
            const QVariantMap configuration = HostHaHelpers::BuildHaConfiguration(this->GetConnection());
            const qint64 maxFailures =
                XenAPI::Pool::ha_compute_hypothetical_max_host_failures_to_tolerate(this->GetSession(), configuration);
            const qint64 currentNtol = pool->HAHostFailuresToTolerate();

            if (currentNtol < maxFailures &&
                this->m_acceptNtolChangesOnEnable(pool, this->m_host, currentNtol, maxFailures))
            {
                XenAPI::Pool::set_ha_host_failures_to_tolerate(this->GetSession(), pool->OpaqueRef(), maxFailures);
            }
        }
    }
}

bool EvacuateHostAction::isCoordinator() const
{
    // Check if this host is the pool coordinator
    // Get pool from cache
    QList<QVariantMap> pools = this->GetConnection()->GetCache()->GetAllData("pool");
    if (pools.isEmpty())
        return false;

    QVariantMap pool = pools.first();
    QString masterRef = pool.value("master").toString();

    return masterRef == this->m_host->OpaqueRef();
}
