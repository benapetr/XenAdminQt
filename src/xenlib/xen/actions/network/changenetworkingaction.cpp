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

#include "changenetworkingaction.h"
#include "networkingactionhelpers.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_PIF.h"
#include "../../xenapi/xenapi_PBD.h"
#include "../../xenapi/xenapi_Cluster_host.h"
#include "../../host.h"
#include "../../pif.h"
#include "../../pbd.h"
#include "../../sr.h"
#include "../../clusterhost.h"
#include "../../pool.h"
#include "../../../xencache.h"
#include <QDebug>
#include <algorithm>

ChangeNetworkingAction::ChangeNetworkingAction(XenConnection* connection,
                                               const QSharedPointer<Pool>& pool,
                                               const QSharedPointer<Host>& host,
                                               const QStringList& pifRefsToReconfigure,
                                               const QStringList& pifRefsToDisable,
                                               const QString& newManagementPifRef,
                                               const QString& oldManagementPifRef,
                                               bool managementIpChanged,
                                               QObject* parent)
    : AsyncOperation(connection,
                     "Changing Network Configuration",
                     "Reconfiguring host networking",
                     parent),
      m_pool(pool),
      m_host(host),
      m_pifRefsToReconfigure(pifRefsToReconfigure),
      m_pifRefsToDisable(pifRefsToDisable),
      m_newManagementPifRef(newManagementPifRef),
      m_oldManagementPifRef(oldManagementPifRef),
      m_managementIpChanged(managementIpChanged)
{
    if (this->m_pool)
    {
        this->m_hosts = this->m_pool->GetHosts();
        if (this->m_hosts.isEmpty() && connection && connection->GetCache())
            this->m_hosts = connection->GetCache()->GetAll<Host>(XenObjectType::Host);
        this->m_hosts.erase(std::remove_if(this->m_hosts.begin(), this->m_hosts.end(),
                                     [](const QSharedPointer<Host>& h) { return !h || !h->IsValid(); }),
                      this->m_hosts.end());
        std::sort(this->m_hosts.begin(), this->m_hosts.end(),
                  [](const QSharedPointer<Host>& a, const QSharedPointer<Host>& b) {
                      return QString::localeAwareCompare(a->GetName(), b->GetName()) < 0;
                  });
    }
}

