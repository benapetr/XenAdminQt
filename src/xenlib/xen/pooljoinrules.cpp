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

#include "pooljoinrules.h"

#include "host.h"
#include "pool.h"
#include "sr.h"
#include "vm.h"
#include "pif.h"
#include "bond.h"
#include "vlan.h"
#include "hostcpu.h"
#include "clusterhost.h"
#include "poolupdate.h"
#include "xenobject.h"
#include "network/connection.h"
#include "../xencache.h"

#include <QCoreApplication>
#include <QDebug>
#include <QStringList>
#include <QtGlobal>

namespace
{
    int compareProductVersion(const QString& left, const QString& right)
    {
        if (left == right)
            return 0;

        const QStringList leftParts = left.split('.', Qt::SkipEmptyParts);
        const QStringList rightParts = right.split('.', Qt::SkipEmptyParts);
        const int maxParts = std::max(leftParts.size(), rightParts.size());

        for (int i = 0; i < maxParts; ++i)
        {
            bool leftOk = false;
            bool rightOk = false;
            const int leftVal = (i < leftParts.size()) ? leftParts.at(i).toInt(&leftOk) : 0;
            const int rightVal = (i < rightParts.size()) ? rightParts.at(i).toInt(&rightOk) : 0;

            if (!leftOk || !rightOk)
            {
                const QString leftStr = (i < leftParts.size()) ? leftParts.at(i) : QString();
                const QString rightStr = (i < rightParts.size()) ? rightParts.at(i) : QString();
                const int stringCompare = QString::compare(leftStr, rightStr, Qt::CaseInsensitive);
                if (stringCompare != 0)
                    return stringCompare;
                continue;
            }

            if (leftVal < rightVal)
                return -1;
            if (leftVal > rightVal)
                return 1;
        }

        return 0;
    }

    bool isVersionAtLeast(const QString& version, const QString& minimum)
    {
        if (version.isEmpty())
            return false;
        return compareProductVersion(version, minimum) >= 0;
    }

    bool elyOrGreater(const QSharedPointer<Host>& host)
    {
        if (!host)
            return true;
        return isVersionAtLeast(host->PlatformVersion(), "2.1.1");
    }

    bool falconOrGreater(const QSharedPointer<Host>& host)
    {
        if (!host)
            return true;
        return isVersionAtLeast(host->PlatformVersion(), "2.2.50");
    }
}

