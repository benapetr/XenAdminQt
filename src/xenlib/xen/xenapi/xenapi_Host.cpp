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

#include "xenapi_Host.h"
#include "../session.h"
#include "../api.h"
#include <stdexcept>

namespace XenAPI
{
    QVariantList Host::get_all(XenSession* session)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("host.get_all", params);
        QByteArray response = session->sendApiRequest(request);

        QVariant result = api.parseJsonRpcResponse(response);

        // Result should be a list of host refs
        if (result.canConvert<QVariantList>())
        {
            return result.toList();
        }

        return QVariantList();
    }

    void Host::set_name_label(XenSession* session, const QString& host, const QString& value)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << host << value;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("host.set_name_label", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response);
    }

    void Host::set_name_description(XenSession* session, const QString& host, const QString& value)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << host << value;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("host.set_name_description", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response);
    }

    void Host::set_tags(XenSession* session, const QString& host, const QStringList& value)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << host << value;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("host.set_tags", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response);
    }

    void Host::set_other_config(XenSession* session, const QString& host, const QVariantMap& otherConfig)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << host << otherConfig;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("host.set_other_config", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response);
    }

    void Host::set_iscsi_iqn(XenSession* session, const QString& host, const QString& value)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << host << value;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("host.set_iscsi_iqn", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response);
    }

    QString Host::call_plugin(XenSession* session,
                              const QString& host,
                              const QString& plugin,
                              const QString& function,
                              const QVariantMap& args)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << host << plugin << function << args;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("host.call_plugin", params);
        QByteArray response = session->sendApiRequest(request);

        QVariant result = api.parseJsonRpcResponse(response);
        return result.toString();
    }

    QString Host::async_disable(XenSession* session, const QString& host)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << host;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.host.disable", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    void Host::enable(XenSession* session, const QString& host)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << host;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("host.enable", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Void method - just check for errors
    }

    QString Host::async_enable(XenSession* session, const QString& host)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << host;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.host.enable", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    QString Host::async_reboot(XenSession* session, const QString& host)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << host;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.host.reboot", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    QString Host::async_shutdown(XenSession* session, const QString& host)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << host;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.host.shutdown", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    QString Host::async_evacuate(XenSession* session, const QString& host)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << host;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.host.evacuate", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    QString Host::async_destroy(XenSession* session, const QString& host)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << host;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.host.destroy", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    void Host::remove_from_other_config(XenSession* session, const QString& host, const QString& key)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << host << key;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("host.remove_from_other_config", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Void method - just check for errors
    }

    void Host::add_to_other_config(XenSession* session, const QString& host, const QString& key, const QString& value)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << host << key << value;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("host.add_to_other_config", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Void method - just check for errors
    }

    void Host::management_reconfigure(XenSession* session, const QString& pif)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pif;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("host.management_reconfigure", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Void method - just check for errors
    }

    QString Host::async_management_reconfigure(XenSession* session, const QString& pif)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pif;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.host.management_reconfigure", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    QVariantMap Host::migrate_receive(XenSession* session, const QString& host,
                                      const QString& network, const QVariantMap& options)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << host << network << options;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("host.migrate_receive", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toMap(); // Returns migration receive data
    }

} // namespace XenAPI
