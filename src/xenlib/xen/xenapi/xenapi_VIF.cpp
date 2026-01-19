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
#include "../network/connection.h"
#include "../api.h"
#include <QDebug>
#include <stdexcept>

namespace XenAPI
{
    QString VIF::async_create(Session* session, const QVariantMap& vifRecord)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vifRecord;

        XenRpcAPI api(session);
        // Use Async.VIF.create (matches C# JsonRpcClient.async_vif_create line 8271)
        QByteArray request = api.BuildJsonRpcCall("Async.VIF.create", params);
        QByteArray response = session->SendApiRequest(request);

        QString taskRef = api.ParseJsonRpcResponse(response).toString();
        qDebug() << "VIF.async_create returned task ref:" << taskRef;
        qDebug() << "Request was:" << QString::fromUtf8(request);
        qDebug() << "Response was:" << QString::fromUtf8(response);

        return taskRef;
    }

    QString VIF::create(Session* session, const QVariantMap& vifRecord)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vifRecord;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VIF.create", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    QString VIF::async_destroy(Session* session, const QString& vif)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vif;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VIF.destroy", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    void VIF::destroy(Session* session, const QString& vif)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vif;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VIF.destroy", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    void VIF::plug(Session* session, const QString& vif)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vif;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VIF.plug", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    void VIF::unplug(Session* session, const QString& vif)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vif;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VIF.unplug", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    QStringList VIF::get_allowed_operations(Session* session, const QString& vif)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vif;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VIF.get_allowed_operations", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

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

    QVariantMap VIF::get_record(Session* session, const QString& vif)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vif;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VIF.get_record", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toMap();
    }

    QVariantList VIF::get_all(Session* session)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID();

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VIF.get_all", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toList();
    }

} // namespace XenAPI