PoolJoinRules::Reason PoolJoinRules::CanJoinPool(XenConnection* supporterConnection,
                                                 XenConnection* coordinatorConnection,
                                                 bool allowLicenseUpgrade,
                                                 bool allowSupporterAdConfig,
                                                 int poolSizeIncrement)
{
    if (!supporterConnection || !supporterConnection->IsConnected())
        return Reason::NotConnected;

    QSharedPointer<Host> supporterHost = GetCoordinator(supporterConnection);
    if (!supporterHost)
        return Reason::Connecting;

    if (LicenseRestriction(supporterHost))
        return Reason::LicenseRestriction;

    if (IsAPool(supporterConnection))
        return Reason::IsAPool;

    if (!coordinatorConnection || !coordinatorConnection->IsConnected())
        return Reason::CoordinatorNotConnected;

    QSharedPointer<Host> coordinatorHost = GetCoordinator(coordinatorConnection);
    if (!coordinatorHost)
        return Reason::CoordinatorConnecting;

    if (supporterConnection == coordinatorConnection)
        return Reason::WillBeCoordinator;

    if (!RoleOK(coordinatorConnection))
        return Reason::WrongRoleOnCoordinator;

    if (!CompatibleCPUs(supporterHost, coordinatorHost))
        return Reason::DifferentCPUs;

    if (DifferentServerVersion(supporterHost, coordinatorHost))
    {
        qDebug() << "PoolJoinRules: DifferentServerVersion"
                 << "supporter=" << (supporterHost ? supporterHost->GetName() : QString())
                 << "coordinator=" << (coordinatorHost ? coordinatorHost->GetName() : QString());
        return Reason::DifferentServerVersion;
    }

    if (DifferentHomogeneousUpdates(supporterHost, coordinatorHost))
    {
        const QSharedPointer<Pool> pool = coordinatorConnection->GetCache() ? coordinatorConnection->GetCache()->GetPool() : QSharedPointer<Pool>();
        return (pool && pool->GetHosts().size() > 1)
            ? Reason::DifferentHomogeneousUpdatesFromPool
            : Reason::DifferentHomogeneousUpdatesFromCoordinator;
    }

    if (FreeHostPaidCoordinator(supporterHost, coordinatorHost, allowLicenseUpgrade))
        return Reason::UnlicensedHostLicensedCoordinator;

    if (PaidHostFreeCoordinator(supporterHost, coordinatorHost))
        return Reason::LicensedHostUnlicensedCoordinator;

    if (LicenseMismatch(supporterHost, coordinatorHost))
        return Reason::LicenseMismatch;

    if (CoordinatorPoolMaxNumberHostReached(coordinatorConnection))
        return Reason::CoordinatorPoolMaxNumberHostReached;

    if (WillExceedPoolMaxSize(coordinatorConnection, poolSizeIncrement))
        return Reason::WillExceedPoolMaxSize;

    if (!SameLinuxPack(supporterHost, coordinatorHost))
        return Reason::NotSameLinuxPack;

    if (!RoleOK(supporterConnection))
        return Reason::WrongRoleOnSupporter;

    if (HasSharedStorage(supporterConnection))
        return Reason::HasSharedStorage;

    if (HasRunningVMs(supporterConnection))
        return Reason::HasRunningVMs;

    if (DifferentNetworkBackends(supporterHost, coordinatorHost))
        return Reason::DifferentNetworkBackends;

    if (!CompatibleAdConfig(supporterHost, coordinatorHost, allowSupporterAdConfig))
        return Reason::DifferentAdConfig;

    if (HaEnabled(coordinatorConnection))
        return Reason::CoordinatorHasHA;

    if (FeatureForbidden(supporterConnection, &Host::RestrictManagementOnVLAN) && HasSupporterAnyNonPhysicalPif(supporterConnection))
        return Reason::NotPhysicalPif;

    if (!FeatureForbidden(supporterConnection, &Host::RestrictManagementOnVLAN) && !HasCompatibleManagementInterface(supporterConnection))
        return Reason::NonCompatibleManagementInterface;

    const QList<QSharedPointer<ClusterHost>> clusterHosts = supporterConnection->GetCache()
        ? supporterConnection->GetCache()->GetAll<ClusterHost>(XenObjectType::ClusterHost)
        : QList<QSharedPointer<ClusterHost>>();
    if (!clusterHosts.isEmpty())
        return Reason::HasClusteringEnabled;

    bool clusterHostInBond = false;
    if (!HasIpForClusterNetwork(coordinatorConnection, supporterHost, &clusterHostInBond))
        return clusterHostInBond ? Reason::WrongNumberOfIpsBond : Reason::WrongNumberOfIpsCluster;

    const QSharedPointer<Pool> coordinatorPool = GetPoolOfOne(coordinatorConnection);
    const QSharedPointer<Pool> supporterPool = GetPoolOfOne(supporterConnection);

    if (coordinatorPool && supporterPool)
    {
        if (coordinatorPool->TLSVerificationEnabled() && !supporterPool->TLSVerificationEnabled())
            return (coordinatorConnection->GetCache() && coordinatorConnection->GetCache()->GetPool())
                ? Reason::TlsVerificationOnlyOnPool
                : Reason::TlsVerificationOnlyOnCoordinator;

        if (!coordinatorPool->TLSVerificationEnabled() && supporterPool->TLSVerificationEnabled())
            return (coordinatorConnection->GetCache() && coordinatorConnection->GetCache()->GetPool())
                ? Reason::TlsVerificationOnlyOnPoolJoiner
                : Reason::TlsVerificationOnlyOnCoordinatorJoiner;
    }

    return Reason::Allowed;
}

