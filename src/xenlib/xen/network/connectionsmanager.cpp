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

#include "connectionsmanager.h"
#include "heartbeat.h"
#include "../session.h"
#include <QtCore/QDebug>
#include <QtCore/QThread>
#include <QtCore/QMutexLocker>
#include <QtCore/QDateTime>

using namespace Xen;

// Define static members
ConnectionsManager* ConnectionsManager::s_instance = nullptr;

// Define static const members
const int ConnectionsManager::DEFAULT_PORT;
const int ConnectionsManager::MONITORING_INTERVAL_MS;
const int ConnectionsManager::RECONNECTION_TIMEOUT_MS;
const int ConnectionsManager::RECONNECTION_SHORT_TIMEOUT_MS;
const int ConnectionsManager::SEARCH_NEW_COORDINATOR_TIMEOUT_MS;
const int ConnectionsManager::SEARCH_NEXT_SUPPORTER_TIMEOUT_MS;
const int ConnectionsManager::SEARCH_NEW_COORDINATOR_STOP_AFTER_MS;

ConnectionsManager* ConnectionsManager::instance()
{
    static QMutex instanceMutex;
    QMutexLocker locker(&instanceMutex);

    if (!s_instance)
        s_instance = new ConnectionsManager();

    return s_instance;
}

ConnectionsManager::ConnectionsManager(QObject* parent)
    : QObject(parent), m_connections(new ObservableList<XenConnection*>(this)), m_monitoringTimer(new QTimer(this)), m_isMonitoring(false), m_autoReconnectionEnabled(false)
{
    // Connect to collection events
    connect(this->m_connections, &ObservableListBase::collectionChanged,
            this, &ConnectionsManager::connectionsChanged);
    connect(this->m_connections, &ObservableListBase::cleared,
            this, &ConnectionsManager::connectionsChanged);

    // Setup monitoring timer
    this->m_monitoringTimer->setInterval(MONITORING_INTERVAL_MS);
    this->m_monitoringTimer->setSingleShot(false);
    connect(this->m_monitoringTimer, &QTimer::timeout,
            this, &ConnectionsManager::onMonitoringTimer);
}

ConnectionsManager::~ConnectionsManager()
{
    this->StopConnectionMonitoring();
    this->DisconnectAll();

    // Clean up all connections
    auto connections = this->m_connections->toList();
    for (XenConnection* conn : connections)
    {
        this->cleanupConnection(conn);
    }
    this->m_connections->clear();
}

void ConnectionsManager::AddConnection(XenConnection* connection)
{
    if (!connection || this->ContainsConnection(connection))
    {
        return;
    }

    this->setupConnection(connection);
    this->m_connections->append(connection);

    emit connectionAdded(connection);
    qDebug() << "Added connection:" << connection->GetHostname() << ":" << connection->GetPort();
}

void ConnectionsManager::RemoveConnection(XenConnection* connection)
{
    if (!connection || !this->ContainsConnection(connection))
    {
        return;
    }

    this->cleanupConnection(connection);
    this->m_connections->removeOne(connection);

    emit connectionRemoved(connection);
    qDebug() << "Removed connection:" << connection->GetHostname() << ":" << connection->GetPort();

    // Check if all connections are now disconnected
    if (this->GetConnectedConnections().isEmpty())
    {
        emit allConnectionsDisconnected();
    }
}

bool ConnectionsManager::ContainsConnection(XenConnection* connection) const
{
    return this->m_connections->contains(connection);
}

XenConnection* ConnectionsManager::FindConnectionByHostname(const QString& hostname, int port) const
{
    auto connections = this->m_connections->toList();
    for (XenConnection* conn : connections)
    {
        if (conn->GetHostname() == hostname)
        {
            if (port == -1 || conn->GetPort() == port)
            {
                return conn;
            }
        }
    }
    return nullptr;
}

QList<XenConnection*> ConnectionsManager::GetConnectedConnections() const
{
    QList<XenConnection*> connected;
    auto connections = this->m_connections->toList();

    for (XenConnection* conn : connections)
    {
        if (conn && conn->IsConnected())
        {
            connected.append(conn);
        }
    }
    return connected;
}

QList<XenConnection*> ConnectionsManager::GetAllConnections() const
{
    return this->m_connections->toList();
}

int ConnectionsManager::ConnectionCount() const
{
    return this->m_connections->size();
}

