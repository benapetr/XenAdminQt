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
#include "../../xenapi/xenapi_Network.h"
#include "../../xenapi/xenapi_Host.h"
#include "../../xenapi/xenapi_Pool.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../../xencache.h"
#include <QThread>
#include <QDebug>

// Helper to get PIF record from cache
static QVariantMap getPIFRecord(AsyncOperation* action, const QString& pifRef)
{
    QList<QVariantMap> pifs = action->connection()->GetCache()->GetAllData("pif");
    for (const QVariantMap& pif : pifs)
    {
        if (pif.value("_ref").toString() == pifRef)
        {
            return pif;
        }
    }
    return QVariantMap();
}

// Helper to get network from PIF
static QString getNetworkFromPIF(const QVariantMap& pifRecord)
{
    return pifRecord.value("network").toString();
}

// Helper to get host from PIF
static QString getHostFromPIF(const QVariantMap& pifRecord)
{
    return pifRecord.value("host").toString();
}

// Helper to get management_purpose from PIF other_config
static QString getManagementPurpose(const QVariantMap& pifRecord)
{
    QVariantMap otherConfig = pifRecord.value("other_config").toMap();
    return otherConfig.value("management_purpose").toString();
}

// Helper to check if PIF is used by clustering
static bool isUsedByClustering(XenConnection* connection, const QString& pifRef)
{
    // Check if any cluster_host references this PIF
    QList<QVariantMap> clusterHosts = connection->GetCache()->GetAllData("cluster_host");
    for (const QVariantMap& ch : clusterHosts)
    {
        if (ch.value("PIF").toString() == pifRef)
        {
            return true;
        }
    }
    return false;
}

// Helper to get coordinator host ref
static QString getCoordinatorRef(XenConnection* connection)
{
    QList<QVariantMap> pools = connection->GetCache()->GetAllData("pool");
    if (!pools.isEmpty())
    {
        return pools.first().value("master").toString();
    }
    return QString();
}

void NetworkingActionHelpers::moveManagementInterfaceName(AsyncOperation* action,
                                                          const QString& fromPifRef,
                                                          const QString& toPifRef)
{
    QVariantMap fromPif = getPIFRecord(action, fromPifRef);
    QString managementPurpose = getManagementPurpose(fromPif);

    if (managementPurpose.isEmpty())
    {
        return; // Primary management interface, nothing to move
    }

    qDebug() << "Moving management interface name from" << fromPifRef << "to" << toPifRef;

    // Set management_purpose on destination PIF
    XenAPI::PIF::add_to_other_config(action->session(), toPifRef,
                                     "management_purpose", managementPurpose);

    // Remove management_purpose from source PIF
    XenAPI::PIF::remove_from_other_config(action->session(), fromPifRef,
                                          "management_purpose");

    qDebug() << "Moved management interface name from" << fromPifRef << "to" << toPifRef;
}

void NetworkingActionHelpers::depurpose(AsyncOperation* action, const QString& pifRef, int hi)
{
    QVariantMap pifRecord = getPIFRecord(action, pifRef);
    QString pifName = pifRecord.value("device").toString();

    qDebug() << "Depurposing PIF" << pifName << pifRef;
    action->setDescription(QString("Depurposing interface %1...").arg(pifName));

    // Clear disallow_unplug
    XenAPI::PIF::set_disallow_unplug(action->session(), pifRef, false);

    // Remove management_purpose if it exists
    QVariantMap otherConfig = pifRecord.value("other_config").toMap();
    if (otherConfig.contains("management_purpose"))
    {
        XenAPI::PIF::remove_from_other_config(action->session(), pifRef, "management_purpose");
    }

    action->setPercentComplete(hi);

    qDebug() << "Depurposed PIF" << pifName << pifRef;
    action->setDescription(QString("Depurposed interface %1").arg(pifName));
}

void NetworkingActionHelpers::reconfigureManagement_(AsyncOperation* action, const QString& pifRef, int hi)
{
    QVariantMap pifRecord = getPIFRecord(action, pifRef);
    QString pifName = pifRecord.value("device").toString();

    qDebug() << "Switching to PIF" << pifName << pifRef << "for management";
    action->setDescription(QString("Reconfiguring management to interface %1...").arg(pifName));

    int lo = action->percentComplete();
    int mid = (lo + hi) / 2;

    // First depurpose the PIF (clear disallow_unplug and management_purpose)
    XenAPI::PIF::set_disallow_unplug(action->session(), pifRef, false);
    QVariantMap otherConfig = pifRecord.value("other_config").toMap();
    if (otherConfig.contains("management_purpose"))
    {
        XenAPI::PIF::remove_from_other_config(action->session(), pifRef, "management_purpose");
    }

    action->setPercentComplete(mid);

    // Now reconfigure management
    QString taskRef = XenAPI::Host::async_management_reconfigure(action->session(), pifRef);
    action->pollToCompletion(taskRef, mid, hi);

    qDebug() << "Switched to PIF" << pifName << pifRef << "for management";
    action->setDescription(QString("Reconfigured management to interface %1").arg(pifName));
}