QString PoolJoinRules::ReasonMessage(Reason reason)
{
    switch (reason)
    {
        case Reason::WillBeCoordinator:
            return QCoreApplication::translate("PoolJoinRules", "Coordinator");
        case Reason::Allowed:
            return QString();
        case Reason::Connecting:
            return QCoreApplication::translate("PoolJoinRules", "Connecting");
        case Reason::CoordinatorNotConnected:
            return QCoreApplication::translate("PoolJoinRules", "Coordinator is not connected");
        case Reason::CoordinatorConnecting:
            return QCoreApplication::translate("PoolJoinRules", "Coordinator is connecting");
        case Reason::DifferentAdConfig:
            return QCoreApplication::translate("PoolJoinRules", "External authentication configuration differs");
        case Reason::HasRunningVMs:
            return QCoreApplication::translate("PoolJoinRules", "Host has running VMs");
        case Reason::HasSharedStorage:
            return QCoreApplication::translate("PoolJoinRules", "Host has shared storage");
        case Reason::IsAPool:
            return QCoreApplication::translate("PoolJoinRules", "Host is already in a pool");
        case Reason::LicenseRestriction:
            return QCoreApplication::translate("PoolJoinRules", "Pooling is restricted by the license");
        case Reason::NotSameLinuxPack:
            return QCoreApplication::translate("PoolJoinRules", "Linux pack mismatch");
        case Reason::LicensedHostUnlicensedCoordinator:
            return QCoreApplication::translate("PoolJoinRules", "Licensed host cannot join an unlicensed coordinator");
        case Reason::UnlicensedHostLicensedCoordinator:
            return QCoreApplication::translate("PoolJoinRules", "Unlicensed host cannot join a licensed coordinator");
        case Reason::LicenseMismatch:
            return QCoreApplication::translate("PoolJoinRules", "License mismatch");
        case Reason::CoordinatorPoolMaxNumberHostReached:
            return QCoreApplication::translate("PoolJoinRules", "The pool has reached the maximum number of hosts");
        case Reason::WillExceedPoolMaxSize:
            return QCoreApplication::translate("PoolJoinRules", "The pool would exceed the maximum size");
        case Reason::DifferentServerVersion:
            return QCoreApplication::translate("PoolJoinRules", "Server versions differ");
        case Reason::DifferentHomogeneousUpdatesFromCoordinator:
            return QCoreApplication::translate("PoolJoinRules", "Homogeneous updates differ from coordinator");
        case Reason::DifferentHomogeneousUpdatesFromPool:
            return QCoreApplication::translate("PoolJoinRules", "Homogeneous updates differ from pool");
        case Reason::DifferentCPUs:
            return QCoreApplication::translate("PoolJoinRules", "CPU mismatch");
        case Reason::DifferentNetworkBackends:
            return QCoreApplication::translate("PoolJoinRules", "Network backend mismatch");
        case Reason::CoordinatorHasHA:
            return QCoreApplication::translate("PoolJoinRules", "Coordinator has HA enabled");
        case Reason::NotPhysicalPif:
            return QCoreApplication::translate("PoolJoinRules", "Host has non-physical management PIFs");
        case Reason::NonCompatibleManagementInterface:
            return QCoreApplication::translate("PoolJoinRules", "Management interface is not compatible");
        case Reason::WrongRoleOnCoordinator:
            return QCoreApplication::translate("PoolJoinRules", "Insufficient permissions on coordinator");
        case Reason::WrongRoleOnSupporter:
            return QCoreApplication::translate("PoolJoinRules", "Insufficient permissions on host");
        case Reason::HasClusteringEnabled:
            return QCoreApplication::translate("PoolJoinRules", "Clustering is enabled");
        case Reason::WrongNumberOfIpsCluster:
            return QCoreApplication::translate("PoolJoinRules", "Cluster network IP configuration mismatch");
        case Reason::WrongNumberOfIpsBond:
            return QCoreApplication::translate("PoolJoinRules", "Cluster bond IP configuration mismatch");
        case Reason::NotConnected:
            return QCoreApplication::translate("PoolJoinRules", "Disconnected");
        case Reason::TlsVerificationOnlyOnPool:
            return QCoreApplication::translate("PoolJoinRules", "TLS verification enabled on pool only");
        case Reason::TlsVerificationOnlyOnPoolJoiner:
            return QCoreApplication::translate("PoolJoinRules", "TLS verification enabled on joiner only");
        case Reason::TlsVerificationOnlyOnCoordinator:
            return QCoreApplication::translate("PoolJoinRules", "TLS verification enabled on coordinator only");
        case Reason::TlsVerificationOnlyOnCoordinatorJoiner:
            return QCoreApplication::translate("PoolJoinRules", "TLS verification enabled on joiner coordinator only");
    }

    return QString();
}

bool PoolJoinRules::CompatibleCPUs(const QSharedPointer<Host>& supporter,
                                   const QSharedPointer<Host>& coordinator)
{
    if (!supporter || !coordinator)
        return true;

    const QVariantMap supporterCpuInfo = supporter->GetCPUInfo();
    const QVariantMap coordinatorCpuInfo = coordinator->GetCPUInfo();

    if (!supporterCpuInfo.isEmpty() && !coordinatorCpuInfo.isEmpty())
    {
        const QString supporterVendor = supporterCpuInfo.value("vendor").toString();
        const QString coordinatorVendor = coordinatorCpuInfo.value("vendor").toString();
        if (!supporterVendor.isEmpty() && !coordinatorVendor.isEmpty() && supporterVendor != coordinatorVendor)
            return false;
        return true;
    }

    XenConnection* supporterConn = supporter->GetConnection();
    XenConnection* coordinatorConn = coordinator->GetConnection();
    if (!supporterConn || !coordinatorConn)
        return true;

    XenCache* supporterCache = supporterConn->GetCache();
    XenCache* coordinatorCache = coordinatorConn->GetCache();
    if (!supporterCache || !coordinatorCache)
        return true;

    const QStringList supporterCpuRefs = supporter->GetHostCPURefs();
    const QStringList coordinatorCpuRefs = coordinator->GetHostCPURefs();

    for (const QString& supporterCpuRef : supporterCpuRefs)
    {
        QSharedPointer<HostCPU> supporterCpu = supporterCache->ResolveObject<HostCPU>(XenObjectType::HostCPU, supporterCpuRef);
        if (!supporterCpu || !supporterCpu->IsValid())
            continue;
        for (const QString& coordinatorCpuRef : coordinatorCpuRefs)
        {
            QSharedPointer<HostCPU> coordinatorCpu = coordinatorCache->ResolveObject<HostCPU>(XenObjectType::HostCPU, coordinatorCpuRef);
            if (!coordinatorCpu || !coordinatorCpu->IsValid())
                continue;
            if (supporterCpu->Vendor() != coordinatorCpu->Vendor() ||
                supporterCpu->Family() != coordinatorCpu->Family() ||
                supporterCpu->Model() != coordinatorCpu->Model() ||
                supporterCpu->Flags() != coordinatorCpu->Flags())
            {
                return false;
            }
        }
    }

    return true;
}

