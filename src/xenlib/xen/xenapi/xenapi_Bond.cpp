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

#include "xenapi_Bond.h"
#include "../session.h"
#include "../api.h"
#include <stdexcept>

namespace XenAPI
{
    QString Bond::async_create(Session* session, const QString& network,
                               const QStringList& members, const QString& mac,
                               const QString& mode, const QVariantMap& properties)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList memberRefs;
        for (const QString& member : members)
        {
            memberRefs << member;
        }

        QVariantList params;
        params << session->getSessionId() << network << memberRefs << mac << mode << properties;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.Bond.create", params);
        QByteArray response = session->sendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    QString Bond::async_destroy(Session* session, const QString& bond)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << bond;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.Bond.destroy", params);
        QByteArray response = session->sendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    void Bond::set_mode(Session* session, const QString& bond, const QString& mode)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << bond << mode;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Bond.set_mode", params);
        QByteArray response = session->sendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    void Bond::set_property(Session* session, const QString& bond, const QString& name, const QString& value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << bond << name << value;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Bond.set_property", params);
        QByteArray response = session->sendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    QVariantMap Bond::get_record(Session* session, const QString& bond)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << bond;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Bond.get_record", params);
        QByteArray response = session->sendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toMap();
    }

    QString Bond::get_master(Session* session, const QString& bond)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << bond;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Bond.get_master", params);
        QByteArray response = session->sendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    QVariantList Bond::get_slaves(Session* session, const QString& bond)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << bond;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Bond.get_slaves", params);
        QByteArray response = session->sendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toList();
    }

} // namespace XenAPI
