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

#ifndef XENLIB_H
#define XENLIB_H

#include "xenlib_global.h"
#include <QtCore/QObject>
#include <QtCore/QVariant>

QT_FORWARD_DECLARE_CLASS(XenConnection)
QT_FORWARD_DECLARE_CLASS(XenSession)
QT_FORWARD_DECLARE_CLASS(XenRpcAPI)
QT_FORWARD_DECLARE_CLASS(XenAsyncOperations)
QT_FORWARD_DECLARE_CLASS(XenCertificateManager)
QT_FORWARD_DECLARE_CLASS(ConnectionsManager)
QT_FORWARD_DECLARE_CLASS(XenCache)
QT_FORWARD_DECLARE_CLASS(MetricUpdater)

class XENLIB_EXPORT XenLib : public QObject
{
    Q_OBJECT
public:
    XenLib(QObject* parent = nullptr);
    ~XenLib();

    // Core Xen API functionality
    bool initialize();
    void cleanup();

    // Connection management
    bool connectToServer(const QString& hostname, int port, const QString& username, const QString& password, bool useSSL = true);
    void disconnectFromServer();
    bool isConnected() const;

    // API access
    XenRpcAPI* getAPI() const;
    XenConnection* getConnection() const;
    XenAsyncOperations* getAsyncOperations() const;
    XenCertificateManager* getCertificateManager() const;
    ConnectionsManager* getConnectionsManager() const;
    XenCache* getCache() const;
    MetricUpdater* getMetricUpdater() const;

    // High-level operations (blocking - use for backward compatibility)
    QVariantList getPools();

    // Get full object data by type and reference
    QVariantMap getCachedObjectData(const QString& objectType, const QString& objectRef);

    // Strongly-typed cache helpers (recommended over getObjectData)
    QVariantMap getVMRecord(const QString& vmRef);
    QVariantMap getHostRecord(const QString& hostRef);
    QVariantMap getPoolRecord(const QString& poolRef);
    QVariantMap getSRRecord(const QString& srRef);
    QVariantMap getNetworkRecord(const QString& networkRef);
    QVariantMap getVDIRecord(const QString& vdiRef);
    QVariantMap getVBDRecord(const QString& vbdRef);
    QVariantMap getVIFRecord(const QString& vifRef);
    QVariantMap getPIFRecord(const QString& pifRef);
    QVariantMap getPBDRecord(const QString& pbdRef);
    QVariantMap getVMGuestMetricsRecord(const QString& metricsRef);
    QVariantMap getHostMetricsRecord(const QString& metricsRef);
    QVariantMap getVMMetricsRecord(const QString& metricsRef);

    // Async high-level operations (non-blocking - recommended for UI)
    // These methods return immediately and emit signals when data is ready
    void requestVirtualMachines();
    void requestHosts();
    void requestPools();
    void requestStorageRepositories();
    void requestNetworks();
    void requestPIFs();
    void requestObjectData(const QString& objectType, const QString& objectRef);

    // VM management operations
    bool exportVM(const QString& vmRef, const QString& fileName, const QString& format = "xva");
    bool updateVM(const QString& vmRef, const QVariantMap& updates);
    bool setVMVCPUs(const QString& vmRef, int vcpus);
    bool setVMMemory(const QString& vmRef, qint64 memoryMB);

    // VM property helpers (use cached data)
    QString getVMProperty(const QString& vmRef, const QString& property);
    QString getGuestMetricsRef(const QString& vmRef);
    QVariantMap getGuestMetrics(const QString& vmRef);
    bool isControlDomainZero(const QString& vmRef, QString* outHostRef = nullptr);
    bool isSRDriverDomain(const QString& vmRef, QString* outSRRef = nullptr);
    bool srHasDriverDomain(const QString& srRef, QString* outVMRef = nullptr);
    bool isHVM(const QString& vmRef);
    bool hasRDP(const QString& vmRef);
    bool isRDPEnabled(const QString& vmRef);
    bool canEnableRDP(const QString& vmRef);
    bool isRDPControlEnabled(const QString& vmRef);
    bool isVMWindows(const QString& vmRef);
    QString getVMIPAddressForSSH(const QString& vmRef);
    bool hasGPUPassthrough(const QString& vmRef);

    // Host property helpers
    QString getControlDomainForHost(const QString& hostRef);

    // Snapshot operations
    QVariantList getVMSnapshots(const QString& vmRef);
    QString createVMSnapshot(const QString& vmRef, const QString& name, const QString& description = QString());
    bool deleteSnapshot(const QString& snapshotRef);
    bool revertToSnapshot(const QString& snapshotRef);

    // VBD/VDI (Virtual Disk) operations
    QVariantList getVMVBDs(const QString& vmRef);

    // CD/DVD operations
    bool changeVMISO(const QString& vmRef, const QString& vbdRef, const QString& vdiRef);
    bool createCdDrive(const QString& vmRef);

