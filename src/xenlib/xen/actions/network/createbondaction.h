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

#ifndef CREATEBONDACTION_H
#define CREATEBONDACTION_H

#include "../../asyncoperation.h"
#include <QStringList>
#include <QVariantMap>

/**
 * @brief CreateBondAction - Creates a network bond
 *
 * Qt port of C# XenAdmin.Actions.CreateBondAction.
 * Creates a bonded network across multiple hosts with specified PIFs.
 * Uses NetworkingActionHelpers for management interface name migration.
 *
 * C# path: XenModel/Actions/Network/CreateBondAction.cs
 *
 * Features:
 * - Creates network and bonds on all pool hosts
 * - Processes hosts in coordinator-last order for stability
 * - Moves management interface names from bond members to bond interface
 * - Handles cleanup on failure (destroys bonds and network)
 *
 * TODO:
 * - Implement network/bond/PIF locking (Locked property)
 */
class CreateBondAction : public AsyncOperation
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param connection XenConnection
     * @param networkName Name for the new bond network
     * @param pifRefs List of PIF references on coordinator to bond
     * @param autoplug Whether to mark network as AutoPlug
     * @param mtu MTU for the bond
     * @param bondMode Bond mode (e.g., "balance-slb", "lacp")
     * @param hashingAlgorithm Hashing algorithm for LACP mode (e.g., "src_mac", "tcpudp_ports")
     * @param parent Parent QObject
     */
    CreateBondAction(XenConnection* connection,
                     const QString& networkName,
                     const QStringList& pifRefs,
                     bool autoplug,
                     qint64 mtu,
                     const QString& bondMode,
                     const QString& hashingAlgorithm,
                     QObject* parent = nullptr);

protected:
    void run() override;

private:
    struct NewBond
    {
        QString bondRef;
        QString bondInterfaceRef;
        QStringList memberRefs;
    };

    /**
     * @brief Get hosts ordered with coordinator last
     * @return List of host records
     */
    QList<QVariantMap> getHostsCoordinatorLast() const;

    /**
     * @brief Find PIFs on a host matching coordinator PIF device names
     * @param hostRef Host opaque reference
     * @return List of matching PIF references
     */
    QStringList findMatchingPIFsOnHost(const QString& hostRef) const;

    /**
     * @brief Cleanup on error: revert management, destroy bonds, destroy network
     */
    void cleanupOnError();

    QString m_networkName;
    QStringList m_pifRefs;
    bool m_autoplug;
    qint64 m_mtu;
    QString m_bondMode;
    QString m_hashingAlgorithm;
    QString m_networkRef;
    QList<NewBond> m_newBonds;
};

#endif // CREATEBONDACTION_H
