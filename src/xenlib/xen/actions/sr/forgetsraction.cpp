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

#include "forgetsraction.h"
#include "../../../xencache.h"
#include "../../network/connection.h"
#include "../../xenapi/xenapi_SR.h"

ForgetSrAction::ForgetSrAction(XenConnection* connection,
                               const QString& srRef,
                               const QString& srName,
                               QObject* parent)
    : AsyncOperation(connection,
                     QString("Forgetting SR '%1'").arg(srName),
                     QString("Forgetting storage repository..."),
                     parent),
      m_srRef(srRef), m_srName(srName)
{
    addApiMethodToRoleCheck("SR.async_forget");
}

void ForgetSrAction::run()
{
    try
    {
        setDescription(QString("Forgetting SR '%1'...").arg(m_srName));

        // Check if SR allows forget operation
        XenCache* cache = connection()->getCache();
        QVariantMap srData = cache->ResolveObjectData("sr", m_srRef);
        if (srData.isEmpty())
        {
            setError(QString("SR '%1' not found in cache").arg(m_srRef));
            return;
        }

        QVariantList allowedOps = srData.value("allowed_operations").toList();
        bool canForget = false;
        for (const QVariant& op : allowedOps)
        {
            if (op.toString() == "forget")
            {
                canForget = true;
                break;
            }
        }

        if (!canForget)
        {
            setError("SR does not allow 'forget' operation");
            return;
        }

        // Forget SR
        QString taskRef = XenAPI::SR::async_forget(session(), m_srRef);
        pollToCompletion(taskRef);

        if (state() != Failed)
        {
            setState(Completed);
            setDescription(QString("Successfully forgotten SR '%1'").arg(m_srName));
        }

    } catch (const std::exception& e)
    {
        setError(QString("Failed to forget SR: %1").arg(e.what()));
    }
}