XenAPI::Session* ConnectionsManager::AcquireSession(XenConnection* connection)
{
    if (!connection || !connection->IsConnected())
    {
        qWarning() << "Cannot acquire session: connection not available or not connected";
        return nullptr;
    }

    QMutexLocker locker(&this->m_sessionPoolMutex);

    // Check if we have any available sessions in the pool for this connection
    QList<XenAPI::Session*>& pool = this->m_sessionPool[connection];

    if (!pool.isEmpty())
    {
        // Reuse an existing session from the pool
        XenAPI::Session* session = pool.takeFirst();
        qDebug() << "Reusing pooled session for" << connection->GetHostname();
        return session;
    }

    // No available sessions, need to create a new one
    // We need the original session to duplicate from
    // For now, return nullptr as we need integration with XenLib to get the original session
    qWarning() << "Session pool empty and duplication not yet fully integrated";
    return nullptr;
}

void ConnectionsManager::ReleaseSession(XenAPI::Session* session)
{
    if (!session)
    {
        return;
    }

    QMutexLocker locker(&this->m_sessionPoolMutex);

    // Find which connection this session belongs to
    XenConnection* connection = this->m_sessionToConnection.value(session);
    if (!connection)
    {
        qWarning() << "Cannot release session: connection not found";
        // Clean up orphaned session
        session->Logout();
        session->deleteLater();
        return;
    }

    // Return the session to the pool
    QList<XenAPI::Session*>& pool = this->m_sessionPool[connection];
    if (!pool.contains(session))
    {
        pool.append(session);
        qDebug() << "Returned session to pool for" << connection->GetHostname()
                 << "(pool size:" << pool.size() << ")";
    }
}

void ConnectionsManager::ConnectAll()
{
    // TODO: Update for worker-based connections - need username/password
    qWarning() << "ConnectionsManager::connectAll: Not yet implemented for worker-based connections";
    /*
    auto connections = m_connections->toList();
    for (XenConnection* conn : connections)
    {
        if (conn && !conn->isConnected())
        {
            conn->connectToHost(conn->getHostname(), conn->getPort());
        }
    }
    */
}

void ConnectionsManager::DisconnectAll()
{
    auto connections = this->m_connections->toList();
    for (XenConnection* conn : connections)
    {
        if (conn && conn->IsConnected())
        {
            conn->EndConnect();
        }
    }
}

void ConnectionsManager::CancelAllOperations(XenConnection* connection)
{
    if (connection)
    {
        // TODO this looks incorrect we are disconnecting here instead of just cancelling ops
        // Cancel operations for specific connection
        qDebug() << "Canceling operations for connection:" << connection->GetHostname();
        connection->EndConnect();
    } else
    {
        // Cancel operations for all connections
        qDebug() << "Canceling operations for all connections";
        this->DisconnectAll();
    }
}

void ConnectionsManager::StartConnectionMonitoring()
{
    if (!this->m_isMonitoring)
    {
        this->m_isMonitoring = true;
        this->m_monitoringTimer->start();
        qDebug() << "Started connection monitoring";
    }
}

void ConnectionsManager::StopConnectionMonitoring()
{
    if (this->m_isMonitoring)
    {
        this->m_isMonitoring = false;
        this->m_monitoringTimer->stop();
        qDebug() << "Stopped connection monitoring";
    }
}

bool ConnectionsManager::IsMonitoring() const
{
    return this->m_isMonitoring;
}

void ConnectionsManager::onConnectionConnected()
{
    XenConnection* connection = qobject_cast<XenConnection*>(sender());
    if (connection)
    {
        this->m_connectionStates[connection] = "connected";
        emit connectionStateChanged(connection, true);
        qDebug() << "Connection established:" << connection->GetHostname();

        // Start heartbeat monitoring for this connection
        XenHeartbeat* heartbeat = this->m_heartbeats.value(connection);
        if (heartbeat)
        {
            heartbeat->start();
        }

        // Stop reconnection timer for this connection since it's now connected
        QTimer* timer = this->m_reconnectionTimers.value(connection);
        if (timer && timer->isActive())
        {
            timer->stop();
        }
    }
}

