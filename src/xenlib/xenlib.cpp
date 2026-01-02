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

#include "xenlib.h"
#include "xen/network/connection.h"
#include "xen/session.h"
#include "xen/api.h"
#include "xen/xenapi/xenapi_VBD.h"
#include "xen/xenapi/xenapi_VDI.h"
#include "xen/xenapi/xenapi_VM.h"
#include "xen/xenapi/xenapi_Host.h"
#include "xen/xenapi/xenapi_Pool.h"
#include "xen/xenapi/xenapi_SR.h"
#include "xen/xenapi/xenapi_Network.h"
#include "xen/asyncoperations.h"
#include "xen/network/certificatemanager.h"
#include "xen/eventpoller.h"
#include "xencache.h"
#include "xen/failure.h"
#include "metricupdater.h"
#include "xen/network/connectionsmanager.h"
#include "utils/misc.h"
#include <QtCore/QHash>
#include <QtCore/QJsonDocument>
#include <QtCore/QDateTime>
#include <QtCore/QThread>
#include <QtCore/QMetaObject>
#include <QtCore/QRegularExpression>
#include <initializer_list>
#include <exception>

// Helper for timestamped debug output
static QString timestamp()
{
    return QDateTime::currentDateTime().toString("[HH:mm:ss.zzz]");
}

// Request types for async API tracking
enum RequestType
{
    GetVirtualMachines,
    GetHosts,
    GetPools,
    GetStorageRepositories,
    GetNetworks,
    GetPIFs,
    GetAllObjects, // Event.from with empty token to get all object types
    GetObjectData
};

class XenLib::Private
{
    public:
        bool initialized = false;
        bool connected = false;
        QString lastError;
        QString connectionInfo;
        // Pending credentials for async login after connection
        QString pendingHostname;
        int pendingPort = 443;
        QString pendingUsername;
        QString pendingPassword;
        bool isRedirecting = false; // True when handling HOST_IS_SLAVE redirect

        XenConnection* connection = nullptr;
        XenAPI::Session* session = nullptr;
        XenRpcAPI* api = nullptr;
        XenAsyncOperations* asyncOps = nullptr;
        XenCertificateManager* certManager = nullptr;
        MetricUpdater* metricUpdater = nullptr;
        // Cache now owned by XenConnection (matching C# architecture)
        EventPoller* eventPoller = nullptr;
        QThread* eventPollerThread = nullptr; // Dedicated thread for EventPoller to avoid blocking UI

        // Async API request tracking
        QHash<int, RequestType> pendingRequests;
        // For GetObjectData requests, we need to track the object type and ref
        QHash<int, QPair<QString, QString>> objectDataRequests; // requestId -> (objectType, objectRef)
        // For cache population requests
        QHash<int, QString> cachePopulationRequests; // requestId -> objectType
};

XenLib::XenLib(QObject* parent) : QObject(parent), d(new Private)
{
    // Initialize components
    this->d->certManager = XenCertificateManager::instance();
    this->d->connection = new XenConnection(this);
    // Set xenLib property so XenObject subclasses can access cache
    this->d->connection->setProperty("xenLib", QVariant::fromValue(this));
    this->d->session = new XenAPI::Session(this->d->connection, this->d->connection);
    this->d->api = new XenRpcAPI(this->d->session, this);
    this->d->asyncOps = new XenAsyncOperations(this->d->session, this);
    this->d->metricUpdater = new MetricUpdater(this->d->connection, this);
    if (this->d->connection)
        this->d->connection->SetMetricUpdater(this->d->metricUpdater);
    // Cache now owned by XenConnection (matching C# architecture)

    // Create EventPoller on a dedicated thread to avoid blocking the UI with long-poll requests
    // EventPoller will create its own Connection/Session/API stack when initialized
    this->d->eventPollerThread = new QThread(this);
    this->d->eventPollerThread->start();

    // Create EventPoller with no parent and no API (it will create its own)
    this->d->eventPoller = new EventPoller(nullptr);
    this->d->eventPoller->moveToThread(this->d->eventPollerThread);

    qDebug() << "XenLib: EventPoller created on dedicated thread";

    // Associate session with connection for heartbeat and other operations
    this->d->connection->SetSession(this->d->session);

    this->setupConnections();
}

