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

#include "xenapi_Session.h"
#include "../session.h"
#include "../api.h"
#include <stdexcept>

namespace XenAPI
{
    bool SessionAPI::get_is_local_superuser(Session* session, const QString& sessionRef)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << sessionRef;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("session.get_is_local_superuser", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toBool();
    }

    QString SessionAPI::get_subject(Session* session, const QString& sessionRef)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << sessionRef;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("session.get_subject", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    QString SessionAPI::get_auth_user_sid(Session* session, const QString& sessionRef)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << sessionRef;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("session.get_auth_user_sid", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString();
    }

    QStringList SessionAPI::get_rbac_permissions(Session* session, const QString& sessionRef)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << sessionRef;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("session.get_rbac_permissions", params);
        QByteArray response = session->SendApiRequest(request);
        
        QVariant result = api.ParseJsonRpcResponse(response);
        if (result.canConvert<QVariantList>())
        {
            QStringList permissions;
            QVariantList list = result.toList();
            for (const QVariant& item : list)
            {
                permissions << item.toString();
            }
            return permissions;
        }

        return QStringList();
    }

    void SessionAPI::change_password(Session* session, const QString& oldPassword, const QString& newPassword)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << oldPassword << newPassword;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("session.change_password", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Void method
    }

} // namespace XenAPI
