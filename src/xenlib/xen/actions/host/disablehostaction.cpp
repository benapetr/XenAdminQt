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

#include "disablehostaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_Host.h"
#include <QDebug>

DisableHostAction::DisableHostAction(XenConnection* connection,
                                     Host* host,
                                     QObject* parent)
    : AsyncOperation(connection,
                     "Disabling host",
                     QString("Evacuating '%1'").arg(host ? host->GetName() : ""),
                     parent),
      m_host(host)
{
    if (!m_host)
        throw std::invalid_argument("Host cannot be null");
}

void DisableHostAction::run()
{
    try
    {
        setDescription(QString("Evacuating '%1'").arg(m_host->GetName()));

        try
        {
            // Disable the host (this will evacuate VMs)
            QString taskRef = XenAPI::Host::async_disable(session(), m_host->OpaqueRef());
            pollToCompletion(taskRef, 0, 100);
        } catch (...)
        {
            // On error, remove MAINTENANCE_MODE flag
            XenAPI::Host::remove_from_other_config(session(), m_host->OpaqueRef(), "MAINTENANCE_MODE");
            throw;
        }

        setDescription(QString("Evacuated '%1'").arg(m_host->GetName()));

    } catch (const std::exception& e)
    {
        setError(QString("Failed to disable host: %1").arg(e.what()));
    }
}