XenLib::~XenLib()
{
    return;
    // Explicitly destroy dependent objects in a safe order to avoid QObject
    // deleting already-freed children (e.g., session using connection).
    if (this->d->api)
    {
        this->d->api->setParent(nullptr);
        delete this->d->api;
        this->d->api = nullptr;
    }
    if (this->d->asyncOps)
    {
        this->d->asyncOps->setParent(nullptr);
        delete this->d->asyncOps;
        this->d->asyncOps = nullptr;
    }
    if (this->d->session)
    {
        this->d->session->detachConnection();
        this->d->session->setParent(nullptr);
        delete this->d->session;
        this->d->session = nullptr;
    }

    delete this->d;
}

void XenLib::setupConnections()
{
    // Connection signals
    // NOTE: DO NOT connect XenConnection::connected to handleConnectionStateChanged!
    // That signal means "TCP/SSL ready", not "logged in".
    // connectionStateChanged is emitted from handleLoginResult() after successful login.

    //connect(this->d->connection, &XenConnection::disconnected,
    //        [this]() { this->handleConnectionStateChanged(false); });
    //connect(this->d->connection, &XenConnection::error, this, &XenLib::handleConnectionError);

    // Async API response signal
    connect(this->d->connection, &XenConnection::apiResponse, this, &XenLib::onConnectionApiResponse);

    // Session signals
    /*connect(this->d->session, &XenAPI::Session::loginSuccessful,
            [this]() { this->handleLoginResult(true); });
    connect(this->d->session, &XenAPI::Session::loginFailed,
            [this](const QString& reason) {
                this->setError(reason);
                this->handleLoginResult(false);
            });
    */

    // API signals
    connect(this->d->api, &XenRpcAPI::apiCallCompleted, this, &XenLib::handleApiCallResult);
    connect(this->d->api, &XenRpcAPI::apiCallFailed, this, &XenLib::handleApiCallError);

    // EventPoller signals

    connect(this->d->eventPoller, &EventPoller::connectionLost, this, &XenLib::onEventPollerConnectionLost);

    // Forward EventPoller task signals for TaskRehydrationManager
    connect(this->d->eventPoller, &EventPoller::taskAdded,
            this, [this](const QString& taskRef, const QVariantMap& taskData) {
                emit this->taskAdded(this->d->connection, taskRef, taskData);
            });
    connect(this->d->eventPoller, &EventPoller::taskModified,
            this, [this](const QString& taskRef, const QVariantMap& taskData) {
                emit this->taskModified(this->d->connection, taskRef, taskData);
            });
    connect(this->d->eventPoller, &EventPoller::taskDeleted,
            this, [this](const QString& taskRef) {
                emit this->taskDeleted(this->d->connection, taskRef);
            });

    // Populate pool members when hosts are received (for failover)
    connect(this, &XenLib::hostsReceived, this, &XenLib::onHostsReceivedForPoolMembers);

    // Update HA and coordinator change flags when pool data is received
    connect(this, &XenLib::poolsReceived, this, &XenLib::onPoolsReceivedForHATracking);
}

bool XenLib::initialize()
{
    if (this->d->initialized)
    {
        return true;
    }

    this->clearError();

    // Initialize certificate manager policy
    this->d->certManager->setValidationPolicy(true, false); // Allow self-signed, not expired

    this->d->initialized = true;
    return true;
}

bool XenLib::isConnected() const
{
    return this->d->connected &&
           this->d->connection && this->d->connection->IsConnected() &&
           this->d->session && this->d->session->IsLoggedIn();
}

