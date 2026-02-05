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

#include "networkingactionhelpers.h"
#include "../../asyncoperation.h"
#include "../../xenapi/xenapi_PIF.h"
#include "../../xenapi/xenapi_Host.h"
#include "../../xenapi/xenapi_Pool.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../network.h"
#include "../../pif.h"
#include "../../host.h"
#include "../../pool.h"
#include "../../../xencache.h"
#include <QThread>
#include <QDebug>

static QSharedPointer<PIF> resolvePif(AsyncOperation* action, const QString& pifRef)
{
    if (!action || !action->GetConnection())
        return QSharedPointer<PIF>();

    XenCache* cache = action->GetConnection()->GetCache();
    if (!cache)
        return QSharedPointer<PIF>();

    QSharedPointer<PIF> pif = cache->ResolveObject<PIF>(pifRef);
    if (pif && pif->IsValid())
        return pif;

    return QSharedPointer<PIF>();
}

static QString getManagementPurpose(const QSharedPointer<PIF>& pif)
{
    if (!pif)
        return QString();

    return pif->GetOtherConfig().value("management_purpose").toString();
}

void NetworkingActionHelpers::moveManagementInterfaceName(AsyncOperation* action, const QString& fromPifRef, const QString& toPifRef)
{
    QSharedPointer<PIF> fromPif = resolvePif(action, fromPifRef);
    QString managementPurpose = getManagementPurpose(fromPif);

    if (managementPurpose.isEmpty())
    {
        return; // Primary management interface, nothing to move
    }

    qDebug() << "Moving management interface name from" << fromPifRef << "to" << toPifRef;

    // Set management_purpose on destination PIF
    XenAPI::PIF::add_to_other_config(action->GetSession(), toPifRef,
                                     "management_purpose", managementPurpose);

    // Remove management_purpose from source PIF
    XenAPI::PIF::remove_from_other_config(action->GetSession(), fromPifRef,
                                          "management_purpose");

    qDebug() << "Moved management interface name from" << fromPifRef << "to" << toPifRef;
}

void NetworkingActionHelpers::depurpose(AsyncOperation* action, const QString& pifRef, int hi)
{
    QSharedPointer<PIF> pif = resolvePif(action, pifRef);
    if (!pif)
    {
        action->SetPercentComplete(hi);
        return;
    }

    QString pifName = pif->GetDevice();

    qDebug() << "Depurposing PIF" << pifName << pifRef;
    action->SetDescription(QString("Depurposing interface %1...").arg(pifName));

    // Clear disallow_unplug
    XenAPI::PIF::set_disallow_unplug(action->GetSession(), pifRef, false);

    // Remove management_purpose if it exists
    QVariantMap otherConfig = pif->GetOtherConfig();
    if (otherConfig.contains("management_purpose"))
    {
        XenAPI::PIF::remove_from_other_config(action->GetSession(), pifRef, "management_purpose");
    }

    action->SetPercentComplete(hi);

    qDebug() << "Depurposed PIF" << pifName << pifRef;
    action->SetDescription(QString("Depurposed interface %1").arg(pifName));
}

void NetworkingActionHelpers::reconfigureManagement_(AsyncOperation* action, const QString& pifRef, int hi)
{
    QSharedPointer<PIF> pif = resolvePif(action, pifRef);
    if (!pif)
    {
        action->SetPercentComplete(hi);
        return;
    }

    QString pifName = pif->GetDevice();

    qDebug() << "Switching to PIF" << pifName << pifRef << "for management";
    action->SetDescription(QString("Reconfiguring management to interface %1...").arg(pifName));

    int lo = action->GetPercentComplete();
    int mid = (lo + hi) / 2;

    // First depurpose the PIF (clear disallow_unplug and management_purpose)
    XenAPI::PIF::set_disallow_unplug(action->GetSession(), pifRef, false);
    QVariantMap otherConfig = pif->GetOtherConfig();
    if (otherConfig.contains("management_purpose"))
    {
        XenAPI::PIF::remove_from_other_config(action->GetSession(), pifRef, "management_purpose");
    }

    action->SetPercentComplete(mid);

    // Now reconfigure management
    QString taskRef = XenAPI::Host::async_management_reconfigure(action->GetSession(), pifRef);
    action->pollToCompletion(taskRef, mid, hi);

    qDebug() << "Switched to PIF" << pifName << pifRef << "for management";
    action->SetDescription(QString("Reconfigured management to interface %1").arg(pifName));
}

