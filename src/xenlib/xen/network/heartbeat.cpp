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

#include "heartbeat.h"
#include "connection.h"
#include "../session.h"
#include "../xenapi/xenapi_Pool.h"
#include "../xenapi/xenapi_Host.h"
#include <QtCore/QDebug>
#include <QtCore/QMutexLocker>

// Define static const members
const int XenHeartbeat::HEARTBEAT_INTERVAL_MS;
const int XenHeartbeat::MIN_CONNECTION_TIMEOUT_MS;

XenHeartbeat::XenHeartbeat(XenConnection* connection, int connectionTimeout, QObject* parent)
    : QObject(parent), m_connection(connection), m_session(nullptr), m_heartbeatTimer(new QTimer(this)), m_connectionTimeout(qMax(connectionTimeout, MIN_CONNECTION_TIMEOUT_MS)), m_running(false), m_retrying(false)
{
    // Setup heartbeat timer
    this->m_heartbeatTimer->setInterval(HEARTBEAT_INTERVAL_MS);
    this->m_heartbeatTimer->setSingleShot(false);
    connect(this->m_heartbeatTimer, &QTimer::timeout, this, &XenHeartbeat::onHeartbeatTimer);

    qDebug() << "Heartbeat created for connection" << connection->GetHostname()
             << "with timeout" << this->m_connectionTimeout << "ms";
}

XenHeartbeat::~XenHeartbeat()
{
    this->stop();
    this->dropSession();

    qDebug() << "Heartbeat destroyed for connection" << (this->m_connection ? this->m_connection->GetHostname() : "null");
}

void XenHeartbeat::start()
{
    QMutexLocker locker(&this->m_mutex);

    if (this->m_running)
    {
        qDebug() << "Heartbeat already running for" << this->m_connection->GetHostname();
        return;
    }

    if (!this->m_connection || !this->m_connection->IsConnected())
    {
        qWarning() << "Cannot start heartbeat: connection not available or not connected";
        return;
    }

    this->m_running = true;
    this->m_retrying = false;
    this->m_heartbeatTimer->start();

    qDebug() << "Heartbeat started for connection" << this->m_connection->GetHostname();
}

void XenHeartbeat::stop()
{
    QMutexLocker locker(&this->m_mutex);

    if (!this->m_running)
    {
        return;
    }

    this->m_running = false;
    this->m_heartbeatTimer->stop();

    qDebug() << "Heartbeat stopped for connection" << (this->m_connection ? this->m_connection->GetHostname() : "null");
}

bool XenHeartbeat::isRunning() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_running;
}

QDateTime XenHeartbeat::getServerTimeOffset() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_serverTimeOffset;
}

void XenHeartbeat::onHeartbeatTimer()
{
    // Perform heartbeat on timer thread - this is safe for network operations
    this->performHeartbeat();
}

void XenHeartbeat::performHeartbeat()
{
    if (!this->m_connection || !this->m_connection->IsConnected())
    {
        qDebug() << "Skipping heartbeat: connection not available";
        return;
    }

    try
    {
        // Create session if needed
        if (!this->m_session)
        {
            if (!this->createSession())
            {
                this->handleConnectionLoss();
                return;
            }
        }

        // Perform the actual heartbeat by getting server time
        this->getServerTime();

        // Reset retry flag on successful heartbeat
        if (this->m_retrying)
        {
            qDebug() << "Heartbeat for" << this->m_connection->GetHostname() << "has recovered";
            this->m_retrying = false;
        }
    } catch (...)
    {
        qWarning() << "Exception during heartbeat for" << this->m_connection->GetHostname();
        this->handleConnectionLoss();
    }
}

bool XenHeartbeat::createSession()
{
    if (!this->m_connection || !this->m_connection->IsConnected())
    {
        return false;
    }

    qDebug() << "Creating heartbeat session for" << this->m_connection->GetHostname();

    // Get the main session from the connection
    XenSession* mainSession = this->m_connection->GetSession();
    if (!mainSession || !mainSession->IsLoggedIn())
    {
        qWarning() << "Cannot create heartbeat session: main session not logged in";
        return false;
    }

    // Duplicate the session for heartbeat (separate TCP stream)
    this->m_session = XenSession::DuplicateSession(mainSession, this);
    if (!this->m_session)
    {
        qWarning() << "Failed to duplicate session for heartbeat";
        return false;
    }

    // Get pool master host reference for heartbeat target
    try
    {
        QVariant poolsVar = XenAPI::Pool::get_all(this->m_session);
        QVariantList pools = poolsVar.toList();
        if (!pools.isEmpty())
        {
            QString poolRef = pools.first().toString();
            QVariantMap pool = XenAPI::Pool::get_record(this->m_session, poolRef);
            this->m_masterHostRef = pool.value("master").toString();
            qDebug() << "Heartbeat targeting pool master:" << this->m_masterHostRef;
        }
    }
    catch (const std::exception& ex)
    {
        qWarning() << "Failed to resolve pool master for heartbeat:" << ex.what();
    }

    return true;
}