void NetworkingActionHelpers::poolManagementReconfigure_(AsyncOperation* action, const QString& pifRef, int hi)
{
    QVariantMap pifRecord = getPIFRecord(action, pifRef);
    QString pifName = pifRecord.value("device").toString();
    QString networkRef = getNetworkFromPIF(pifRecord);

    qDebug() << "Pool-wide switching to PIF" << pifName << pifRef << "for management";
    action->setDescription(QString("Reconfiguring pool management to interface %1...").arg(pifName));

    QString taskRef = XenAPI::Pool::async_management_reconfigure(action->session(), networkRef);
    action->pollToCompletion(taskRef, action->percentComplete(), hi);

    qDebug() << "Pool-wide switched to PIF" << pifName << pifRef << "for management";
    action->setDescription(QString("Reconfigured pool management to interface %1").arg(pifName));
}

void NetworkingActionHelpers::clearIP(AsyncOperation* action, const QString& pifRef, int hi)
{
    // Don't clear IP if PIF is used by clustering
    if (isUsedByClustering(action->connection(), pifRef))
    {
        qDebug() << "Skipping IP clear for clustering PIF" << pifRef;
        action->setPercentComplete(hi);
        return;
    }

    QVariantMap pifRecord = getPIFRecord(action, pifRef);
    QString pifName = pifRecord.value("device").toString();

    qDebug() << "Removing IP address from" << pifName << pifRef;
    action->setDescription(QString("Bringing down interface %1...").arg(pifName));

    QString taskRef = XenAPI::PIF::async_reconfigure_ip(action->session(), pifRef,
                                                        "None", "", "", "", "");
    action->pollToCompletion(taskRef, action->percentComplete(), hi);

    qDebug() << "Removed IP address from" << pifName << pifRef;
    action->setDescription(QString("Brought down interface %1").arg(pifName));
}

