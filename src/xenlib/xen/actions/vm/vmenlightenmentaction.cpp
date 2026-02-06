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

#include "vmenlightenmentaction.h"
#include "../../vm.h"
#include "../../host.h"
#include "../../pool.h"
#include "../../session.h"
#include "../../network/connection.h"
#include "../../xenapi/xenapi_Host.h"
#include "../../../xencache.h"

VMEnlightenmentAction::VMEnlightenmentAction(QSharedPointer<VM> vm,
                                             bool enable,
                                             bool suppressHistory,
                                             QObject* parent)
    : AsyncOperation(enable ? tr("Enable VM Enlightenment") : tr("Disable VM Enlightenment"),
                     enable ? tr("Enabling Windows guest enlightenment...") : tr("Disabling Windows guest enlightenment..."),
                     parent),
      m_vm(vm),
      m_enable(enable)
{
    if (this->m_vm)
    {
        this->m_connection = this->m_vm->GetConnection();
        this->setAppliesToFromObject(this->m_vm);
    }

    this->AddApiMethodToRoleCheck("host.call_plugin");
    this->SetSuppressHistory(suppressHistory);
}

QSharedPointer<Host> VMEnlightenmentAction::resolveTargetHost() const
{
    if (!this->m_vm || !this->m_connection || !this->m_connection->GetCache())
        return QSharedPointer<Host>();

    QSharedPointer<Pool> pool = this->m_connection->GetCache()->GetPoolOfOne();
    if (!pool || !pool->IsValid())
        return QSharedPointer<Host>();

    return pool->GetMasterHost();
}

void VMEnlightenmentAction::run()
{
    if (!this->m_vm || !this->m_vm->IsValid())
    {
        this->setError(tr("VM is no longer available."));
        return;
    }

    XenAPI::Session* session = this->GetSession();
    if (!session || !session->IsLoggedIn())
    {
        this->setError(tr("No valid session."));
        return;
    }

    QSharedPointer<Host> host = this->resolveTargetHost();
    if (!host || !host->IsValid())
    {
        this->setError(tr("Unable to determine a host to run xscontainer plugin."));
        return;
    }

    SetPercentComplete(20);

    const QString action = this->m_enable ? QStringLiteral("register")
                                          : QStringLiteral("deregister");

    QVariantMap args;
    args["vmuuid"] = this->m_vm->GetUUID();

    try
    {
        const QString result = XenAPI::Host::call_plugin(session,
                                                         host->OpaqueRef(),
                                                         QStringLiteral("xscontainer"),
                                                         action,
                                                         args);
        this->SetResult(result);

        if (result.toLower().startsWith(QStringLiteral("true")))
        {
            this->SetDescription(tr("Succeeded"));
            SetPercentComplete(100);
            return;
        }

        this->setError(result.isEmpty() ? tr("Unknown plugin error.") : result);
    }
    catch (const std::exception& ex)
    {
        this->setError(QString::fromLatin1(ex.what()));
    }
}