void XenLib::setConnection(XenConnection* connection)
{
    if (connection == this->d->connection)
        return;

    if (this->d->connection)
        this->d->connection->disconnect(this);

    if (this->d->session)
    {
        this->d->session->detachConnection();
        this->d->session->deleteLater();
        this->d->session = nullptr;
    }

    if (this->d->api)
    {
        this->d->api->deleteLater();
        this->d->api = nullptr;
    }

    if (this->d->asyncOps)
    {
        this->d->asyncOps->deleteLater();
        this->d->asyncOps = nullptr;
    }

    this->d->connection = connection;
    if (!this->d->connection)
        return;

    this->d->connection->setParent(this);
    this->d->connection->setProperty("xenLib", QVariant::fromValue(this));

    if (this->d->metricUpdater)
        this->d->metricUpdater->deleteLater();
    this->d->metricUpdater = new MetricUpdater(this->d->connection, this);
    if (this->d->connection)
        this->d->connection->SetMetricUpdater(this->d->metricUpdater);

    connect(this->d->connection, &XenConnection::disconnected,
            [this]() { this->handleConnectionStateChanged(false); });
    connect(this->d->connection, &XenConnection::error, this, &XenLib::handleConnectionError);
    connect(this->d->connection, &XenConnection::connectionStateChanged, this, [this]() {
        this->handleConnectionStateChanged(this->d->connection->IsConnectedNewFlow());
    });
    connect(this->d->connection, &XenConnection::connectionClosed, this, [this]() {
        this->handleConnectionStateChanged(false);
    });
    connect(this->d->connection, &XenConnection::connectionLost, this, [this]() {
        this->handleConnectionStateChanged(false);
    });
    connect(this->d->connection, &XenConnection::connectionResult, this, [this](bool connected, const QString& error) {
        if (connected)
        {
            XenAPI::Session* session = this->d->connection->GetConnectSession();
            if (session && session != this->d->session)
            {
                this->d->session = session;
                this->d->connection->SetSession(session);

                if (this->d->api)
                    this->d->api->deleteLater();
                this->d->api = new XenRpcAPI(this->d->session, this);
                connect(this->d->api, &XenRpcAPI::apiCallCompleted, this, &XenLib::handleApiCallResult);
                connect(this->d->api, &XenRpcAPI::apiCallFailed, this, &XenLib::handleApiCallError);

                if (this->d->asyncOps)
                    this->d->asyncOps->deleteLater();
                this->d->asyncOps = new XenAsyncOperations(this->d->session, this);
            }

            this->handleConnectionStateChanged(true);
            return;
        }

        const QStringList failureDescription = this->d->connection->GetLastFailureDescription();
        if (!failureDescription.isEmpty() && failureDescription.first() == Failure::HOST_IS_SLAVE)
            return;

        const QString hostname = this->d->connection->GetHostname();
        const int port = this->d->connection->GetPort();
        const QString username = this->d->connection->GetUsername();
        if (!error.isEmpty())
            this->setError(error);
        emit this->authenticationFailed(hostname, port, username, error);
    });
    connect(this->d->connection, &XenConnection::apiResponse, this, &XenLib::onConnectionApiResponse);
    connect(this->d->connection, &XenConnection::taskAdded,
            this, [this](const QString& taskRef, const QVariantMap& taskData) {
                emit this->taskAdded(this->d->connection, taskRef, taskData);
            });
    connect(this->d->connection, &XenConnection::taskModified,
            this, [this](const QString& taskRef, const QVariantMap& taskData) {
                emit this->taskModified(this->d->connection, taskRef, taskData);
            });
    connect(this->d->connection, &XenConnection::taskDeleted,
            this, [this](const QString& taskRef) {
                emit this->taskDeleted(this->d->connection, taskRef);
            });
}

XenCache* XenLib::GetCache() const
{
    // Cache is now owned by connection (matching C# architecture)
    return this->d->connection ? this->d->connection->GetCache() : nullptr;
}

QVariantMap XenLib::getCachedObjectData(const QString& objectType, const QString& objectRef)
{
    // Reference: XenModel/Network/Cache.cs lines 438-448 (Resolve method)
    // CRITICAL: This method must be synchronous and cache-only (no API calls)
    // to match C# Connection.Resolve() behavior. All data must be pre-fetched
    // asynchronously via requestObjectData() before calling this method.

    if (!this->isConnected())
    {
        this->setError("Not connected to server");
        return QVariantMap();
    }

    // Cache-only lookup - matches C# Cache.Resolve exactly
    XenCache* cache = GetCache();
    if (!cache)
    {
        qWarning() << "XenLib::getObjectData - Cache not initialized";
        return QVariantMap();
    }

    if (!cache->Contains(objectType, objectRef))
    {
        // Cache miss - this is NOT an error, caller should use requestObjectData() first
        qDebug() << "XenLib::getObjectData - Cache miss for" << objectType << objectRef
                 << "(use requestObjectData() to fetch asynchronously)";
        return QVariantMap();
    }

    return cache->ResolveObjectData(objectType, objectRef);
}

