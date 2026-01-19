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
    QString VBD::create(Session* session, const QVariantMap& vbdRecord)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbdRecord;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.create", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    QString VBD::async_create(Session* session, const QVariantMap& vbdRecord)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbdRecord;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VBD.create", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    // VBD.async_plug - Asynchronous plug
    QString VBD::async_plug(Session* session, const QString& vbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VBD.plug", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    // VBD.async_unplug - Asynchronous unplug
    QString VBD::async_unplug(Session* session, const QString& vbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VBD.unplug", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    // VBD.async_destroy - Asynchronous destroy
    QString VBD::async_destroy(Session* session, const QString& vbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VBD.destroy", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    // VBD.async_eject - Asynchronous eject
    QString VBD::async_eject(Session* session, const QString& vbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VBD.eject", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    // VBD.async_insert - Asynchronous insert
    QString VBD::async_insert(Session* session, const QString& vbd, const QString& vdi)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VBD.insert", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    // VBD.plug - Synchronous plug
    void VBD::plug(Session* session, const QString& vbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.plug", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    // VBD.unplug - Synchronous unplug
    void VBD::unplug(Session* session, const QString& vbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.unplug", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    // VBD.destroy - Destroy a VBD
    void VBD::destroy(Session* session, const QString& vbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.destroy", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    // VBD.eject - Synchronous eject
    void VBD::eject(Session* session, const QString& vbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.eject", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    // VBD.insert - Synchronous insert
    void VBD::insert(Session* session, const QString& vbd, const QString& vdi)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd << vdi;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.insert", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    // VBD.get_allowed_operations
    QVariantList VBD::get_allowed_operations(Session* session, const QString& vbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.get_allowed_operations", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toList();
    }

    // VBD.get_VM
    QString VBD::get_VM(Session* session, const QString& vbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.get_VM", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    // VBD.get_VDI
    QString VBD::get_VDI(Session* session, const QString& vbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.get_VDI", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    // VBD.get_device
    QString VBD::get_device(Session* session, const QString& vbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.get_device", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    // VBD.get_userdevice
    QString VBD::get_userdevice(Session* session, const QString& vbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.get_userdevice", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    // VBD.get_bootable
    bool VBD::get_bootable(Session* session, const QString& vbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.get_bootable", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toBool();
    }

    // VBD.get_mode
    QString VBD::get_mode(Session* session, const QString& vbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.get_mode", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    // VBD.get_type
    QString VBD::get_type(Session* session, const QString& vbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.get_type", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    // VBD.get_unpluggable
    bool VBD::get_unpluggable(Session* session, const QString& vbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.get_unpluggable", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toBool();
    }

    // VBD.get_currently_attached
    bool VBD::get_currently_attached(Session* session, const QString& vbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.get_currently_attached", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toBool();
    }

    // VBD.get_empty
    bool VBD::get_empty(Session* session, const QString& vbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.get_empty", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toBool();
    }

    // VBD.get_qos_algorithm_params
    QVariantMap VBD::get_qos_algorithm_params(Session* session, const QString& vbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.get_qos_algorithm_params", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toMap();
    }

    // VBD.set_bootable
    void VBD::set_bootable(Session* session, const QString& vbd, bool bootable)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd << bootable;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.set_bootable", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    // VBD.set_mode
    void VBD::set_mode(Session* session, const QString& vbd, const QString& mode)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd << mode;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.set_mode", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    // VBD.set_userdevice
    void VBD::set_userdevice(Session* session, const QString& vbd, const QString& userdevice)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd << userdevice;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.set_userdevice", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    // VBD.set_qos_algorithm_type
    void VBD::set_qos_algorithm_type(Session* session, const QString& vbd, const QString& algorithmType)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd << algorithmType;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.set_qos_algorithm_type", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    // VBD.set_qos_algorithm_params
    void VBD::set_qos_algorithm_params(Session* session, const QString& vbd, const QVariantMap& algorithmParams)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd << algorithmParams;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.set_qos_algorithm_params", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    // VBD.get_record
    QVariantMap VBD::get_record(Session* session, const QString& vbd)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vbd;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.get_record", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toMap();
    }

    // VBD.get_all
    QVariantList VBD::get_all(Session* session)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID();

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.get_all", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toList();
    }

    // VBD.get_all_records
    QVariantMap VBD::get_all_records(Session* session)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID();

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VBD.get_all_records", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toMap();
    }

} // namespace XenAPI
