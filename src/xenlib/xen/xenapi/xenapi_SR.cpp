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

#include "xenapi_SR.h"
#include "../api.h"
#include "../session.h"
#include <stdexcept>

namespace XenAPI
{
    QVariantMap SR::get_record(Session* session, const QString& sr)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << sr;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("SR.get_record", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toMap();
    }

    QString SR::get_name_label(Session* session, const QString& sr)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << sr;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("SR.get_name_label", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    QString SR::get_by_uuid(Session* session, const QString& uuid)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << uuid;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("SR.get_by_uuid", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    QVariantList SR::get_PBDs(Session* session, const QString& sr)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << sr;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("SR.get_PBDs", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toList();
    }

    QString SR::create(Session* session,
                       const QString& host,
                       const QVariantMap& deviceConfig,
                       qint64 physicalSize,
                       const QString& nameLabel,
                       const QString& nameDescription,
                       const QString& type,
                       const QString& contentType,
                       bool shared,
                       const QVariantMap& smConfig)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID()
               << host
               << deviceConfig
               << QString::number(physicalSize)
               << nameLabel
               << nameDescription
               << type
               << contentType
               << shared
               << smConfig;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("SR.create", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    QString SR::async_introduce(Session* session,
                                const QString& uuid,
                                const QString& nameLabel,
                                const QString& nameDescription,
                                const QString& type,
                                const QString& contentType,
                                bool shared,
                                const QVariantMap& smConfig)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID()
               << uuid
               << nameLabel
               << nameDescription
               << type
               << contentType
               << shared
               << smConfig;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.SR.introduce", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    QString SR::async_forget(Session* session, const QString& sr)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << sr;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.SR.forget", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    void SR::forget(Session* session, const QString& sr)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << sr;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("SR.forget", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    QString SR::async_destroy(Session* session, const QString& sr)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << sr;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.SR.destroy", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    void SR::set_name_label(Session* session, const QString& sr, const QString& value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << sr << value;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("SR.set_name_label", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    void SR::set_name_description(Session* session, const QString& sr, const QString& value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << sr << value;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("SR.set_name_description", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    void SR::set_tags(Session* session, const QString& sr, const QStringList& tags)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << sr << tags;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("SR.set_tags", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    void SR::set_other_config(Session* session, const QString& sr, const QVariantMap& value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << sr << value;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("SR.set_other_config", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    void SR::scan(Session* session, const QString& sr)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << sr;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("SR.scan", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    QString SR::async_probe(Session* session, const QString& host,
                            const QVariantMap& device_config,
                            const QString& type,
                            const QVariantMap& sm_config)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host << device_config << type << sm_config;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.SR.probe", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    QVariantList SR::probe_ext(Session* session, const QString& host,
                               const QVariantMap& device_config,
                               const QString& type,
                               const QVariantMap& sm_config)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host << device_config << type << sm_config;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("SR.probe_ext", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toList();
    }

    QString SR::async_create(Session* session,
                             const QString& host,
                             const QVariantMap& device_config,
                             qint64 physical_size,
                             const QString& name_label,
                             const QString& name_description,
                             const QString& type,
                             const QString& content_type,
                             bool shared,
                             const QVariantMap& sm_config)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host << device_config << physical_size
               << name_label << name_description << type << content_type << shared << sm_config;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.SR.create", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    void SR::assert_can_host_ha_statefile(Session* session, const QString& sr)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << sr;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("SR.assert_can_host_ha_statefile", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Throws on failure
    }

    QString SR::async_assert_can_host_ha_statefile(Session* session, const QString& sr)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << sr;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.SR.assert_can_host_ha_statefile", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

} // namespace XenAPI
