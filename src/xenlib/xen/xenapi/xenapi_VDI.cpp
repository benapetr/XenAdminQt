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
    QString VDI::create(XenSession* session, const QVariantMap& vdiRecord)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vdiRecord;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VDI.create", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString();
    }

    // VDI.async_destroy - Asynchronous destroy
    QString VDI::async_destroy(XenSession* session, const QString& vdi)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VDI.destroy", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    // VDI.destroy - Synchronous destroy
    void VDI::destroy(XenSession* session, const QString& vdi)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VDI.destroy", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Check for errors
    }

    // VDI.async_copy - Copy VDI to another SR
    QString VDI::async_copy(XenSession* session, const QString& vdi, const QString& sr)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vdi << sr;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VDI.copy", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    // VDI.async_pool_migrate - Live migrate VDI to another SR
    QString VDI::async_pool_migrate(XenSession* session, const QString& vdi, const QString& sr, const QVariantMap& options)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vdi << sr << options;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VDI.pool_migrate", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    // VDI.get_VBDs - Get list of VBDs attached to this VDI
    QVariantList VDI::get_VBDs(XenSession* session, const QString& vdi)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VDI.get_VBDs", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toList();
    }

    // VDI.get_SR - Get the SR this VDI belongs to
    QString VDI::get_SR(XenSession* session, const QString& vdi)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VDI.get_SR", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString();
    }

    // VDI.get_name_label
    QString VDI::get_name_label(XenSession* session, const QString& vdi)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VDI.get_name_label", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString();
    }

    // VDI.get_name_description
    QString VDI::get_name_description(XenSession* session, const QString& vdi)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VDI.get_name_description", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString();
    }

    // VDI.get_virtual_size
    qint64 VDI::get_virtual_size(XenSession* session, const QString& vdi)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VDI.get_virtual_size", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toLongLong();
    }

    // VDI.get_read_only
    bool VDI::get_read_only(XenSession* session, const QString& vdi)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VDI.get_read_only", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toBool();
    }

    // VDI.get_type
    QString VDI::get_type(XenSession* session, const QString& vdi)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VDI.get_type", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString();
    }

    // VDI.get_sharable
    bool VDI::get_sharable(XenSession* session, const QString& vdi)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VDI.get_sharable", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toBool();
    }

    // VDI.set_name_label
    void VDI::set_name_label(XenSession* session, const QString& vdi, const QString& label)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vdi << label;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VDI.set_name_label", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Check for errors
    }

    // VDI.set_name_description
    void VDI::set_name_description(XenSession* session, const QString& vdi, const QString& description)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vdi << description;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VDI.set_name_description", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Check for errors
    }

    // VDI.resize
    void VDI::resize(XenSession* session, const QString& vdi, qint64 size)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vdi << size;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VDI.resize", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Check for errors
    }

    // VDI.resize_online
    void VDI::resize_online(XenSession* session, const QString& vdi, qint64 size)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vdi << size;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VDI.resize_online", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Check for errors
    }

    // VDI.get_record
    QVariantMap VDI::get_record(XenSession* session, const QString& vdi)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VDI.get_record", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toMap();
    }

    // VDI.async_disable_cbt - Disable Changed Block Tracking (async)
    QString VDI::async_disable_cbt(XenSession* session, const QString& vdi)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VDI.disable_cbt", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    // VDI.get_cbt_enabled - Check if CBT is enabled
    bool VDI::get_cbt_enabled(XenSession* session, const QString& vdi)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VDI.get_cbt_enabled", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toBool();
    }

    // VDI.get_all
    QVariantList VDI::get_all(XenSession* session)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VDI.get_all", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toList();
    }

    // VDI.get_all_records
    QVariantMap VDI::get_all_records(XenSession* session)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VDI.get_all_records", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toMap();
    }

} // namespace XenAPI
