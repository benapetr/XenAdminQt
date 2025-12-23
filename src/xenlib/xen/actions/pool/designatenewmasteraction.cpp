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

#include "designatenewmasteraction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_Pool.h"
#include <stdexcept>

DesignateNewMasterAction::DesignateNewMasterAction(XenConnection* connection,
                                                   const QString& newMasterRef,
                                                   QObject* parent)
    : AsyncOperation(connection,
                     QString("Designating new pool coordinator"),
                     "Transitioning coordinator role",
                     parent),
      m_newMasterRef(newMasterRef)
{
    if (m_newMasterRef.isEmpty())
    {
        throw std::invalid_argument("New master reference cannot be empty");
    }
}

void DesignateNewMasterAction::run()
{
    try
    {
        setPercentComplete(0);
        setDescription("Preparing to designate new coordinator...");

        // Signal to the connection that the coordinator is going to change
        // This matches C# pattern: Connection.CoordinatorMayChange = true;
        connection()->setCoordinatorMayChange(true);

        setPercentComplete(10);
        setDescription("Designating new pool coordinator...");

        try
        {
            // Call Pool.async_designate_new_master
            QString taskRef = XenAPI::Pool::async_designate_new_master(session(), m_newMasterRef);

            // Poll to completion
            pollToCompletion(taskRef, 10, 100);

        } catch (...)
        {
            // If there's an error during designate, clear flag to prevent leak
            connection()->setCoordinatorMayChange(false);
            throw;
        }

        setDescription("New coordinator designated successfully");

        // Note: The connection will automatically detect and reconnect to the new coordinator
        // via the failover mechanism in ConnectionWorker

    } catch (const std::exception& e)
    {
        if (isCancelled())
        {
            setDescription("Designate new coordinator cancelled");
        } else
        {
            setError(QString("Failed to designate new coordinator: %1").arg(e.what()));
        }
    }
}
