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

#include "xenapi_Task.h"
#include "../session.h"
#include "../api.h"
#include "../network/connection.h"
#include <QVariantList>
#include <QByteArray>
#include <stdexcept>

namespace XenAPI
{
    QString Task::Create(Session* session, const QString& label, const QString& description)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");
        
        QVariantList params;
        params << session->getSessionId() << label << description;
        
        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("task.create", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString();
    }
    
    void Task::Destroy(Session* session, const QString& taskRef)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");
        
        QVariantList params;
        params << session->getSessionId() << taskRef;
        
        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("task.destroy", params);
        session->sendApiRequest(request);
    }
    
    void Task::Cancel(Session* session, const QString& taskRef)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");
        
        QVariantList params;
        params << session->getSessionId() << taskRef;
        
        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("task.cancel", params);
        session->sendApiRequest(request);
    }
}
