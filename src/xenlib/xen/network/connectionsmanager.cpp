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
#include <QtCore/QMetaType>

using namespace Xen;

// Define static members
ConnectionsManager* ConnectionsManager::s_instance = nullptr;

// Define static const members
const int ConnectionsManager::DEFAULT_PORT;
const int ConnectionsManager::MONITORING_INTERVAL_MS;

ConnectionsManager* ConnectionsManager::instance()
{
    static QMutex instanceMutex;
    QMutexLocker locker(&instanceMutex);

    if (!s_instance)
        s_instance = new ConnectionsManager();

    return s_instance;
}

ConnectionsManager::ConnectionsManager(QObject* parent)
    : QObject(parent), m_connections(new ObservableList<XenConnection*>(this)), m_monitoringTimer(new QTimer(this)), m_isMonitoring(false)
{
    // Required for queued cross-thread delivery of ObservableList collectionChanged on Qt5.
    qRegisterMetaType<ObservableListBase::CollectionChangeAction>("ObservableListBase::CollectionChangeAction");

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

void ConnectionsManager::onConnectionConnected(XenConnection* connection)
{
    if (connection)
    {
        this->m_connectionStates[connection] = "connected";
        emit connectionStateChanged(connection, true);
        qDebug() << "Connection established:" << connection->GetHostname();

        this->startHeartbeat(connection);
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

    this->stopHeartbeat(connection);

    // Reconnecting, or finding a new pool coordinator, is XenConnection's job (C# HandleConnectionLost)

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
    // Logical readiness is emitted after login and initial cache synchronization.
    // Connected belongs to the internal socket transport, not this outer connection.
    connect(connection, &XenConnection::ConnectionResult, this,
            [this, connection](bool ready, const QString&) {
                if (ready && connection->IsConnected())
                    this->onConnectionConnected(connection);
                else
                    this->stopHeartbeat(connection);
            });
    connect(connection, &XenConnection::BeforeConnectionEnd, this, [this, connection]() {
        this->stopHeartbeat(connection);
    });
    connect(connection, &XenConnection::Disconnected,
            this, &ConnectionsManager::onConnectionDisconnected);
    connect(connection, &XenConnection::Error,
            this, &ConnectionsManager::onConnectionError);

    // Initialize state tracking
    this->m_connectionStates[connection] = "new";
}

void ConnectionsManager::startHeartbeat(XenConnection* connection)
{
    if (this->m_heartbeats.contains(connection))
        return;

    auto* workerThread = new QThread();
    workerThread->setObjectName(QString("Heartbeat for %1").arg(connection->GetHostname()));
    auto* heartbeat = new XenHeartbeat(connection, 15000);
    heartbeat->moveToThread(workerThread);
    this->m_heartbeats.insert(connection, heartbeat);

    connect(workerThread, &QThread::started, heartbeat, &XenHeartbeat::start);
    connect(workerThread, &QThread::finished, heartbeat, &QObject::deleteLater);
    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);
    const QPointer<XenHeartbeat> monitor(heartbeat);
    connect(heartbeat, &XenHeartbeat::connectionLost, this, [this, connection, monitor]() {
        // Ignore queued results from a monitor retired by disconnect/reconnect.
        if (monitor && this->m_heartbeats.value(connection) == monitor.data())
            this->onHeartbeatConnectionLost(connection);
    });
    connect(heartbeat, &XenHeartbeat::serverTimeUpdated, this,
            [this, connection, monitor](const QDateTime& serverTime, const QDateTime& localTime) {
                if (monitor && this->m_heartbeats.value(connection) == monitor.data() && connection->IsConnected())
                    connection->SetServerTimeOffsetSeconds(serverTime.secsTo(localTime));
            });
    workerThread->start();
}

void ConnectionsManager::stopHeartbeat(XenConnection* connection)
{
    XenHeartbeat* heartbeat = this->m_heartbeats.take(connection);
    if (!heartbeat)
        return;

    // Probe/cleanup may block on transport I/O. Keep both on the heartbeat thread.
    QMetaObject::invokeMethod(heartbeat, [heartbeat]() {
        heartbeat->stop();
        QThread::currentThread()->quit();
    }, Qt::QueuedConnection);
}

void ConnectionsManager::cleanupConnection(XenConnection* connection)
{
    // Disconnect from connection signals
    disconnect(connection, nullptr, this, nullptr);

    this->stopHeartbeat(connection);

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

void ConnectionsManager::onHeartbeatConnectionLost(XenConnection* connection)
{
    if (!connection || !this->ContainsConnection(connection) || !connection->IsConnected())
        return;

    qWarning() << "Heartbeat detected connection loss for" << connection->GetHostname();
    this->stopHeartbeat(connection);
    this->m_connectionStates[connection] = "heartbeat_lost";
    emit this->connectionStateChanged(connection, false);
    connection->HandleHeartbeatConnectionLost();
}

