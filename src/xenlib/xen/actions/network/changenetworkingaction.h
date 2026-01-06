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

#ifndef CHANGENETWORKINGACTION_H
#define CHANGENETWORKINGACTION_H

#include "../../asyncoperation.h"
#include <QStringList>
#include <QVariantMap>

/**
 * @brief ChangeNetworkingAction - Reconfigures host networking
 *
 * Qt port of C# XenAdmin.Actions.ChangeNetworkingAction.
 * Handles complex networking changes including PIF configuration,
 * management interface reconfiguration, and pool coordination.
 *
 * C# path: XenModel/Actions/Network/ChangeNetworkingAction.cs
 *
 * Uses NetworkingActionHelpers for coordinated network changes across
 * pool members with proper ordering (supporters first, then coordinator).
 *
 * Supports:
 * - Pool-wide and single-host operations
 * - Management interface migration
 * - Coordinated PIF bring-up/bring-down
 * - Pool.management_reconfigure with fallback to host-by-host
 *
 * TODO:
 * - Cluster/GFS2 SR coordination (disable clustering before network changes)
 * - IP address range calculation for static IPs across pool members
 * - Feature checks (Host.RestrictManagementOnVLAN)
 */
class ChangeNetworkingAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Constructor
         * @param connection XenConnection
         * @param pifRefsToReconfigure List of PIF refs to bring up/reconfigure
         * @param pifRefsToDisable List of PIF refs to bring down
         * @param newManagementPifRef New management PIF ref (or empty if unchanged)
         * @param oldManagementPifRef Old management PIF ref (or empty if unchanged)
         * @param parent Parent QObject
         */
        ChangeNetworkingAction(XenConnection* connection,
                               const QStringList& pifRefsToReconfigure,
                               const QStringList& pifRefsToDisable,
                               const QString& newManagementPifRef,
                               const QString& oldManagementPifRef,
                               QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        /**
         * @brief Reconfigure a PIF (bring up or down) on appropriate hosts
         * @param pifRef PIF reference
         * @param up true to bring up, false to bring down
         * @param thisHost true for coordinator, false for supporters
         * @param hi Target progress percentage
         */
        void reconfigure(const QString& pifRef, bool up, bool thisHost, int hi);

        /**
         * @brief Bring up a PIF with IP configuration
         * @param newPifRef Source PIF with configuration
         * @param existingPifRef Target PIF to configure
         * @param hi Target progress percentage
         */
        void bringUp(const QString& newPifRef, const QString& existingPifRef, int hi);

        QStringList m_pifRefsToReconfigure;
        QStringList m_pifRefsToDisable;
        QString m_newManagementPifRef;
        QString m_oldManagementPifRef;
};

#endif // CHANGENETWORKINGACTION_H