void ChangeNetworkingAction::run()
{
    try
    {
        this->GetConnection()->SetExpectDisruption(!this->m_managementIpChanged);

        int totalOps = this->m_pifRefsToReconfigure.count() + this->m_pifRefsToDisable.count();
        if (!this->m_newManagementPifRef.isEmpty())
        {
            totalOps += 1; // Management reconfiguration
        }

        // Determine if we're operating on a pool or single host
        const bool isPool = this->m_pool && this->m_pool->IsValid();

        bool restrictManagementOnVlan = false;
        if (isPool)
        {
            for (const QSharedPointer<Host>& host : this->m_hosts)
            {
                if (host && host->RestrictManagementOnVLAN())
                {
                    restrictManagementOnVlan = true;
                    break;
                }
            }
        }
        else if (this->m_host && this->m_host->RestrictManagementOnVLAN())
        {
            restrictManagementOnVlan = true;
        }

        int inc = totalOps > 0 ? (isPool ? 50 : 100) / totalOps : 100;
        int progress = 0;

        // Phase 1: Bring up/reconfigure new PIFs on supporters first, then coordinator
        if (isPool)
        {
            for (const QString& pifRef : this->m_pifRefsToReconfigure)
            {
                progress += inc;
                this->reconfigure(pifRef, true, false, progress); // Supporters
            }
        }

        for (const QString& pifRef : this->m_pifRefsToReconfigure)
        {
            progress += inc;
            this->reconfigure(pifRef, true, true, progress); // Coordinator (or single host)
        }

        // Phase 2: Reconfigure management interface if requested
        if (!this->m_newManagementPifRef.isEmpty() && !this->m_oldManagementPifRef.isEmpty())
        {
            QVariantMap newPifData = this->GetConnection()->GetCache()->ResolveObjectData("pif", this->m_newManagementPifRef);
            QVariantMap oldPifData = this->GetConnection()->GetCache()->ResolveObjectData("pif", this->m_oldManagementPifRef);

            // Check if we should clear the old management IP
            bool clearDownManagementIP = true;
            for (const QString& pifRef : this->m_pifRefsToReconfigure)
            {
                QVariantMap pifData = this->GetConnection()->GetCache()->ResolveObjectData("pif", pifRef);
                if (pifData.value("uuid").toString() == oldPifData.value("uuid").toString())
                {
                    clearDownManagementIP = false;
                    break;
                }
            }

            if (isPool)
            {
                progress += inc;
                if (!restrictManagementOnVlan)
                {
                    QString poolRef = this->m_pool ? this->m_pool->OpaqueRef() : QString();
                    if (poolRef.isEmpty())
                    {
                        QList<QVariantMap> pools = this->GetConnection()->GetCache()->GetAllData("pool");
                        if (!pools.isEmpty())
                            poolRef = pools.first().value("_ref").toString();
                    }
                    try
                    {
                        NetworkingActionHelpers::poolReconfigureManagement(
                            this, poolRef, this->m_newManagementPifRef, this->m_oldManagementPifRef, progress);

                        this->SetDescription("Network configuration complete");
                        this->GetConnection()->SetExpectDisruption(false);
                        return; // Pool reconfiguration handles everything

                    } catch (const std::exception& e)
                    {
                        qWarning() << "Pool management reconfiguration not available, falling back to host-by-host:" << e.what();
                        // Fall back to host-by-host reconfiguration
                        NetworkingActionHelpers::reconfigureManagement(
                            this, this->m_oldManagementPifRef, this->m_newManagementPifRef,
                            false, true, progress, clearDownManagementIP); // Supporters
                    }
                }
                else
                {
                    NetworkingActionHelpers::reconfigureManagement(
                        this, this->m_oldManagementPifRef, this->m_newManagementPifRef,
                        false, true, progress, clearDownManagementIP); // Supporters
                }
            }

            progress += inc;
            NetworkingActionHelpers::reconfigureManagement(
                this, this->m_oldManagementPifRef, this->m_newManagementPifRef,
                true, true, progress, clearDownManagementIP); // Coordinator or single host
        }

        // Phase 3: Bring down old PIFs on supporters first, then coordinator
        if (isPool)
        {
            for (const QString& pifRef : this->m_pifRefsToDisable)
            {
                progress += inc;
                this->reconfigure(pifRef, false, false, progress); // Supporters
            }
        }

        for (const QString& pifRef : this->m_pifRefsToDisable)
        {
            progress += inc;
            this->reconfigure(pifRef, false, true, progress); // Coordinator (or single host)
        }

        this->SetDescription("Network configuration complete");
        this->GetConnection()->SetExpectDisruption(false);

    } catch (const std::exception& e)
    {
        this->GetConnection()->SetExpectDisruption(false);
        this->setError(QString("Failed to change networking: %1").arg(e.what()));
    }
}

void ChangeNetworkingAction::reconfigure(const QString& pifRef, bool up, bool thisHost, int hi)
{
    // Use NetworkingActionHelpers::forSomeHosts to execute on appropriate hosts
    // This handles coordinator vs supporter logic automatically

    if (up)
    {
        // Bring up: configure IP and plug the PIF
        NetworkingActionHelpers::forSomeHosts(this, pifRef, thisHost, true, hi,
                                              [this, pifRef](AsyncOperation* action, const QString& existingPifRef, int h) {
                                                  QList<QSharedPointer<PBD>> gfs2Pbds;
                                                  this->disableClustering(existingPifRef, gfs2Pbds);
                                                  this->bringUp(pifRef, existingPifRef, h);
                                                  this->enableClustering(existingPifRef, gfs2Pbds);
                                              });
    } else
    {
        // Bring down: depurpose and clear IP
        NetworkingActionHelpers::forSomeHosts(this, pifRef, thisHost, true, hi,
                                              [this](AsyncOperation* action, const QString& existingPifRef, int h) {
                                                  QList<QSharedPointer<PBD>> gfs2Pbds;
                                                  this->disableClustering(existingPifRef, gfs2Pbds);
                                                  NetworkingActionHelpers::bringDown(action, existingPifRef, h);
                                                  this->enableClustering(existingPifRef, gfs2Pbds);
                                              });
    }
}

void ChangeNetworkingAction::bringUp(const QString& newPifRef, const QString& existingPifRef, int hi)
{
    QVariantMap newPifData = this->GetConnection()->GetCache()->ResolveObjectData("pif", newPifRef);
    QString ipMode = newPifData.value("ip_configuration_mode").toString();
    QString ip = newPifData.value("IP").toString();

    // For static IP in pool environments, calculate IP from range
    if (this->m_pool && ipMode.compare("Static", Qt::CaseInsensitive) == 0)
        ip = this->getIPInRange(ip, existingPifRef);

    NetworkingActionHelpers::bringUp(this, newPifRef, ip, existingPifRef, hi);
}

