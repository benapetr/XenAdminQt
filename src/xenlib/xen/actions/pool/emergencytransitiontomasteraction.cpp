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

#include "emergencytransitiontomasteraction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_Pool.h"
#include <stdexcept>

EmergencyTransitionToMasterAction::EmergencyTransitionToMasterAction(XenConnection* slaveConnection,
                                                                     QObject* parent)
    : AsyncOperation(slaveConnection,
                     QString("Emergency transition to master"),
                     "Promoting slave to master",
                     parent)
{
}

void EmergencyTransitionToMasterAction::run()
{
    try
    {
        setPercentComplete(0);
        setDescription("Performing emergency transition to master...");

        // This is a synchronous operation - no task polling
        // The slave host will transition to become the new pool coordinator
        XenAPI::Pool::emergency_transition_to_master(session());

        setPercentComplete(100);
        setDescription("Emergency transition to master completed");

        // Note: The connection state will change after this operation
        // The host is now the pool coordinator

    } catch (const std::exception& e)
    {
        if (isCancelled())
        {
            setDescription("Emergency transition cancelled");
        } else
        {
            setError(QString("Failed to transition to master: %1").arg(e.what()));
        }
    }
}
