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

#include "../../xenlib_global.h"
#include "../../collections/observablelist.h"
#include "connection.h"
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QMutex>
#include <QtCore/QTimer>
#include <QtCore/QPointer>
#include <QtCore/QHash>
#include <QtCore/QDateTime>

class XenHeartbeat;

namespace XenAPI
{
    class Session;
}

namespace Xen
{
    class XENLIB_EXPORT ConnectionsManager : public QObject
    {
        Q_OBJECT

        public:
            static ConnectionsManager* instance();

            ~ConnectionsManager();

            // Connection management
            void AddConnection(XenConnection* connection);
            void RemoveConnection(XenConnection* connection);
            bool ContainsConnection(XenConnection* connection) const;

            // Connection queries
            XenConnection* FindConnectionByHostname(const QString& hostname, int port = -1) const;
            QList<XenConnection*> GetConnectedConnections() const;
            QList<XenConnection*> GetAllConnections() const;
            int ConnectionCount() const;

            // Session pooling - get or create duplicate session for long-running operations
            XenAPI::Session *AcquireSession(XenConnection* connection);
            void ReleaseSession(XenAPI::Session *session);

            // Connection state management
            void ConnectAll();
            void DisconnectAll();
            void CancelAllOperations(XenConnection* connection = nullptr);

            // Monitoring and health
            void StartConnectionMonitoring();
            void StopConnectionMonitoring();
            bool IsMonitoring() const;

            // Reconnection management
            void EnableAutoReconnection(bool enabled);
            bool IsAutoReconnectionEnabled() const;
            void ReconnectConnection(XenConnection* connection);
            void ReconnectAll();

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
            void onReconnectionTimer();
            void onHeartbeatConnectionLost(XenConnection* connection);

        private:
            ConnectionsManager(QObject* parent = nullptr);
            static ConnectionsManager* s_instance;

            void setupConnection(XenConnection* connection);
            void cleanupConnection(XenConnection* connection);
            void checkConnectionHealth();

            // Pool member failover methods
            void startCoordinatorSearchTimer(XenConnection* connection, int timeoutMs);
            void tryNextPoolMember(XenConnection* connection);

            ObservableList<XenConnection*>* m_connections;
            QMutex m_connectionsMutex;
            QTimer* m_monitoringTimer;
            QHash<XenConnection*, QString> m_connectionStates; // Track last known states
            bool m_isMonitoring;

            // Reconnection management
            QHash<XenConnection*, QTimer*> m_reconnectionTimers;
            QHash<XenConnection*, XenHeartbeat*> m_heartbeats;
            QHash<XenConnection*, QDateTime> m_lastConnectionAttempt; // Track reconnection attempts
            bool m_autoReconnectionEnabled;

            // Session pool for duplicate sessions
            QHash<XenConnection*, QList<XenAPI::Session*>> m_sessionPool;
            QHash<XenAPI::Session*, XenConnection*> m_sessionToConnection;
            QMutex m_sessionPoolMutex;

            // Default connection parameters
            static const int DEFAULT_PORT = 443;
            static const int MONITORING_INTERVAL_MS = 30000;       // 30 seconds
            static const int RECONNECTION_TIMEOUT_MS = 120000;     // 2 minutes
            static const int RECONNECTION_SHORT_TIMEOUT_MS = 5000; // 5 seconds

            // Pool member failover timeouts (matching C# XenConnection)
            static const int SEARCH_NEW_COORDINATOR_TIMEOUT_MS = 60000;     // 60 seconds - initial wait before searching
            static const int SEARCH_NEXT_SUPPORTER_TIMEOUT_MS = 15000;      // 15 seconds - between each supporter try
            static const int SEARCH_NEW_COORDINATOR_STOP_AFTER_MS = 360000; // 6 minutes - total search time
    };
}

#endif // CONNECTIONSMANAGER_H