void NetworkingActionHelpers::waitForMembersToRecover(XenConnection* connection, const QString& poolRef)
{
    const int retryLimit = 60;
    int retryAttempt = 0;

    QList<QString> deadHosts;

    QList<QVariantMap> hosts = connection->GetCache()->GetAllData("host");
    QString coordinatorRef = getCoordinatorRef(connection);

    int totalHosts = 0;
    for (const QVariantMap& host : hosts)
    {
        if (host.value("_ref").toString() != coordinatorRef)
        {
            totalHosts++;
        }
    }

    // Wait for supporters to go offline
    while (deadHosts.count() < totalHosts && retryAttempt <= retryLimit)
    {
        hosts = connection->GetCache()->GetAllData("host");
        for (const QVariantMap& host : hosts)
        {
            QString hostRef = host.value("_ref").toString();
            QString hostUuid = host.value("uuid").toString();

            if (hostRef == coordinatorRef)
            {
                continue;
            }

            bool isLive = host.value("live", true).toBool();
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
        hosts = connection->GetCache()->GetAllData("host");
        for (const QVariantMap& host : hosts)
        {
            QString hostRef = host.value("_ref").toString();
            QString hostUuid = host.value("uuid").toString();

            if (hostRef == coordinatorRef)
            {
                continue;
            }

            bool isLive = host.value("live", true).toBool();
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

void NetworkingActionHelpers::reconfigureManagement(AsyncOperation* action,
                                                    const QString& downPifRef,
                                                    const QString& upPifRef,
                                                    bool thisHost,
                                                    bool lockPif,
                                                    int hi,
                                                    bool bringDownDownPif)
{
    QVariantMap downPif = getPIFRecord(action, downPifRef);
    QVariantMap upPif = getPIFRecord(action, upPifRef);

    // Verify both PIFs are on same host
    if (getHostFromPIF(downPif) != getHostFromPIF(upPif))
    {
        throw std::runtime_error("PIFs must be on same host for management reconfiguration");
    }

    int lo = action->percentComplete();
    int inc = (hi - lo) / 3;

    // Phase 1: Depurpose down_pif if requested
    if (bringDownDownPif)
    {
        forSomeHosts(action, downPifRef, thisHost, lockPif, lo + inc, depurpose);
    } else
    {
        action->setPercentComplete(lo + inc);
    }

    // Phase 2: Reconfigure management to up_pif
    forSomeHosts(action, upPifRef, thisHost, lockPif, lo + 2 * inc, reconfigureManagement_);

    // Phase 3: Clear IP from down_pif if requested
    if (bringDownDownPif)
    {
        forSomeHosts(action, downPifRef, thisHost, lockPif, hi, clearIP);
    } else
    {
        action->setPercentComplete(hi);
    }
}

void NetworkingActionHelpers::poolReconfigureManagement(AsyncOperation* action,
                                                        const QString& poolRef,
                                                        const QString& upPifRef,
                                                        const QString& downPifRef,
                                                        int hi)
{
    QVariantMap downPif = getPIFRecord(action, downPifRef);
    QVariantMap upPif = getPIFRecord(action, upPifRef);

    // Verify both PIFs are on same host
    if (getHostFromPIF(downPif) != getHostFromPIF(upPif))
    {
        throw std::runtime_error("PIFs must be on same host for management reconfiguration");
    }

    int lo = action->percentComplete();
    int inc = (hi - lo) / 3;

    // Phase 1: Pool management reconfigure
    poolManagementReconfigure_(action, upPifRef, lo + inc);

    // Phase 2: Wait for supporters to recover
    action->setDescription("Waiting for pool members to recover...");
    waitForMembersToRecover(action->connection(), poolRef);

    // Phase 3: Clear IP on supporters, then coordinator
    forSomeHosts(action, downPifRef, false, true, lo + 2 * inc, clearIP);
    forSomeHosts(action, downPifRef, true, true, hi, clearIP);
}

void NetworkingActionHelpers::bringUp(AsyncOperation* action,
                                      const QString& newPifRef,
                                      const QString& newIp,
                                      const QString& existingPifRef,
                                      int hi)
{
    QVariantMap newPif = getPIFRecord(action, newPifRef);
    QVariantMap existingPif = getPIFRecord(action, existingPifRef);

    QString pifName = existingPif.value("device").toString();
    QString managementPurpose = getManagementPurpose(newPif);
    bool isPrimary = managementPurpose.isEmpty();

    int lo = action->percentComplete();
    int inc = (hi - lo) / (isPrimary ? 2 : 3);

    qDebug() << "Bringing PIF" << pifName << existingPifRef << "up as"
             << newIp << "/" << newPif.value("netmask").toString()
             << newPif.value("gateway").toString() << newPif.value("DNS").toString();
    action->setDescription(QString("Bringing up interface %1...").arg(pifName));

    // Set disallow_unplug and management_purpose
    XenAPI::PIF::set_disallow_unplug(action->session(), existingPifRef, !isPrimary);

    if (!managementPurpose.isEmpty())
    {
        XenAPI::PIF::add_to_other_config(action->session(), existingPifRef,
                                         "management_purpose", managementPurpose);
    }

    action->setPercentComplete(lo + inc);

    // Reconfigure IP
    reconfigureIP(action, newPifRef, existingPifRef, newIp, lo + 2 * inc);

    // Plug if secondary management
    if (!isPrimary)
    {
        plug(action, existingPifRef, hi);
    } else
    {
        action->setPercentComplete(hi);
    }

    qDebug() << "Brought PIF" << pifName << existingPifRef << "up";
    action->setDescription(QString("Brought up interface %1").arg(pifName));
}

void NetworkingActionHelpers::bringUp(AsyncOperation* action,
                                      const QString& newPifRef,
                                      const QString& existingPifRef,
                                      int hi)
{
    QVariantMap newPif = getPIFRecord(action, newPifRef);
    QString newIp = newPif.value("IP").toString();
    bringUp(action, newPifRef, newIp, existingPifRef, hi);
}

void NetworkingActionHelpers::bringUp(AsyncOperation* action,
                                      const QString& newPifRef,
                                      bool thisHost,
                                      int hi)
{
    forSomeHosts(action, newPifRef, thisHost, false, hi,
                 [](AsyncOperation* a, const QString& pifRef, int h) {
                     bringUp(a, pifRef, pifRef, h);
                 });
}

void NetworkingActionHelpers::bringDown(AsyncOperation* action,
                                        const QString& pifRef,
                                        int hi)
{
    int lo = action->percentComplete();
    int mid = (lo + hi) / 2;

    depurpose(action, pifRef, mid);
    clearIP(action, pifRef, hi);
}

void NetworkingActionHelpers::forSomeHosts(AsyncOperation* action,
                                           const QString& pifRef,
                                           bool thisHost,
                                           bool lockPif,
                                           int hi,
                                           PIFMethod pifMethod)
{
    QVariantMap pifRecord = getPIFRecord(action, pifRef);
    if (pifRecord.isEmpty())
    {
        qWarning() << "PIF" << pifRef << "not found";
        return;
    }

    QString networkRef = getNetworkFromPIF(pifRecord);
    QString pifHostRef = getHostFromPIF(pifRecord);
    QString coordinatorRef = getCoordinatorRef(action->connection());

    // Find all PIFs in the same network
    QList<QVariantMap> allPifs = action->connection()->GetCache()->GetAllData("pif");
    QList<QString> pifsToReconfigure;

    for (const QVariantMap& pif : allPifs)
    {
        if (pif.value("network").toString() != networkRef)
        {
            continue;
        }

        QString hostRef = pif.value("host").toString();
        bool isCoordinator = (hostRef == coordinatorRef);

        // Filter by thisHost parameter
        if (thisHost && isCoordinator && hostRef == pifHostRef)
        {
            pifsToReconfigure.append(pif.value("_ref").toString());
        } else if (!thisHost && !isCoordinator)
        {
            pifsToReconfigure.append(pif.value("_ref").toString());
        }
    }

    if (pifsToReconfigure.isEmpty())
    {
        action->setPercentComplete(hi);
        return;
    }

    // Execute method on each PIF
    int lo = action->percentComplete();
    int inc = (hi - lo) / pifsToReconfigure.count();

    for (const QString& pRef : pifsToReconfigure)
    {
        // Note: lockPif parameter not yet implemented (requires PIF.Locked property)
        try
        {
            lo += inc;
            pifMethod(action, pRef, lo);
        } catch (const std::exception& e)
        {
            qWarning() << "Error processing PIF" << pRef << ":" << e.what();
            throw;
        }
    }
}

void NetworkingActionHelpers::reconfigureSinglePrimaryManagement(AsyncOperation* action,
                                                                 const QString& srcPifRef,
                                                                 const QString& destPifRef,
                                                                 int hi)
{
    // Copy IP config from src to dest (conceptually - we'll use src's values for dest)
    QVariantMap srcPif = getPIFRecord(action, srcPifRef);

    int lo = action->percentComplete();
    int mid = (lo + hi) / 2;

    // Bring up destination with source's IP config
    QString srcIp = srcPif.value("IP").toString();
    bringUp(action, srcPifRef, srcIp, destPifRef, mid);

    // Reconfigure management
    reconfigureManagement(action, srcPifRef, destPifRef, true, false, hi, true);
}

void NetworkingActionHelpers::reconfigurePrimaryManagement(AsyncOperation* action,
                                                           const QString& srcPifRef,
                                                           const QString& destPifRef,
                                                           int hi)
{
    int lo = action->percentComplete();
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

void NetworkingActionHelpers::reconfigureIP(AsyncOperation* action,
                                            const QString& newPifRef,
                                            const QString& existingPifRef,
                                            const QString& ip,
                                            int hi)
{
    QVariantMap newPif = getPIFRecord(action, newPifRef);
    QVariantMap existingPif = getPIFRecord(action, existingPifRef);

    QString pifName = existingPif.value("device").toString();
    qDebug() << "Reconfiguring IP on" << pifName << existingPifRef;

    QString mode = newPif.value("ip_configuration_mode").toString();
    QString netmask = newPif.value("netmask").toString();
    QString gateway = newPif.value("gateway").toString();
    QString dns = newPif.value("DNS").toString();

    QString taskRef = XenAPI::PIF::async_reconfigure_ip(action->session(), existingPifRef,
                                                        mode, ip, netmask, gateway, dns);
    action->pollToCompletion(taskRef, action->percentComplete(), hi);

    qDebug() << "Reconfigured IP on" << pifName << existingPifRef;
}

void NetworkingActionHelpers::plug(AsyncOperation* action,
                                   const QString& pifRef,
                                   int hi)
{
    QVariantMap pifRecord = getPIFRecord(action, pifRef);
    bool currentlyAttached = pifRecord.value("currently_attached", false).toBool();

    if (!currentlyAttached)
    {
        QString pifName = pifRecord.value("device").toString();
        qDebug() << "Plugging" << pifName << pifRef;

        QString taskRef = XenAPI::PIF::async_plug(action->session(), pifRef);
        action->pollToCompletion(taskRef, action->percentComplete(), hi);

        qDebug() << "Plugged" << pifName << pifRef;
    } else
    {
        action->setPercentComplete(hi);
    }
}
