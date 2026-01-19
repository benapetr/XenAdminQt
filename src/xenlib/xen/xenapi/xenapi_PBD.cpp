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

#include "xenapi_PBD.h"
#include "../api.h"
#include "../session.h"
#include <stdexcept>

namespace XenAPI
{
    QVariantMap PBD::get_record(Session* session, const QString& pbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << pbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("PBD.get_record", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toMap();
    }

    bool PBD::get_currently_attached(Session* session, const QString& pbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << pbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("PBD.get_currently_attached", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toBool();
    }

    QString PBD::async_create(Session* session, const QVariantMap& record)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << record;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.PBD.create", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    QString PBD::async_plug(Session* session, const QString& pbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << pbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.PBD.plug", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    void PBD::plug(Session* session, const QString& pbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << pbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("PBD.plug", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    void PBD::unplug(Session* session, const QString& pbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << pbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("PBD.unplug", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    QString PBD::async_unplug(Session* session, const QString& pbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << pbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.PBD.unplug", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    QString PBD::async_destroy(Session* session, const QString& pbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << pbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.PBD.destroy", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

} // namespace XenAPI