QString XenLib::getConnectionInfo() const
{
    return this->d->connectionInfo;
}

QString XenLib::getLastError() const
{
    return this->d->lastError;
}

bool XenLib::hasError() const
{
    return !this->d->lastError.isEmpty();
}

void XenLib::handleConnectionStateChanged(bool connected)
{
    this->d->connected = connected;
    emit this->connectionStateChanged(connected);
}

void XenLib::handleConnectionError(const QString& error)
{
    this->setError(error);
    emit this->connectionError(error);
}

void XenLib::onConnectionEstablished()
{
    // qDebug() << "XenLib: TCP/SSL connection established";
    // qDebug() << "XenLib: Starting login with username:" << this->d->pendingUsername;

    // TCP/SSL connection is ready, now login using XenSession
    // This will queue the login request to the worker thread
    if (!this->d->session->Login(this->d->pendingUsername, this->d->pendingPassword))
    {
        QString error = this->d->session->getLastError();
        qWarning() << "XenLib: Login failed:" << error;
        this->setError("Login failed: " + error);
        this->d->connected = false;

        // Only emit connectionError for non-authentication failures
        // Authentication failures are handled by loginFailed signal from XenSession
        if (!error.contains("Authentication failed", Qt::CaseInsensitive))
        {
            emit this->connectionError("Login failed: " + error);
        }

        emit this->connectionStateChanged(false);
        return;
    }

    // Login initiated successfully (XenSession will emit loginSuccessful or loginFailed)
    // qDebug() << "XenLib: Login request sent, waiting for response...";
}

void XenLib::onConnectionError(const QString& errorMessage)
{
    qWarning() << "XenLib: Connection error:" << errorMessage;
    this->setError("Connection failed: " + errorMessage);

    this->d->connected = false;
    emit this->connectionStateChanged(false);
}

void XenLib::onConnectionProgress(const QString& message)
{
    (void)message;
    // qDebug() << "XenLib: Connection progress:" << message;
    // Could emit a signal here for UI progress updates
}

void XenLib::handleApiCallResult(const QString& method, const QVariant& result)
{
    emit this->apiCallCompleted(method, result);
}

void XenLib::handleApiCallError(const QString& method, const QString& error)
{
    this->setError(error);
    emit this->apiCallFailed(method, error);
}

void XenLib::clearError()
{
    this->d->lastError.clear();
}

void XenLib::setError(const QString& error)
{
    this->d->lastError = error;
}

void XenLib::onHostsReceivedForPoolMembers(const QVariantList& hosts)
{
    // Populate pool members from host addresses for connection failover
    // This matches C# XenConnection line 781: "PoolMembers = members"
    if (!this->d->connection)
    {
        return;
    }

    QStringList members;
    for (const QVariant& hostVariant : hosts)
    {
        QVariantMap hostRecord = hostVariant.toMap();
        QString address = hostRecord.value("address").toString();
        if (!address.isEmpty())
        {
            members.append(address);
        }
    }

    if (!members.isEmpty())
    {
        this->d->connection->SetPoolMembers(members);
        qDebug() << "XenLib: Populated" << members.size() << "pool members for failover:" << members;
    }
}

void XenLib::onPoolsReceivedForHATracking(const QVariantList& pools)
{
    // Update HA enabled and coordinator may change flags
    // This matches C# logic that checks pool.ha_enabled and other HA properties
    if (!this->d->connection || pools.isEmpty())
    {
        return;
    }

    // Get the first (and usually only) pool
    QVariantMap poolRecord = pools.first().toMap();

    // Check if HA is enabled
    bool haEnabled = poolRecord.value("ha_enabled", false).toBool();

    // In XenServer/XCP-ng, coordinator may change if:
    // 1. HA is enabled, OR
    // 2. Pool has multiple hosts (manual failover possible)
    QVariantList coordinator = poolRecord.value("master").toList(); // This is a reference
    bool hasPool = !coordinator.isEmpty();                          // If there's a coordinator, we're in a pool

    // Set coordinator may change flag
    // In C# this is set to true if ha_enabled OR if it's a multi-host pool
    bool coordinatorMayChange = haEnabled || hasPool;

    this->d->connection->setCoordinatorMayChange(coordinatorMayChange);

    qDebug() << "XenLib: HA tracking - ha_enabled:" << haEnabled
             << ", coordinator_may_change:" << coordinatorMayChange;
}

