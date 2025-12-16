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

#include "PIF.h"
#include "../session.h"
#include "../api.h"
#include "../jsonrpcclient.h"
#include <stdexcept>

namespace XenAPI
{

    void PIF::reconfigure_ip(XenSession* session, const QString& pif,
                             const QString& mode, const QString& ip,
                             const QString& netmask, const QString& gateway,
                             const QString& dns)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pif << mode << ip << netmask << gateway << dns;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("PIF.reconfigure_ip", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response);
    }

    void PIF::plug(XenSession* session, const QString& pif)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pif;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("PIF.plug", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response);
    }

    void PIF::unplug(XenSession* session, const QString& pif)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pif;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("PIF.unplug", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response);
    }

    QString PIF::async_plug(XenSession* session, const QString& pif)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pif;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("PIF.async_plug", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString();
    }

    QString PIF::async_unplug(XenSession* session, const QString& pif)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pif;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.PIF.unplug", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString();
    }

    void PIF::set_disallow_unplug(XenSession* session, const QString& pif, bool value)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pif << value;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("PIF.set_disallow_unplug", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response);
    }

    void PIF::set_property(XenSession* session, const QString& pif, const QString& name, const QString& value)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pif << name << value;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("PIF.set_property", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response);
    }

    QString PIF::async_reconfigure_ip(XenSession* session, const QString& pif,
                                      const QString& mode, const QString& ip,
                                      const QString& netmask, const QString& gateway,
                                      const QString& dns)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pif << mode << ip << netmask << gateway << dns;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("PIF.async_reconfigure_ip", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString();
    }

    void PIF::add_to_other_config(XenSession* session, const QString& pif, const QString& key, const QString& value)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pif << key << value;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("PIF.add_to_other_config", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response);
    }

    void PIF::remove_from_other_config(XenSession* session, const QString& pif, const QString& key)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pif << key;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("PIF.remove_from_other_config", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response);
    }

    QVariantMap PIF::get_record(XenSession* session, const QString& pif)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pif;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("PIF.get_record", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toMap();
    }

    QVariantList PIF::get_all(XenSession* session)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("PIF.get_all", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toList();
    }

    QString PIF::get_network(XenSession* session, const QString& pif)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pif;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("PIF.get_network", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString();
    }

    QString PIF::get_host(XenSession* session, const QString& pif)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pif;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("PIF.get_host", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString();
    }

    void PIF::scan(XenSession* session, const QString& host)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << host;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("PIF.scan", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response);
    }

} // namespace XenAPI