bool PoolJoinRules::CompatibleAdConfig(const QSharedPointer<Host>& supporter,
                                       const QSharedPointer<Host>& coordinator,
                                       bool allowSupporterConfig)
{
    if (!supporter || !coordinator)
        return false;

    if (supporter->ExternalAuthType() != coordinator->ExternalAuthType() ||
        supporter->ExternalAuthServiceName() != coordinator->ExternalAuthServiceName())
    {
        if (supporter->ExternalAuthType().isEmpty() && allowSupporterConfig)
            return true;
        return false;
    }

    return true;
}

bool PoolJoinRules::FreeHostPaidCoordinator(const QSharedPointer<Host>& supporter,
                                            const QSharedPointer<Host>& coordinator,
                                            bool allowLicenseUpgrade)
{
    if (!supporter || !coordinator)
        return false;

    return supporter->IsFreeLicense() && !coordinator->IsFreeLicense() && !allowLicenseUpgrade;
}

bool PoolJoinRules::HostHasMoreFeatures(const QSharedPointer<Host>& supporter,
                                        const QSharedPointer<Pool>& pool)
{
    if (!supporter || !pool)
        return false;

    const QVariantMap supporterCpuInfo = supporter->GetCPUInfo();
    const QVariantMap poolCpuInfo = pool->CPUInfo();
    if (!supporterCpuInfo.isEmpty() && !poolCpuInfo.isEmpty())
        return FewerFeatures(poolCpuInfo, supporterCpuInfo);

    return false;
}

bool PoolJoinRules::HostHasFewerFeatures(const QSharedPointer<Host>& supporter,
                                         const QSharedPointer<Pool>& pool)
{
    if (!supporter || !pool)
        return false;

    const QVariantMap supporterCpuInfo = supporter->GetCPUInfo();
    const QVariantMap poolCpuInfo = pool->CPUInfo();
    if (!supporterCpuInfo.isEmpty() && !poolCpuInfo.isEmpty())
        return FewerFeatures(supporterCpuInfo, poolCpuInfo);

    return false;
}

bool PoolJoinRules::FewerFeatures(const QVariantMap& cpuInfoA, const QVariantMap& cpuInfoB)
{
    const QString featuresHvmA = cpuInfoA.value("features_hvm").toString();
    const QString featuresHvmB = cpuInfoB.value("features_hvm").toString();
    if (!featuresHvmA.isEmpty() && !featuresHvmB.isEmpty() && fewerFeatures(featuresHvmA, featuresHvmB))
        return true;

    const QString featuresPvA = cpuInfoA.value("features_pv").toString();
    const QString featuresPvB = cpuInfoB.value("features_pv").toString();
    if (!featuresPvA.isEmpty() && !featuresPvB.isEmpty() && fewerFeatures(featuresPvA, featuresPvB))
        return true;

    return false;
}

