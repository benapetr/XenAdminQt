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

#include "xenapi_Network.h"
#include "../session.h"
#include "../api.h"
#include <stdexcept>

namespace XenAPI
{
    QString Network::create(Session* session, const QVariantMap& record)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << record;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("network.create", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    QString Network::async_create(Session* session, const QVariantMap& record)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << record;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.network.create", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    void Network::destroy(Session* session, const QString& network)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << network;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("network.destroy", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    void Network::set_name_label(Session* session, const QString& network, const QString& label)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << network << label;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("network.set_name_label", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    void Network::set_name_description(Session* session, const QString& network, const QString& description)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << network << description;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("network.set_name_description", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    void Network::set_tags(Session* session, const QString& network, const QStringList& tags)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << network << tags;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("network.set_tags", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    void Network::set_MTU(Session* session, const QString& network, qint64 mtu)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << network << QString::number(mtu);

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("network.set_MTU", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    void Network::set_other_config(Session* session, const QString& network, const QVariantMap& otherConfig)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << network << otherConfig;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("network.set_other_config", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    void Network::add_to_other_config(Session* session, const QString& network, const QString& key, const QString& value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << network << key << value;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("network.add_to_other_config", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    void Network::remove_from_other_config(Session* session, const QString& network, const QString& key)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << network << key;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("network.remove_from_other_config", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    QVariantMap Network::get_record(Session* session, const QString& network)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << network;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("network.get_record", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toMap();
    }

    QVariantList Network::get_all(Session* session)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID();

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("network.get_all", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toList();
    }

    QVariantList Network::get_PIFs(Session* session, const QString& network)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << network;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("network.get_PIFs", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toList();
    }

} // namespace XenAPI
