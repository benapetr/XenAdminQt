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

#include "xenapi_Auth.h"
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
    const char* const Auth::AUTH_TYPE_AD = "AD";

    QString Auth::get_subject_identifier(Session* session, const QString& subjectName)
    {
        return call(session, "auth.get_subject_identifier", {subjectName}).toString();
    }

    QVariantMap Auth::get_subject_information_from_identifier(Session* session, const QString& subjectIdentifier)
    {
        return call(session, "auth.get_subject_information_from_identifier", {subjectIdentifier}).toMap();
    }

    QStringList Auth::get_group_membership(Session* session, const QString& subjectIdentifier)
    {
        return call(session, "auth.get_group_membership", {subjectIdentifier}).toStringList();
    }
}