QList<QString> PoolJoinRules::HomogeneousSuppPacksDiffering(const QList<QSharedPointer<Host>>& supporters,
                                                            const QSharedPointer<XenObject>& poolOrCoordinator)
{
    QList<QSharedPointer<Host>> allHosts = supporters;
    if (poolOrCoordinator)
    {
        XenConnection* conn = poolOrCoordinator->GetConnection();
        if (conn && conn->GetCache())
        {
            const QList<QSharedPointer<Host>> poolHosts = conn->GetCache()->GetAll<Host>(XenObjectType::Host);
            for (const QSharedPointer<Host>& host : poolHosts)
                if (host && host->IsValid())
                    allHosts.append(host);
        }
    }

    QMap<QString, QString> homogeneousPacks;
    for (const QSharedPointer<Host>& host : allHosts)
    {
        if (!host || !host->IsValid())
            continue;
        const QList<Host::SuppPack> packs = host->SuppPacks();
        for (const Host::SuppPack& pack : packs)
        {
            if (pack.Homogeneous)
                homogeneousPacks.insert(pack.OriginatorAndName(), pack.Description);
        }
    }

    QList<QString> badPacks;

    for (auto it = homogeneousPacks.constBegin(); it != homogeneousPacks.constEnd(); ++it)
    {
        const QString packName = it.key();
        const QString packDesc = it.value();
        QList<QString> missingHosts;
        QString expectedVersion;
        bool versionsDiffer = false;

        for (const QSharedPointer<Host>& host : allHosts)
        {
            if (!host || !host->IsValid())
                continue;
            const QList<Host::SuppPack> packs = host->SuppPacks();
            const Host::SuppPack* matching = nullptr;
            for (const Host::SuppPack& pack : packs)
            {
                if (pack.OriginatorAndName() == packName)
                {
                    matching = &pack;
                    break;
                }
            }
            if (!matching)
            {
                missingHosts.append(host->GetName());
            }
            else if (expectedVersion.isEmpty())
            {
                expectedVersion = matching->Version;
            }
            else if (matching->Version != expectedVersion)
            {
                versionsDiffer = true;
            }
        }

        if (!missingHosts.isEmpty())
        {
            badPacks.append(QString("%1 (missing on: %2)").arg(packDesc, missingHosts.join("\n")));
        }
        else if (versionsDiffer)
        {
            badPacks.append(QString("%1 (versions differ)").arg(packDesc));
        }
    }

    return badPacks;
}

bool PoolJoinRules::WillExceedPoolMaxSize(XenConnection* connection, int poolSizeIncrement)
{
    if (!connection || !connection->GetCache())
        return false;

    const QList<QSharedPointer<Host>> hosts = connection->GetCache()->GetAll<Host>(XenObjectType::Host);
    if (hosts.isEmpty())
        return false;

    const bool restrictPoolSize = FeatureForbidden(connection, &Host::RestrictPoolSize);
    if (!restrictPoolSize)
        return false;

    return hosts.size() + poolSizeIncrement > 3;
}

bool PoolJoinRules::HasSupporterAnyNonPhysicalPif(XenConnection* supporterConnection)
{
    if (!supporterConnection || !supporterConnection->GetCache())
        return false;

    const QList<QSharedPointer<PIF>> pifs = supporterConnection->GetCache()->GetAll<PIF>(XenObjectType::PIF);
    for (const QSharedPointer<PIF>& pif : pifs)
    {
        if (!pif || !pif->IsValid())
            continue;
        if (!pif->IsPhysical())
            return true;
    }

    return false;
}

bool PoolJoinRules::HasCompatibleManagementInterface(XenConnection* supporterConnection)
{
    if (!supporterConnection || !supporterConnection->GetCache())
        return true;

    const QList<QSharedPointer<PIF>> pifs = supporterConnection->GetCache()->GetAll<PIF>(XenObjectType::PIF);
    int nonPhysical = 0;
    for (const QSharedPointer<PIF>& pif : pifs)
    {
        if (!pif || !pif->IsValid())
            continue;
        if (!pif->IsPhysical())
            ++nonPhysical;
    }

    if (nonPhysical == 0)
        return true;

    if (nonPhysical != 1)
        return false;

    for (const QSharedPointer<PIF>& pif : pifs)
    {
        if (!pif || !pif->IsValid())
            continue;
        if (!pif->IsPhysical() && pif->Management() && pif->GetVLAN() != -1)
            return true;
    }

    return false;
}

