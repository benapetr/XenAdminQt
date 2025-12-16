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

#include "Secret.h"
#include "../api.h"
#include <stdexcept>
#include <QVariantMap>

namespace XenAPI
{

    QString Secret::create(XenSession* session, const QString& value)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        // Create a secret record with the value
        QVariantMap record;
        record["value"] = value;

        QVariantList params;
        params << session->getSessionId() << record;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("secret.create", params);
        QByteArray response = session->sendApiRequest(request);

        // Returns secret reference, then get UUID from it
        QString secretRef = api.parseJsonRpcResponse(response).toString();

        // Get the UUID
        QVariantList getUuidParams;
        getUuidParams << session->getSessionId() << secretRef;
        QByteArray uuidRequest = api.buildJsonRpcCall("secret.get_uuid", getUuidParams);
        QByteArray uuidResponse = session->sendApiRequest(uuidRequest);
        return api.parseJsonRpcResponse(uuidResponse).toString();
    }

    QString Secret::get_by_uuid(XenSession* session, const QString& uuid)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << uuid;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("secret.get_by_uuid", params);
        QByteArray response = session->sendApiRequest(request);
        return api.parseJsonRpcResponse(response).toString();
    }

    void Secret::destroy(XenSession* session, const QString& secret)
    {
        if (!session || !session->isLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->getSessionId() << secret;

        XenRpcAPI api(session);
        QByteArray request = api.buildJsonRpcCall("secret.destroy", params);
        QByteArray response = session->sendApiRequest(request);
        api.parseJsonRpcResponse(response); // Check for errors
    }

} // namespace XenAPI