void NetworkingActionHelpers::poolManagementReconfigure_(AsyncOperation* action, const QString& pifRef, int hi)
{
    QSharedPointer<PIF> pif = resolvePif(action, pifRef);
    if (!pif)
    {
        action->SetPercentComplete(hi);
        return;
    }

    QString pifName = pif->GetDevice();
    QString networkRef = pif->GetNetworkRef();

    qDebug() << "Pool-wide switching to PIF" << pifName << pifRef << "for management";
    action->SetDescription(QString("Reconfiguring pool management to interface %1...").arg(pifName));

    QString taskRef = XenAPI::Pool::async_management_reconfigure(action->GetSession(), networkRef);
    action->pollToCompletion(taskRef, action->GetPercentComplete(), hi);

    qDebug() << "Pool-wide switched to PIF" << pifName << pifRef << "for management";
    action->SetDescription(QString("Reconfigured pool management to interface %1").arg(pifName));
}

void NetworkingActionHelpers::clearIP(AsyncOperation* action, const QString& pifRef, int hi)
{
    QSharedPointer<PIF> pif = resolvePif(action, pifRef);
    if (!pif)
    {
        action->SetPercentComplete(hi);
        return;
    }

    // Don't clear IP if PIF is used by clustering
    if (pif->IsUsedByClustering())
    {
        qDebug() << "Skipping IP clear for clustering PIF" << pifRef;
        action->SetPercentComplete(hi);
        return;
    }

    QString pifName = pif->GetDevice();

    qDebug() << "Removing IP address from" << pifName << pifRef;
    action->SetDescription(QString("Bringing down interface %1...").arg(pifName));

    QString taskRef = XenAPI::PIF::async_reconfigure_ip(action->GetSession(), pifRef, "None", "", "", "", "");
    action->pollToCompletion(taskRef, action->GetPercentComplete(), hi);

    qDebug() << "Removed IP address from" << pifName << pifRef;
    action->SetDescription(QString("Brought down interface %1").arg(pifName));
}

void NetworkingActionHelpers::waitForMembersToRecover(XenConnection* connection, const QString& poolRef)
{
    Q_UNUSED(poolRef)
    const int retryLimit = 60;
    int retryAttempt = 0;

    QList<QString> deadHosts;

    if (!connection || !connection->GetCache())
        return;

    QSharedPointer<Pool> pool = connection->GetCache()->GetPoolOfOne();
    const QString coordinatorRef = pool ? pool->GetMasterHostRef() : QString();

    int totalHosts = 0;
    QList<QSharedPointer<Host>> hosts = connection->GetCache()->GetAll<Host>(XenObjectType::Host);
    for (const QSharedPointer<Host>& host : hosts)
    {
        if (!host || !host->IsValid())
            continue;
        if (host->OpaqueRef() != coordinatorRef)
            totalHosts++;
    }

    // Wait for supporters to go offline
    while (deadHosts.count() < totalHosts && retryAttempt <= retryLimit)
    {
        hosts = connection->GetCache()->GetAll<Host>(XenObjectType::Host);
        for (const QSharedPointer<Host>& host : hosts)
        {
            if (!host || !host->IsValid())
                continue;

            QString hostRef = host->OpaqueRef();
            QString hostUuid = host->GetUUID();

            if (hostRef == coordinatorRef)
            {
                continue;
            }

            bool isLive = host->IsLive();
            if (!isLive && !deadHosts.contains(hostUuid))
            {
                deadHosts.append(hostUuid);
            }
        }
        retryAttempt++;
        QThread::msleep(1000);
    }

    // Wait for supporters to come back online
    retryAttempt = 0;
    while (!deadHosts.isEmpty() && retryAttempt <= retryLimit)
    {
        hosts = connection->GetCache()->GetAll<Host>(XenObjectType::Host);
        for (const QSharedPointer<Host>& host : hosts)
        {
            if (!host || !host->IsValid())
                continue;

            QString hostRef = host->OpaqueRef();
            QString hostUuid = host->GetUUID();

            if (hostRef == coordinatorRef)
            {
                continue;
            }

            bool isLive = host->IsLive();
            if (isLive && deadHosts.contains(hostUuid))
            {
                deadHosts.removeAll(hostUuid);
            }
        }
        retryAttempt++;
        QThread::msleep(1000);
    }

    qDebug() << "Pool members recovered";
}

