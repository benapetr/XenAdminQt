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

#ifndef DESTROYHOSTACTION_H
#define DESTROYHOSTACTION_H

#include "../../asyncoperation.h"
#include "../../host.h"
#include "../../pool.h"

/**
 * @brief DestroyHostAction - Removes a host from the pool
 *
 * Matches C# XenAdmin.Actions.DestroyHostAction
 *
 * This action:
 * 1. Destroys the host (removes it from pool)
 * 2. Forgets all local SRs that belonged to the host
 *
 * C# location: XenModel/Actions/Host/DestroyHostAction.cs
 */
class DestroyHostAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Constructor
         * @param connection XenConnection to use
         * @param pool Pool the host belongs to
         * @param host Host object to destroy
         * @param parent Parent QObject
         */
        explicit DestroyHostAction(XenConnection* connection,
                                   QSharedPointer<Host> host,
                                   QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        /**
         * @brief Check if SR is detached (all PBDs gone)
         * @param srRef SR opaque reference
         * @return True if SR has no PBDs attached
         *
         * Waits up to 2 minutes for PBDs to detach
         */
        bool isSRDetached(const QString& srRef);

        QSharedPointer<Pool> m_pool;
        QSharedPointer<Host> m_host;
};

#endif // DESTROYHOSTACTION_H
