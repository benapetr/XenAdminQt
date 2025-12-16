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

#include "addhosttopoolaction.h"
#include "../../../xen/connection.h"
#include "../../../xen/session.h"
#include "../../xenapi/xenapi_Pool.h"
#include "../../../xencache.h"
#include <stdexcept>

AddHostToPoolAction::AddHostToPoolAction(XenConnection* poolConnection,
                                         XenConnection* hostConnection,
                                         Host* joiningHost,
                                         QObject* parent)
    : AsyncOperation(hostConnection, // Use host connection as primary (matches C# pattern)
                     QString("Adding host to pool"),
                     QString("Join pool operation"),
                     parent),
      m_poolConnection(poolConnection),
      m_hostConnection(hostConnection),
      m_joiningHost(joiningHost)
{
    // Note: We don't use the Host* object in this simplified version
    // The C# version uses it for licensing and AD checks
}

void AddHostToPoolAction::run()
{
    try
    {
        setPercentComplete(0);
        setDescription("Preparing to join pool...");

        // Get pool coordinator address and credentials from pool's session
        // In C# they get Pool.Connection.Hostname and Pool.Connection.Username/Password
        QString coordinatorAddress = m_poolConnection->getHostname();

        // Get credentials from the pool's session
        XenSession* poolSession = m_poolConnection->getSession();
        if (!poolSession)
        {
            throw std::runtime_error("Pool connection has no session");
        }
        QString username = poolSession->getUsername();
        QString password = poolSession->getPassword();

        if (coordinatorAddress.isEmpty() || username.isEmpty() || password.isEmpty())
        {
            throw std::runtime_error("Missing pool connection credentials");
        }

        setPercentComplete(5);
        setDescription("Joining pool...");

        // Call Pool.async_join from the JOINING HOST's session (matches C# pattern where
        // the action's primary connection is the joining host)
        // This is critical - Pool.async_join must be called from the host being joined, not the pool
        QString taskRef = XenAPI::Pool::async_join(session(), coordinatorAddress, username, password);

        // Poll to completion using host's session (already our primary session)
        pollToCompletion(taskRef, 5, 90);

        setPercentComplete(90);
        setDescription("Join complete");

        // TODO: In full implementation:
        // 1. Create new session to coordinator and clear non-shared SRs on the joined host
        // 2. Handle license compatibility (PoolJoinRules.FreeHostPaidCoordinator)
        // 3. Synchronize AD configuration (FixAd)
        // 4. Remove the host's connection from ConnectionsManager

        setDescription("Host added to pool successfully");

    } catch (const std::exception& e)
    {
        if (isCancelled())
        {
            setDescription("Join cancelled");
        } else
        {
            setError(QString("Failed to add host to pool: %1").arg(e.what()));
        }
    }
}
