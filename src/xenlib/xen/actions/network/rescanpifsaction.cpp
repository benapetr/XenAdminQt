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

#include "rescanpifsaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_PIF.h"
#include "../../../xencache.h"

RescanPIFsAction::RescanPIFsAction(XenConnection* connection,
                                   const QString& hostRef,
                                   QObject* parent)
    : AsyncOperation(connection,
                     "Scanning for NICs",
                     "Scanning for physical network interfaces",
                     parent),
      m_hostRef(hostRef)
{
    if (this->m_hostRef.isEmpty())
        throw std::invalid_argument("Host reference cannot be empty");

    // Get host name for display
    QVariantMap hostData = connection->GetCache()->ResolveObjectData(XenObjectType::Host, this->m_hostRef);
    this->m_hostName = hostData.value("name_label").toString();

    this->SetTitle(QString("Scanning for NICs on %1").arg(this->m_hostName));
    this->SetDescription(QString("Scanning for physical network interfaces on %1").arg(this->m_hostName));
}

void RescanPIFsAction::run()
{
    try
    {
        this->SetPercentComplete(40);
        this->SetDescription(QString("Scanning for NICs on %1...").arg(this->m_hostName));

        XenAPI::PIF::scan(this->GetSession(), this->m_hostRef);

        this->SetPercentComplete(100);
        this->SetDescription(QString("Scan complete on %1").arg(this->m_hostName));

    } catch (const std::exception& e)
    {
        this->setError(QString("Failed to scan NICs: %1").arg(e.what()));
    }
}
