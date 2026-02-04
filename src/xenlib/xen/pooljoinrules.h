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

#ifndef POOLJOINRULES_H
#define POOLJOINRULES_H

#include <QList>
#include <QSharedPointer>
#include <QString>

class XenConnection;
class Host;
class Pool;
class XenObject;

class PoolJoinRules
{
    public:
        // Keep enum order aligned with C# PoolJoinRules.Reason for sorting.
        enum class Reason
        {
            WillBeCoordinator,
            Allowed,
            Connecting,
            CoordinatorNotConnected,
            CoordinatorConnecting,
            DifferentAdConfig,
            HasRunningVMs,
            HasSharedStorage,
            IsAPool,
            LicenseRestriction,
            NotSameLinuxPack,
            LicensedHostUnlicensedCoordinator,
            UnlicensedHostLicensedCoordinator,
            LicenseMismatch,
            CoordinatorPoolMaxNumberHostReached,
            WillExceedPoolMaxSize,
            DifferentServerVersion,
            DifferentHomogeneousUpdatesFromCoordinator,
            DifferentHomogeneousUpdatesFromPool,
            DifferentCPUs,
            DifferentNetworkBackends,
            CoordinatorHasHA,
            NotPhysicalPif,
            NonCompatibleManagementInterface,
            WrongRoleOnCoordinator,
            WrongRoleOnSupporter,
            HasClusteringEnabled,
            WrongNumberOfIpsCluster,
            WrongNumberOfIpsBond,
            NotConnected,
            TlsVerificationOnlyOnPool,
            TlsVerificationOnlyOnPoolJoiner,
            TlsVerificationOnlyOnCoordinator,
            TlsVerificationOnlyOnCoordinatorJoiner
        };

        static Reason CanJoinPool(XenConnection* supporterConnection,
                                  XenConnection* coordinatorConnection,
                                  bool allowLicenseUpgrade,
                                  bool allowSupporterAdConfig,
                                  int poolSizeIncrement = 1);

        static QString ReasonMessage(Reason reason);

        static bool CompatibleCPUs(const QSharedPointer<Host>& supporter,
                                   const QSharedPointer<Host>& coordinator);
        static bool CompatibleAdConfig(const QSharedPointer<Host>& supporter,
                                       const QSharedPointer<Host>& coordinator,
                                       bool allowSupporterConfig);
        static bool FreeHostPaidCoordinator(const QSharedPointer<Host>& supporter,
                                            const QSharedPointer<Host>& coordinator,
                                            bool allowLicenseUpgrade);
        static bool HostHasMoreFeatures(const QSharedPointer<Host>& supporter,
                                        const QSharedPointer<Pool>& pool);
        static bool HostHasFewerFeatures(const QSharedPointer<Host>& supporter,
                                         const QSharedPointer<Pool>& pool);
        static bool FewerFeatures(const QVariantMap& cpuInfoA, const QVariantMap& cpuInfoB);

        static QList<QString> HomogeneousSuppPacksDiffering(const QList<QSharedPointer<Host>>& supporters,
                                                            const QSharedPointer<XenObject>& poolOrCoordinator);

        static bool WillExceedPoolMaxSize(XenConnection* connection, int poolSizeIncrement);

        static bool HasSupporterAnyNonPhysicalPif(XenConnection* supporterConnection);
        static bool HasCompatibleManagementInterface(XenConnection* supporterConnection);
        static bool HasIpForClusterNetwork(XenConnection* coordinatorConnection,
                                           const QSharedPointer<Host>& supporterHost,
                                           bool* clusterHostInBond);

    private:
        static QSharedPointer<Host> GetCoordinator(XenConnection* connection);
        static QSharedPointer<Pool> GetPoolOfOne(XenConnection* connection);
        static bool IsAPool(XenConnection* connection);
        static bool LicenseRestriction(const QSharedPointer<Host>& host);
        static bool SameLinuxPack(const QSharedPointer<Host>& supporter,
                                  const QSharedPointer<Host>& coordinator);
        static bool DifferentServerVersion(const QSharedPointer<Host>& supporter,
                                           const QSharedPointer<Host>& coordinator);
        static bool DifferentHomogeneousUpdates(const QSharedPointer<Host>& supporter,
                                                const QSharedPointer<Host>& coordinator);
        static bool DifferentNetworkBackends(const QSharedPointer<Host>& supporter,
                                             const QSharedPointer<Host>& coordinator);
        static bool HasSharedStorage(XenConnection* connection);
        static bool HasRunningVMs(XenConnection* connection);
        static bool CoordinatorPoolMaxNumberHostReached(XenConnection* connection);
        static bool RoleOK(XenConnection* connection);
        static bool HaEnabled(XenConnection* connection);
        static bool FeatureForbidden(XenConnection* connection, bool (Host::*feature)() const);
        static bool PaidHostFreeCoordinator(const QSharedPointer<Host>& supporter,
                                            const QSharedPointer<Host>& coordinator);
        static bool LicenseMismatch(const QSharedPointer<Host>& supporter,
                                    const QSharedPointer<Host>& coordinator);

        static bool maskableTo(const QString& maskType,
                               const QString& from,
                               const QString& to,
                               const QString& featureMask);
        static bool fewerFeatures(const QString& featureSetA, const QString& featureSetB);
};

#endif // POOLJOINRULES_H
