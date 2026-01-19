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

#include "xenapi_VM.h"
#include "../session.h"
#include "../api.h"
#include "../failure.h"
#include <QtCore/QVariantList>
#include <QtCore/QStringList>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace XenAPI
{
    namespace
    {
        void maybeThrowFailureFromResponse(const QByteArray& response)
        {
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(response, &parseError);
            if (parseError.error != QJsonParseError::NoError || !doc.isObject())
                return;

            QJsonObject root = doc.object();

            auto throwFromArray = [](const QJsonArray& array)
            {
                QStringList errors;
                for (const QJsonValue& val : array)
                    errors << val.toString();
                if (!errors.isEmpty())
                    throw Failure(errors);
            };

            if (root.contains("result"))
            {
                QJsonValue resultVal = root.value("result");
                if (resultVal.isObject())
                {
                    QJsonObject resultObj = resultVal.toObject();
                    if (resultObj.value("Status").toString() == "Failure")
                    {
                        QJsonValue errorDesc = resultObj.value("ErrorDescription");
                        if (errorDesc.isArray())
                            throwFromArray(errorDesc.toArray());
                    }
                }
            }

            if (root.contains("error"))
            {
                QJsonObject errorObj = root.value("error").toObject();
                QStringList errors;
                QString message = errorObj.value("message").toString();
                if (!message.isEmpty())
                    errors << message;
                QJsonValue dataVal = errorObj.value("data");
                if (dataVal.isArray())
                {
                    for (const QJsonValue& val : dataVal.toArray())
                        errors << val.toVariant().toString();
                } else if (dataVal.isString())
                {
                    QString dataStr = dataVal.toString();
                    if (!dataStr.isEmpty())
                        errors << dataStr;
                }
                if (!errors.isEmpty())
                    throw Failure(errors);
            }
        }
    }

    // VM.start - Synchronous
    void VM::start(Session* session, const QString& vm, bool start_paused, bool force)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << vm;                      // vm
        params << start_paused;            // start_paused
        params << force;                   // force

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.start", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        // Sync call - no return value, will throw on error
    }

    // VM.async_start - Asynchronous (returns task ref)
    QString VM::async_start(Session* session, const QString& vm, bool start_paused, bool force)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << vm;                      // vm
        params << start_paused;            // start_paused
        params << force;                   // force

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VM.start", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        return result.toString(); // Task ref
    }

    // VM.start_on - Synchronous
    void VM::start_on(Session* session, const QString& vm, const QString& host, bool start_paused, bool force)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << vm;                      // vm
        params << host;                    // host
        params << start_paused;            // start_paused
        params << force;                   // force

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.start_on", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);
    }

    // VM.async_start_on - Asynchronous
    QString VM::async_start_on(Session* session, const QString& vm, const QString& host, bool start_paused, bool force)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << vm;                      // vm
        params << host;                    // host
        params << start_paused;            // start_paused
        params << force;                   // force

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VM.start_on", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        return result.toString();
    }

    // VM.resume - Synchronous
    void VM::resume(Session* session, const QString& vm, bool start_paused, bool force)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << vm;                      // vm
        params << start_paused;            // start_paused
        params << force;                   // force

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.resume", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);
    }

    // VM.async_resume - Asynchronous
    QString VM::async_resume(Session* session, const QString& vm, bool start_paused, bool force)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << vm;                      // vm
        params << start_paused;            // start_paused
        params << force;                   // force

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VM.resume", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        return result.toString();
    }

    // VM.resume_on - Synchronous
    void VM::resume_on(Session* session, const QString& vm, const QString& host, bool start_paused, bool force)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << vm;                      // vm
        params << host;                    // host
        params << start_paused;            // start_paused
        params << force;                   // force

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.resume_on", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);
    }

    // VM.async_resume_on - Asynchronous
    QString VM::async_resume_on(Session* session, const QString& vm, const QString& host, bool start_paused, bool force)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << vm;                      // vm
        params << host;                    // host
        params << start_paused;            // start_paused
        params << force;                   // force

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VM.resume_on", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        return result.toString();
    }

    // VM.clean_shutdown - Synchronous
    void VM::clean_shutdown(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.clean_shutdown", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);
    }

    // VM.async_clean_shutdown - Asynchronous
    QString VM::async_clean_shutdown(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VM.clean_shutdown", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        return result.toString();
    }

    // VM.hard_shutdown - Synchronous
    void VM::hard_shutdown(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.hard_shutdown", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);
    }

    // VM.async_hard_shutdown - Asynchronous
    QString VM::async_hard_shutdown(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VM.hard_shutdown", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        return result.toString();
    }

    // VM.suspend - Synchronous
    void VM::suspend(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.suspend", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);
    }

    // VM.async_suspend - Asynchronous
    QString VM::async_suspend(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VM.suspend", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        return result.toString();
    }

    // VM.clean_reboot - Synchronous
    void VM::clean_reboot(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.clean_reboot", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);
    }

    // VM.async_clean_reboot - Asynchronous
    QString VM::async_clean_reboot(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VM.clean_reboot", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        return result.toString();
    }

    // VM.hard_reboot - Synchronous
    void VM::hard_reboot(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.hard_reboot", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);
    }

    // VM.async_hard_reboot - Asynchronous
    QString VM::async_hard_reboot(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VM.hard_reboot", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        return result.toString();
    }

    // VM.pause - Synchronous
    void VM::pause(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.pause", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);
    }

    // VM.async_pause - Asynchronous
    QString VM::async_pause(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VM.pause", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        return result.toString();
    }

    // VM.unpause - Synchronous
    void VM::unpause(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.unpause", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);
    }

    // VM.async_unpause - Asynchronous
    QString VM::async_unpause(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VM.unpause", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        return result.toString();
    }

    // VM.assert_can_boot_here
    void VM::assert_can_boot_here(Session* session, const QString& self, const QString& host)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << self;                    // self (vm)
        params << host;                    // host

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.assert_can_boot_here", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        if (result.isNull())
        {
            maybeThrowFailureFromResponse(response);
        }
    }

    // VM.assert_can_migrate
    void VM::assert_can_migrate(Session* session, const QString& self, const QString& session_to)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << self;                    // self (vm)
        params << session_to;              // session_to

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.assert_can_migrate", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        // Will throw exception if assertion fails
    }

    void VM::assert_can_migrate(Session* session, const QString& self,
                                const QVariantMap& dest, bool live,
                                const QVariantMap& vdi_map, const QVariantMap& vif_map,
                                const QVariantMap& options)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID();
        params << self;
        params << dest;
        params << live;
        params << vdi_map;
        params << vif_map;
        params << options;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.assert_can_migrate", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    // VM.assert_agile - Used for HA protection checks
    void VM::assert_agile(Session* session, const QString& self)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << self;                    // self (vm)

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.assert_agile", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        // Will throw exception if VM is not agile
    }

    // VM.get_allowed_VBD_devices - Get list of allowed VBD device numbers
    QVariant VM::get_allowed_VBD_devices(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.get_allowed_VBD_devices", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response);
    }

    QVariant VM::get_allowed_VIF_devices(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.get_allowed_VIF_devices", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response);
    }

    // VM.get_record - Get full VM record
    QVariantMap VM::get_record(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.get_record", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toMap();
    }

    // VM.get_all_records - Get all VMs and their records
    QVariantMap VM::get_all_records(Session* session)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.get_all_records", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toMap();
    }

    double VM::query_data_source(Session* session, const QString& vm, const QString& data_source)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << data_source;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.query_data_source", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toDouble();
    }

    // VM.set_suspend_VDI - Set the suspend VDI
    void VM::set_suspend_VDI(Session* session, const QString& vm, const QString& value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID(); // session
        params << vm;                      // vm
        params << value;                   // suspend_VDI ref

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.set_suspend_VDI", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    QString VM::async_pool_migrate(Session* session, const QString& vm, const QString& host, const QVariantMap& options)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << host << options;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VM.pool_migrate", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    QString VM::async_clone(Session* session, const QString& vm, const QString& new_name)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << new_name;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VM.clone", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    QString VM::clone(Session* session, const QString& vm, const QString& new_name)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << new_name;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.clone", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    QString VM::async_copy(Session* session, const QString& vm, const QString& new_name, const QString& sr)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << new_name << sr;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VM.copy", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    QString VM::async_provision(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VM.provision", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    void VM::destroy(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.destroy", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    void VM::set_is_a_template(Session* session, const QString& vm, bool value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << value;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.set_is_a_template", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    void VM::set_name_label(Session* session, const QString& vm, const QString& value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << value;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.set_name_label", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    void VM::set_name_description(Session* session, const QString& vm, const QString& value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << value;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.set_name_description", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    void VM::set_tags(Session* session, const QString& vm, const QStringList& value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << value;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.set_tags", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    void VM::set_suspend_SR(Session* session, const QString& vm, const QString& value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << value;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.set_suspend_SR", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Check for errors
    }

    QString VM::async_snapshot(Session* session, const QString& vm, const QString& new_name)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << new_name;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VM.snapshot", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    QString VM::async_snapshot_with_quiesce(Session* session, const QString& vm, const QString& new_name)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << new_name;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VM.snapshot_with_quiesce", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    QString VM::async_checkpoint(Session* session, const QString& vm, const QString& new_name)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << new_name;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VM.checkpoint", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    QString VM::async_revert(Session* session, const QString& snapshot)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << snapshot;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VM.revert", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    void VM::set_memory_limits(Session* session, const QString& vm,
                               qint64 static_min, qint64 static_max,
                               qint64 dynamic_min, qint64 dynamic_max)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm
               << static_min << static_max << dynamic_min << dynamic_max;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.set_memory_limits", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Void method
    }

    void VM::set_memory_dynamic_range(Session* session, const QString& vm,
                                      qint64 dynamic_min, qint64 dynamic_max)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << dynamic_min << dynamic_max;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.set_memory_dynamic_range", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Void method
    }

    void VM::set_VCPUs_max(Session* session, const QString& vm, qint64 value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << value;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.set_VCPUs_max", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Void method
    }

    void VM::set_VCPUs_at_startup(Session* session, const QString& vm, qint64 value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << value;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.set_VCPUs_at_startup", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Void method
    }

    void VM::set_VCPUs_number_live(Session* session, const QString& vm, qint64 nvcpu)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << nvcpu;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.set_VCPUs_number_live", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Void method
    }

    QString VM::async_migrate_send(Session* session, const QString& vm,
                                   const QVariantMap& dest, bool live,
                                   const QVariantMap& vdi_map, const QVariantMap& vif_map,
                                   const QVariantMap& options)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << dest << live << vdi_map << vif_map << options;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VM.migrate_send", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    void VM::set_ha_restart_priority(Session* session, const QString& vm, const QString& value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << value;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.set_ha_restart_priority", params);
        session->SendApiRequest(request);
    }

    void VM::set_order(Session* session, const QString& vm, qint64 value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << value;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.set_order", params);
        session->SendApiRequest(request);
    }

    void VM::set_start_delay(Session* session, const QString& vm, qint64 value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << value;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.set_start_delay", params);
        session->SendApiRequest(request);
    }

    void VM::set_HVM_shadow_multiplier(Session* session, const QString& vm, double value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << value;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.set_HVM_shadow_multiplier", params);
        session->SendApiRequest(request);
    }

    void VM::set_shadow_multiplier_live(Session* session, const QString& vm, double value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << value;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.set_shadow_multiplier_live", params);
        session->SendApiRequest(request);
    }

    // VM.set_HVM_boot_policy
    void VM::set_HVM_boot_policy(Session* session, const QString& vm, const QString& value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID();
        params << vm;
        params << value;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.set_HVM_boot_policy", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Throws on error
    }

    // VM.set_HVM_boot_params
    void VM::set_HVM_boot_params(Session* session, const QString& vm, const QVariantMap& bootParams)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID();
        params << vm;
        params << bootParams;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.set_HVM_boot_params", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Throws on error
    }

    // VM.get_HVM_boot_policy
    QString VM::get_HVM_boot_policy(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID();
        params << vm;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.get_HVM_boot_policy", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        return result.toString();
    }

    // VM.get_HVM_boot_params
    QVariantMap VM::get_HVM_boot_params(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID();
        params << vm;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.get_HVM_boot_params", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        return result.toMap();
    }

    void VM::set_PV_args(Session* session, const QString& vm, const QString& value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << value;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.set_PV_args", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    void VM::set_other_config(Session* session, const QString& vm, const QVariantMap& otherConfig)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << otherConfig;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.set_other_config", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    void VM::set_VCPUs_params(Session* session, const QString& vm, const QVariantMap& vcpusParams)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << vcpusParams;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.set_VCPUs_params", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    void VM::set_platform(Session* session, const QString& vm, const QVariantMap& platform)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << platform;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.set_platform", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    void VM::set_affinity(Session* session, const QString& vm, const QString& host)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm << host;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.set_affinity", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    QString VM::create_new_blob(Session* session,
                                const QString& vm,
                                const QString& name,
                                const QString& mimeType,
                                bool isPublic)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID();
        params << vm;
        params << name;
        params << mimeType;
        params << isPublic;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.create_new_blob", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        return result.toString();
    }

    QHash<QString, QStringList> VM::retrieve_wlb_recommendations(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.retrieve_wlb_recommendations", params);
        QByteArray response = session->SendApiRequest(request);
        
        maybeThrowFailureFromResponse(response);
        
        QVariant result = api.ParseJsonRpcResponse(response);

        // Result is a Map<XenRef<Host>, string[]>
        // Parse into QHash<QString, QStringList>
        QHash<QString, QStringList> recommendations;
        
        if (result.type() == QVariant::Map)
        {
            QVariantMap map = result.toMap();
            for (auto it = map.constBegin(); it != map.constEnd(); ++it)
            {
                QString hostRef = it.key();
                QVariant value = it.value();
                
                QStringList recArray;
                if (value.type() == QVariant::List)
                {
                    QVariantList list = value.toList();
                    foreach (const QVariant& item, list)
                    {
                        recArray.append(item.toString());
                    }
                }
                
                recommendations[hostRef] = recArray;
            }
        }

        return recommendations;
    }

    // VM.get_power_state
    QString VM::get_power_state(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << vm;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM.get_power_state", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

} // namespace XenAPI
