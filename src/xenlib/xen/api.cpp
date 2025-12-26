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

#include "api.h"
#include "session.h"
#include "jsonrpcclient.h"
#include "../utils/misc.h"
#include <QDebug>

using namespace XenAPI;

class XenRpcAPI::Private
{
    public:
        Session* session = nullptr;
        int nextRequestId = 1; // Request ID counter for JSON-RPC
};

XenRpcAPI::XenRpcAPI(Session *session, QObject* parent) : QObject(parent), d(new Private)
{
    this->d->session = session;
}

XenRpcAPI::~XenRpcAPI()
{
    delete this->d;
}

QString XenRpcAPI::getSessionId() const
{
    if (this->d->session && this->d->session->IsLoggedIn())
    {
        return this->d->session->getSessionId();
    }
    return QString();
}

// VM migration operations

void XenRpcAPI::handleEventPolling()
{
    // TODO: Implement actual event polling mechanism
    // This would poll the Xen server for events and emit appropriate signals
    emit this->eventReceived("session.pool_patch_upload_cancelled");
}

// JSON-RPC wrapper methods (delegate to JsonRpcClient)
QByteArray XenRpcAPI::buildJsonRpcCall(const QString& method, const QVariantList& params)
{
    int requestId = this->d->nextRequestId++;
    return Xen::JsonRpcClient::buildJsonRpcCall(method, params, requestId);
}

QVariant XenRpcAPI::parseJsonRpcResponse(const QByteArray& response)
{
    return Xen::JsonRpcClient::parseJsonRpcResponse(response);
}

void XenRpcAPI::makeAsyncCall(const QString& method, const QVariantList& params, const QString& callId)
{
    Q_UNUSED(callId);

    if (!this->d->session || !this->d->session->IsLoggedIn())
    {
        emit this->apiCallFailed(method, "Not logged in");
        return;
    }

    // TODO: Implement async API calls
    emit this->apiCallCompleted(method, QVariant());
}

