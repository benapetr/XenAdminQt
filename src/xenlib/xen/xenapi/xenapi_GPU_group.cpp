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

#include "xenapi_GPU_group.h"
#include "../session.h"
#include "../api.h"
#include <QVariantList>
#include <stdexcept>

namespace XenAPI
{
QString AllocationAlgorithmToWireValue(AllocationAlgorithm algorithm)
{
    switch (algorithm)
    {
        case AllocationAlgorithm::BreadthFirst:
            return QStringLiteral("breadth_first");
        case AllocationAlgorithm::DepthFirst:
            return QStringLiteral("depth_first");
        case AllocationAlgorithm::Unknown:
        default:
            return QStringLiteral("unknown");
    }
}

AllocationAlgorithm AllocationAlgorithmFromWireValue(const QString& value)
{
    if (value.compare(QStringLiteral("breadth_first"), Qt::CaseInsensitive) == 0)
        return AllocationAlgorithm::BreadthFirst;
    if (value.compare(QStringLiteral("depth_first"), Qt::CaseInsensitive) == 0)
        return AllocationAlgorithm::DepthFirst;
    return AllocationAlgorithm::Unknown;
}

void GPU_group::set_allocation_algorithm(Session* session, const QString& gpuGroupRef, AllocationAlgorithm algorithm)
{
    if (!session || !session->IsLoggedIn())
        throw std::runtime_error("Not connected to XenServer");

    QVariantList params;
    params << session->GetSessionID() << gpuGroupRef << AllocationAlgorithmToWireValue(algorithm);

    XenRpcAPI api(session);
    QByteArray request = api.BuildJsonRpcCall("GPU_group.set_allocation_algorithm", params);
    QByteArray response = session->SendApiRequest(request);
    api.ParseJsonRpcResponse(response);
}

QString GPU_group::async_set_allocation_algorithm(Session* session, const QString& gpuGroupRef, AllocationAlgorithm algorithm)
{
    if (!session || !session->IsLoggedIn())
        throw std::runtime_error("Not connected to XenServer");

    QVariantList params;
    params << session->GetSessionID() << gpuGroupRef << AllocationAlgorithmToWireValue(algorithm);

    XenRpcAPI api(session);
    QByteArray request = api.BuildJsonRpcCall("Async.GPU_group.set_allocation_algorithm", params);
    QByteArray response = session->SendApiRequest(request);
    return api.ParseJsonRpcResponse(response).toString();
}
} // namespace XenAPI
