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

#include "xenapi_Pool.h"
#include "../api.h"
#include <stdexcept>

namespace XenAPI
{
    QVariant Pool::get_all(XenSession* session)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("pool.get_all", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response);
    }

    void Pool::set_default_SR(XenSession* session, const QString& pool, const QString& sr)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pool << sr;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("pool.set_default_SR", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Check for errors
    }

    void Pool::set_suspend_image_SR(XenSession* session, const QString& pool, const QString& sr)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pool << sr;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("pool.set_suspend_image_SR", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Check for errors
    }

    void Pool::set_crash_dump_SR(XenSession* session, const QString& pool, const QString& sr)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pool << sr;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("pool.set_crash_dump_SR", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Check for errors
    }

    QString Pool::async_designate_new_master(XenSession* session, const QString& host)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << host;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.pool.designate_new_master", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    QString Pool::async_management_reconfigure(XenSession* session, const QString& network)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << network;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.pool.management_reconfigure", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    QVariantMap Pool::get_record(XenSession* session, const QString& pool)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pool;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("pool.get_record", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toMap();
    }

    QString Pool::get_master(XenSession* session, const QString& pool)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pool;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("pool.get_master", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString();
    }

    QString Pool::async_join(XenSession* session, const QString& master_address,
                             const QString& master_username, const QString& master_password)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << master_address << master_username << master_password;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.pool.join", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString();
    }

    void Pool::eject(XenSession* session, const QString& host)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << host;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("pool.eject", params);
        session->sendApiRequest(request);
    }

    void Pool::set_name_label(XenSession* session, const QString& pool, const QString& label)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pool << label;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("pool.set_name_label", params);
        session->sendApiRequest(request);
    }

    void Pool::set_name_description(XenSession* session, const QString& pool, const QString& description)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pool << description;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("pool.set_name_description", params);
        session->sendApiRequest(request);
    }

    void Pool::set_tags(XenSession* session, const QString& pool, const QStringList& tags)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pool << tags;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("pool.set_tags", params);
        session->sendApiRequest(request);
    }

    void Pool::set_migration_compression(XenSession* session, const QString& pool, bool enabled)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pool << enabled;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("pool.set_migration_compression", params);
        session->sendApiRequest(request);
    }

    QString Pool::async_enable_ha(XenSession* session, const QStringList& heartbeat_srs,
                                  const QVariantMap& configuration)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << QVariant(heartbeat_srs) << configuration;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("pool.async_enable_ha", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString();
    }

    QString Pool::async_disable_ha(XenSession* session)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("pool.async_disable_ha", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString();
    }

    void Pool::set_ha_host_failures_to_tolerate(XenSession* session, const QString& pool, qint64 value)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pool << value;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("pool.set_ha_host_failures_to_tolerate", params);
        session->sendApiRequest(request);
    }

    void Pool::emergency_transition_to_master(XenSession* session)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("pool.emergency_transition_to_master", params);
        session->sendApiRequest(request);
    }

    QString Pool::async_sync_database(XenSession* session)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.pool.sync_database", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString();
    }

    void Pool::rotate_secret(XenSession* session, const QString& pool)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << pool;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("pool.rotate_secret", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Parse to check for errors
    }

} // namespace XenAPI
