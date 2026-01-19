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

#include "xenapi_VDI.h"
#include "../session.h"
#include "../api.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <stdexcept>

namespace XenAPI
{
    // VDI.create - Create a new VDI
    QString VDI::create(Session* session, const QVariantMap& vdiRecord)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vdiRecord;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VDI.create", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    // VDI.async_create - Asynchronous create
    QString VDI::async_create(Session* session, const QVariantMap& vdiRecord)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vdiRecord;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VDI.create", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    // VDI.async_destroy - Asynchronous destroy
    QString VDI::async_destroy(Session* session, const QString& vdi)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VDI.destroy", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    // VDI.destroy - Synchronous destroy
    void VDI::destroy(Session* session, const QString& vdi)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VDI.destroy", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    // VDI.async_copy - Copy VDI to another SR
    QString VDI::async_copy(Session* session, const QString& vdi, const QString& sr)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vdi << sr;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VDI.copy", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    // VDI.async_pool_migrate - Live migrate VDI to another SR
    QString VDI::async_pool_migrate(Session* session, const QString& vdi, const QString& sr, const QVariantMap& options)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vdi << sr << options;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VDI.pool_migrate", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    // VDI.get_VBDs - Get list of VBDs attached to this VDI
    QVariantList VDI::get_VBDs(Session* session, const QString& vdi)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VDI.get_VBDs", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toList();
    }

    // VDI.get_SR - Get the SR this VDI belongs to
    QString VDI::get_SR(Session* session, const QString& vdi)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VDI.get_SR", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    // VDI.get_name_label
    QString VDI::get_name_label(Session* session, const QString& vdi)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VDI.get_name_label", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    // VDI.get_name_description
    QString VDI::get_name_description(Session* session, const QString& vdi)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VDI.get_name_description", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    // VDI.get_virtual_size
    qint64 VDI::get_virtual_size(Session* session, const QString& vdi)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VDI.get_virtual_size", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toLongLong();
    }

    // VDI.get_read_only
    bool VDI::get_read_only(Session* session, const QString& vdi)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VDI.get_read_only", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toBool();
    }

    // VDI.get_type
    QString VDI::get_type(Session* session, const QString& vdi)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VDI.get_type", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    // VDI.get_sharable
    bool VDI::get_sharable(Session* session, const QString& vdi)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VDI.get_sharable", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toBool();
    }

    // VDI.get_sm_config
    QVariantMap VDI::get_sm_config(Session* session, const QString& vdi)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VDI.get_sm_config", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toMap();
    }

    // VDI.set_name_label
    void VDI::set_name_label(Session* session, const QString& vdi, const QString& label)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vdi << label;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VDI.set_name_label", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    // VDI.set_name_description
    void VDI::set_name_description(Session* session, const QString& vdi, const QString& description)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vdi << description;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VDI.set_name_description", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    // VDI.set_sm_config
    void VDI::set_sm_config(Session* session, const QString& vdi, const QVariantMap& smConfig)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vdi << smConfig;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VDI.set_sm_config", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    // VDI.resize
    void VDI::resize(Session* session, const QString& vdi, qint64 size)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vdi << size;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VDI.resize", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    // VDI.resize_online
    void VDI::resize_online(Session* session, const QString& vdi, qint64 size)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vdi << size;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VDI.resize_online", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    // VDI.get_record
    QVariantMap VDI::get_record(Session* session, const QString& vdi)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VDI.get_record", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toMap();
    }

    // VDI.async_disable_cbt - Disable Changed Block Tracking (async)
    QString VDI::async_disable_cbt(Session* session, const QString& vdi)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VDI.disable_cbt", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    // VDI.get_cbt_enabled - Check if CBT is enabled
    bool VDI::get_cbt_enabled(Session* session, const QString& vdi)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VDI.get_cbt_enabled", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toBool();
    }

    // VDI.get_all
    QVariantList VDI::get_all(Session* session)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID();

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VDI.get_all", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toList();
    }

    // VDI.get_all_records
    QVariantMap VDI::get_all_records(Session* session)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID();

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VDI.get_all_records", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toMap();
    }

} // namespace XenAPI
