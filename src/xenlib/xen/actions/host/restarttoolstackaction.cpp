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

#include "restarttoolstackaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_Host.h"
#include "../../../xencache.h"
#include <QDebug>

RestartToolstackAction::RestartToolstackAction(QSharedPointer<Host> host, QObject* parent)
    : AsyncOperation(host->GetConnection(), QString("Restart toolstack on '%1'").arg(host ? host->GetName() : ""), "", parent), m_host(host)
{
    if (!this->m_host)
        throw std::invalid_argument("Host cannot be null");
    this->AddApiMethodToRoleCheck("host.restart_agent");
}

RestartToolstackAction::~RestartToolstackAction()
{
    qDebug() << "deleted RestartToolstackAction";
}

void RestartToolstackAction::run()
{
    try
    {
        this->SetDescription(QString("Restarting toolstack on '%1'...").arg(this->m_host->GetName()));

        // Restart the XAPI agent (toolstack)
        QString taskRef = XenAPI::Host::async_restart_agent(this->GetSession(), this->m_host->OpaqueRef());
        this->pollToCompletion(taskRef, 0, 100);

        // If this is the pool coordinator, interrupt the connection to force reconnect
        // This is necessary because the XAPI service is restarting
        if (this->m_host->IsMaster())
        {
            qDebug() << "RestartToolstackAction: Host is coordinator, interrupting connection to force reconnect";
            this->GetConnection()->Interrupt();
        }

        this->SetDescription(QString("Toolstack restarted on '%1'.").arg(this->m_host->GetName()));

    } catch (const std::exception& e)
    {
        this->setError(QString("Failed to restart toolstack: %1").arg(e.what()));
    }
}
