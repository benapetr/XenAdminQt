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
    this->d->certManager = new XenCertificateManager(this);
    this->d->connection = new XenConnection(this);
    this->d->connection->setCertificateManager(this->d->certManager);
    // Set xenLib property so XenObject subclasses can access cache
    this->d->connection->setProperty("xenLib", QVariant::fromValue(this));
    this->d->session = new XenAPI::Session(this->d->connection, this->d->connection);
    this->d->api = new XenRpcAPI(this->d->session, this);
    this->d->asyncOps = new XenAsyncOperations(this->d->session, this);
    this->d->metricUpdater = new MetricUpdater(this->d->connection, this);
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
    if (this->d->connection)
    {
        this->d->connection->setParent(nullptr);
        delete this->d->connection;
        this->d->connection = nullptr;
    }
    if (this->d->certManager)
    {
        this->d->certManager->setParent(nullptr);
        delete this->d->certManager;
        this->d->certManager = nullptr;
    }

    delete this->d;
}

void XenLib::setupConnections()
{
    // Connection signals
    // NOTE: DO NOT connect XenConnection::connected to handleConnectionStateChanged!
    // That signal means "TCP/SSL ready", not "logged in".
    // connectionStateChanged is emitted from handleLoginResult() after successful login.

    connect(this->d->connection, &XenConnection::disconnected,
            [this]() { this->handleConnectionStateChanged(false); });
    connect(this->d->connection, &XenConnection::error, this, &XenLib::handleConnectionError);

    // Async API response signal
    connect(this->d->connection, &XenConnection::apiResponse, this, &XenLib::onConnectionApiResponse);

    // Session signals
    connect(this->d->session, &XenAPI::Session::loginSuccessful,
            [this]() { this->handleLoginResult(true); });
    connect(this->d->session, &XenAPI::Session::loginFailed,
            [this](const QString& reason) {
                this->setError(reason);
                this->handleLoginResult(false);
            });

    // API signals
    connect(this->d->api, &XenRpcAPI::apiCallCompleted, this, &XenLib::handleApiCallResult);
    connect(this->d->api, &XenRpcAPI::apiCallFailed, this, &XenLib::handleApiCallError);

    // EventPoller signals
    connect(this->d->eventPoller, &EventPoller::eventReceived, this, &XenLib::onEventReceived);
    connect(this->d->eventPoller, &EventPoller::cachePopulated, this, &XenLib::onCachePopulated);
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

XenConnection* XenLib::getConnection() const
{
    return this->d->connection;
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
    this->d->connection->setCertificateManager(this->d->certManager);

    if (this->d->metricUpdater)
        this->d->metricUpdater->deleteLater();
    this->d->metricUpdater = new MetricUpdater(this->d->connection, this);

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
    connect(this->d->connection, &XenConnection::cachePopulated, this, &XenLib::onCachePopulated);
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

XenCertificateManager* XenLib::getCertificateManager() const
{
    return this->d->certManager;
}

XenCache* XenLib::getCache() const
{
    // Cache is now owned by connection (matching C# architecture)
    return this->d->connection ? this->d->connection->GetCache() : nullptr;
}

MetricUpdater* XenLib::getMetricUpdater() const
{
    return this->d->metricUpdater;
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
    XenCache* cache = getCache();
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

bool XenLib::updateVM(const QString& vmRef, const QVariantMap& updates)
{
    if (!this->isConnected())
    {
        this->setError("Not connected to server");
        return false;
    }

    if (vmRef.isEmpty() || updates.isEmpty())
    {
        this->setError("Invalid parameters for VM update");
        return false;
    }

    if (!this->d->session || !this->d->session->IsLoggedIn())
    {
        this->setError("Not authenticated");
        return false;
    }

    // Update each field using explicit XenAPI calls (matches C# VM.SaveChanges)
    bool allSuccess = true;

    for (auto it = updates.constBegin(); it != updates.constEnd(); ++it)
    {
        const QString& field = it.key();
        const QVariant& value = it.value();

        try
        {
            if (field == "name_label")
            {
                XenAPI::VM::set_name_label(this->d->session, vmRef, value.toString());
            }
            else if (field == "name_description")
            {
                XenAPI::VM::set_name_description(this->d->session, vmRef, value.toString());
            }
            else if (field == "tags")
            {
                XenAPI::VM::set_tags(this->d->session, vmRef, value.toStringList());
            }
            else if (field == "other_config")
            {
                XenAPI::VM::set_other_config(this->d->session, vmRef, value.toMap());
            }
            else
            {
                qWarning() << "XenLib::updateVM: Unsupported VM field:" << field;
                this->setError(QString("Unsupported VM field: %1").arg(field));
                allSuccess = false;
            }
        }
        catch (const std::exception& ex)
        {
            qWarning() << "XenLib::updateVM: Failed to set field" << field << "to" << value << ":" << ex.what();
            this->setError(QString("Failed to update VM field: %1").arg(field));
            allSuccess = false;
        }
    }

    return allSuccess;
}

QString XenLib::getGuestMetricsRef(const QString& vmRef)
{
    // Get VM data from cache
    QVariantMap vmData = getCachedObjectData("vm", vmRef);
    if (vmData.isEmpty())
        return QString();

    // Return guest_metrics reference
    return vmData.value("guest_metrics").toString();
}

QVariantMap XenLib::getGuestMetrics(const QString& vmRef)
{
    // Get guest_metrics reference
    QString guestMetricsRef = getGuestMetricsRef(vmRef);
    if (guestMetricsRef.isEmpty() || guestMetricsRef == "OpaqueRef:NULL")
        return QVariantMap();

    // Resolve guest_metrics from cache
    // Note: XenCache should populate guest_metrics when it fetches VMs
    // For now, we'll need to fetch it via API if not cached
    // TODO: Enhance cache to auto-resolve references
    return QVariantMap(); // Placeholder - need cache enhancement
}

bool XenLib::isControlDomainZero(const QString& vmRef, QString* outHostRef)
{
    // Reference: XenModel/XenAPI-Extensions/VM.cs lines 1362-1378
    QVariantMap vmData = getCachedObjectData("vm", vmRef);
    if (vmData.isEmpty())
        return false;

    // Check if is_control_domain flag is set
    if (!vmData.value("is_control_domain").toBool())
        return false;

    // Get resident host
    QString hostRef = vmData.value("resident_on").toString();
    if (hostRef.isEmpty() || hostRef == "OpaqueRef:NULL")
        return false;

    if (outHostRef)
        *outHostRef = hostRef;

    // Get host data to check control_domain field
    QVariantMap hostData = getCachedObjectData("host", hostRef);
    if (hostData.isEmpty())
        return false;

    // Check if host.control_domain matches this VM
    QString hostControlDomain = hostData.value("control_domain").toString();
    if (!hostControlDomain.isEmpty() && hostControlDomain != "OpaqueRef:NULL")
        return hostControlDomain == vmRef;

    // Fallback: check if domid == 0 (control domain zero has domid 0)
    qint64 domid = vmData.value("domid").toLongLong();
    return domid == 0;
}

bool XenLib::isSRDriverDomain(const QString& vmRef, QString* outSRRef)
{
    // Reference: XenModel/XenAPI-Extensions/VM.cs lines 1380-1399
    QVariantMap vmData = getCachedObjectData("vm", vmRef);
    if (vmData.isEmpty())
        return false;

    // Must be control domain but NOT dom0
    if (!vmData.value("is_control_domain").toBool())
        return false;

    if (isControlDomainZero(vmRef))
        return false;

    // Check all cached PBDs to see if any reference this VM as storage_driver_domain
    XenCache* cache = getCache();
    if (!cache)
        return false;

    QList<QVariantMap> allPBDs = cache->GetAllData("pbd");
    for (const QVariantMap& pbd : allPBDs)
    {
        QVariantMap otherConfig = pbd.value("other_config").toMap();

        QString driverDomainRef = otherConfig.value("storage_driver_domain").toString();
        if (driverDomainRef == vmRef)
        {
            QString srRef = pbd.value("SR").toString();
            if (!srRef.isEmpty() && srRef != "OpaqueRef:NULL")
            {
                if (outSRRef)
                    *outSRRef = srRef;
                return true;
            }
        }
    }

    return false;
}

bool XenLib::srHasDriverDomain(const QString& srRef, QString* outVMRef)
{
    // C# Equivalent: XenModel/XenAPI-Extensions/SR.cs lines 289-305 (HasDriverDomain method)
    // Checks if this SR has a driver domain VM by looking through all its PBDs
    // for the "storage_driver_domain" key in other_config
    
    if (srRef.isEmpty() || srRef == "OpaqueRef:NULL")
        return false;
    
    QVariantMap srData = this->getCachedObjectData("sr", srRef);
    if (srData.isEmpty())
        return false;
    
    // Get all PBDs for this SR
    QVariantList pbdRefs = srData.value("PBDs").toList();
    
    for (const QVariant& pbdRefVar : pbdRefs)
    {
        QString pbdRef = pbdRefVar.toString();
        if (pbdRef.isEmpty() || pbdRef == "OpaqueRef:NULL")
            continue;
        
        QVariantMap pbdData = this->getCachedObjectData("pbd", pbdRef);
        if (pbdData.isEmpty())
            continue;
        
        // Check if this PBD has a storage_driver_domain in other_config
        QVariantMap otherConfig = pbdData.value("other_config").toMap();
        QString vmRef = otherConfig.value("storage_driver_domain").toString();
        
        if (!vmRef.isEmpty() && vmRef != "OpaqueRef:NULL")
        {
            // Verify the VM exists and is not dom0
            QVariantMap vmData = this->getCachedObjectData("vm", vmRef);
            if (!vmData.isEmpty() && !this->isControlDomainZero(vmRef))
            {
                if (outVMRef)
                    *outVMRef = vmRef;
                return true;
            }
        }
    }
    
    return false;
}

QString XenLib::getControlDomainForHost(const QString& hostRef)
{
    // Reference: XenModel/XenAPI/Host.cs field "control_domain" (VM reference)
    // Returns the control domain (dom0) VM reference for the given host

    if (hostRef.isEmpty())
        return QString();

    XenCache* cache = getCache();
    if (!cache)
        return QString();

    // Try 1: Check if host record has control_domain field directly
    QVariantMap hostData = cache->ResolveObjectData("host", hostRef);
    if (!hostData.isEmpty())
    {
        QString controlDomainRef = hostData.value("control_domain").toString();
        if (!controlDomainRef.isEmpty() && controlDomainRef != "OpaqueRef:NULL")
        {
            return controlDomainRef;
        }
    }

    // Try 2: Search for VM with is_control_domain=true and resident_on=hostRef
    // This is the fallback for older API versions that don't expose control_domain
    QStringList allVMRefs = cache->GetAllRefs("vm");
    for (const QString& vmRef : allVMRefs)
    {
        QVariantMap vmData = cache->ResolveObjectData("vm", vmRef);
        if (vmData.isEmpty())
            continue;

        bool isControlDomain = vmData.value("is_control_domain").toBool();
        QString residentOn = vmData.value("resident_on").toString();

        if (isControlDomain && residentOn == hostRef)
        {
            return vmRef;
        }
    }

    return QString(); // No control domain found
}

// Snapshot operations
bool XenLib::changeVMISO(const QString& vmRef, const QString& vbdRef, const QString& vdiRef)
{
    // Reference: XenAdmin/Actions/VM/ChangeVMISOAction.cs
    if (!this->isConnected())
    {
        this->setError("Not connected to server");
        return false;
    }

    if (vmRef.isEmpty() || vbdRef.isEmpty())
    {
        this->setError("Invalid VM or VBD reference");
        return false;
    }

    // C# ChangeVMISOAction.cs lines 79-93: MUST eject first if not empty, then insert
    // VBD.insert only works on empty VBDs, so we need to eject any existing disc first

    // Get current VBD state
    QVariantMap vbdData = getCachedObjectData("vbd", vbdRef);
    bool isEmpty = vbdData.value("empty", true).toBool();

    // Step 1: Eject current disc if not empty
    if (!isEmpty)
    {
        try
        {
            XenAPI::VBD::eject(this->d->session, vbdRef);
        }
        catch (const std::exception&)
        {
            this->setError("Failed to eject ISO");
            return false;
        }
    }

    // Step 2: Insert new disc if provided
    if (!vdiRef.isEmpty())
    {
        try
        {
            XenAPI::VBD::insert(this->d->session, vbdRef, vdiRef);
        }
        catch (const std::exception&)
        {
            this->setError("Failed to insert ISO");
            return false;
        }
        return true;
    }

    return true; // Successfully ejected (and nothing to insert)
}

bool XenLib::createCdDrive(const QString& vmRef)
{
    // Reference: XenAdmin/Actions/VM/CreateCdDriveAction.cs
    if (!this->isConnected())
    {
        this->setError("Not connected to server");
        return false;
    }

    if (vmRef.isEmpty())
    {
        this->setError("Invalid VM reference");
        return false;
    }

    // Get VM data to find next available device number
    QVariantMap vmData = getCachedObjectData("vm", vmRef);
    QVariantList vbdRefs = vmData.value("VBDs").toList();

    // Find highest CD device number
    int highestDevice = -1;
    for (const QVariant& vbdRefVar : vbdRefs)
    {
        QString vbdRef = vbdRefVar.toString();
        QVariantMap vbdData = getCachedObjectData("vbd", vbdRef);

        if (vbdData.value("type").toString() == "CD")
        {
            QString userdevice = vbdData.value("userdevice").toString();
            bool ok;
            int deviceNum = userdevice.toInt(&ok);
            if (ok && deviceNum > highestDevice)
            {
                highestDevice = deviceNum;
            }
        }
    }

    // Create new CD drive with next device number
    QString nextDevice = QString::number(highestDevice + 1);

    QVariantMap vbdRecord;
    vbdRecord["VM"] = vmRef;
    vbdRecord["VDI"] = "OpaqueRef:NULL";
    vbdRecord["bootable"] = false;
    vbdRecord["device"] = "";
    vbdRecord["userdevice"] = nextDevice;
    vbdRecord["empty"] = true;
    vbdRecord["type"] = "CD";
    vbdRecord["mode"] = "RO";
    vbdRecord["unpluggable"] = true;
    vbdRecord["other_config"] = QVariantMap();
    vbdRecord["qos_algorithm_type"] = "";
    vbdRecord["qos_algorithm_params"] = QVariantMap();

    try
    {
        QString newVbdRef = XenAPI::VBD::create(this->d->session, vbdRecord);
        return !newVbdRef.isEmpty();
    }
    catch (const std::exception&)
    {
        this->setError("Failed to create CD drive");
        return false;
    }
}

// VM migration operations
bool XenLib::canMigrateVM(const QString& vmRef, const QString& hostRef)
{
    if (!this->isConnected())
    {
        this->setError("Not connected to server");
        return false;
    }

    if (vmRef.isEmpty())
    {
        this->setError("Invalid VM reference");
        return false;
    }

    if (hostRef.isEmpty())
    {
        this->setError("Invalid host reference");
        return false;
    }

    QVariantMap vmData = getCachedObjectData("vm", vmRef);
    if (vmData.isEmpty())
    {
        this->setError("VM not found in cache");
        return false;
    }

    QVariantList allowedOps = vmData.value("allowed_operations").toList();
    bool canMigrate = false;
    for (const QVariant& op : allowedOps)
    {
        if (op.toString() == "pool_migrate")
        {
            canMigrate = true;
            break;
        }
    }

    if (!canMigrate)
    {
        this->setError("VM does not allow migration");
        return false;
    }

    QString residentOn = vmData.value("resident_on").toString();
    if (!residentOn.isEmpty() && residentOn == hostRef)
    {
        this->setError("VM is already on the selected host");
        return false;
    }

    return true;
}

// Host management operations
bool XenLib::setHostName(const QString& hostRef, const QString& name)
{
    if (!this->isConnected())
    {
        this->setError("Not connected to server");
        return false;
    }

    if (hostRef.isEmpty() || name.isEmpty())
    {
        this->setError("Invalid parameters for host name update");
        return false;
    }

    if (!this->d->session || !this->d->session->IsLoggedIn())
    {
        this->setError("Not authenticated");
        return false;
    }

    try
    {
        XenAPI::Host::set_name_label(this->d->session, hostRef, name);
        return true;
    }
    catch (const std::exception& ex)
    {
        qWarning() << "XenLib::setHostName: Failed to set host name:" << ex.what();
        this->setError("Failed to set host name");
        return false;
    }
}

bool XenLib::setHostDescription(const QString& hostRef, const QString& description)
{
    if (!this->isConnected())
    {
        this->setError("Not connected to server");
        return false;
    }

    if (hostRef.isEmpty())
    {
        this->setError("Invalid host reference");
        return false;
    }

    if (!this->d->session || !this->d->session->IsLoggedIn())
    {
        this->setError("Not authenticated");
        return false;
    }

    try
    {
        XenAPI::Host::set_name_description(this->d->session, hostRef, description);
        return true;
    }
    catch (const std::exception& ex)
    {
        qWarning() << "XenLib::setHostDescription: Failed to set host description:" << ex.what();
        this->setError("Failed to set host description");
        return false;
    }
}

bool XenLib::setHostTags(const QString& hostRef, const QStringList& tags)
{
    if (!this->isConnected())
    {
        this->setError("Not connected to server");
        return false;
    }

    if (hostRef.isEmpty())
    {
        this->setError("Invalid host reference");
        return false;
    }

    if (!this->d->session || !this->d->session->IsLoggedIn())
    {
        this->setError("Not authenticated");
        return false;
    }

    try
    {
        XenAPI::Host::set_tags(this->d->session, hostRef, tags);
        return true;
    }
    catch (const std::exception& ex)
    {
        qWarning() << "XenLib::setHostTags: Failed to set host tags:" << ex.what();
        this->setError("Failed to set host tags");
        return false;
    }
}

bool XenLib::setHostOtherConfig(const QString& hostRef, const QString& key, const QString& value)
{
    if (!this->isConnected())
    {
        this->setError("Not connected to server");
        return false;
    }

    if (hostRef.isEmpty() || key.isEmpty())
    {
        this->setError("Invalid parameters for host other_config update");
        return false;
    }

    if (!this->d->session || !this->d->session->IsLoggedIn())
    {
        this->setError("Not authenticated");
        return false;
    }

    QVariantMap hostData = getCachedObjectData("host", hostRef);
    QVariantMap otherConfig = hostData.value("other_config").toMap();

    if (value.isEmpty())
        otherConfig.remove(key);
    else
        otherConfig[key] = value;

    try
    {
        XenAPI::Host::set_other_config(this->d->session, hostRef, otherConfig);
        return true;
    }
    catch (const std::exception& ex)
    {
        qWarning() << "XenLib::setHostOtherConfig: Failed to set host other_config:" << ex.what();
        this->setError("Failed to set host other_config");
        return false;
    }
}

bool XenLib::setHostIqn(const QString& hostRef, const QString& iqn)
{
    if (!this->isConnected())
    {
        this->setError("Not connected to server");
        return false;
    }

    if (hostRef.isEmpty())
    {
        this->setError("Invalid host reference");
        return false;
    }

    if (!this->d->session || !this->d->session->IsLoggedIn())
    {
        this->setError("Not authenticated");
        return false;
    }

    try
    {
        XenAPI::Host::set_iscsi_iqn(this->d->session, hostRef, iqn);
        return true;
    }
    catch (const std::exception& ex)
    {
        qWarning() << "XenLib::setHostIqn: Failed to set host iSCSI IQN:" << ex.what();
        this->setError("Failed to set host iSCSI IQN");
        return false;
    }
}

// Generic object property setters
bool XenLib::setObjectName(const QString& objectType, const QString& objectRef, const QString& name)
{
    if (objectType == "vm")
    {
        return this->updateVM(objectRef, {{"name_label", name}});
    } else if (objectType == "host")
    {
        return this->setHostName(objectRef, name);
    } else if (objectType == "pool")
    {
        return this->setPoolName(objectRef, name);
    } else if (objectType == "sr")
    {
        return this->setSRName(objectRef, name);
    } else if (objectType == "network")
    {
        return this->setNetworkName(objectRef, name);
    } else
    {
        this->setError(QString("setObjectName not implemented for type: %1").arg(objectType));
        return false;
    }
}

bool XenLib::setObjectDescription(const QString& objectType, const QString& objectRef, const QString& description)
{
    if (objectType == "vm")
    {
        return this->updateVM(objectRef, {{"name_description", description}});
    } else if (objectType == "host")
    {
        return this->setHostDescription(objectRef, description);
    } else if (objectType == "pool")
    {
        return this->setPoolDescription(objectRef, description);
    } else if (objectType == "sr")
    {
        return this->setSRDescription(objectRef, description);
    } else if (objectType == "network")
    {
        return this->setNetworkDescription(objectRef, description);
    } else
    {
        this->setError(QString("setObjectDescription not implemented for type: %1").arg(objectType));
        return false;
    }
}

bool XenLib::setObjectTags(const QString& objectType, const QString& objectRef, const QStringList& tags)
{
    if (objectType == "vm")
    {
        return this->updateVM(objectRef, {{"tags", tags}});
    } else if (objectType == "host")
    {
        return this->setHostTags(objectRef, tags);
    } else if (objectType == "pool")
    {
        return this->setPoolTags(objectRef, tags);
    } else if (objectType == "sr")
    {
        return this->setSRTags(objectRef, tags);
    } else if (objectType == "network")
    {
        return this->setNetworkTags(objectRef, tags);
    } else
    {
        this->setError(QString("setObjectTags not implemented for type: %1").arg(objectType));
        return false;
    }
}

bool XenLib::setObjectProperties(const QString& objectType, const QString& objectRef, const QVariantMap& properties)
{
    // Handle common properties
    QVariantMap remaining = properties;

    if (properties.contains("name_label"))
    {
        if (!setObjectName(objectType, objectRef, properties.value("name_label").toString()))
        {
            return false;
        }
        remaining.remove("name_label");
    }

    if (properties.contains("name_description"))
    {
        if (!setObjectDescription(objectType, objectRef, properties.value("name_description").toString()))
        {
            return false;
        }
        remaining.remove("name_description");
    }

    // If there are remaining properties, dispatch to type-specific handler
    if (!remaining.isEmpty())
    {
        if (objectType == "vm")
        {
            return this->updateVM(objectRef, remaining);
        } else
        {
            this->setError(QString("setObjectProperties with custom properties not implemented for type: %1").arg(objectType));
            return false;
        }
    }

    return true;
}

// Pool management operations
bool XenLib::setPoolName(const QString& poolRef, const QString& name)
{
    if (!this->isConnected())
    {
        this->setError("Not connected to server");
        return false;
    }

    if (poolRef.isEmpty() || name.isEmpty())
    {
        this->setError("Invalid parameters for pool name update");
        return false;
    }

    if (!this->d->session || !this->d->session->IsLoggedIn())
    {
        this->setError("Not authenticated");
        return false;
    }

    try
    {
        XenAPI::Pool::set_name_label(this->d->session, poolRef, name);
        return true;
    }
    catch (const std::exception& ex)
    {
        qWarning() << "XenLib::setPoolName: Failed to set pool name:" << ex.what();
        this->setError("Failed to set pool name");
        return false;
    }
}

bool XenLib::setPoolDescription(const QString& poolRef, const QString& description)
{
    if (!this->isConnected())
    {
        this->setError("Not connected to server");
        return false;
    }

    if (poolRef.isEmpty())
    {
        this->setError("Invalid pool reference");
        return false;
    }

    if (!this->d->session || !this->d->session->IsLoggedIn())
    {
        this->setError("Not authenticated");
        return false;
    }

    try
    {
        XenAPI::Pool::set_name_description(this->d->session, poolRef, description);
        return true;
    }
    catch (const std::exception& ex)
    {
        qWarning() << "XenLib::setPoolDescription: Failed to set pool description:" << ex.what();
        this->setError("Failed to set pool description");
        return false;
    }
}

bool XenLib::setPoolTags(const QString& poolRef, const QStringList& tags)
{
    if (!this->isConnected())
    {
        this->setError("Not connected to server");
        return false;
    }

    if (poolRef.isEmpty())
    {
        this->setError("Invalid pool reference");
        return false;
    }

    if (!this->d->session || !this->d->session->IsLoggedIn())
    {
        this->setError("Not authenticated");
        return false;
    }

    try
    {
        XenAPI::Pool::set_tags(this->d->session, poolRef, tags);
        return true;
    }
    catch (const std::exception& ex)
    {
        qWarning() << "XenLib::setPoolTags: Failed to set pool tags:" << ex.what();
        this->setError("Failed to set pool tags");
        return false;
    }
}

bool XenLib::setPoolMigrationCompression(const QString& poolRef, bool enabled)
{
    if (!this->isConnected())
    {
        this->setError("Not connected to server");
        return false;
    }

    if (poolRef.isEmpty())
    {
        this->setError("Invalid pool reference");
        return false;
    }

    if (!this->d->session || !this->d->session->IsLoggedIn())
    {
        this->setError("Not authenticated");
        return false;
    }

    try
    {
        XenAPI::Pool::set_migration_compression(this->d->session, poolRef, enabled);
        return true;
    }
    catch (const std::exception& ex)
    {
        qWarning() << "XenLib::setPoolMigrationCompression: Failed to set pool migration compression:" << ex.what();
        this->setError("Failed to set pool migration compression");
        return false;
    }
}

// SR (Storage Repository) operations
bool XenLib::setSRName(const QString& srRef, const QString& name)
{
    if (!this->isConnected())
    {
        this->setError("Not connected to server");
        return false;
    }

    if (srRef.isEmpty())
    {
        this->setError("Invalid SR reference");
        return false;
    }
    try
    {
        XenAPI::SR::set_name_label(this->d->session, srRef, name);
        return true;
    }
    catch (const std::exception& ex)
    {
        qWarning() << "XenLib::setSRName: Failed to set SR name_label:" << ex.what();
        this->setError("Failed to set SR name");
        return false;
    }
}

bool XenLib::setSRDescription(const QString& srRef, const QString& description)
{
    if (!this->isConnected())
    {
        this->setError("Not connected to server");
        return false;
    }

    if (srRef.isEmpty())
    {
        this->setError("Invalid SR reference");
        return false;
    }
    try
    {
        XenAPI::SR::set_name_description(this->d->session, srRef, description);
        return true;
    }
    catch (const std::exception& ex)
    {
        qWarning() << "XenLib::setSRDescription: Failed to set SR name_description:" << ex.what();
        this->setError("Failed to set SR description");
        return false;
    }
}

bool XenLib::setSRTags(const QString& srRef, const QStringList& tags)
{
    if (!this->isConnected())
    {
        this->setError("Not connected to server");
        return false;
    }

    if (srRef.isEmpty())
    {
        this->setError("Invalid SR reference");
        return false;
    }
    try
    {
        XenAPI::SR::set_tags(this->d->session, srRef, tags);
        return true;
    }
    catch (const std::exception& ex)
    {
        qWarning() << "XenLib::setSRTags: Failed to set SR tags:" << ex.what();
        this->setError("Failed to set SR tags");
        return false;
    }
}

// Network operations
bool XenLib::setNetworkName(const QString& networkRef, const QString& name)
{
    if (!this->isConnected())
    {
        this->setError("Not connected to server");
        return false;
    }

    if (networkRef.isEmpty())
    {
        this->setError("Invalid Network reference");
        return false;
    }
    try
    {
        XenAPI::Network::set_name_label(this->d->session, networkRef, name);
        return true;
    }
    catch (const std::exception& ex)
    {
        qWarning() << "XenLib::setNetworkName: Failed to set network name_label:" << ex.what();
        this->setError("Failed to set network name");
        return false;
    }
}

bool XenLib::setNetworkDescription(const QString& networkRef, const QString& description)
{
    if (!this->isConnected())
    {
        this->setError("Not connected to server");
        return false;
    }

    if (networkRef.isEmpty())
    {
        this->setError("Invalid Network reference");
        return false;
    }
    try
    {
        XenAPI::Network::set_name_description(this->d->session, networkRef, description);
        return true;
    }
    catch (const std::exception& ex)
    {
        qWarning() << "XenLib::setNetworkDescription: Failed to set network name_description:" << ex.what();
        this->setError("Failed to set network description");
        return false;
    }
}

bool XenLib::setNetworkTags(const QString& networkRef, const QStringList& tags)
{
    if (!this->isConnected())
    {
        this->setError("Not connected to server");
        return false;
    }

    if (networkRef.isEmpty())
    {
        this->setError("Invalid Network reference");
        return false;
    }
    try
    {
        XenAPI::Network::set_tags(this->d->session, networkRef, tags);
        return true;
    }
    catch (const std::exception& ex)
    {
        qWarning() << "XenLib::setNetworkTags: Failed to set network tags:" << ex.what();
        this->setError("Failed to set network tags");
        return false;
    }
}

QString XenLib::createNetwork(const QString& name, const QString& description, const QVariantMap& otherConfig)
{
    if (!this->isConnected())
    {
        this->setError("Not connected to server");
        return QString();
    }

    if (name.isEmpty())
    {
        this->setError("Network name cannot be empty");
        return QString();
    }

    try
    {
        QVariantMap networkRecord;
        networkRecord["name_label"] = name;
        networkRecord["name_description"] = description;
        networkRecord["other_config"] = otherConfig;
        networkRecord["MTU"] = 1500;
        networkRecord["tags"] = QVariantList();

        QString networkRef = XenAPI::Network::create(this->d->session, networkRecord);

        if (!networkRef.isEmpty())
        {
            if (XenCache* cache = getCache())
                cache->ClearType("network");
            this->requestNetworks();
        }

        return networkRef;
    }
    catch (const std::exception& ex)
    {
        qWarning() << "XenLib::createNetwork: Failed to create network:" << ex.what();
        this->setError("Failed to create network");
        return QString();
    }

}

bool XenLib::destroyNetwork(const QString& networkRef)
{
    if (!this->isConnected())
    {
        this->setError("Not connected to server");
        return false;
    }

    if (networkRef.isEmpty())
    {
        this->setError("Invalid Network reference");
        return false;
    }

    try
    {
        XenAPI::Network::destroy(this->d->session, networkRef);
        if (XenCache* cache = getCache())
            cache->ClearType("network");
        this->requestNetworks();
        return true;
    }
    catch (const std::exception& ex)
    {
        qWarning() << "XenLib::destroyNetwork: Failed to destroy network:" << ex.what();
        this->setError("Failed to destroy network");
        return false;
    }
}

bool XenLib::setNetworkMTU(const QString& networkRef, int mtu)
{
    if (!this->isConnected())
    {
        this->setError("Not connected to server");
        return false;
    }

    if (networkRef.isEmpty())
    {
        this->setError("Invalid Network reference");
        return false;
    }

    if (mtu < 68 || mtu > 65535)
    {
        this->setError("MTU must be between 68 and 65535");
        return false;
    }

    try
    {
        XenAPI::Network::set_MTU(this->d->session, networkRef, mtu);
        return true;
    }
    catch (const std::exception& ex)
    {
        qWarning() << "XenLib::setNetworkMTU: Failed to set network MTU:" << ex.what();
        this->setError("Failed to set network MTU");
        return false;
    }
}

bool XenLib::setNetworkOtherConfig(const QString& networkRef, const QVariantMap& otherConfig)
{
    if (!this->isConnected())
    {
        this->setError("Not connected to server");
        return false;
    }

    if (networkRef.isEmpty())
    {
        this->setError("Invalid Network reference");
        return false;
    }

    try
    {
        XenAPI::Network::set_other_config(this->d->session, networkRef, otherConfig);
        return true;
    }
    catch (const std::exception& ex)
    {
        qWarning() << "XenLib::setNetworkOtherConfig: Failed to set network other_config:" << ex.what();
        this->setError("Failed to set network other_config");
        return false;
    }
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

void XenLib::handleLoginResult(bool success)
{
    if (success)
    {
        qDebug() << timestamp() << "XenLib: Login successful!";
        if (this->d->session)
        {
            qDebug() << timestamp() << "XenLib: Session ID"
                     << this->d->session->getSessionId().left(20) + "...";
        }
        this->d->connected = true;

        // Build connection info with session ID
        this->d->connectionInfo = QString("%1:%2")
                                      .arg(this->d->connection->GetHostname())
                                      .arg(this->d->connection->GetPort());

        // qDebug() << timestamp() << "XenLib:" << QString("Connected to %1:%2 with session %3")
        //                                             .arg(this->d->connection->getHostname())
        //                                             .arg(this->d->connection->getPort())
        //                                             .arg(this->d->session->getSessionId());

        // Populate cache with all objects for instant lookups
        // CRITICAL: This is now SYNCHRONOUS and returns the event token
        // This matches C# XenObjectDownloader.GetAllObjects() pattern
        qDebug() << timestamp() << "XenLib: Populating cache (synchronous)...";
        QString eventToken = this->populateCache();

        if (eventToken.isEmpty())
        {
            qWarning() << timestamp() << "XenLib: Cache population failed or returned no token";
        } else
        {
            qDebug() << timestamp() << "XenLib: Cache population complete, received token:" << eventToken.left(20) + "...";
        }

        // Initialize EventPoller by duplicating our session (creates separate connection stack)
        // This prevents event.from's 30-second long-poll from blocking main API requests
        // IMPORTANT: Use invokeMethod to call initialize() on EventPoller's thread to avoid
        // "Cannot create children for a parent that is in a different thread" warnings
        qDebug() << timestamp() << "XenLib: Preparing EventPoller for new session (reset+init)...";
        // Ensure the poller drops any stale duplicated session before re-init
        QMetaObject::invokeMethod(this->d->eventPoller, [this]() {
            this->d->eventPoller->reset();
            this->d->eventPoller->initialize(this->d->session); }, Qt::BlockingQueuedConnection);

        // Start EventPoller with the token from cache population
        // This ensures the poller continues from where cache population left off,
        // preventing xapi violation of overlapping event.from calls
        qDebug() << timestamp() << "XenLib: Starting EventPoller with token from cache population...";
        QStringList eventClasses = {
            "vm", "host", "pool", "sr", "vbd", "vdi", "vif",
            "network", "pbd", "pif", "task", "message", "console",
            "vm_guest_metrics", "host_metrics", "vm_metrics"};
        QMetaObject::invokeMethod(this->d->eventPoller, [this, eventClasses, eventToken]() { this->d->eventPoller->start(eventClasses, eventToken); }, Qt::QueuedConnection);

        emit this->connectionStateChanged(true);
    } else
    {
        qWarning() << timestamp() << "XenLib: Login failed";
        this->d->connected = false;

        // Don't emit authenticationFailed or connectionStateChanged if we're in the middle of a HOST_IS_SLAVE redirect
        // The redirect will be handled transparently and reconnection will be automatic
        if (!this->d->isRedirecting)
        {
            // Emit specific authentication failure signal with connection details
            // This allows the UI to show a retry dialog with credentials pre-filled
            emit this->authenticationFailed(this->d->pendingHostname,
                                            this->d->pendingPort,
                                            this->d->pendingUsername,
                                            this->d->lastError);

            emit this->connectionStateChanged(false);
        }
        // Error already set by XenSession signal handler
    }
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

// ============================================================================
// Async API Implementation
// ============================================================================

void XenLib::requestVirtualMachines()
{
    if (!this->isConnected())
    {
        qWarning() << "XenLib::requestVirtualMachines - Not connected";
        emit this->virtualMachinesReceived(QVariantList()); // Empty list on error
        return;
    }

    // CACHE CHECK FIRST - Return cached VMs if available
    XenCache* cache = getCache();
    QList<QVariantMap> cachedMaps = cache ? cache->GetAllData("VM") : QList<QVariantMap>();
    if (!cachedMaps.isEmpty())
    {
        qDebug() << "XenLib::requestVirtualMachines - Cache hit, returning" << cachedMaps.size() << "VMs";
        // Convert QList<QVariantMap> to QVariantList
        QVariantList cachedVMs;
        for (const QVariantMap& vm : cachedMaps)
        {
            cachedVMs.append(vm);
        }
        emit this->virtualMachinesReceived(cachedVMs);
        return;
    }

    // Cache miss - fall back to API call
    qDebug() << "XenLib::requestVirtualMachines - Cache miss, fetching from API";

    // Build the JSON-RPC request for VM.get_all_records
    QVariantList params;
    params.append(this->d->session->getSessionId());

    QByteArray jsonRequest = this->d->api->buildJsonRpcCall("VM.get_all_records", params);

    // Send async and track the request
    int requestId = this->d->connection->SendRequestAsync(jsonRequest);
    if (requestId < 0)
    {
        qWarning() << "XenLib::requestVirtualMachines - Failed to queue request";
        emit this->virtualMachinesReceived(QVariantList());
        return;
    }

    this->d->pendingRequests[requestId] = RequestType::GetVirtualMachines;
}

void XenLib::requestHosts()
{
    if (!this->isConnected())
    {
        qWarning() << "XenLib::requestHosts - Not connected";
        emit this->hostsReceived(QVariantList());
        return;
    }

    // CACHE CHECK FIRST
    QList<QVariantMap> cachedMaps = getCache()->GetAllData("host");
    if (!cachedMaps.isEmpty())
    {
        qDebug() << "XenLib::requestHosts - Cache hit, returning" << cachedMaps.size() << "hosts";
        QVariantList cachedHosts;
        for (const QVariantMap& host : cachedMaps)
        {
            cachedHosts.append(host);
        }
        emit this->hostsReceived(cachedHosts);
        return;
    }

    qDebug() << "XenLib::requestHosts - Cache miss, fetching from API";

    QVariantList params;
    params.append(this->d->session->getSessionId());

    QByteArray jsonRequest = this->d->api->buildJsonRpcCall("host.get_all_records", params);

    int requestId = this->d->connection->SendRequestAsync(jsonRequest);
    if (requestId < 0)
    {
        qWarning() << "XenLib::requestHosts - Failed to queue request";
        emit this->hostsReceived(QVariantList());
        return;
    }

    this->d->pendingRequests[requestId] = RequestType::GetHosts;
}

void XenLib::requestPools()
{
    if (!this->isConnected())
    {
        qWarning() << "XenLib::requestPools - Not connected";
        emit this->poolsReceived(QVariantList());
        return;
    }

    // CACHE CHECK FIRST
    QList<QVariantMap> cachedMaps = getCache()->GetAllData("pool");
    if (!cachedMaps.isEmpty())
    {
        qDebug() << "XenLib::requestPools - Cache hit, returning" << cachedMaps.size() << "pools";
        QVariantList cachedPools;
        for (const QVariantMap& pool : cachedMaps)
        {
            cachedPools.append(pool);
        }
        emit this->poolsReceived(cachedPools);
        return;
    }

    qDebug() << "XenLib::requestPools - Cache miss, fetching from API";

    QVariantList params;
    params.append(this->d->session->getSessionId());

    QByteArray jsonRequest = this->d->api->buildJsonRpcCall("pool.get_all_records", params);

    int requestId = this->d->connection->SendRequestAsync(jsonRequest);
    if (requestId < 0)
    {
        qWarning() << "XenLib::requestPools - Failed to queue request";
        emit this->poolsReceived(QVariantList());
        return;
    }

    this->d->pendingRequests[requestId] = RequestType::GetPools;
}

void XenLib::requestStorageRepositories()
{
    if (!this->isConnected())
    {
        qWarning() << "XenLib::requestStorageRepositories - Not connected";
        emit this->storageRepositoriesReceived(QVariantList());
        return;
    }

    // CACHE CHECK FIRST
    QList<QVariantMap> cachedMaps = getCache()->GetAllData("SR");
    if (!cachedMaps.isEmpty())
    {
        qDebug() << "XenLib::requestStorageRepositories - Cache hit, returning" << cachedMaps.size() << "SRs";
        QVariantList cachedSRs;
        for (const QVariantMap& sr : cachedMaps)
        {
            cachedSRs.append(sr);
        }
        emit this->storageRepositoriesReceived(cachedSRs);
        return;
    }

    qDebug() << "XenLib::requestStorageRepositories - Cache miss, fetching from API";

    QVariantList params;
    params.append(this->d->session->getSessionId());

    QByteArray jsonRequest = this->d->api->buildJsonRpcCall("SR.get_all_records", params);

    int requestId = this->d->connection->SendRequestAsync(jsonRequest);
    if (requestId < 0)
    {
        qWarning() << "XenLib::requestStorageRepositories - Failed to queue request";
        emit this->storageRepositoriesReceived(QVariantList());
        return;
    }

    this->d->pendingRequests[requestId] = RequestType::GetStorageRepositories;
}

void XenLib::requestNetworks()
{
    if (!this->isConnected())
    {
        qWarning() << "XenLib::requestNetworks - Not connected";
        emit this->networksReceived(QVariantList());
        return;
    }

    // CACHE CHECK FIRST
    QList<QVariantMap> cachedMaps = getCache()->GetAllData("network");
    if (!cachedMaps.isEmpty())
    {
        qDebug() << "XenLib::requestNetworks - Cache hit, returning" << cachedMaps.size() << "networks";
        QVariantList cachedNetworks;
        for (const QVariantMap& network : cachedMaps)
        {
            cachedNetworks.append(network);
        }
        emit this->networksReceived(cachedNetworks);
        return;
    }

    qDebug() << "XenLib::requestNetworks - Cache miss, fetching from API";

    QVariantList params;
    params.append(this->d->session->getSessionId());

    QByteArray jsonRequest = this->d->api->buildJsonRpcCall("network.get_all_records", params);

    int requestId = this->d->connection->SendRequestAsync(jsonRequest);
    if (requestId < 0)
    {
        qWarning() << "XenLib::requestNetworks - Failed to queue request";
        emit this->networksReceived(QVariantList());
        return;
    }

    this->d->pendingRequests[requestId] = RequestType::GetNetworks;
}

void XenLib::requestPIFs()
{
    if (!this->isConnected())
    {
        qWarning() << "XenLib::requestPIFs - Not connected";
        return;
    }

    // CACHE CHECK FIRST
    QList<QVariantMap> cachedMaps = getCache()->GetAllData("PIF");
    if (!cachedMaps.isEmpty())
    {
        qDebug() << "XenLib::requestPIFs - Cache hit, returning" << cachedMaps.size() << "PIFs";
        return;
    }

    qDebug() << "XenLib::requestPIFs - Cache miss, fetching from API";

    QVariantList params;
    params.append(this->d->session->getSessionId());

    QByteArray jsonRequest = this->d->api->buildJsonRpcCall("PIF.get_all_records", params);

    int requestId = this->d->connection->SendRequestAsync(jsonRequest);
    if (requestId < 0)
    {
        qWarning() << "XenLib::requestPIFs - Failed to queue request";
        return;
    }

    this->d->pendingRequests[requestId] = RequestType::GetPIFs;
}

void XenLib::requestObjectData(const QString& objectType, const QString& objectRef)
{
    if (!this->isConnected())
    {
        qWarning() << "XenLib::requestObjectData - Not connected";
        emit this->objectDataReceived(objectType, objectRef, QVariantMap());
        return;
    }

    // CACHE CHECK FIRST - Transparent cache integration (like C# Connection.Resolve)
    QVariantMap cachedData = getCache()->ResolveObjectData(objectType, objectRef);
    if (!cachedData.isEmpty())
    {
        qDebug() << "XenLib::requestObjectData - Cache hit for" << objectType << objectRef;
        // Emit immediately with cached data (synchronous from caller's perspective)
        emit this->objectDataReceived(objectType, objectRef, cachedData);
        return;
    }

    // Cache miss - fall back to API call
    qDebug() << "XenLib::requestObjectData - Cache miss for" << objectType << objectRef << "- fetching from API";

    // Build appropriate API call based on object type
    QString methodName;
    if (objectType == "vm")
        methodName = "VM.get_all_records";
    else if (objectType == "host")
        methodName = "host.get_all_records";
    else if (objectType == "pool")
        methodName = "pool.get_all_records";
    else if (objectType == "storage")
        methodName = "SR.get_all_records";
    else if (objectType == "network")
        methodName = "network.get_all_records";
    else
    {
        qWarning() << "XenLib::requestObjectData - Unknown object type:" << objectType;
        emit this->objectDataReceived(objectType, objectRef, QVariantMap());
        return;
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());

    QByteArray jsonRequest = this->d->api->buildJsonRpcCall(methodName, params);

    int requestId = this->d->connection->SendRequestAsync(jsonRequest);
    if (requestId < 0)
    {
        qWarning() << "XenLib::requestObjectData - Failed to queue request";
        emit this->objectDataReceived(objectType, objectRef, QVariantMap());
        return;
    }

    this->d->pendingRequests[requestId] = RequestType::GetObjectData;
    this->d->objectDataRequests[requestId] = qMakePair(objectType, objectRef);
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
        getCache()->UpdateBulk("VM", allRecords);

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
        getCache()->UpdateBulk("host", allRecords);

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
        getCache()->UpdateBulk("pool", allRecords);
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
        getCache()->UpdateBulk("SR", allRecords);

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
        getCache()->UpdateBulk("network", allRecords);

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
        getCache()->UpdateBulk("PIF", allRecords);

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
                    getCache()->Update(objectClass.toLower(), objectRef, objectData);
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
        getCache()->UpdateBulk(objectType, allRecords);
    }
}

QString XenLib::populateCache()
{
    if (!this->isConnected())
    {
        qWarning() << "XenLib::populateCache - Not connected";
        return QString();
    }

    // Clear existing cache
    if (XenCache* cache = getCache())
        cache->Clear();

    // Preload roles (not delivered by event.from) to mirror C# XenObjectDownloader.GetAllObjects
    try
    {
        QVariantList roleParams;
        roleParams.append(this->d->session->getSessionId());
        QByteArray roleRequest = this->d->api->buildJsonRpcCall("role.get_all_records", roleParams);
        QByteArray roleResponse = this->d->connection->SendRequest(roleRequest);

        if (roleResponse.isEmpty())
        {
            qWarning() << "XenLib::populateCache - role.get_all_records returned empty response";
        } else
        {
            QVariant parsed = this->d->api->parseJsonRpcResponse(roleResponse);
            QVariant roleData = parsed;
            if (Misc::QVariantIsMap(parsed))
            {
                QVariantMap map = parsed.toMap();
                if (map.contains("Value"))
                    roleData = map.value("Value");
            }

            if (Misc::QVariantIsMap(roleData))
            {
                QVariantMap roles = roleData.toMap();
                for (auto it = roles.constBegin(); it != roles.constEnd(); ++it)
                {
                    QString roleRef = it.key();
                    QVariantMap roleRecord = it.value().toMap();
                    roleRecord["ref"] = roleRef;
                    roleRecord["opaqueRef"] = roleRef;
                    if (XenCache* cache = getCache())
                        cache->Update("role", roleRef, roleRecord);
                }
                qDebug() << timestamp() << "XenLib::populateCache - Cached" << roles.size() << "role records";
            } else
            {
                qWarning() << "XenLib::populateCache - role.get_all_records returned unexpected type"
                           << roleData.typeName();
            }
        }
    } catch (const std::exception& exn)
    {
        qWarning() << "XenLib::populateCache - Failed to fetch role records:" << exn.what();
    }

    // qDebug() << "XenLib: Starting synchronous cache population using Event.from...";

    // Use Event.from with empty token to get ALL object types at once
    // This matches C# XenObjectDownloader.GetAllObjects pattern
    // CRITICAL: This MUST be synchronous (sendRequest, not sendRequestAsync)
    // to prevent overlapping event.from calls on the same session ID

    QVariantList params;
    params.append(this->d->session->getSessionId());

    // Classes parameter - use QStringList just like XenAPI::eventFrom() does
    QStringList classes;
    classes << "*";    // "*" means all classes
    params << classes; // Append QStringList directly (NOT converted to QVariantList)

    params.append("");   // Empty token = get all records (like get_all_records)
    params.append(30.0); // 30 second timeout

    QByteArray jsonRequest = this->d->api->buildJsonRpcCall("event.from", params);

    // Use SYNCHRONOUS SendRequest() to block until response arrives
    // This prevents xapi violation of multiple outstanding event.from calls
    QByteArray response = this->d->connection->SendRequest(jsonRequest);

    if (response.isEmpty())
    {
        qWarning() << "XenLib::populateCache - Event.from returned empty response";
        return QString();
    }

    // Parse the response
    QVariant parsedResponse = this->d->api->parseJsonRpcResponse(response);

    // Extract responseData (unwrap Status/Value if needed)
    QVariant responseData = parsedResponse;
    if (Misc::QVariantIsMap(parsedResponse))
    {
        QVariantMap responseMap = parsedResponse.toMap();
        if (responseMap.contains("Value"))
        {
            responseData = responseMap.value("Value");
        }
    }

    if (!Misc::QVariantIsMap(responseData))
    {
        qWarning() << "XenLib::populateCache - Event.from response is not a map, type:" << responseData.typeName();
        return QString();
    }

    QVariantMap eventBatch = responseData.toMap();

    if (!eventBatch.contains("events"))
    {
        qWarning() << "XenLib::populateCache - Event.from response missing 'events' field";
        qDebug() << "Available keys:" << eventBatch.keys();
        return QString();
    }

    if (!eventBatch.contains("token"))
    {
        qWarning() << "XenLib::populateCache - Event.from response missing 'token' field";
        return QString();
    }

    // Extract the token BEFORE processing events
    QString token = eventBatch["token"].toString();

    QVariantList events = eventBatch["events"].toList();
    qDebug() << timestamp() << "XenLib: Event.from returned" << events.size() << "events for cache population";

    // Process each event and populate cache
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

        // Skip events we don't care about
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
                objectData["ref"] = objectRef;
                objectData["opaqueRef"] = objectRef;

                // Update cache
                getCache()->Update(objectClass.toLower(), objectRef, objectData);
                objectCounts[objectClass]++;
            }
        }
    }

    // Log what we cached
    /*qDebug() << timestamp() << "XenLib: Cache populated with objects:";
    for (auto it = objectCounts.constBegin(); it != objectCounts.constEnd(); ++it)
    {
        qDebug() << timestamp() << "  -" << it.key() << ":" << it.value();
    }*/

    // qDebug() << timestamp() << "XenLib: Returning token for EventPoller:" << token.left(20) + "...";

    // Fetch console records explicitly: event.from does not always include console snapshots,
    // but the VNC console path depends on console location/protocol being cached.
    try
    {
        QVariantList consoleParams;
        consoleParams.append(this->d->session->getSessionId());
        QByteArray consoleRequest = this->d->api->buildJsonRpcCall("console.get_all_records", consoleParams);
        QByteArray consoleResponse = this->d->connection->SendRequest(consoleRequest);

        if (consoleResponse.isEmpty())
        {
            qWarning() << "XenLib::populateCache - console.get_all_records returned empty response";
        } else
        {
            QVariant parsed = this->d->api->parseJsonRpcResponse(consoleResponse);
            QVariant responseData = parsed;
            if (Misc::QVariantIsMap(parsed))
            {
                QVariantMap map = parsed.toMap();
                if (map.contains("Value"))
                    responseData = map.value("Value");
            }

            if (Misc::QVariantIsMap(responseData))
            {
                QVariantMap consolesMap = responseData.toMap();
                for (auto it = consolesMap.constBegin(); it != consolesMap.constEnd(); ++it)
                {
                    QString consoleRef = it.key();
                    QVariantMap consoleData = it.value().toMap();
                    consoleData["ref"] = consoleRef;
                    consoleData["opaqueRef"] = consoleRef;
                    if (XenCache* cache = getCache())
                        cache->Update("console", consoleRef, consoleData);
                }
                qDebug() << "XenLib::populateCache - Cached" << consolesMap.size() << "console records";
            } else
            {
                qWarning() << "XenLib::populateCache - console.get_all_records returned unexpected type"
                           << responseData.typeName();
            }
        }
    } catch (const std::exception& exn)
    {
        qWarning() << "XenLib::populateCache - Failed to fetch console records:" << exn.what();
    }

    return token;
}

void XenLib::onEventReceived(const QVariantMap& eventData)
{
    // Process XenServer events and update cache
    //
    // Normalize field naming differences between XML-RPC ("class", "ref") and JSON-RPC ("class_", "opaqueRef")
    auto valueForKeys = [](const QVariantMap& map, std::initializer_list<const char*> keys) -> QString {
        for (const char* key : keys)
        {
            const QString value = map.value(QString::fromLatin1(key)).toString();
            if (!value.isEmpty())
                return value;
        }
        return QString();
    };

    QString eventClass = valueForKeys(eventData, {"class_", "class"});
    QString operation = eventData.value("operation").toString(); // Same in both protocols
    QString ref = valueForKeys(eventData, {"opaqueRef", "ref"});

    if (eventClass.isEmpty() || operation.isEmpty() || ref.isEmpty())
    {
        // Silently ignore invalid events - these might be partial/continuation data
        return;
    }

    // Normalize class name to match our cache naming
    QString cacheType = eventClass.toLower();

    // Special handling for XenAPI Messages - create alerts
    // C# Reference: MainWindow.cs line 993 - MessageCollectionChanged()
    if (cacheType == "message")
    {
        if (operation == "add" || operation == "mod")
        {
            // Get message snapshot
            QVariantMap snapshot = eventData.value("snapshot").toMap();
            if (!snapshot.isEmpty())
            {
                snapshot["ref"] = ref;
                snapshot["opaqueRef"] = ref;
                
                // TODO: Check if message should be shown as alert
                // C# checks: !m.ShowOnGraphs() && !m.IsSquelched()
                // For now, create alert for all messages
                emit this->messageReceived(ref, snapshot);
            }
        }
        else if (operation == "del")
        {
            // Message was dismissed/deleted
            emit this->messageRemoved(ref);
        }
    }

    if (operation == "del")
    {
        // Remove object from cache
        // qDebug() << "XenLib: Event - Removing" << cacheType << ref;
        if (XenCache* cache = getCache())
            cache->Remove(cacheType, ref);
    } else if (operation == "add" || operation == "mod")
    {
        // Get snapshot from event
        QVariantMap snapshot = eventData.value("snapshot").toMap();

        if (!snapshot.isEmpty())
        {
            // Add ref to snapshot
            snapshot["ref"] = ref;
            snapshot["opaqueRef"] = ref;

            // qDebug() << "XenLib: Event -" << operation << cacheType << ref;
            getCache()->Update(cacheType, ref, snapshot);
        } else
        {
            // Snapshot not provided - fetch full record
            // qDebug() << "XenLib: Event - No snapshot, fetching" << cacheType << ref;
            this->requestObjectData(cacheType, ref);
        }
    }
}

void XenLib::onCachePopulated()
{
    // qDebug() << "XenLib: EventPoller reports cache populated";
    //  This is emitted after the first batch of events is processed
    //  Trigger UI updates now that cache has data
    emit cachePopulated();
}

void XenLib::onEventPollerConnectionLost()
{
    qWarning() << "XenLib: EventPoller lost connection - too many consecutive errors";
    // EventPoller has stopped due to repeated failures
    // This likely means the XenServer connection is broken

    this->setError("Event polling failed - connection lost");
    emit this->connectionError("Event polling connection lost");
}