void ConnectionsManager::onConnectionDisconnected()
{
    XenConnection* connection = qobject_cast<XenConnection*>(sender());
    if (!connection)
    {
        return;
    }

    this->m_connectionStates[connection] = "disconnected";
    emit connectionStateChanged(connection, false);
    qDebug() << "Connection lost:" << connection->GetHostname();

    // Stop heartbeat monitoring for this connection
    XenHeartbeat* heartbeat = this->m_heartbeats.value(connection);
    if (heartbeat)
    {
        heartbeat->stop();
    }

    // Check if pool member failover is needed (matches C# HandleConnectionLost logic)
    QStringList poolMembers = connection->GetPoolMembers();
    bool hasMultipleMembers = poolMembers.size() > 1;
    bool coordinatorMayChange = connection->GetCoordinatorMayChange();
    bool expectDisruption = connection->GetExpectDisruption();

    // If we have multiple pool members and coordinator may change (HA enabled or pool),
    // try to failover to another pool member
    if (hasMultipleMembers && (coordinatorMayChange || expectDisruption))
    {
        qDebug() << "ConnectionsManager: Connection lost, attempting pool member failover";
        qDebug() << "  Pool has" << poolMembers.size() << "members, coordinator may change:" << coordinatorMayChange;

        // Save the current hostname as last coordinator
        connection->SetLastCoordinatorHostname(connection->GetHostname());
        connection->SetFindingNewCoordinator(true);
        connection->SetFindingNewCoordinatorStartedAt(QDateTime::currentDateTime());

        // Reset pool member index to try all members
        // Don't reconnect to coordinator first if there are supporters
        connection->ResetPoolMemberIndex();
        if (poolMembers.size() > 1 && !poolMembers.isEmpty())
        {
            // Skip coordinator (first member), try a supporter first
            connection->SetCurrentPoolMemberIndex(1);
        }

        // Start searching for new coordinator with initial timeout
        this->startCoordinatorSearchTimer(connection, SEARCH_NEW_COORDINATOR_TIMEOUT_MS);
    } else
    {
        qDebug() << "ConnectionsManager: Simple reconnection (single host or no HA)";

        // Simple reconnection for standalone hosts
        if (this->m_autoReconnectionEnabled)
        {
            this->ReconnectConnection(connection);
        }
    }

    // Check if all connections are now disconnected
    if (this->GetConnectedConnections().isEmpty())
    {
        emit allConnectionsDisconnected();
    }
}

void ConnectionsManager::onConnectionError(const QString& error)
{
    XenConnection* connection = qobject_cast<XenConnection*>(sender());
    if (connection)
    {
        this->m_connectionStates[connection] = "error";
        qWarning() << "Connection error for" << connection->GetHostname() << ":" << error;
        emit connectionStateChanged(connection, false);
    }
}

void ConnectionsManager::onMonitoringTimer()
{
    this->checkConnectionHealth();
}

void ConnectionsManager::setupConnection(XenConnection* connection)
{
    // Connect to connection signals
    connect(connection, &XenConnection::Connected,
            this, &ConnectionsManager::onConnectionConnected);
    connect(connection, &XenConnection::Disconnected,
            this, &ConnectionsManager::onConnectionDisconnected);
    connect(connection, &XenConnection::Error,
            this, &ConnectionsManager::onConnectionError);

    // Create and setup heartbeat for this connection
    XenHeartbeat* heartbeat = new XenHeartbeat(connection, 15000, this); // 15 second timeout
    this->m_heartbeats[connection] = heartbeat;

    connect(heartbeat, &XenHeartbeat::connectionLost,
            this, [this, connection]() {
                this->onHeartbeatConnectionLost(connection);
            });
    connect(heartbeat, &XenHeartbeat::serverTimeUpdated,
            this, [connection](const QDateTime& serverTime, const QDateTime& localTime) {
                Q_UNUSED(serverTime);
                const qint64 offsetSeconds = serverTime.secsTo(localTime);
                connection->SetServerTimeOffsetSeconds(offsetSeconds);
            });

    // Initialize state tracking
    this->m_connectionStates[connection] = "new";
}

