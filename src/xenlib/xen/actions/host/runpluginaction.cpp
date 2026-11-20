/*
 * Copyright (c) 2026, Petr Bena <petr@bena.rocks>
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

#include "runpluginaction.h"
#include "../../host.h"
#include "../../xenapi/xenapi_Host.h"
#include <QDebug>

RunPluginAction::RunPluginAction(XenConnection* connection, QSharedPointer<Host> host, const QString& plugin, const QString& function,
                                 const QVariantMap& args, bool suppressHistory, QObject* parent)
    : AsyncOperation(connection, tr("Call plug-in '%1'").arg(plugin), QString(), suppressHistory, parent),
      m_hostRef(host ? host->OpaqueRef() : QString()),
      m_plugin(plugin),
      m_function(function),
      m_args(args)
{
    this->SetHost(host);
    this->AddApiMethodToRoleCheck("Host.call_plugin");
}

void RunPluginAction::run()
{
    try
    {
        this->SetDescription(tr("Running plug-in '%1'").arg(this->m_plugin));
        QString result = XenAPI::Host::call_plugin(this->GetSession(), this->m_hostRef, this->m_plugin, this->m_function, this->m_args);

        if (result.startsWith("True"))
        {
            this->SetResult(result.mid(4));
            this->SetDescription(tr("Plug-in '%1' completed successfully.").arg(this->m_plugin));
            return;
        }

        // C# logs the arguments too; they can hold configuration, so only the call is logged here
        qWarning() << "Plugin call" << this->m_plugin + "." + this->m_function << "failed with" << result;
        this->SetResult(result);
        this->setError(result);
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}
