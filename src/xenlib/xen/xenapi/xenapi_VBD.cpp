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

#include "xenapi_VBD.h"
#include "../session.h"
#include "../api.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <stdexcept>

namespace XenAPI
{
    // VBD.create - Create a new VBD
    QString VBD::create(XenSession* session, const QVariantMap& vbdRecord)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbdRecord;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VBD.create", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString();
    }

    QString VBD::async_create(XenSession* session, const QVariantMap& vbdRecord)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbdRecord;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VBD.create", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    // VBD.async_plug - Asynchronous plug
    QString VBD::async_plug(XenSession* session, const QString& vbd)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VBD.plug", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    // VBD.async_unplug - Asynchronous unplug
    QString VBD::async_unplug(XenSession* session, const QString& vbd)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VBD.unplug", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    // VBD.async_destroy - Asynchronous destroy
    QString VBD::async_destroy(XenSession* session, const QString& vbd)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VBD.destroy", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    // VBD.async_eject - Asynchronous eject
    QString VBD::async_eject(XenSession* session, const QString& vbd)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VBD.eject", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    // VBD.async_insert - Asynchronous insert
    QString VBD::async_insert(XenSession* session, const QString& vbd, const QString& vdi)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbd << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VBD.insert", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    // VBD.plug - Synchronous plug
    void VBD::plug(XenSession* session, const QString& vbd)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VBD.plug", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Check for errors
    }

    // VBD.unplug - Synchronous unplug
    void VBD::unplug(XenSession* session, const QString& vbd)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VBD.unplug", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Check for errors
    }

    // VBD.destroy - Destroy a VBD
    void VBD::destroy(XenSession* session, const QString& vbd)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VBD.destroy", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Check for errors
    }

    // VBD.eject - Synchronous eject
    void VBD::eject(XenSession* session, const QString& vbd)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VBD.eject", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Check for errors
    }

    // VBD.insert - Synchronous insert
    void VBD::insert(XenSession* session, const QString& vbd, const QString& vdi)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbd << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VBD.insert", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Check for errors
    }

    // VBD.get_allowed_operations
    QVariantList VBD::get_allowed_operations(XenSession* session, const QString& vbd)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VBD.get_allowed_operations", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toList();
    }

    // VBD.get_VM
    QString VBD::get_VM(XenSession* session, const QString& vbd)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VBD.get_VM", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString();
    }

    // VBD.get_VDI
    QString VBD::get_VDI(XenSession* session, const QString& vbd)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VBD.get_VDI", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString();
    }

    // VBD.get_device
    QString VBD::get_device(XenSession* session, const QString& vbd)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VBD.get_device", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString();
    }

    // VBD.get_userdevice
    QString VBD::get_userdevice(XenSession* session, const QString& vbd)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VBD.get_userdevice", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString();
    }

    // VBD.get_bootable
    bool VBD::get_bootable(XenSession* session, const QString& vbd)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VBD.get_bootable", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toBool();
    }

    // VBD.get_mode
    QString VBD::get_mode(XenSession* session, const QString& vbd)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VBD.get_mode", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString();
    }

    // VBD.get_type
    QString VBD::get_type(XenSession* session, const QString& vbd)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VBD.get_type", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString();
    }

    // VBD.get_unpluggable
    bool VBD::get_unpluggable(XenSession* session, const QString& vbd)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VBD.get_unpluggable", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toBool();
    }

    // VBD.get_currently_attached
    bool VBD::get_currently_attached(XenSession* session, const QString& vbd)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VBD.get_currently_attached", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toBool();
    }

    // VBD.get_empty
    bool VBD::get_empty(XenSession* session, const QString& vbd)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VBD.get_empty", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toBool();
    }

    // VBD.set_bootable
    void VBD::set_bootable(XenSession* session, const QString& vbd, bool bootable)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbd << bootable;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VBD.set_bootable", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Check for errors
    }

    // VBD.set_mode
    void VBD::set_mode(XenSession* session, const QString& vbd, const QString& mode)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbd << mode;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VBD.set_mode", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Check for errors
    }

    // VBD.set_userdevice
    void VBD::set_userdevice(XenSession* session, const QString& vbd, const QString& userdevice)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbd << userdevice;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VBD.set_userdevice", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Check for errors
    }

    // VBD.get_record
    QVariantMap VBD::get_record(XenSession* session, const QString& vbd)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VBD.get_record", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toMap();
    }

    // VBD.get_all
    QVariantList VBD::get_all(XenSession* session)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VBD.get_all", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toList();
    }

    // VBD.get_all_records
    QVariantMap VBD::get_all_records(XenSession* session)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VBD.get_all_records", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toMap();
    }

} // namespace XenAPI
