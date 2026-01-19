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

#include "xenapi_PGPU.h"
#include "../session.h"
#include "../api.h"
#include <QtCore/QVariantList>
#include <stdexcept>

namespace XenAPI
{

// PGPU.set_enabled_VGPU_types - Synchronous
void PGPU::set_enabled_VGPU_types(Session* session, const QString& pgpu, const QStringList& value)
{
    if (!session || !session->IsLoggedIn())
        throw std::runtime_error("Not connected to XenServer");

    QVariantList params;
    params << session->GetSessionID(); // session
    params << pgpu;                    // pgpu ref
    params << QVariant(value);         // list of VGPU_type refs

    XenRpcAPI api(session);
    QByteArray request = api.BuildJsonRpcCall("PGPU.set_enabled_VGPU_types", params);
    QByteArray response = session->SendApiRequest(request);
    api.ParseJsonRpcResponse(response); // Throws on error
}

// PGPU.async_set_enabled_VGPU_types - Asynchronous (returns task ref)
QString PGPU::async_set_enabled_VGPU_types(Session* session, const QString& pgpu, const QStringList& value)
{
    if (!session || !session->IsLoggedIn())
        throw std::runtime_error("Not connected to XenServer");

    QVariantList params;
    params << session->GetSessionID(); // session
    params << pgpu;                    // pgpu ref
    params << QVariant(value);         // list of VGPU_type refs

    XenRpcAPI api(session);
    QByteArray request = api.BuildJsonRpcCall("Async.PGPU.set_enabled_VGPU_types", params);
    QByteArray response = session->SendApiRequest(request);
    QVariant result = api.ParseJsonRpcResponse(response);

    return result.toString(); // Task ref
}

} // namespace XenAPI
