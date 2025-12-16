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

#ifndef DESTROYBONDACTION_H
#define DESTROYBONDACTION_H

#include "../../asyncoperation.h"
#include <QStringList>

/**
 * @brief DestroyBondAction - Destroys a network bond
 *
 * Qt port of C# XenAdmin.Actions.DestroyBondAction.
 * Destroys a bond and its associated network across all pool hosts.
 * Uses NetworkingActionHelpers::moveManagementInterfaceName() to restore
 * management interface names to primary bond members before destruction.
 *
 * C# path: XenModel/Actions/Network/DestroyBondAction.cs
 *
 * Features:
 * - Finds all equivalent bonds across pool hosts (by device name)
 * - Moves management interface names from bonds to primary members
 * - Destroys bonds on all hosts
 * - Destroys network (if not used elsewhere)
 *
 * TODO:
 * - Implement lock management (Bond.Locked, PIF.Locked, Network.Locked)
 */
class DestroyBondAction : public AsyncOperation
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param connection XenConnection
     * @param bondRef Bond reference to destroy (typically coordinator bond)
     * @param parent Parent QObject
     */
    DestroyBondAction(XenConnection* connection,
                      const QString& bondRef,
                      QObject* parent = nullptr);

protected:
    void run() override;

private:
    struct BondInfo
    {
        QString bondRef;
        QString bondInterfaceRef;
        QString primarySlaveRef;
        QString networkRef;
        QString hostRef;
        QString hostName;
    };

    /**
     * @brief Find all bonds equivalent to m_bondRef across all hosts
     * @return List of bond information for all hosts
     */
    QList<BondInfo> findAllEquivalentBonds() const;

    QString m_bondRef;
    QString m_bondName;
};

#endif // DESTROYBONDACTION_H
