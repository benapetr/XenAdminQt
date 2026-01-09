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

#ifndef NETWORKACTION_H
#define NETWORKACTION_H

#include "../../asyncoperation.h"
#include "../../network.h"
#include "../../pif.h"
#include <QSharedPointer>

/**
 * @brief NetworkAction - Create, update, or destroy networks
 *
 * Qt port of C# XenAdmin.Actions.NetworkAction.
 * Handles creation, modification, and deletion of both internal (private)
 * and external (VLAN) networks. Can also convert between network types.
 *
 * C# path: XenModel/Actions/Network/NetworkAction.cs
 *
 * Key capabilities:
 * - Create internal (private) networks
 * - Create external (VLAN) networks
 * - Destroy networks (with PIF cleanup)
 * - Update network properties
 * - Convert between internal/external (requires PIF recreation)
 */
class XENLIB_EXPORT NetworkAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Create an external (VLAN) network
         * @param network The network to create
         * @param basePif The PIF representing the physical NIC for the VLAN
         * @param vlan The VLAN tag
         * @param parent Parent QObject
         */
        NetworkAction(QSharedPointer<Network> network,
                      QSharedPointer<PIF> basePif,
                      qint64 vlan,
                      QObject* parent = nullptr);

        /**
         * @brief Create or destroy an internal (private) network
         * @param network The network to create/destroy
         * @param create True to create, false to destroy
         * @param parent Parent QObject
         */
        NetworkAction(QSharedPointer<Network> network,
                      bool create,
                      QObject* parent = nullptr);

        /**
         * @brief Update a network (properties and/or type)
         * @param network The modified network to save
         * @param changePIFs True if changing between internal/external (requires PIF recreation)
         * @param external True if the new network should be external (VLAN)
         * @param basePif PIF for VLAN creation (null if changePIFs is false or external is false)
         * @param vlan VLAN tag (ignored if changePIFs is false or external is false)
         * @param suppressHistory True to suppress history tracking
         * @param parent Parent QObject
         */
        NetworkAction(QSharedPointer<Network> network,
                      bool changePIFs,
                      bool external,
                      QSharedPointer<PIF> basePif,
                      qint64 vlan,
                      bool suppressHistory,
                      QObject* parent = nullptr);

        ~NetworkAction() override = default;

    protected:
        void run() override;

    private:
        enum class ActionType
        {
            Create,
            Destroy,
            Update
        };

        void destroyPIFs();
        void createVLAN(const QString& networkRef);

        QSharedPointer<Network> m_network;
        QSharedPointer<PIF> m_basePif;
        QList<QSharedPointer<PIF>> m_pifs;
        ActionType m_actionType;
        qint64 m_vlan;
        bool m_external;
        bool m_changePIFs;
};

#endif // NETWORKACTION_H
