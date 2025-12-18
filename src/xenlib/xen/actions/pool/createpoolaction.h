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

#ifndef CREATEPOOLACTION_H
#define CREATEPOOLACTION_H

#include "../../asyncoperation.h"
#include <QString>
#include <QList>

class XenConnection;
class Host;

/**
 * @brief CreatePoolAction creates a new pool from a coordinator and optional member hosts.
 *
 * Matches C# XenModel/Actions/Pool/CreatePoolAction.cs
 *
 * Note: This is a simplified version. The C# implementation includes:
 * - License compatibility checks and relicensing for all members
 * - Active Directory configuration synchronization
 * - Non-shared SR cleanup on coordinator
 * These features are deferred for initial implementation.
 */

class CreatePoolAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Constructor for creating pool
         * @param coordinatorConnection Connection to host that will be coordinator
         * @param coordinator Host object that will be pool coordinator
         * @param memberConnections Connections to hosts that will join as members
         * @param members Host objects that will join as members
         * @param name Name for the new pool
         * @param description Description for the new pool
         * @param parent Parent QObject
         */
        CreatePoolAction(XenConnection* coordinatorConnection,
                        Host* coordinator,
                        const QList<XenConnection*>& memberConnections,
                        const QList<Host*>& members,
                        const QString& name,
                        const QString& description,
                        QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        XenConnection* m_coordinatorConnection;
        Host* m_coordinator;
        QList<XenConnection*> m_memberConnections;
        QList<Host*> m_members;
        QString m_name;
        QString m_description;
};

#endif // CREATEPOOLACTION_H