void ConnectionsManager::cleanupConnection(XenConnection* connection)
{
    // Disconnect from connection signals
    disconnect(connection, &XenConnection::Connected,
               this, &ConnectionsManager::onConnectionConnected);
    disconnect(connection, &XenConnection::Disconnected,
               this, &ConnectionsManager::onConnectionDisconnected);
    disconnect(connection, &XenConnection::Error,
               this, &ConnectionsManager::onConnectionError);

    // Clean up heartbeat
    XenHeartbeat* heartbeat = this->m_heartbeats.take(connection);
    if (heartbeat)
    {
        heartbeat->stop();
        heartbeat->deleteLater();
    }

    // Clean up reconnection timer
    QTimer* timer = this->m_reconnectionTimers.take(connection);
    if (timer)
    {
        timer->stop();
        timer->deleteLater();
    }

    // Clean up pooled sessions for this connection
    QMutexLocker locker(&this->m_sessionPoolMutex);
    QList<XenAPI::Session*> sessions = this->m_sessionPool.take(connection);
    for (XenAPI::Session* session : sessions)
    {
        if (session)
        {
            session->Logout();
            this->m_sessionToConnection.remove(session);
            session->deleteLater();
        }
    }

    // Remove from state tracking
    this->m_connectionStates.remove(connection);
    this->m_lastConnectionAttempt.remove(connection);
    this->m_lastConnectionAttempt.remove(connection);

    // Ensure connection is disconnected
    if (connection->IsConnected())
    {
        connection->EndConnect();
    }
}

void ConnectionsManager::checkConnectionHealth()
{
    auto connections = this->m_connections->toList();
    int connectedCount = 0;
    int totalCount = connections.size();

    for (XenConnection* conn : connections)
    {
        if (conn && conn->IsConnected())
        {
            connectedCount++;
            // Could add ping/heartbeat checks here
        }
    }

    qDebug() << "Connection health check:" << connectedCount << "/" << totalCount << "connected";
}

void ConnectionsManager::EnableAutoReconnection(bool enabled)
{
    this->m_autoReconnectionEnabled = enabled;

    if (enabled)
    {
        qDebug() << "Auto-reconnection enabled";
        // Individual timers will be created per connection as needed
    } else
    {
        qDebug() << "Auto-reconnection disabled";
        // Stop all individual reconnection timers
        for (auto timer : this->m_reconnectionTimers)
        {
            if (timer && timer->isActive())
            {
                timer->stop();
            }
        }
    }
}

bool ConnectionsManager::IsAutoReconnectionEnabled() const
{
    return this->m_autoReconnectionEnabled;
}

void ConnectionsManager::ReconnectConnection(XenConnection* connection)
{
    if (!connection)
    {
        return;
    }

    QDateTime now = QDateTime::currentDateTime();
    QDateTime lastAttempt = this->m_lastConnectionAttempt.value(connection, QDateTime());

    // Rate limiting - don't retry more than once per 5 seconds
    if (lastAttempt.isValid() && lastAttempt.msecsTo(now) < RECONNECTION_SHORT_TIMEOUT_MS)
    {
        qDebug() << "Rate limiting reconnection for" << connection->GetHostname();
        return;
    }

    this->m_lastConnectionAttempt[connection] = now;

    if (!connection->IsConnected())
    {
        // TODO: Update for worker-based connections - need username/password
        qWarning() << "ConnectionsManager::reconnectConnection: Not yet implemented for worker-based connections";
        /*
        qDebug() << "Attempting to reconnect:" << connection->getHostname();
        connection->connectToHost(connection->getHostname(), connection->getPort());
        */

        // Setup a reconnection timer for this specific connection if it doesn't exist
        if (!this->m_reconnectionTimers.contains(connection))
        {
            QTimer* timer = new QTimer(this);
            timer->setInterval(RECONNECTION_TIMEOUT_MS);
            timer->setSingleShot(false);

            connect(timer, &QTimer::timeout, [this, connection]() {
                if (connection && !connection->IsConnected())
                {
                    this->ReconnectConnection(connection);
                }
            });

            this->m_reconnectionTimers[connection] = timer;
            timer->start();
        }
    }
}

void ConnectionsManager::ReconnectAll()
{
    auto connections = this->m_connections->toList();
    for (XenConnection* conn : connections)
    {
        if (conn && !conn->IsConnected())
        {
            this->ReconnectConnection(conn);
        }
    }
}

