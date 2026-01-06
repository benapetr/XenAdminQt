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

#ifndef RESTARTTOOLSTACKACTION_H
#define RESTARTTOOLSTACKACTION_H

#include "../../asyncoperation.h"
#include "../../host.h"
#include <QSharedPointer>

/**
 * @brief RestartToolstackAction - Restarts the XenServer toolstack (XAPI agent)
 *
 * Matches C# XenAdmin.Actions.RestartToolstackAction
 *
 * This action:
 * 1. Calls Host.async_restart_agent to restart the toolstack on the host
 * 2. Polls for completion
 * 3. If host is pool coordinator, interrupts the connection to force reconnect
 *
 * The toolstack restart is useful for:
 * - Applying certain configuration changes
 * - Recovering from toolstack issues
 * - Resetting agent state without rebooting the host
 *
 * Note: This restarts the XAPI service, NOT the physical host. VMs continue running.
 *
 * C# location: XenModel/Actions/Host/RestartToolstackAction.cs
 */
class XENLIB_EXPORT RestartToolstackAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Constructor
         * @param connection XenConnection to use
         * @param host Host object whose toolstack will be restarted
         * @param parent Parent QObject
         */
        explicit RestartToolstackAction(XenConnection* connection,
                                        QSharedPointer<Host> host,
                                        QObject* parent = nullptr);
        ~RestartToolstackAction();

    protected:
        void run() override;

    private:
        QSharedPointer<Host> m_host;
};

#endif // RESTARTTOOLSTACKACTION_H