bool PoolJoinRules::HasIpForClusterNetwork(XenConnection* coordinatorConnection,
                                           const QSharedPointer<Host>& supporterHost,
                                           bool* clusterHostInBond)
{
    if (clusterHostInBond)
        *clusterHostInBond = false;

    if (!coordinatorConnection || !coordinatorConnection->GetCache() || !supporterHost)
        return true;

    const QList<QSharedPointer<ClusterHost>> clusterHosts = coordinatorConnection->GetCache()->GetAll<ClusterHost>(XenObjectType::ClusterHost);
    QSharedPointer<ClusterHost> clusterHost = clusterHosts.isEmpty() ? QSharedPointer<ClusterHost>() : clusterHosts.first();
    if (!clusterHost || !clusterHost->IsValid())
        return true;

    QSharedPointer<PIF> clusterHostPif = clusterHost->GetPIF();
    if (!clusterHostPif || !clusterHostPif->IsValid())
        return true;

    if (clusterHostPif->IsVLAN())
    {
        const QString vlanRef = clusterHostPif->VLANMasterOfRef();
        QSharedPointer<VLAN> vlan = coordinatorConnection->GetCache()->ResolveObject<VLAN>(XenObjectType::VLAN, vlanRef);
        if (vlan && vlan->IsValid())
        {
            const QString taggedPifRef = vlan->GetTaggedPIFRef();
            QSharedPointer<PIF> taggedPif = coordinatorConnection->GetCache()->ResolveObject<PIF>(XenObjectType::PIF, taggedPifRef);
            if (taggedPif && taggedPif->IsValid())
                clusterHostPif = taggedPif;
        }
    }

    const bool inBond = clusterHostPif->IsBondNIC();
    if (clusterHostInBond)
        *clusterHostInBond = inBond;

    QStringList ids;
    if (inBond)
    {
        const QStringList bondRefs = clusterHostPif->BondMasterOfRefs();
        for (const QString& bondRef : bondRefs)
        {
            QSharedPointer<Bond> bond = coordinatorConnection->GetCache()->ResolveObject<Bond>(XenObjectType::Bond, bondRef);
            if (!bond || !bond->IsValid())
                continue;
            const QStringList slaveRefs = bond->SlaveRefs();
            for (const QString& slaveRef : slaveRefs)
            {
                QSharedPointer<PIF> slave = coordinatorConnection->GetCache()->ResolveObject<PIF>(XenObjectType::PIF, slaveRef);
                if (!slave || !slave->IsValid())
                    continue;
                ids.append(slave->GetDevice());
            }
        }
    }
    else
    {
        ids.append(clusterHostPif->GetDevice());
    }

    int pifsWithIp = 0;
    const QList<QSharedPointer<PIF>> supporterPifs = supporterHost->GetPIFs();
    for (const QSharedPointer<PIF>& pif : supporterPifs)
    {
        if (!pif || !pif->IsValid())
            continue;
        if (pif->Management() && ids.contains(pif->GetDevice()))
            ++pifsWithIp;
    }

    return pifsWithIp == 1;
}

QSharedPointer<Host> PoolJoinRules::GetCoordinator(XenConnection* connection)
{
    if (!connection || !connection->GetCache())
        return QSharedPointer<Host>();

    QSharedPointer<Pool> pool = connection->GetCache()->GetPool();
    if (pool && pool->IsValid())
    {
        QSharedPointer<Host> master = pool->GetMasterHost();
        if (master && master->IsValid())
            return master;
    }

    QList<QSharedPointer<Host>> hosts = connection->GetCache()->GetAll<Host>(XenObjectType::Host);
    if (!hosts.isEmpty())
        return hosts.first();

    return QSharedPointer<Host>();
}

QSharedPointer<Pool> PoolJoinRules::GetPoolOfOne(XenConnection* connection)
{
    if (!connection || !connection->GetCache())
        return QSharedPointer<Pool>();

    return connection->GetCache()->GetPoolOfOne();
}

bool PoolJoinRules::IsAPool(XenConnection* connection)
{
    if (!connection || !connection->GetCache())
        return false;

    return connection->GetCache()->GetPool() != nullptr;
}

bool PoolJoinRules::LicenseRestriction(const QSharedPointer<Host>& host)
{
    return host && host->RestrictPooling();
}

bool PoolJoinRules::SameLinuxPack(const QSharedPointer<Host>& supporter,
                                  const QSharedPointer<Host>& coordinator)
{
    if (!supporter || !coordinator)
        return true;

    return supporter->LinuxPackPresent() == coordinator->LinuxPackPresent();
}

