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

#ifndef CONNECTIONSMANAGER_H
#define CONNECTIONSMANAGER_H

#include "../xenlib_global.h"
#include "observablelist.h"
#include "../xen/connection.h"
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QMutex>
#include <QtCore/QTimer>
#include <QtCore/QPointer>
#include <QtCore/QHash>

class XENLIB_EXPORT ConnectionsManager : public QObject
{
    Q_OBJECT

public:
    static ConnectionsManager* instance();

    ~ConnectionsManager();

    // Connection management
    XenConnection* createConnection(const QString& hostname, int port, bool useSSL = true);
    void addConnection(XenConnection* connection);
    void removeConnection(XenConnection* connection);
    bool containsConnection(XenConnection* connection) const;

    // Connection queries
    XenConnection* findConnectionByHostname(const QString& hostname, int port = -1) const;
    QList<XenConnection*> getConnectedConnections() const;
    QList<XenConnection*> getAllConnections() const;
    int connectionCount() const;

    // Connection state management
    void connectAll();
    void disconnectAll();
    void cancelAllOperations(XenConnection* connection = nullptr);

    // Monitoring and health
    void startConnectionMonitoring();
    void stopConnectionMonitoring();
    bool isMonitoring() const;

    // Reconnection management
    void enableAutoReconnection(bool enabled);
    bool isAutoReconnectionEnabled() const;
    void reconnectConnection(XenConnection* connection);
    void reconnectAll();

signals:
    void connectionAdded(XenConnection* connection);
    void connectionRemoved(XenConnection* connection);
    void connectionStateChanged(XenConnection* connection, bool connected);
    void connectionsChanged();
    void allConnectionsDisconnected();

private slots:
    void onConnectionConnected();
    void onConnectionDisconnected();
    void onConnectionError(const QString& error);
    void onMonitoringTimer();
    void onHeartbeatConnectionLost();
    void onReconnectionTimer();
    void onHeartbeatConnectionLost(XenConnection* connection);
    void onReconnectionTimer();

private:
    ConnectionsManager(QObject* parent = nullptr);
    static ConnectionsManager* s_instance;

    void setupConnection(XenConnection* connection);
    void cleanupConnection(XenConnection* connection);
    void checkConnectionHealth();

    ObservableList<XenConnection*>* m_connections;
    QMutex m_connectionsMutex;
    QTimer* m_monitoringTimer;
    QTimer* m_reconnectionTimer;
    QHash<XenConnection*, QString> m_connectionStates;        // Track last known states
    QHash<XenConnection*, QDateTime> m_lastConnectionAttempt; // Track reconnection attempts
    bool m_isMonitoring;
    bool m_autoReconnectionEnabled;

    // Default connection parameters
    static const int DEFAULT_PORT = 443;
    static const int MONITORING_INTERVAL_MS = 30000;    // 30 seconds
    static const int RECONNECTION_INTERVAL_MS = 120000; // 2 minutes
};

#endif // CONNECTIONSMANAGER_H