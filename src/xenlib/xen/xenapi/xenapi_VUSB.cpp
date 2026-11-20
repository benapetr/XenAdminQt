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

#include "xenapi_VUSB.h"
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
    QString VUSB::create(Session* session, const QString& vm, const QString& usbGroup, const QVariantMap& otherConfig)
    {
        return call(session, "VUSB.create", {vm, usbGroup, otherConfig}).toString();
    }

    QString VUSB::async_unplug(Session* session, const QString& vusb)
    {
        return call(session, "Async.VUSB.unplug", {vusb}).toString();
    }

    QString VUSB::async_destroy(Session* session, const QString& vusb)
    {
        return call(session, "Async.VUSB.destroy", {vusb}).toString();
    }

    QStringList VUSB::get_allowed_operations(Session* session, const QString& vusb)
    {
        QStringList operations;
        const QVariantList list = call(session, "VUSB.get_allowed_operations", {vusb}).toList();
        for (const QVariant& op : list)
            operations << op.toString();
        return operations;
    }
}
