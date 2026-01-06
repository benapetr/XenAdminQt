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

#include "vdidisablecbtaction.h"
#include "../../network/connection.h"
#include "../../xenapi/xenapi_VDI.h"
#include "../../session.h"

VDIDisableCbtAction::VDIDisableCbtAction(XenConnection* connection,
                                         const QString& vmName,
                                         const QString& vdiRef,
                                         QObject* parent)
    : AsyncOperation(connection,
                     QString("Disable changed block tracking for %1").arg(vmName),
                     QString("Disabling changed block tracking for %1").arg(vmName),
                     parent),
      m_vdiRef(vdiRef),
      m_vmName(vmName)
{
    // Register RBAC method
    AddApiMethodToRoleCheck("VDI.async_disable_cbt");
}

void VDIDisableCbtAction::run()
{
    if (!GetConnection() || !GetSession())
    {
        setError("Connection lost");
        return;
    }

    XenAPI::Session* xenSession = GetSession();

    try
    {
        // Update description
        SetDescription(QString("Disabling changed block tracking for %1").arg(m_vmName));

        // Call VDI.async_disable_cbt
        QString taskRef = XenAPI::VDI::async_disable_cbt(xenSession, m_vdiRef);

        // Poll task to completion
        pollToCompletion(taskRef);

        // Check if successful
        if (GetState() == Failed)
        {
            return;
        }

        // Success
        SetDescription("Disabled");
        setState(Completed);

    } catch (const std::exception& e)
    {
        setError(QString("Failed to disable CBT: %1").arg(e.what()));
    }
}