bool PoolJoinRules::DifferentServerVersion(const QSharedPointer<Host>& supporter,
                                           const QSharedPointer<Host>& coordinator)
{
    if (!supporter || !coordinator)
        return false;

    if (supporter->APIVersionMajor() != coordinator->APIVersionMajor() ||
        supporter->APIVersionMinor() != coordinator->APIVersionMinor())
    {
        qDebug() << "PoolJoinRules: API version mismatch"
                 << "supporter=" << supporter->APIVersionMajor() << supporter->APIVersionMinor()
                 << "coordinator=" << coordinator->APIVersionMajor() << coordinator->APIVersionMinor();
        return true;
    }

    if (falconOrGreater(supporter) && supporter->GetDatabaseSchema().isEmpty())
    {
        qDebug() << "PoolJoinRules: supporter database_schema missing (Falcon+)"
                 << "supporter=" << supporter->GetName()
                 << "platform_version=" << supporter->PlatformVersion();
        return true;
    }
    if (falconOrGreater(coordinator) && coordinator->GetDatabaseSchema().isEmpty())
    {
        qDebug() << "PoolJoinRules: coordinator database_schema missing (Falcon+)"
                 << "coordinator=" << coordinator->GetName()
                 << "platform_version=" << coordinator->PlatformVersion();
        return true;
    }

    if (supporter->GetDatabaseSchema() != coordinator->GetDatabaseSchema())
    {
        qDebug() << "PoolJoinRules: database_schema mismatch"
                 << "supporter=" << supporter->GetDatabaseSchema()
                 << "coordinator=" << coordinator->GetDatabaseSchema();
        return true;
    }

    if (!elyOrGreater(coordinator) && !elyOrGreater(supporter) &&
        supporter->BuildNumberRaw() != coordinator->BuildNumberRaw())
    {
        qDebug() << "PoolJoinRules: build_number mismatch (pre-Ely)"
                 << "supporter=" << supporter->BuildNumberRaw()
                 << "coordinator=" << coordinator->BuildNumberRaw();
        return true;
    }

    if (supporter->PlatformVersion() != coordinator->PlatformVersion())
    {
        qDebug() << "PoolJoinRules: platform_version mismatch"
                 << "supporter=" << supporter->PlatformVersion()
                 << "coordinator=" << coordinator->PlatformVersion();
        return true;
    }

    if (supporter->ProductBrand() != coordinator->ProductBrand())
    {
        qDebug() << "PoolJoinRules: product_brand mismatch"
                 << "supporter=" << supporter->ProductBrand()
                 << "coordinator=" << coordinator->ProductBrand();
        return true;
    }

    return false;
}

bool PoolJoinRules::DifferentHomogeneousUpdates(const QSharedPointer<Host>& supporter,
                                                const QSharedPointer<Host>& coordinator)
{
    if (!supporter || !coordinator)
        return false;

    if (!elyOrGreater(supporter) || !elyOrGreater(coordinator))
        return false;

    const QList<QSharedPointer<PoolUpdate>> coordinatorUpdates = coordinator->AppliedUpdates();
    const QList<QSharedPointer<PoolUpdate>> supporterUpdates = supporter->AppliedUpdates();

    QStringList coordinatorIds;
    QStringList supporterIds;

    for (const QSharedPointer<PoolUpdate>& update : coordinatorUpdates)
    {
        if (update && update->IsValid() && update->EnforceHomogeneity())
            coordinatorIds.append(update->GetUUID());
    }

    for (const QSharedPointer<PoolUpdate>& update : supporterUpdates)
    {
        if (update && update->IsValid() && update->EnforceHomogeneity())
            supporterIds.append(update->GetUUID());
    }

    if (coordinatorIds.size() != supporterIds.size())
        return true;

    for (const QString& id : coordinatorIds)
        if (!supporterIds.contains(id))
            return true;

    return false;
}

bool PoolJoinRules::DifferentNetworkBackends(const QSharedPointer<Host>& supporter,
                                             const QSharedPointer<Host>& coordinator)
{
    if (!supporter || !coordinator)
        return false;

    const QString supporterBackend = supporter->SoftwareVersion().value("network_backend").toString();
    const QString coordinatorBackend = coordinator->SoftwareVersion().value("network_backend").toString();
    if (!supporterBackend.isEmpty() && !coordinatorBackend.isEmpty() && supporterBackend != coordinatorBackend)
        return true;

    QSharedPointer<Pool> coordinatorPool = GetPoolOfOne(coordinator->GetConnection());
    QSharedPointer<Pool> supporterPool = GetPoolOfOne(supporter->GetConnection());
    if (coordinatorPool && supporterPool && coordinatorPool->vSwitchController() && supporterPool->vSwitchController())
        return coordinatorPool->VswitchController() != supporterPool->VswitchController();

    return false;
}

bool PoolJoinRules::HasSharedStorage(XenConnection* connection)
{
    if (!connection || !connection->GetCache())
        return false;

    const QList<QSharedPointer<SR>> srs = connection->GetCache()->GetAll<SR>(XenObjectType::SR);
    for (const QSharedPointer<SR>& sr : srs)
    {
        if (!sr || !sr->IsValid())
            continue;
        if (sr->IsShared() && !sr->IsToolsSR())
            return true;
    }

    return false;
}

bool PoolJoinRules::HasRunningVMs(XenConnection* connection)
{
    if (!connection || !connection->GetCache())
        return false;

    const QList<QSharedPointer<VM>> vms = connection->GetCache()->GetAll<VM>(XenObjectType::VM);
    for (const QSharedPointer<VM>& vm : vms)
    {
        if (!vm || !vm->IsValid())
            continue;
        if (vm->IsRealVM() && vm->IsRunning())
            return true;
    }

    return false;
}

