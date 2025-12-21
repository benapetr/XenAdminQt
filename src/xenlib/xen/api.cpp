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

class XenRpcAPI::Private
{
    public:
        XenSession* session = nullptr;
        int nextRequestId = 1; // Request ID counter for JSON-RPC
};

XenRpcAPI::XenRpcAPI(XenSession* session, QObject* parent) : QObject(parent), d(new Private)
{
    this->d->session = session;
}

XenRpcAPI::~XenRpcAPI()
{
    delete this->d;
}

QString XenRpcAPI::getSessionId() const
{
    if (this->d->session && this->d->session->isLoggedIn())
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

    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed(method, "Not logged in");
        return;
    }

    // TODO: Implement async API calls
    emit this->apiCallCompleted(method, QVariant());
}

// ============================================================================
// Data Source Operations (Performance Monitoring)
// ============================================================================

double XenRpcAPI::queryVMDataSource(const QString& vmRef, const QString& dataSource)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("VM.query_data_source", "Not logged in");
        return 0.0;
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << vmRef;
    params << dataSource;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("VM.query_data_source", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        emit this->apiCallFailed("VM.query_data_source", "Empty response");
        return 0.0;
    }

    QVariant result = this->parseJsonRpcResponse(response);
    return result.toDouble();
}

double XenRpcAPI::queryHostDataSource(const QString& hostRef, const QString& dataSource)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("host.query_data_source", "Not logged in");
        return 0.0;
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << hostRef;
    params << dataSource;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("host.query_data_source", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        emit this->apiCallFailed("host.query_data_source", "Empty response");
        return 0.0;
    }

    QVariant result = this->parseJsonRpcResponse(response);
    return result.toDouble();
}

QVariant XenRpcAPI::getTaskRecord(const QString& taskRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
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
    if (!this->d->session || !this->d->session->isLoggedIn())
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
    if (!this->d->session || !this->d->session->isLoggedIn())
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
    if (!this->d->session || !this->d->session->isLoggedIn())
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
    if (!this->d->session || !this->d->session->isLoggedIn())
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
    if (!this->d->session || !this->d->session->isLoggedIn())
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
    if (!this->d->session || !this->d->session->isLoggedIn())
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
    if (!this->d->session || !this->d->session->isLoggedIn())
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
    if (!this->d->session || !this->d->session->isLoggedIn())
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
    if (!this->d->session || !this->d->session->isLoggedIn())
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
    if (!this->d->session || !this->d->session->isLoggedIn())
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
    if (!this->d->session || !this->d->session->isLoggedIn())
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
    if (!this->d->session || !this->d->session->isLoggedIn())
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
    if (!this->d->session || !this->d->session->isLoggedIn())
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


QString XenRpcAPI::createBond(const QString& networkRef, const QStringList& pifRefs, const QString& mac, const QString& mode)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("createBond", "Not authenticated");
        return QString();
    }

    if (pifRefs.size() < 2)
    {
        emit this->apiCallFailed("createBond", "At least 2 PIFs required for bond");
        return QString();
    }

    // Convert PIF refs to QVariantList
    QVariantList pifList;
    for (const QString& pifRef : pifRefs)
    {
        pifList.append(pifRef);
    }

    // Build properties map
    QVariantMap properties;
    if (!mode.isEmpty())
    {
        properties["mode"] = mode;
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(networkRef);
    params.append(pifList);
    params.append(mac.isEmpty() ? QString("") : mac);
    params.append(properties);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("Bond.create", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("createBond", "Failed to communicate with server: " + this->d->session->getLastError());
        return QString();
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("createBond", "Server returned an error");
        return QString();
    }

    QString bondRef = response.toString();
    emit this->apiCallCompleted("createBond", bondRef);
    return bondRef;
}

bool XenRpcAPI::destroyBond(const QString& bondRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("destroyBond", "Not authenticated");
        return false;
    }

    if (bondRef.isEmpty())
    {
        emit this->apiCallFailed("destroyBond", "Invalid bond reference");
        return false;
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(bondRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("Bond.destroy", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("destroyBond", "Failed to communicate with server: " + this->d->session->getLastError());
        return false;
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("destroyBond", "Server returned an error");
        return false;
    }

    emit this->apiCallCompleted("destroyBond", true);
    return true;
}

bool XenRpcAPI::reconfigurePIF(const QString& pifRef, const QString& mode, const QString& ip,
                               const QString& netmask, const QString& gateway, const QString& dns)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("reconfigurePIF", "Not authenticated");
        return false;
    }

    if (pifRef.isEmpty())
    {
        emit this->apiCallFailed("reconfigurePIF", "Invalid PIF reference");
        return false;
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(pifRef);
    params.append(mode); // "Static" or "DHCP"
    params.append(ip);
    params.append(netmask);
    params.append(gateway);
    params.append(dns);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("PIF.reconfigure_ip", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("reconfigurePIF", "Failed to communicate with server: " + this->d->session->getLastError());
        return false;
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("reconfigurePIF", "Server returned an error");
        return false;
    }

    emit this->apiCallCompleted("reconfigurePIF", true);
    return true;
}

bool XenRpcAPI::reconfigurePIFDHCP(const QString& pifRef)
{
    return reconfigurePIF(pifRef, "DHCP", "", "", "", "");
}