void XenHeartbeat::getServerTime()
{
    if (!this->m_session || !this->m_connection)
    {
        return;
    }

    // Call Host.get_servertime on the pool master
    if (this->m_masterHostRef.isEmpty())
    {
        qWarning() << "No master host ref for heartbeat";
        return;
    }

    QVariant serverTimeVar;
    try
    {
        serverTimeVar = XenAPI::Host::get_servertime(this->m_session, this->m_masterHostRef);
    }
    catch (const std::exception& ex)
    {
        qWarning() << "Failed to get server time from" << this->m_masterHostRef << ":" << ex.what();
        this->handleConnectionLoss();
        return;
    }
    if (serverTimeVar.isNull())
    {
        qWarning() << "Failed to get server time from" << this->m_masterHostRef;
        this->handleConnectionLoss();
        return;
    }

    // Parse server time (XenAPI returns an ISO 8601 string; older servers may return a Unix timestamp)
    qDebug() << "Heartbeat: raw server time value"
             << serverTimeVar
             << "type" << serverTimeVar.typeName();

    QDateTime serverTime;
    bool parsed = false;

    // Try ISO 8601 first (native XenAPI format)
    const QString serverTimeStr = serverTimeVar.toString();
    serverTime = QDateTime::fromString(serverTimeStr, Qt::ISODate);
    if (!serverTime.isValid())
    {
        // Some servers use compact Zulu format: 20250204T120102Z
        serverTime = QDateTime::fromString(serverTimeStr, "yyyyMMddTHHmmssZ");
    }
    if (!serverTime.isValid())
    {
        // Servers can also return ISO without separators: 20251128T11:57:56Z
        serverTime = QDateTime::fromString(serverTimeStr, "yyyyMMdd'T'HH:mm:ss'Z'");
    }
    if (serverTime.isValid())
    {
        serverTime = serverTime.toUTC();
        parsed = true;
    }

    // Fall back to Unix timestamp (number of seconds since epoch)
    if (!parsed)
    {
        bool ok = false;
        qint64 serverTimestamp = serverTimeVar.toLongLong(&ok);
        if (ok)
        {
            serverTime = QDateTime::fromSecsSinceEpoch(serverTimestamp, Qt::UTC);
            parsed = true;
        }
    }

    if (!parsed)
    {
        qWarning() << "Failed to parse server time";
        this->handleConnectionLoss();
        return;
    }

    QDateTime localTime = QDateTime::currentDateTimeUtc();

    // Calculate offset: server time + offset = local time
    qint64 offsetSeconds = serverTime.secsTo(localTime);
    this->m_serverTimeOffset = QDateTime::fromSecsSinceEpoch(offsetSeconds, Qt::UTC);

    emit serverTimeUpdated(serverTime, localTime);

    qDebug() << "Heartbeat successful for" << this->m_connection->GetHostname()
             << "offset:" << offsetSeconds << "seconds";
}

void XenHeartbeat::handleConnectionLoss()
{
    if (this->m_retrying)
    {
        // Second failure - connection is likely dead
        qDebug() << "Heartbeat for" << this->m_connection->GetHostname()
                 << "failed for second time - closing connection";

        this->dropSession();
        emit connectionLost();
    } else
    {
        // First failure - give it another chance
        qDebug() << "Heartbeat for" << this->m_connection->GetHostname()
                 << "failed - will retry";
        this->m_retrying = true;
        this->dropSession(); // Drop session to create a fresh one next time
    }
}

void XenHeartbeat::dropSession()
{
    if (this->m_session)
    {
        // In a full implementation, this would logout the session on a background thread
        // to avoid blocking if the coordinator has died
        qDebug() << "Dropping heartbeat session for" << (this->m_connection ? this->m_connection->GetHostname() : "null");

        // this->m_session->logoutAsync(); // Would be implemented
        this->m_session = nullptr;
    }
}