void ConnectionsManager::onHeartbeatConnectionLost(XenConnection* connection)
{
    if (!connection)
    {
        return;
    }

    qWarning() << "Heartbeat detected connection loss for" << connection->GetHostname();

    // Mark as disconnected
    this->m_connectionStates[connection] = "heartbeat_lost";
    emit connectionStateChanged(connection, false);

    // Use same failover logic as onConnectionDisconnected
    // Check if pool member failover is needed
    QStringList poolMembers = connection->GetPoolMembers();
    bool hasMultipleMembers = poolMembers.size() > 1;
    bool coordinatorMayChange = connection->GetCoordinatorMayChange();
    bool expectDisruption = connection->GetExpectDisruption();

    if (hasMultipleMembers && (coordinatorMayChange || expectDisruption))
    {
        qDebug() << "ConnectionsManager: Heartbeat lost, attempting pool member failover";

        connection->SetLastCoordinatorHostname(connection->GetHostname());
        connection->SetFindingNewCoordinator(true);
        connection->SetFindingNewCoordinatorStartedAt(QDateTime::currentDateTime());

        connection->ResetPoolMemberIndex();
        if (poolMembers.size() > 1)
        {
            connection->SetCurrentPoolMemberIndex(1); // Skip coordinator
        }

        this->startCoordinatorSearchTimer(connection, SEARCH_NEW_COORDINATOR_TIMEOUT_MS);
    } else if (this->m_autoReconnectionEnabled)
    {
        this->ReconnectConnection(connection);
    }
}

void ConnectionsManager::startCoordinatorSearchTimer(XenConnection* connection, int timeoutMs)
{
    if (!connection)
    {
        return;
    }

    qDebug() << "ConnectionsManager: Starting coordinator search timer for"
             << connection->GetLastCoordinatorHostname() << "with timeout" << timeoutMs << "ms";

    // Create or reuse timer for this connection
    QTimer* timer = this->m_reconnectionTimers.value(connection);
    if (!timer)
    {
        timer = new QTimer(this);
        this->m_reconnectionTimers[connection] = timer;
    }

    timer->setSingleShot(true);
    timer->setInterval(timeoutMs);

    // Connect to tryNextPoolMember when timer fires
    disconnect(timer, nullptr, nullptr, nullptr); // Clear old connections
    connect(timer, &QTimer::timeout, this, [this, connection]() {
        this->tryNextPoolMember(connection);
    });

    timer->start();
}

void ConnectionsManager::tryNextPoolMember(XenConnection* connection)
{
    if (!connection)
    {
        return;
    }

    // Check if we should give up the search
    QDateTime searchStarted = connection->GetFindingNewCoordinatorStartedAt();
    if (searchStarted.isValid())
    {
        qint64 elapsedMs = searchStarted.msecsTo(QDateTime::currentDateTime());
        bool expectDisruption = connection->GetExpectDisruption();

        if (!expectDisruption && elapsedMs > SEARCH_NEW_COORDINATOR_STOP_AFTER_MS)
        {
            qWarning() << "ConnectionsManager: Stopping coordinator search after" << elapsedMs << "ms";
            qDebug() << "  Trying last coordinator one more time:" << connection->GetLastCoordinatorHostname();

            connection->SetFindingNewCoordinator(false);

            // Try the original coordinator one last time
            // TODO: Need to implement reconnect with specific hostname
            // For now just log it
            qWarning() << "ConnectionsManager: Final reconnect attempt not yet implemented";
            return;
        }
    }

    // Try next pool member
    if (connection->HasMorePoolMembers())
    {
        QString nextMember = connection->GetNextPoolMember();
        qDebug() << "ConnectionsManager: Trying next pool member:" << nextMember;

        // TODO: Need to implement connection to specific pool member
        // This requires modifying connectToHost to accept hostname override
        // For now, schedule next retry
        qWarning() << "ConnectionsManager: Pool member connection not yet implemented";

        // Schedule next attempt with shorter timeout
        this->startCoordinatorSearchTimer(connection, SEARCH_NEXT_SUPPORTER_TIMEOUT_MS);
    } else
    {
        // Tried all pool members, loop back if expecting disruption or within time limit
        qDebug() << "ConnectionsManager: Tried all pool members, looping back";
        connection->ResetPoolMemberIndex();

        if (connection->HasMorePoolMembers())
        {
            this->startCoordinatorSearchTimer(connection, SEARCH_NEXT_SUPPORTER_TIMEOUT_MS);
        } else
        {
            qWarning() << "ConnectionsManager: No pool members available for failover";
            connection->SetFindingNewCoordinator(false);
        }
    }
}

void ConnectionsManager::onReconnectionTimer()
{
    // This method is called by individual connection timers through lambda functions
    // The actual reconnection logic is in the lambda connected to each timer
    // This method can be used for general cleanup or logging if needed
    qDebug() << "Reconnection timer triggered - handled by individual connection timers";
}