QVariant XenRpcAPI::getTaskRecord(const QString& taskRef)
{
    if (!this->d->session || !this->d->session->IsLoggedIn())
    {
        emit this->apiCallFailed("task.get_record", "Not logged in");
        return QVariant();
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << taskRef;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("task.get_record", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        emit this->apiCallFailed("task.get_record", "Empty response");
        return QVariant();
    }

    QVariant result = this->parseJsonRpcResponse(response);
    return result;
}

QString XenRpcAPI::getTaskStatus(const QString& taskRef)
{
    if (!this->d->session || !this->d->session->IsLoggedIn())
    {
        return QString();
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << taskRef;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("task.get_status", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        return QString();
    }

    QVariant result = this->parseJsonRpcResponse(response);
    return result.toString();
}

double XenRpcAPI::getTaskProgress(const QString& taskRef)
{
    if (!this->d->session || !this->d->session->IsLoggedIn())
    {
        return -1.0;
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << taskRef;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("task.get_progress", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        return -1.0;
    }

    QVariant result = this->parseJsonRpcResponse(response);
    return result.toDouble();
}

QString XenRpcAPI::getTaskResult(const QString& taskRef)
{
    if (!this->d->session || !this->d->session->IsLoggedIn())
    {
        return QString();
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << taskRef;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("task.get_result", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        return QString();
    }

    QVariant result = this->parseJsonRpcResponse(response);
    return result.toString();
}

QStringList XenRpcAPI::getTaskErrorInfo(const QString& taskRef)
{
    if (!this->d->session || !this->d->session->IsLoggedIn())
    {
        return QStringList();
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << taskRef;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("task.get_error_info", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        return QStringList();
    }

    QVariant result = this->parseJsonRpcResponse(response);
    QStringList errorList;

    if (result.canConvert<QVariantList>())
    {
        QVariantList list = result.toList();
        for (const QVariant& item : list)
        {
            errorList.append(item.toString());
        }
    }

    return errorList;
}

QVariantMap XenRpcAPI::getAllTaskRecords()
{
    if (!this->d->session || !this->d->session->IsLoggedIn())
    {
        emit this->apiCallFailed("task.get_all_records", "Not logged in");
        return QVariantMap();
    }

    QVariantList params;
    params << this->d->session->getSessionId();

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("task.get_all_records", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        emit this->apiCallFailed("task.get_all_records", "Empty response");
        return QVariantMap();
    }

    QVariant result = this->parseJsonRpcResponse(response);

    if (result.canConvert<QVariantMap>())
    {
        return result.toMap();
    }

    return QVariantMap();
}

bool XenRpcAPI::cancelTask(const QString& taskRef)
{
    if (!this->d->session || !this->d->session->IsLoggedIn())
    {
        emit this->apiCallFailed("task.cancel", "Not logged in");
        return false;
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << taskRef;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("task.cancel", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        emit this->apiCallFailed("task.cancel", "Empty response");
        return false;
    }

    // Check if response indicates success
    QVariant result = this->parseJsonRpcResponse(response);
    return !result.isNull();
}

bool XenRpcAPI::destroyTask(const QString& taskRef)
{
    if (!this->d->session || !this->d->session->IsLoggedIn())
    {
        emit this->apiCallFailed("task.destroy", "Not logged in");
        return false;
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << taskRef;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("task.destroy", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        emit this->apiCallFailed("task.destroy", "Empty response");
        return false;
    }

    // Check if response indicates success
    QVariant result = this->parseJsonRpcResponse(response);
    return !result.isNull();
}

bool XenRpcAPI::addToTaskOtherConfig(const QString& taskRef, const QString& key, const QString& value)
{
    if (!this->d->session || !this->d->session->IsLoggedIn())
    {
        return false;
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << taskRef;
    params << key;
    params << value;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("task.add_to_other_config", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        return false;
    }

    QVariant result = this->parseJsonRpcResponse(response);
    return !result.isNull();
}

bool XenRpcAPI::removeFromTaskOtherConfig(const QString& taskRef, const QString& key)
{
    if (!this->d->session || !this->d->session->IsLoggedIn())
    {
        return false;
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << taskRef;
    params << key;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("task.remove_from_other_config", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        return false;
    }

    QVariant result = this->parseJsonRpcResponse(response);
    return !result.isNull();
}

QVariantMap XenRpcAPI::getTaskOtherConfig(const QString& taskRef)
{
    if (!this->d->session || !this->d->session->IsLoggedIn())
    {
        return QVariantMap();
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << taskRef;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("task.get_other_config", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        return QVariantMap();
    }

    QVariant result = this->parseJsonRpcResponse(response);
    if (result.canConvert<QVariantMap>())
    {
        return result.toMap();
    }

    return QVariantMap();
}

// Event.from - Modern event API (API 1.9+)
// This is the main event method used by XenServer
// Returns: { "events": [...], "token": "...", "valid_ref_counts": {...} }
QVariantMap XenRpcAPI::eventFrom(const QStringList& classes, const QString& token, double timeout)
{
    if (!this->d->session || !this->d->session->IsLoggedIn())
    {
        emit this->apiCallFailed("event.from", "Not logged in");
        return QVariantMap();
    }

    // Build parameters: session_id, classes, token, timeout
    QVariantList params;
    params << this->d->session->getSessionId();
    params << classes;
    params << token;
    params << timeout;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("event.from", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        emit this->apiCallFailed("event.from", "Empty response");
        return QVariantMap();
    }

    // Parse response - should be a struct with "events", "token", "valid_ref_counts"
    QVariant result = this->parseJsonRpcResponse(response);

    // Convert to map if it's not already
    if (Misc::QVariantIsMap(result))
    {
        return result.toMap();
    }

    emit this->apiCallFailed("event.from", "Invalid response format");
    return QVariantMap();
}

// Legacy event registration (pre-1.9 API, rarely used now)
bool XenRpcAPI::eventRegister(const QStringList& classes)
{
    if (!this->d->session || !this->d->session->IsLoggedIn())
    {
        emit this->apiCallFailed("event.register", "Not logged in");
        return false;
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << classes;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("event.register", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    return !response.isEmpty();
}

// Legacy event unregistration (pre-1.9 API, rarely used now)
bool XenRpcAPI::eventUnregister(const QStringList& classes)
{
    if (!this->d->session || !this->d->session->IsLoggedIn())
    {
        emit this->apiCallFailed("event.unregister", "Not logged in");
        return false;
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << classes;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("event.unregister", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    return !response.isEmpty();
}

