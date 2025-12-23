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

#include "createpoolaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_Pool.h"
#include "../../../xencache.h"
#include <stdexcept>

CreatePoolAction::CreatePoolAction(XenConnection* coordinatorConnection,
                                   Host* coordinator,
                                   const QList<XenConnection*>& memberConnections,
                                   const QList<Host*>& members,
                                   const QString& name,
                                   const QString& description,
                                   QObject* parent)
    : AsyncOperation(coordinatorConnection,
                     QString("Creating pool: %1").arg(name),
                     "Creating new pool",
                     parent),
      m_coordinatorConnection(coordinatorConnection),
      m_coordinator(coordinator),
      m_memberConnections(memberConnections),
      m_members(members),
      m_name(name),
      m_description(description)
{
    // Note: We don't use Host* objects in this simplified version
    // The C# version uses them for licensing and AD checks
}

void CreatePoolAction::run()
{
    try
    {
        setPercentComplete(0);
        setDescription("Creating pool...");

        // Get pool reference from cache
        // There should be one pool reference for a standalone coordinator
        QStringList poolRefs = m_coordinatorConnection->getCache()->GetAllRefs("pool");
        if (poolRefs.isEmpty())
        {
            throw std::runtime_error("No pool found on coordinator");
        }
        QString poolRef = poolRefs.first();

        setPercentComplete(10);
        setDescription("Setting pool name and description...");

        // Set pool name and description on the coordinator
        XenAPI::Pool::set_name_label(session(), poolRef, m_name);
        XenAPI::Pool::set_name_description(session(), poolRef, m_description);

        setPercentComplete(20);

        // If no members to add, we're done
        if (m_members.isEmpty())
        {
            setDescription("Pool created successfully");
            return;
        }

        // Add each member sequentially
        // Progress: 20% done, 80% remaining for members
        double progressPerMember = 80.0 / m_members.size();

        for (int i = 0; i < m_members.size(); ++i)
        {
            XenConnection* memberConnection = m_memberConnections.at(i);
            // Note: Host* not used in simplified version

            int progressStart = 20 + static_cast<int>(i * progressPerMember);
            int progressEnd = 20 + static_cast<int>((i + 1) * progressPerMember);

            setDescription(QString("Adding member %1 of %2...").arg(i + 1).arg(m_members.size()));

            // Get coordinator credentials from session
            QString coordinatorAddress = m_coordinatorConnection->getHostname();

            XenSession* coordinatorSession = m_coordinatorConnection->getSession();
            if (!coordinatorSession)
            {
                throw std::runtime_error("Coordinator connection has no session");
            }
            QString username = coordinatorSession->getUsername();
            QString password = coordinatorSession->getPassword();

            // Call Pool.async_join from the member's session
            // We need to use member's connection temporarily to poll correctly
            XenConnection* oldConnection = connection();
            setConnection(memberConnection);

            QString taskRef = XenAPI::Pool::async_join(session(),
                                                       coordinatorAddress, username, password);

            // Poll using member's session
            pollToCompletion(taskRef, progressStart, progressEnd);

            // Restore original connection
            setConnection(oldConnection);

            setDescription(QString("Member %1 of %2 joined successfully").arg(i + 1).arg(m_members.size()));

            // TODO: Drop connection and remove from ConnectionsManager
        }

        setDescription("Pool created successfully");

    } catch (const std::exception& e)
    {
        if (isCancelled())
        {
            setDescription("Pool creation cancelled");
        } else
        {
            setError(QString("Failed to create pool: %1").arg(e.what()));
        }
    }
}