void NetworkingActionHelpers::reconfigureManagement(AsyncOperation* action, const QString& downPifRef, const QString& upPifRef, bool thisHost, bool lockPif, int hi, bool bringDownDownPif)
{
    QSharedPointer<PIF> downPif = resolvePif(action, downPifRef);
    QSharedPointer<PIF> upPif = resolvePif(action, upPifRef);

    // Verify both PIFs are on same host
    if (!downPif || !upPif || downPif->GetHostRef() != upPif->GetHostRef())
    {
        throw std::runtime_error("PIFs must be on same host for management reconfiguration");
    }

    int lo = action->GetPercentComplete();
    int inc = (hi - lo) / 3;

    // Phase 1: Depurpose down_pif if requested
    if (bringDownDownPif)
    {
        forSomeHosts(action, downPifRef, thisHost, lockPif, lo + inc, depurpose);
    } else
    {
        action->SetPercentComplete(lo + inc);
    }

    // Phase 2: Reconfigure management to up_pif
    forSomeHosts(action, upPifRef, thisHost, lockPif, lo + 2 * inc, reconfigureManagement_);

    // Phase 3: Clear IP from down_pif if requested
    if (bringDownDownPif)
    {
        forSomeHosts(action, downPifRef, thisHost, lockPif, hi, clearIP);
    } else
    {
        action->SetPercentComplete(hi);
    }
}

void NetworkingActionHelpers::poolReconfigureManagement(AsyncOperation* action, const QString& poolRef, const QString& upPifRef, const QString& downPifRef, int hi)
{
    QSharedPointer<PIF> downPif = resolvePif(action, downPifRef);
    QSharedPointer<PIF> upPif = resolvePif(action, upPifRef);

    // Verify both PIFs are on same host
    if (!downPif || !upPif || downPif->GetHostRef() != upPif->GetHostRef())
    {
        throw std::runtime_error("PIFs must be on same host for management reconfiguration");
    }

    int lo = action->GetPercentComplete();
    int inc = (hi - lo) / 3;

    // Phase 1: Pool management reconfigure
    poolManagementReconfigure_(action, upPifRef, lo + inc);

    // Phase 2: Wait for supporters to recover
    action->SetDescription("Waiting for pool members to recover...");
    waitForMembersToRecover(action->GetConnection(), poolRef);

    // Phase 3: Clear IP on supporters, then coordinator
    forSomeHosts(action, downPifRef, false, true, lo + 2 * inc, clearIP);
    forSomeHosts(action, downPifRef, true, true, hi, clearIP);
}

