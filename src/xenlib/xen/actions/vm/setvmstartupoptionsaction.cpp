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

#include "setvmstartupoptionsaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../xenapi/xenapi_Pool.h"
#include <QtGlobal>

SetVMStartupOptionsAction::SetVMStartupOptionsAction(XenConnection* connection,
                                                     const QString& poolRef,
                                                     const QMap<QString, QVariantMap>& vmStartupOptions,
                                                     QObject* parent)
    : AsyncOperation(connection, tr("Setting VM startup options"), QString(), parent),
      m_poolRef(poolRef),
      m_vmStartupOptions(vmStartupOptions)
{
}

void SetVMStartupOptionsAction::run()
{
    try
    {
        int totalVMs = m_vmStartupOptions.size();
        int processed = 0;

        QMapIterator<QString, QVariantMap> it(m_vmStartupOptions);
        while (it.hasNext())
        {
            it.next();
            const QString& vmRef = it.key();
            const QVariantMap& options = it.value();

            setDescription(QString("Setting VM startup options"));

            if (options.contains("order"))
                XenAPI::VM::set_order(session(), vmRef, options.value("order").toLongLong());
            if (options.contains("start_delay"))
                XenAPI::VM::set_start_delay(session(), vmRef, options.value("start_delay").toLongLong());

            processed++;
            setPercentComplete(static_cast<int>(processed * (60.0 / qMax(totalVMs, 1))));

            if (isCancelled())
                return;
        }

        if (!m_poolRef.isEmpty())
        {
            QString taskRef = XenAPI::Pool::async_sync_database(session());
            pollToCompletion(taskRef, 60, 100);
        }

        setDescription("Completed");
    }
    catch (const std::exception& e)
    {
        if (isCancelled())
        {
            setDescription("Cancelled");
        }
        else
        {
            setError(QString("Failed to set VM startup options: %1").arg(e.what()));
        }
    }
}
