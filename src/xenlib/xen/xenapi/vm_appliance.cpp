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

#include "vm_appliance.h"
#include "../session.h"
#include "../api.h"
#include <QtCore/QVariantList>
#include <stdexcept>

namespace XenAPI
{

    // VM_appliance.get_allowed_operations
    QStringList VM_appliance::get_allowed_operations(Session* session, const QString& applianceRef)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();
        params << applianceRef;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM_appliance.get_allowed_operations", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        // Result is array of strings: ["start", "clean_shutdown", etc.]
        QStringList operations;
        if (result.canConvert<QVariantList>())
        {
            for (const QVariant& op : result.toList())
            {
                operations << op.toString();
            }
        }
        return operations;
    }

    // VM_appliance.get_current_operations
    QVariantMap VM_appliance::get_current_operations(Session* session, const QString& applianceRef)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();
        params << applianceRef;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM_appliance.get_current_operations", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        // Result is map: task_ref → operation_name
        return result.toMap();
    }

    // VM_appliance.get_VMs
    QStringList VM_appliance::get_VMs(Session* session, const QString& applianceRef)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();
        params << applianceRef;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM_appliance.get_VMs", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        // Result is array of VM OpaqueRefs
        QStringList vmRefs;
        if (result.canConvert<QVariantList>())
        {
            for (const QVariant& vmRef : result.toList())
            {
                vmRefs << vmRef.toString();
            }
        }
        return vmRefs;
    }

    // VM_appliance.get_record
    QVariantMap VM_appliance::get_record(Session* session, const QString& applianceRef)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();
        params << applianceRef;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM_appliance.get_record", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        return result.toMap();
    }

    // VM_appliance.get_all_records
    QVariantMap VM_appliance::get_all_records(Session* session)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM_appliance.get_all_records", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        return result.toMap();
    }

    // VM_appliance.set_name_label
    void VM_appliance::set_name_label(Session* session, const QString& applianceRef, const QString& label)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();
        params << applianceRef;
        params << label;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM_appliance.set_name_label", params);
        QByteArray response = session->sendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Throws on error
    }

    // VM_appliance.set_name_description
    void VM_appliance::set_name_description(Session* session, const QString& applianceRef, const QString& description)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();
        params << applianceRef;
        params << description;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM_appliance.set_name_description", params);
        QByteArray response = session->sendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Throws on error
    }

    // VM_appliance.async_start
    QString VM_appliance::async_start(Session* session, const QString& applianceRef, bool paused)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();
        params << applianceRef;
        params << paused;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VM_appliance.start", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        return result.toString(); // Task ref
    }

    // VM_appliance.start
    void VM_appliance::start(Session* session, const QString& applianceRef, bool paused)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();
        params << applianceRef;
        params << paused;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM_appliance.start", params);
        QByteArray response = session->sendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Throws on error
    }

    // VM_appliance.async_clean_shutdown
    QString VM_appliance::async_clean_shutdown(Session* session, const QString& applianceRef)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();
        params << applianceRef;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VM_appliance.clean_shutdown", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        return result.toString(); // Task ref
    }

    // VM_appliance.clean_shutdown
    void VM_appliance::clean_shutdown(Session* session, const QString& applianceRef)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();
        params << applianceRef;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM_appliance.clean_shutdown", params);
        QByteArray response = session->sendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Throws on error
    }

    // VM_appliance.async_hard_shutdown
    QString VM_appliance::async_hard_shutdown(Session* session, const QString& applianceRef)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();
        params << applianceRef;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VM_appliance.hard_shutdown", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        return result.toString(); // Task ref
    }

    // VM_appliance.hard_shutdown
    void VM_appliance::hard_shutdown(Session* session, const QString& applianceRef)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();
        params << applianceRef;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM_appliance.hard_shutdown", params);
        QByteArray response = session->sendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Throws on error
    }

    // VM_appliance.async_shutdown
    QString VM_appliance::async_shutdown(Session* session, const QString& applianceRef)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();
        params << applianceRef;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.VM_appliance.shutdown", params);
        QByteArray response = session->sendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);

        return result.toString(); // Task ref
    }

    // VM_appliance.shutdown
    void VM_appliance::shutdown(Session* session, const QString& applianceRef)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId();
        params << applianceRef;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("VM_appliance.shutdown", params);
        QByteArray response = session->sendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Throws on error
    }

} // namespace XenAPI
