/*
 * Copyright (c) 2026, Petr Bena <petr@bena.rocks>
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

#include "xenapi_VMSS.h"
#include "../session.h"
#include "../api.h"
#include <stdexcept>

namespace
{
    QVariant call(XenAPI::Session* session, const QString& method, const QVariantList& args)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << args;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall(method, params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response);
    }
}

namespace XenAPI
{
    QString VMSS::create(Session* session, const QVariantMap& record)
    {
        return call(session, "VMSS.create", {record}).toString();
    }

    void VMSS::destroy(Session* session, const QString& vmss)
    {
        call(session, "VMSS.destroy", {vmss});
    }

    QString VMSS::snapshot_now(Session* session, const QString& vmss)
    {
        return call(session, "VMSS.snapshot_now", {vmss}).toString();
    }

    void VMSS::set_name_label(Session* session, const QString& vmss, const QString& value)
    {
        call(session, "VMSS.set_name_label", {vmss, value});
    }

    void VMSS::set_name_description(Session* session, const QString& vmss, const QString& value)
    {
        call(session, "VMSS.set_name_description", {vmss, value});
    }

    void VMSS::set_enabled(Session* session, const QString& vmss, bool value)
    {
        call(session, "VMSS.set_enabled", {vmss, value});
    }

    void VMSS::set_type(Session* session, const QString& vmss, const QString& value)
    {
        call(session, "VMSS.set_type", {vmss, value});
    }

    void VMSS::set_retained_snapshots(Session* session, const QString& vmss, qint64 value)
    {
        call(session, "VMSS.set_retained_snapshots", {vmss, value});
    }

    void VMSS::set_frequency(Session* session, const QString& vmss, const QString& value)
    {
        call(session, "VMSS.set_frequency", {vmss, value});
    }

    void VMSS::set_schedule(Session* session, const QString& vmss, const QVariantMap& value)
    {
        call(session, "VMSS.set_schedule", {vmss, value});
    }
}