void NetworkingActionHelpers::bringUp(AsyncOperation* action, const QString& newPifRef, const QString& newIp, const QString& existingPifRef, int hi)
{
    QSharedPointer<PIF> newPif = resolvePif(action, newPifRef);
    QSharedPointer<PIF> existingPif = resolvePif(action, existingPifRef);
    if (!newPif || !existingPif)
    {
        action->SetPercentComplete(hi);
        return;
    }

    QString pifName = existingPif->GetDevice();
    QString managementPurpose = getManagementPurpose(newPif);
    bool isPrimary = managementPurpose.isEmpty();

    int lo = action->GetPercentComplete();
    int inc = (hi - lo) / (isPrimary ? 2 : 3);

    qDebug() << "Bringing PIF" << pifName << existingPifRef << "up as"
             << newIp << "/" << newPif->Netmask()
             << newPif->Gateway() << newPif->DNS();
    action->SetDescription(QString("Bringing up interface %1...").arg(pifName));

    // Set disallow_unplug and management_purpose
    XenAPI::PIF::set_disallow_unplug(action->GetSession(), existingPifRef, !isPrimary);

    if (!managementPurpose.isEmpty())
    {
        XenAPI::PIF::add_to_other_config(action->GetSession(), existingPifRef,
                                         "management_purpose", managementPurpose);
    }

    action->SetPercentComplete(lo + inc);

    // Reconfigure IP
    reconfigureIP(action, newPifRef, existingPifRef, newIp, lo + 2 * inc);

    // Plug if secondary management
    if (!isPrimary)
    {
        plug(action, existingPifRef, hi);
    } else
    {
        action->SetPercentComplete(hi);
    }

    qDebug() << "Brought PIF" << pifName << existingPifRef << "up";
    action->SetDescription(QString("Brought up interface %1").arg(pifName));
}

void NetworkingActionHelpers::bringUp(AsyncOperation* action, const QString& newPifRef, const QString& existingPifRef, int hi)
{
    QSharedPointer<PIF> newPif = resolvePif(action, newPifRef);
    if (!newPif)
    {
        action->SetPercentComplete(hi);
        return;
    }
    QString newIp = newPif->IP();
    bringUp(action, newPifRef, newIp, existingPifRef, hi);
}

void NetworkingActionHelpers::bringUp(AsyncOperation* action, const QString& newPifRef, bool thisHost, int hi)
{
    forSomeHosts(action, newPifRef, thisHost, false, hi,
                 [](AsyncOperation* a, const QString& pifRef, int h) {
                     bringUp(a, pifRef, pifRef, h);
                 });
}

void NetworkingActionHelpers::bringDown(AsyncOperation* action, const QString& pifRef, int hi)
{
    int lo = action->GetPercentComplete();
    int mid = (lo + hi) / 2;

    depurpose(action, pifRef, mid);
    clearIP(action, pifRef, hi);
}

void NetworkingActionHelpers::forSomeHosts(AsyncOperation* action, const QString& pifRef, bool thisHost, bool lockPif, int hi, PIFMethod pifMethod)
{
    if (!action || !action->GetConnection() || !action->GetConnection()->GetCache())
        return;

    XenCache* cache = action->GetConnection()->GetCache();
    QSharedPointer<PIF> pif = cache->ResolveObject<PIF>(XenObjectType::PIF, pifRef);
    if (!pif || !pif->IsValid())
    {
        qWarning() << "PIF" << pifRef << "not found";
        return;
    }

    QSharedPointer<Network> network = pif->GetNetwork();
    if (!network || !network->IsValid())
    {
        qWarning() << "Network has gone away";
        return;
    }

    // Find all PIFs in the same network
    QList<QSharedPointer<PIF>> pifsToReconfigure;
    const QString pifHostRef = pif->GetHostRef();
    const QList<QSharedPointer<PIF>> networkPifs = network->GetPIFs();
    for (const QSharedPointer<PIF>& candidate : networkPifs)
    {
        if (!candidate || !candidate->IsValid())
            continue;

        const bool sameHost = candidate->GetHostRef() == pifHostRef;
        if (thisHost == sameHost)
            pifsToReconfigure.append(candidate);
    }

    if (pifsToReconfigure.isEmpty())
    {
        action->SetPercentComplete(hi);
        return;
    }

    // Execute method on each PIF
    int lo = action->GetPercentComplete();
    int inc = (hi - lo) / pifsToReconfigure.count();

    for (const QSharedPointer<PIF>& candidate : pifsToReconfigure)
    {
        if (!candidate)
            continue;
        try
        {
            lo += inc;
            if (lockPif)
                candidate->Lock();
            pifMethod(action, candidate->OpaqueRef(), lo);
        } catch (const std::exception& e)
        {
            qWarning() << "Error processing PIF" << (candidate ? candidate->OpaqueRef() : QString()) << ":" << e.what();
            if (lockPif && candidate)
                candidate->Unlock();
            throw;
        }
        if (lockPif && candidate)
            candidate->Unlock();
    }
}
void NetworkingActionHelpers::reconfigureSinglePrimaryManagement(AsyncOperation* action, const QString& srcPifRef, const QString& destPifRef, int hi)
{
    // Copy IP config from src to dest (conceptually - we'll use src's values for dest)
    QSharedPointer<PIF> srcPif = resolvePif(action, srcPifRef);
    if (!srcPif)
    {
        action->SetPercentComplete(hi);
        return;
    }

    int lo = action->GetPercentComplete();
    int mid = (lo + hi) / 2;

    // Bring up destination with source's IP config
    QString srcIp = srcPif->IP();
    bringUp(action, srcPifRef, srcIp, destPifRef, mid);

    // Reconfigure management
    reconfigureManagement(action, srcPifRef, destPifRef, true, false, hi, true);
}