void XenLib::onConnectionApiResponse(int requestId, const QByteArray& response)
{
    // Check if this is one of our pending requests
    if (!this->d->pendingRequests.contains(requestId))
    {
        qDebug() << "XenLib::onConnectionApiResponse - Unknown request ID:" << requestId;
        return;
    }

    RequestType requestType = this->d->pendingRequests.take(requestId);

    // Parse the JSON-RPC response using XenAPI's parser
    QVariant parsedResponse = this->d->api->parseJsonRpcResponse(response);

    if (parsedResponse.isNull())
    {
        qWarning() << "XenLib::onConnectionApiResponse - Failed to parse response for request" << requestId;
        // Emit empty results
        switch (requestType)
        {
        case GetVirtualMachines:
            emit this->virtualMachinesReceived(QVariantList());
            break;
        case GetHosts:
            emit this->hostsReceived(QVariantList());
            break;
        case GetPools:
            emit this->poolsReceived(QVariantList());
            break;
        case GetStorageRepositories:
            emit this->storageRepositoriesReceived(QVariantList());
            break;
        case GetNetworks:
            emit this->networksReceived(QVariantList());
            break;
        case GetPIFs:
            // PIFs don't have a signal, just populate cache
            break;
        case GetAllObjects:
            qWarning() << "XenLib: Event.from failed during cache population";
            break;
        case GetObjectData:
            if (this->d->objectDataRequests.contains(requestId))
            {
                QPair<QString, QString> objInfo = this->d->objectDataRequests.take(requestId);
                emit this->objectDataReceived(objInfo.first, objInfo.second, QVariantMap());
            }
            break;
        }
        return;
    }

    // qDebug() << "XenLib::onConnectionApiResponse - Parsed response type:" << parsedResponse.typeName();

    // IMPORTANT: parseJsonRpcResponse() should have unwrapped Status/Value,
    // but in case it returned the full map, extract just the Value part
    QVariant responseData = parsedResponse;
    if (Misc::QVariantIsMap(parsedResponse))
    {
        QVariantMap responseMap = parsedResponse.toMap();
        // qDebug() << "XenLib::onConnectionApiResponse - Response map has" << responseMap.size() << "keys";
        // qDebug() << "XenLib::onConnectionApiResponse - First 5 keys:" << responseMap.keys().mid(0, 5);
        // qDebug() << "XenLib::onConnectionApiResponse - Contains 'Value'?" << responseMap.contains("Value");
        // qDebug() << "XenLib::onConnectionApiResponse - Contains 'Status'?" << responseMap.contains("Status");

        if (responseMap.contains("Value"))
        {
            // qDebug() << "XenLib::onConnectionApiResponse - Extracting Value from wrapped response";
            responseData = responseMap.value("Value");
            // qDebug() << "XenLib::onConnectionApiResponse - After extraction, responseData type:" << responseData.typeName();
            if (Misc::QVariantIsMap(responseData))
            {
                QVariantMap extractedMap = responseData.toMap();
                // qDebug() << "XenLib::onConnectionApiResponse - Extracted map has" << extractedMap.size() << "keys";
                // qDebug() << "XenLib::onConnectionApiResponse - Extracted map first 5 keys:" << extractedMap.keys().mid(0, 5);
            }
        }
    }

    // Convert response to appropriate format and emit signal
    switch (requestType)
    {
    case GetVirtualMachines: {
        // get_all_records returns a map of ref -> record
        QVariantMap allRecords = responseData.toMap();
        // qDebug() << "XenLib::onConnectionApiResponse - GetVirtualMachines: responseData type:" << responseData.typeName();
        // qDebug() << "XenLib::onConnectionApiResponse - GetVirtualMachines: allRecords size:" << allRecords.size();
        // qDebug() << "XenLib::onConnectionApiResponse - GetVirtualMachines: first few keys:" << allRecords.keys().mid(0, 3);

        // Populate cache with bulk data
        GetCache()->UpdateBulk("VM", allRecords);

        QVariantList vms;
        for (auto it = allRecords.begin(); it != allRecords.end(); ++it)
        {
            QString key = it.key();
            if (!key.startsWith("OpaqueRef:"))
                continue;

            QVariantMap vmRecord = it.value().toMap();
            vmRecord["ref"] = it.key(); // Add ref to the record

            vms.append(vmRecord);
        }
        // qDebug() << "XenLib::onConnectionApiResponse - Emitting" << vms.size() << "VMs";
        emit this->virtualMachinesReceived(vms);
        break;
    }
    case GetHosts: {
        QVariantMap allRecords = responseData.toMap();

        // Populate cache
        GetCache()->UpdateBulk("host", allRecords);

        QVariantList hosts;
        for (auto it = allRecords.begin(); it != allRecords.end(); ++it)
        {
            QString key = it.key();
            if (!key.startsWith("OpaqueRef:"))
                continue;
            QVariantMap hostRecord = it.value().toMap();
            hostRecord["ref"] = it.key();
            hosts.append(hostRecord);
        }
        emit this->hostsReceived(hosts);
        break;
    }
    case GetPools: {
        QVariantMap allRecords = responseData.toMap();

        // Populate cache
        GetCache()->UpdateBulk("pool", allRecords);
        // qDebug() << "XenLib::onConnectionApiResponse - GetPools: responseData type:" << responseData.typeName();
        // qDebug() << "XenLib::onConnectionApiResponse - GetPools: allRecords size:" << allRecords.size();
        // qDebug() << "XenLib::onConnectionApiResponse - GetPools: first few keys:" << allRecords.keys().mid(0, 3);

        QVariantList pools;
        for (auto it = allRecords.begin(); it != allRecords.end(); ++it)
        {
            QString key = it.key();

            // IMPORTANT: The XML-RPC parsing creates a flattened map with BOTH
            // the pool refs (OpaqueRef:...) AND all pool record fields at the same level.
            // Filter to only process keys that look like XenServer object references.
            if (!key.startsWith("OpaqueRef:"))
            {
                continue;
            }

            QVariantMap poolRecord = it.value().toMap();
            poolRecord["ref"] = it.key();
            pools.append(poolRecord);
        }
        // qDebug() << "XenLib::onConnectionApiResponse - Emitting" << pools.size() << "pools";
        emit this->poolsReceived(pools);
        break;
    }
    case GetStorageRepositories: {
        QVariantMap allRecords = responseData.toMap();

        // Populate cache
        GetCache()->UpdateBulk("SR", allRecords);

        QVariantList srs;
        for (auto it = allRecords.begin(); it != allRecords.end(); ++it)
        {
            QString key = it.key();
            if (!key.startsWith("OpaqueRef:"))
                continue;

            QVariantMap srRecord = it.value().toMap();
            srRecord["ref"] = it.key();
            srs.append(srRecord);
        }
        emit this->storageRepositoriesReceived(srs);
        break;
    }
    case GetNetworks: {
        QVariantMap allRecords = responseData.toMap();

        // Populate cache
        GetCache()->UpdateBulk("network", allRecords);

        QVariantList networks;
        for (auto it = allRecords.begin(); it != allRecords.end(); ++it)
        {
            QString key = it.key();
            if (!key.startsWith("OpaqueRef:"))
                continue;

            QVariantMap networkRecord = it.value().toMap();
            networkRecord["ref"] = it.key();
            networks.append(networkRecord);
        }
        emit this->networksReceived(networks);
        break;
    }
    case GetPIFs: {
        QVariantMap allRecords = responseData.toMap();

        // Populate cache with PIFs
        GetCache()->UpdateBulk("PIF", allRecords);

        qDebug() << "XenLib: Cached" << allRecords.size() << "PIFs";
        break;
    }
    case GetAllObjects: {
        // qDebug() << timestamp() << "XenLib: Processing Event.from response for cache population";

        // Event.from response structure: The response from parseJsonRpcResponse is already unwrapped
        // and contains: {events: [...], token: "...", valid_ref_counts: {...}}

        QVariantMap eventBatch;

        if (Misc::QVariantIsMap(responseData))
        {
            eventBatch = responseData.toMap();
        } else
        {
            qWarning() << timestamp() << "XenLib: Event.from response is not a map, type:" << responseData.typeName();
            break;
        }

        if (!eventBatch.contains("events"))
        {
            qWarning() << timestamp() << "XenLib: Event.from response missing 'events' field";
            qDebug() << timestamp() << "XenLib: Available keys:" << eventBatch.keys();
            qDebug() << timestamp() << "XenLib: Full response (first 500 chars):" << QString::fromUtf8(QJsonDocument::fromVariant(eventBatch).toJson()).left(500);
            break;
        }

        QVariantList events = eventBatch["events"].toList();
        // qDebug() << timestamp() << "XenLib: Event.from returned" << events.size() << "events for cache population";

        // Process each event and populate cache
        // Event structure: {class_: "VM", operation: "add", opaqueRef: "OpaqueRef:...", snapshot: {...}}
        QHash<QString, int> objectCounts; // Track counts per type for logging

        auto valueForKeys = [](const QVariantMap& map, std::initializer_list<const char*> keys) -> QString {
            for (const char* key : keys)
            {
                const QString value = map.value(QString::fromLatin1(key)).toString();
                if (!value.isEmpty())
                    return value;
            }
            return QString();
        };

        for (const QVariant& eventVar : events)
        {
            QVariantMap event = eventVar.toMap();
            QString objectClass = valueForKeys(event, {"class_", "class"});
            QString operation = event.value("operation").toString();
            QString objectRef = valueForKeys(event, {"opaqueRef", "ref"});
            QVariant snapshot = event["snapshot"];

            // Skip events we don't care about (session, event, user, secret)
            if (objectClass == "session" || objectClass == "event" ||
                objectClass == "user" || objectClass == "secret")
            {
                continue;
            }

            // For "add" and "mod" operations, add to cache
            if (operation == "add" || operation == "mod")
            {
                if (snapshot.isValid() && Misc::QVariantIsMap(snapshot))
                {
                    QVariantMap objectData = snapshot.toMap();
                    objectData["ref"] = objectRef; // Ensure legacy ref key is available
                    objectData["opaqueRef"] = objectRef;

                    // Update cache with this object
                    // XenCache normalizes type to lowercase, so "VM" becomes "vm"
                    GetCache()->Update(objectClass.toLower(), objectRef, objectData);
                    objectCounts[objectClass]++;
                }
            }
            // "del" operations are ignored during initial population
        }

        // Log what we cached
        /*qDebug() << timestamp() << "XenLib: Cache populated with objects:";
        for (auto it = objectCounts.constBegin(); it != objectCounts.constEnd(); ++it)
        {
            qDebug() << timestamp() << "  -" << it.key() << ":" << it.value();
        }*/

        break;
    }
    case GetObjectData: {
        if (!this->d->objectDataRequests.contains(requestId))
        {
            qWarning() << "XenLib::onConnectionApiResponse - GetObjectData request context not found";
            return;
        }

        QPair<QString, QString> objInfo = this->d->objectDataRequests.take(requestId);

        // get_all_records returns a map of ref -> record
        // Extract the specific object we requested
        QVariantMap allRecords = responseData.toMap();
        QVariantMap objectData;

        if (allRecords.contains(objInfo.second))
        {
            objectData = allRecords.value(objInfo.second).toMap();
            objectData["ref"] = objInfo.second;
        } else
        {
            qWarning() << "XenLib::onConnectionApiResponse - Object ref not found in response:" << objInfo.second;
            qDebug() << "Available refs:" << allRecords.keys().mid(0, 5);
        }

        emit this->objectDataReceived(objInfo.first, objInfo.second, objectData);
        break;
    }
    }

    // Check if this is a cache population request
    if (this->d->cachePopulationRequests.contains(requestId))
    {
        QString objectType = this->d->cachePopulationRequests.take(requestId);
        QVariantMap allRecords = responseData.toMap();

        // qDebug() << "XenLib: Received" << allRecords.size() << objectType << "records for cache";

        // Populate cache with bulk data
        GetCache()->UpdateBulk(objectType, allRecords);
    }
}

void XenLib::onEventPollerConnectionLost()
{
    qWarning() << "XenLib: EventPoller lost connection - too many consecutive errors";
    // EventPoller has stopped due to repeated failures
    // This likely means the XenServer connection is broken

    this->setError("Event polling failed - connection lost");
    emit this->connectionError("Event polling connection lost");
}
