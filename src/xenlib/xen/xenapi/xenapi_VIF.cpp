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

#include "xenapi_VIF.h"
#include "../session.h"
#include "../connection.h"
#include "../api.h"
#include <QDebug>
#include <stdexcept>

namespace XenAPI
{

    QString VIF::async_create(XenSession* session, const QVariantMap& vifRecord)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vifRecord;

        XenRpcAPI api(session);
        // Use Async.VIF.create (matches C# JsonRpcClient.async_vif_create line 8271)
        QByteArray request = api.buildJsonRpcCall("Async.VIF.create", params);
        QByteArray response = session->sendApiRequest(request);

        QString taskRef = api.parseJsonRpcResponse(response).toString();
        qDebug() << "VIF.async_create returned task ref:" << taskRef;
        qDebug() << "Request was:" << QString::fromUtf8(request);
        qDebug() << "Response was:" << QString::fromUtf8(response);

        return taskRef;
    }

    void VIF::destroy(XenSession* session, const QString& vif)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vif;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VIF.destroy", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response);
    }

    void VIF::plug(XenSession* session, const QString& vif)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vif;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VIF.plug", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response);
    }

    void VIF::unplug(XenSession* session, const QString& vif)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vif;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VIF.unplug", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response);
    }

    QStringList VIF::get_allowed_operations(XenSession* session, const QString& vif)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vif;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VIF.get_allowed_operations", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);

        QStringList operations;
        if (result.canConvert<QVariantList>())
        {
            QVariantList list = result.toList();
            for (const QVariant& op : list)
            {
                operations.append(op.toString());
            }
        }

        return operations;
    }

    QVariantMap VIF::get_record(XenSession* session, const QString& vif)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vif;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VIF.get_record", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toMap();
    }

    QVariantList VIF::get_all(XenSession* session)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VIF.get_all", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toList();
    }

} // namespace XenAPI