bool PoolJoinRules::CoordinatorPoolMaxNumberHostReached(XenConnection* connection)
{
    if (!connection || !connection->GetCache())
        return false;

    const QList<QSharedPointer<Host>> hosts = connection->GetCache()->GetAll<Host>(XenObjectType::Host);
    if (hosts.isEmpty())
        return false;

    return FeatureForbidden(connection, &Host::RestrictPoolSize) && hosts.size() > 2;
}

bool PoolJoinRules::RoleOK(XenConnection* connection)
{
    Q_UNUSED(connection);
    // RBAC checks are not fully ported yet; assume allowed.
    return true;
}

bool PoolJoinRules::HaEnabled(XenConnection* connection)
{
    QSharedPointer<Pool> pool = GetPoolOfOne(connection);
    return pool && pool->HAEnabled();
}

bool PoolJoinRules::FeatureForbidden(XenConnection* connection, bool (Host::*feature)() const)
{
    if (!connection || !connection->GetCache())
        return false;

    const QList<QSharedPointer<Host>> hosts = connection->GetCache()->GetAll<Host>(XenObjectType::Host);
    for (const QSharedPointer<Host>& host : hosts)
    {
        if (!host || !host->IsValid())
            continue;
        if ((host.data()->*feature)())
            return true;
    }

    return false;
}

bool PoolJoinRules::PaidHostFreeCoordinator(const QSharedPointer<Host>& supporter,
                                            const QSharedPointer<Host>& coordinator)
{
    if (!supporter || !coordinator)
        return false;

    return !supporter->IsFreeLicense() && coordinator->IsFreeLicense();
}

bool PoolJoinRules::LicenseMismatch(const QSharedPointer<Host>& supporter,
                                    const QSharedPointer<Host>& coordinator)
{
    if (!supporter || !coordinator)
        return false;

    const QString supporterEdition = supporter->Edition();
    const QString coordinatorEdition = coordinator->Edition();

    const bool supporterFree = supporter->IsFreeLicense();
    const bool coordinatorFree = coordinator->IsFreeLicense();

    if (supporterFree || coordinatorFree)
        return false;

    return supporterEdition != coordinatorEdition;
}

bool PoolJoinRules::maskableTo(const QString& maskType,
                               const QString& from,
                               const QString& to,
                               const QString& featureMask)
{
    if (maskType == "no")
        return false;

    QString from2 = from;
    QString to2 = to;
    QString mask2 = featureMask;

    from2.remove(' ');
    from2.remove('-');
    to2.remove(' ');
    to2.remove('-');

    if (from2.length() != 32 || to2.length() != 32)
        return false;

    if (mask2.isEmpty())
        mask2 = QStringLiteral("ffffffffffffffffffffffffffffffff");
    else
    {
        mask2.remove(' ');
        mask2.remove('-');
        if (mask2.length() != 32)
            mask2 = QStringLiteral("ffffffffffffffffffffffffffffffff");
    }

    for (int i = 0; i < 2; ++i)
    {
        const QString fromPart = from2.mid(i * 16, 16);
        const QString toPart = to2.mid(i * 16, 16);
        const QString maskPart = mask2.mid(i * 16, 16);

        bool ok1 = false;
        bool ok2 = false;
        bool ok3 = false;
        const quint64 fromInt = fromPart.toULongLong(&ok1, 16);
        const quint64 toInt = toPart.toULongLong(&ok2, 16);
        const quint64 maskInt = maskPart.toULongLong(&ok3, 16);
        if (!ok1 || !ok2 || !ok3)
            return false;

        const quint64 maskedFrom = fromInt & maskInt;
        const quint64 maskedTo = toInt & maskInt;

        if (i == 1 && maskType == "base")
        {
            if (maskedFrom != maskedTo)
                return false;
        }
        else
        {
            if ((maskedFrom & maskedTo) != maskedTo)
                return false;
        }
    }

    return true;
}

bool PoolJoinRules::fewerFeatures(const QString& featureSetA, const QString& featureSetB)
{
    if (featureSetA.isEmpty() || featureSetB.isEmpty())
        return false;

    QString a = featureSetA;
    QString b = featureSetB;
    a.remove(' ');
    a.remove('-');
    b.remove(' ');
    b.remove('-');

    if (a.length() < b.length())
        a = a.leftJustified(b.length(), '0');
    if (b.length() < a.length())
        b = b.leftJustified(a.length(), '0');

    for (int i = 0; i < a.length() / 8; ++i)
    {
        bool ok1 = false;
        bool ok2 = false;
        const quint32 intA = a.mid(i * 8, 8).toUInt(&ok1, 16);
        const quint32 intB = b.mid(i * 8, 8).toUInt(&ok2, 16);
        if (!ok1 || !ok2)
            return false;
        if ((intA & intB) != intB)
            return true;
    }

    return false;
}
