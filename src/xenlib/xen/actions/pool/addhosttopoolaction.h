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

#ifndef ADDHOSTTOPOOLACTION_H
#define ADDHOSTTOPOOLACTION_H

#include "../../asyncoperation.h"
#include <QString>

class XenConnection;
class Host;
class Pool;

/**
 * @brief AddHostToPoolAction adds a standalone host to an existing pool.
 *
 * Matches C# XenModel/Actions/Pool/AddHostToPoolAction.cs
 *
 * Note: This is a simplified version. The C# implementation includes:
 * - License compatibility checks and relicensing
 * - Active Directory configuration synchronization
 * - Non-shared SR cleanup
 * These features are deferred for initial implementation.
 */

class AddHostToPoolAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Constructor for adding host to pool
         * @param poolConnection Connection to the pool to join
         * @param hostConnection Connection to the host being joined
         * @param joiningHost Host object being joined to pool
         * @param parent Parent QObject
         */
        AddHostToPoolAction(XenConnection* poolConnection,
                            XenConnection* hostConnection,
                            QSharedPointer<Host> joiningHost,
                            QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        XenConnection* m_poolConnection;
        XenConnection* m_hostConnection;
        QSharedPointer<Host> m_joiningHost;
};

#endif // ADDHOSTTOPOOLACTION_H