QString ChangeNetworkingAction::getIPInRange(const QString& rangeStart, const QString& existingPifRef) const
{
    if (this->m_hosts.isEmpty())
        return rangeStart;

    QVariantMap existingPifData = this->GetConnection()->GetCache()->ResolveObjectData("pif", existingPifRef);
    QString hostRef = existingPifData.value("host").toString();
    if (hostRef.isEmpty())
        throw std::runtime_error("PIF host reference not found");

    int index = -1;
    for (int i = 0; i < this->m_hosts.size(); ++i)
    {
        if (this->m_hosts[i] && this->m_hosts[i]->OpaqueRef() == hostRef)
        {
            index = i;
            break;
        }
    }
    if (index < 0)
        throw std::runtime_error("Host not found for IP range allocation");

    const QStringList bits = rangeStart.split('.');
    if (bits.size() != 4)
        throw std::runtime_error("Invalid IP address format for range allocation");

    bool ok = false;
    int last = bits[3].toInt(&ok);
    if (!ok)
        throw std::runtime_error("Invalid IP address format for range allocation");

    return QString("%1.%2.%3.%4").arg(bits[0], bits[1], bits[2]).arg(last + index);
}

void ChangeNetworkingAction::disableClustering(const QString& pifRef, QList<QSharedPointer<PBD>>& gfs2Pbds)
{
    QSharedPointer<PIF> pif = this->GetConnection()->GetCache()->ResolveObject<PIF>(pifRef);
    if (!pif || !pif->IsValid())
        return;

    if (!pif->IsUsedByClustering())
        return;

    QSharedPointer<Host> host = pif->GetHost();
    if (!host || !host->IsValid())
        return;

    QSharedPointer<ClusterHost> clusterHost;
    const QList<QSharedPointer<ClusterHost>> clusterHosts = this->GetConnection()->GetCache()->GetAll<ClusterHost>(XenObjectType::ClusterHost);
    for (const QSharedPointer<ClusterHost>& ch : clusterHosts)
    {
        if (ch && ch->IsValid() && ch->GetHostRef() == host->OpaqueRef())
        {
            clusterHost = ch;
            break;
        }
    }
    if (!clusterHost)
        return;

    for (const QSharedPointer<PBD>& pbd : host->GetPBDs())
    {
        if (!pbd || !pbd->IsValid() || !pbd->IsCurrentlyAttached())
            continue;

        QSharedPointer<SR> sr = pbd->GetSR();
        if (!sr || !sr->IsValid())
            continue;

        if (sr->GetType() == "gfs2")
        {
            gfs2Pbds.append(pbd);
            this->SetDescription(QString("Detaching %1 on %2").arg(sr->GetName(), host->GetName()));
            XenAPI::PBD::unplug(this->GetSession(), pbd->OpaqueRef());
        }
    }

    this->SetDescription(QString("Disabling clustering on %1").arg(host->GetName()));
    XenAPI::Cluster_host::disable(this->GetSession(), clusterHost->OpaqueRef());
}

void ChangeNetworkingAction::enableClustering(const QString& pifRef, const QList<QSharedPointer<PBD>>& gfs2Pbds)
{
    QSharedPointer<PIF> pif = this->GetConnection()->GetCache()->ResolveObject<PIF>(pifRef);
    if (!pif || !pif->IsValid())
        return;

    if (!pif->IsUsedByClustering())
        return;

    QSharedPointer<Host> host = pif->GetHost();
    if (!host || !host->IsValid())
        return;

    QSharedPointer<ClusterHost> clusterHost;
    const QList<QSharedPointer<ClusterHost>> clusterHosts = this->GetConnection()->GetCache()->GetAll<ClusterHost>(XenObjectType::ClusterHost);
    for (const QSharedPointer<ClusterHost>& ch : clusterHosts)
    {
        if (ch && ch->IsValid() && ch->GetHostRef() == host->OpaqueRef())
        {
            clusterHost = ch;
            break;
        }
    }
    if (!clusterHost)
        return;

    this->SetDescription(QString("Enabling clustering on %1").arg(host->GetName()));
    XenAPI::PIF::set_disallow_unplug(this->GetSession(), pif->OpaqueRef(), true);
    XenAPI::Cluster_host::enable(this->GetSession(), clusterHost->OpaqueRef());

    for (const QSharedPointer<PBD>& pbd : gfs2Pbds)
    {
        if (!pbd || !pbd->IsValid() || pbd->IsCurrentlyAttached())
            continue;

        QSharedPointer<SR> sr = pbd->GetSR();
        if (sr && sr->IsValid())
            this->SetDescription(QString("Attaching %1 on %2").arg(sr->GetName(), host->GetName()));
        XenAPI::PBD::plug(this->GetSession(), pbd->OpaqueRef());
    }
}
