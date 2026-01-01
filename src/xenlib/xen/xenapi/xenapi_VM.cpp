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
        params << session->getSessionId(); // session
        params << vm;                      // vm
        params << start_paused;            // start_paused
        params << force;                   // force

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.start", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);

        // Sync call - no return value, will throw on error
    }

    // VM.async_start - Asynchronous (returns task ref)
    QString VM::async_start(Session* session, const QString& vm, bool start_paused, bool force)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << vm;                      // vm
        params << start_paused;            // start_paused
        params << force;                   // force

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VM.start", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);

        return result.toString(); // Task ref
    }

    // VM.start_on - Synchronous
    void VM::start_on(Session* session, const QString& vm, const QString& host, bool start_paused, bool force)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << vm;                      // vm
        params << host;                    // host
        params << start_paused;            // start_paused
        params << force;                   // force

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.start_on", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);
    }

    // VM.async_start_on - Asynchronous
    QString VM::async_start_on(Session* session, const QString& vm, const QString& host, bool start_paused, bool force)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << vm;                      // vm
        params << host;                    // host
        params << start_paused;            // start_paused
        params << force;                   // force

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VM.start_on", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);

        return result.toString();
    }

    // VM.resume - Synchronous
    void VM::resume(Session* session, const QString& vm, bool start_paused, bool force)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << vm;                      // vm
        params << start_paused;            // start_paused
        params << force;                   // force

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.resume", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);
    }

    // VM.async_resume - Asynchronous
    QString VM::async_resume(Session* session, const QString& vm, bool start_paused, bool force)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << vm;                      // vm
        params << start_paused;            // start_paused
        params << force;                   // force

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VM.resume", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);

        return result.toString();
    }

    // VM.resume_on - Synchronous
    void VM::resume_on(Session* session, const QString& vm, const QString& host, bool start_paused, bool force)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << vm;                      // vm
        params << host;                    // host
        params << start_paused;            // start_paused
        params << force;                   // force

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.resume_on", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);
    }

    // VM.async_resume_on - Asynchronous
    QString VM::async_resume_on(Session* session, const QString& vm, const QString& host, bool start_paused, bool force)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << vm;                      // vm
        params << host;                    // host
        params << start_paused;            // start_paused
        params << force;                   // force

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VM.resume_on", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);

        return result.toString();
    }

    // VM.clean_shutdown - Synchronous
    void VM::clean_shutdown(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.clean_shutdown", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);
    }

    // VM.async_clean_shutdown - Asynchronous
    QString VM::async_clean_shutdown(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VM.clean_shutdown", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);

        return result.toString();
    }

    // VM.hard_shutdown - Synchronous
    void VM::hard_shutdown(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.hard_shutdown", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);
    }

    // VM.async_hard_shutdown - Asynchronous
    QString VM::async_hard_shutdown(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VM.hard_shutdown", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);

        return result.toString();
    }

    // VM.suspend - Synchronous
    void VM::suspend(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.suspend", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);
    }

    // VM.async_suspend - Asynchronous
    QString VM::async_suspend(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VM.suspend", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);

        return result.toString();
    }

    // VM.clean_reboot - Synchronous
    void VM::clean_reboot(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.clean_reboot", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);
    }

    // VM.async_clean_reboot - Asynchronous
    QString VM::async_clean_reboot(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VM.clean_reboot", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);

        return result.toString();
    }

    // VM.hard_reboot - Synchronous
    void VM::hard_reboot(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.hard_reboot", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);
    }

    // VM.async_hard_reboot - Asynchronous
    QString VM::async_hard_reboot(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VM.hard_reboot", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);

        return result.toString();
    }

    // VM.pause - Synchronous
    void VM::pause(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.pause", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);
    }

    // VM.async_pause - Asynchronous
    QString VM::async_pause(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VM.pause", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);

        return result.toString();
    }

    // VM.unpause - Synchronous
    void VM::unpause(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.unpause", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);
    }

    // VM.async_unpause - Asynchronous
    QString VM::async_unpause(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VM.unpause", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);

        return result.toString();
    }

    // VM.assert_can_boot_here
    void VM::assert_can_boot_here(Session* session, const QString& self, const QString& host)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << self;                    // self (vm)
        params << host;                    // host

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.assert_can_boot_here", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);

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
        params << session->getSessionId(); // session
        params << self;                    // self (vm)
        params << session_to;              // session_to

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.assert_can_migrate", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);

        // Will throw exception if assertion fails
    }

    // VM.assert_agile - Used for HA protection checks
    void VM::assert_agile(Session* session, const QString& self)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << self;                    // self (vm)

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.assert_agile", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);

        // Will throw exception if VM is not agile
    }

    // VM.get_allowed_VBD_devices - Get list of allowed VBD device numbers
    QVariant VM::get_allowed_VBD_devices(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.get_allowed_VBD_devices", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response);
    }

    QVariant VM::get_allowed_VIF_devices(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.get_allowed_VIF_devices", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response);
    }

    // VM.get_record - Get full VM record
    QVariantMap VM::get_record(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << vm;                      // vm

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.get_record", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toMap();
    }

    // VM.get_all_records - Get all VMs and their records
    QVariantMap VM::get_all_records(Session* session)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.get_all_records", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toMap();
    }

    double VM::query_data_source(Session* session, const QString& vm, const QString& data_source)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << data_source;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.query_data_source", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toDouble();
    }

    // VM.set_suspend_VDI - Set the suspend VDI
    void VM::set_suspend_VDI(Session* session, const QString& vm, const QString& value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId(); // session
        params << vm;                      // vm
        params << value;                   // suspend_VDI ref

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.set_suspend_VDI", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Check for errors
    }

    QString VM::async_pool_migrate(Session* session, const QString& vm, const QString& host, const QVariantMap& options)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << host << options;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VM.pool_migrate", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    QString VM::async_clone(Session* session, const QString& vm, const QString& new_name)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << new_name;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VM.clone", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    QString VM::clone(Session* session, const QString& vm, const QString& new_name)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << new_name;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.clone", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString();
    }

    QString VM::async_copy(Session* session, const QString& vm, const QString& new_name, const QString& sr)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << new_name << sr;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VM.copy", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    QString VM::async_provision(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VM.provision", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    void VM::destroy(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.destroy", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Check for errors
    }

    void VM::set_is_a_template(Session* session, const QString& vm, bool value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << value;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.set_is_a_template", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Check for errors
    }

    void VM::set_name_label(Session* session, const QString& vm, const QString& value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << value;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.set_name_label", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Check for errors
    }

    void VM::set_name_description(Session* session, const QString& vm, const QString& value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << value;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.set_name_description", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Check for errors
    }

    void VM::set_tags(Session* session, const QString& vm, const QStringList& value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << value;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.set_tags", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Check for errors
    }

    QString VM::async_snapshot(Session* session, const QString& vm, const QString& new_name)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << new_name;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VM.snapshot", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    QString VM::async_snapshot_with_quiesce(Session* session, const QString& vm, const QString& new_name)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << new_name;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VM.snapshot_with_quiesce", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    QString VM::async_checkpoint(Session* session, const QString& vm, const QString& new_name)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << new_name;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VM.checkpoint", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    QString VM::async_revert(Session* session, const QString& snapshot)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << snapshot;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VM.revert", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    void VM::set_memory_limits(Session* session, const QString& vm,
                               qint64 static_min, qint64 static_max,
                               qint64 dynamic_min, qint64 dynamic_max)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm
               << static_min << static_max << dynamic_min << dynamic_max;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.set_memory_limits", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Void method
    }

    void VM::set_memory_dynamic_range(Session* session, const QString& vm,
                                      qint64 dynamic_min, qint64 dynamic_max)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << dynamic_min << dynamic_max;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.set_memory_dynamic_range", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Void method
    }

    void VM::set_VCPUs_max(Session* session, const QString& vm, qint64 value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << value;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.set_VCPUs_max", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Void method
    }

    void VM::set_VCPUs_at_startup(Session* session, const QString& vm, qint64 value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << value;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.set_VCPUs_at_startup", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Void method
    }

    void VM::set_VCPUs_number_live(Session* session, const QString& vm, qint64 nvcpu)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << nvcpu;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.set_VCPUs_number_live", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Void method
    }

    QString VM::async_migrate_send(Session* session, const QString& vm,
                                   const QVariantMap& dest, bool live,
                                   const QVariantMap& vdi_map, const QVariantMap& vif_map,
                                   const QVariantMap& options)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << dest << live << vdi_map << vif_map << options;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("Async.VM.migrate_send", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString(); // Returns task ref
    }

    void VM::set_ha_restart_priority(Session* session, const QString& vm, const QString& value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << value;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.set_ha_restart_priority", params);
        session->sendApiRequest(request);
    }

    void VM::set_order(Session* session, const QString& vm, qint64 value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << value;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.set_order", params);
        session->sendApiRequest(request);
    }

    void VM::set_start_delay(Session* session, const QString& vm, qint64 value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << value;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.set_start_delay", params);
        session->sendApiRequest(request);
    }

    void VM::set_HVM_shadow_multiplier(Session* session, const QString& vm, double value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << value;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.set_HVM_shadow_multiplier", params);
        session->sendApiRequest(request);
    }

    void VM::set_shadow_multiplier_live(Session* session, const QString& vm, double value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << value;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.set_shadow_multiplier_live", params);
        session->sendApiRequest(request);
    }

    // VM.set_HVM_boot_policy
    void VM::set_HVM_boot_policy(Session* session, const QString& vm, const QString& value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();
        params << vm;
        params << value;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.set_HVM_boot_policy", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Throws on error
    }

    // VM.set_HVM_boot_params
    void VM::set_HVM_boot_params(Session* session, const QString& vm, const QVariantMap& bootParams)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();
        params << vm;
        params << bootParams;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.set_HVM_boot_params", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Throws on error
    }

    // VM.get_HVM_boot_policy
    QString VM::get_HVM_boot_policy(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();
        params << vm;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.get_HVM_boot_policy", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);

        return result.toString();
    }

    // VM.get_HVM_boot_params
    QVariantMap VM::get_HVM_boot_params(Session* session, const QString& vm)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();
        params << vm;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.get_HVM_boot_params", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);

        return result.toMap();
    }

    void VM::set_PV_args(Session* session, const QString& vm, const QString& value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << value;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.set_PV_args", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response);
    }

    void VM::set_other_config(Session* session, const QString& vm, const QVariantMap& otherConfig)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << otherConfig;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.set_other_config", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response);
    }

    void VM::set_VCPUs_params(Session* session, const QString& vm, const QVariantMap& vcpusParams)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << vcpusParams;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.set_VCPUs_params", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response);
    }

    void VM::set_platform(Session* session, const QString& vm, const QVariantMap& platform)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << platform;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.set_platform", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response);
    }

    void VM::set_affinity(Session* session, const QString& vm, const QString& host)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << vm << host;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.set_affinity", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response);
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
        params << session->getSessionId();
        params << vm;
        params << name;
        params << mimeType;
        params << isPublic;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("VM.create_new_blob", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.parseJsonRpcResponse(response);

        return result.toString();
    }

} // namespace XenAPI