    // VIF (Virtual Network Interface) operations
    QVariantList getVMVIFs(const QString& vmRef);
    QString createVIF(const QString& vmRef, const QString& networkRef, const QString& device, const QString& mac = QString());

    // VM migration operations
    QString poolMigrateVM(const QString& vmRef, const QString& hostRef, bool live = true);
    bool canMigrateVM(const QString& vmRef, const QString& hostRef);

    // Host management operations
    bool setHostName(const QString& hostRef, const QString& name);
    bool setHostDescription(const QString& hostRef, const QString& description);
    bool setHostTags(const QString& hostRef, const QStringList& tags);
    bool setHostOtherConfig(const QString& hostRef, const QString& key, const QString& value);
    bool setHostIqn(const QString& hostRef, const QString& iqn);

    // Generic object property setters (dispatches to specific type methods)
    bool setObjectName(const QString& objectType, const QString& objectRef, const QString& name);
    bool setObjectDescription(const QString& objectType, const QString& objectRef, const QString& description);
    bool setObjectTags(const QString& objectType, const QString& objectRef, const QStringList& tags);
    bool setObjectProperties(const QString& objectType, const QString& objectRef, const QVariantMap& properties);

    // Pool management operations
    bool setPoolName(const QString& poolRef, const QString& name);
    bool setPoolDescription(const QString& poolRef, const QString& description);
    bool setPoolTags(const QString& poolRef, const QStringList& tags);
    bool setPoolMigrationCompression(const QString& poolRef, bool enabled);

    // SR (Storage Repository) operations
    bool setSRName(const QString& srRef, const QString& name);
    bool setSRDescription(const QString& srRef, const QString& description);
    bool setSRTags(const QString& srRef, const QStringList& tags);

    // Network operations
    QString createNetwork(const QString& name, const QString& description = QString(), const QVariantMap& otherConfig = QVariantMap());
    bool destroyNetwork(const QString& networkRef);
    bool setNetworkName(const QString& networkRef, const QString& name);
    bool setNetworkDescription(const QString& networkRef, const QString& description);
    bool setNetworkTags(const QString& networkRef, const QStringList& tags);
    bool setNetworkMTU(const QString& networkRef, int mtu);
    bool setNetworkOtherConfig(const QString& networkRef, const QVariantMap& otherConfig);

    // Status and information
    QString getConnectionInfo() const;
    QString getLastError() const;
    bool hasError() const;

signals:
    void connectionStateChanged(bool connected);
    void connectionError(const QString& error);
    void authenticationFailed(const QString& hostname, int port, const QString& username, const QString& errorMessage);
    void redirectedToMaster(const QString& masterAddress);
    void apiCallCompleted(const QString& method, const QVariant& result);
    void apiCallFailed(const QString& method, const QString& error);
    void certificateError(const QString& hostname, const QString& error);

    // Async API result signals (emitted when async requests complete)
    void virtualMachinesReceived(QVariantList vms);
    void hostsReceived(QVariantList hosts);
    void poolsReceived(QVariantList pools);
    void storageRepositoriesReceived(QVariantList srs);
    void networksReceived(QVariantList networks);
    void objectDataReceived(QString objectType, QString objectRef, QVariantMap data);

    // Task rehydration signals (forwarded from EventPoller for task monitoring)
    void taskAdded(XenConnection* connection, const QString& taskRef, const QVariantMap& taskData);
    void taskModified(XenConnection* connection, const QString& taskRef, const QVariantMap& taskData);
    void taskDeleted(XenConnection* connection, const QString& taskRef);

    // XenAPI Message signals (for alert system integration)
    void messageReceived(const QString& messageRef, const QVariantMap& messageData);
    void messageRemoved(const QString& messageRef);

    // Cache population complete (emitted after EventPoller's initial cache load)
    void cachePopulated();

private slots:
    void handleConnectionStateChanged(bool connected);
    void handleConnectionError(const QString& error);
    void handleLoginResult(bool success);
    void handleApiCallResult(const QString& method, const QVariant& result);
    void handleApiCallError(const QString& method, const QString& error);

    // Async connection handling (worker-based)
    void onConnectionEstablished();
    void onConnectionError(const QString& errorMessage);
    void onConnectionProgress(const QString& message);
    void onRedirectToMaster(const QString& masterAddress);

    // Async API response handler
    void onConnectionApiResponse(int requestId, const QByteArray& response);

    // Pool member population for failover
    void onHostsReceivedForPoolMembers(const QVariantList& hosts);
    void onPoolsReceivedForHATracking(const QVariantList& pools);

    // EventPoller handlers
    void onEventReceived(const QVariantMap& eventData);
    void onCachePopulated();
    void onEventPollerConnectionLost();

private:
    class Private;
    Private* d;

    void setupConnections();
    void clearError();
    void setError(const QString& error);
    QString populateCache(); // Returns the event token from initial event.from
    bool fetchAllRecordsForType(const QString& normalizedType, QVariantMap* outRecords);
};

#endif // XENLIB_H