void NetworkingActionHelpers::reconfigurePrimaryManagement(AsyncOperation* action, const QString& srcPifRef, const QString& destPifRef, int hi)
{
    int lo = action->GetPercentComplete();
    int inc = (hi - lo) / 4;

    // Phase 1: Bring up bond members on supporters (without plugging)
    bringUp(action, destPifRef, false, lo + inc);

    // Phase 2: Bring up bond members on coordinator
    bringUp(action, destPifRef, true, lo + 2 * inc);

    // Phase 3: Reconfigure management on supporters
    reconfigureManagement(action, srcPifRef, destPifRef, false, false, lo + 3 * inc, true);

    // Phase 4: Reconfigure management on coordinator
    reconfigureManagement(action, srcPifRef, destPifRef, true, false, hi, true);
}

void NetworkingActionHelpers::reconfigureIP(AsyncOperation* action, const QString& newPifRef, const QString& existingPifRef, const QString& ip, int hi)
{
    QSharedPointer<PIF> newPif = resolvePif(action, newPifRef);
    QSharedPointer<PIF> existingPif = resolvePif(action, existingPifRef);
    if (!newPif || !existingPif)
    {
        action->SetPercentComplete(hi);
        return;
    }

    QString pifName = existingPif->GetDevice();
    qDebug() << "Reconfiguring IP on" << pifName << existingPifRef;

    QString mode = newPif->IpConfigurationMode();
    QString netmask = newPif->Netmask();
    QString gateway = newPif->Gateway();
    QString dns = newPif->DNS();

    QString taskRef = XenAPI::PIF::async_reconfigure_ip(action->GetSession(), existingPifRef, mode, ip, netmask, gateway, dns);
    action->pollToCompletion(taskRef, action->GetPercentComplete(), hi);

    qDebug() << "Reconfigured IP on" << pifName << existingPifRef;
}

void NetworkingActionHelpers::plug(AsyncOperation* action,
                                   const QString& pifRef,
                                   int hi)
{
    QSharedPointer<PIF> pif = resolvePif(action, pifRef);
    if (!pif)
    {
        action->SetPercentComplete(hi);
        return;
    }

    bool currentlyAttached = pif->IsCurrentlyAttached();

    if (!currentlyAttached)
    {
        QString pifName = pif->GetDevice();
        qDebug() << "Plugging" << pifName << pifRef;

        QString taskRef = XenAPI::PIF::async_plug(action->GetSession(), pifRef);
        action->pollToCompletion(taskRef, action->GetPercentComplete(), hi);

        qDebug() << "Plugged" << pifName << pifRef;
    } else
    {
        action->SetPercentComplete(hi);
    }
}
