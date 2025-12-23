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

#include "connection.h"
#include "connectionworker.h"
#include "certificatemanager.h"
#include "../session.h"
#include "../../xencache.h"
#include <QtCore/QMutex>
#include <QtCore/QDebug>
#include <QtCore/QDateTime>

class XenConnection::Private
{
    public:
        bool connected = false;
        QString host;
        int port = 443;
        QString username;
        QString password;
        QString sessionId;

        Xen::ConnectionWorker* worker = nullptr;
        XenCertificateManager* certManager = nullptr;

        // Session association
        XenSession* session = nullptr;

        // Cache (each connection owns its own cache, matching C# architecture)
        XenCache* cache = nullptr;

        // Pool member tracking for failover
        QStringList poolMembers;
        int poolMemberIndex = 0;
        mutable QMutex poolMembersMutex;

        // Coordinator tracking for failover
        QString lastCoordinatorHostname;
        bool findingNewCoordinator = false;
        QDateTime findingNewCoordinatorStartedAt;

        // Failover state
        bool expectDisruption = false;
        bool coordinatorMayChange = false;
};

XenConnection::XenConnection(QObject* parent) : QObject(parent), d(new Private)
{
    // Each connection owns its own cache (matching C# architecture)
    this->d->cache = new XenCache(this);
}

XenConnection::~XenConnection()
{
    this->disconnect();
    delete this->d->cache;
    delete this->d;
}

bool XenConnection::connectToHost(const QString& host, int port, const QString& username, const QString& password)
{
    // qDebug() << "XenConnection: Connecting to" << host << ":" << port;

    // Disconnect any existing connection
    if (this->isConnected())
        this->disconnect();

    this->d->host = host;
    this->d->port = port;
    this->d->username = username;
    this->d->password = password;

    // Store credentials for later login
    this->d->username = username;
    this->d->password = password;

    // Create worker thread with our certificate manager (no credentials - login happens separately)
    this->d->worker = new Xen::ConnectionWorker(host, port, this->d->certManager, this);

    // Connect worker signals
    connect(this->d->worker, &Xen::ConnectionWorker::connectionProgress, this, &XenConnection::onWorkerProgress);
    connect(this->d->worker, &Xen::ConnectionWorker::connectionEstablished, this, &XenConnection::onWorkerEstablished);
    connect(this->d->worker, &Xen::ConnectionWorker::connectionFailed, this, &XenConnection::onWorkerFailed);
    connect(this->d->worker, &Xen::ConnectionWorker::cacheDataReceived, this, &XenConnection::onWorkerCacheData);
    connect(this->d->worker, &Xen::ConnectionWorker::workerFinished, this, &XenConnection::onWorkerFinished);
    connect(this->d->worker, &Xen::ConnectionWorker::apiResponse, this, &XenConnection::onWorkerApiResponse);

    // Start worker thread
    this->d->worker->start();

    return true;
}

void XenConnection::disconnect()
{
    // qDebug() << "XenConnection: Disconnecting";

    // Stop worker thread
    if (this->d->worker)
    {
        this->d->worker->requestStop();
        this->d->worker->wait(5000); // Wait up to 5 seconds
        if (this->d->worker)
        {
            this->d->worker->deleteLater();
            this->d->worker = nullptr;
        }
    }

    // Update state
    if (this->d->connected)
    {
        this->d->connected = false;
        this->d->sessionId.clear();
        emit this->disconnected();
    }
}

bool XenConnection::isConnected() const
{
    return this->d->connected;
}

QString XenConnection::getHostname() const
{
    return this->d->host;
}

int XenConnection::getPort() const
{
    return this->d->port;
}

QString XenConnection::getSessionId() const
{
    return this->d->sessionId;
}

QByteArray XenConnection::sendRequest(const QByteArray& data)
{
    if (!this->isConnected() || !this->d->worker)
    {
        qWarning() << "XenConnection::sendRequest: Not connected or no worker";
        return QByteArray();
    }

    // Queue request to worker thread (emitSignal=false for blocking calls)
    // This prevents spurious "Unknown request ID" warnings for sync calls like EventPoller
    int requestId = this->d->worker->queueRequest(data, false);

    // qDebug() << "Created sync request with ID: " << requestId;

    // Wait for response (blocking)
    // Use a 60s wait to accommodate long-poll calls like event.from (server timeout is 30s)
    QByteArray response = this->d->worker->waitForResponse(requestId, 60000);

    return response;
}

int XenConnection::sendRequestAsync(const QByteArray& data)
{
    if (!this->isConnected() || !this->d->worker)
    {
        qWarning() << "XenConnection::sendRequestAsync: Not connected or no worker";
        return -1;
    }

    // Queue request to worker thread and return immediately (non-blocking)
    int requestId = this->d->worker->queueRequest(data);

    // Response will be delivered via apiResponse signal
    return requestId;
}

void XenConnection::setCertificateManager(XenCertificateManager* certManager)
{
    this->d->certManager = certManager;
}

// Worker signal handlers
void XenConnection::onWorkerEstablished()
{
    // qDebug() << "XenConnection: Worker established TCP/SSL connection";
    this->d->connected = true;
    // Note: sessionId will be set after XenSession::login() succeeds
    emit this->connected();
}

void XenConnection::onWorkerFailed(const QString& error)
{
    qWarning() << "XenConnection: Worker failed:" << error;
    this->d->connected = false;
    this->d->sessionId.clear();
    emit this->error(error);
}

void XenConnection::onWorkerFinished()
{
    // qDebug() << "XenConnection: Worker finished";
    if (this->d->connected)
    {
        this->d->connected = false;
        this->d->sessionId.clear();
        emit this->disconnected();
    }
}

void XenConnection::onWorkerProgress(const QString& message)
{
    emit this->progressUpdate(message);
}

void XenConnection::onWorkerCacheData(const QByteArray& data)
{
    emit this->cacheDataReceived(data);
}

void XenConnection::onWorkerApiResponse(int requestId, const QByteArray& response)
{
    // Forward the worker's apiResponse signal to our own apiResponse signal
    emit this->apiResponse(requestId, response);
}

// Session association methods
void XenConnection::setSession(XenSession* session)
{
    this->d->session = session;
}

XenSession* XenConnection::getSession() const
{
    return this->d->session;
}

// Pool member tracking methods
void XenConnection::setPoolMembers(const QStringList& members)
{
    QMutexLocker locker(&this->d->poolMembersMutex);
    this->d->poolMembers = members;
    this->d->poolMemberIndex = 0;
}

QStringList XenConnection::getPoolMembers() const
{
    QMutexLocker locker(&this->d->poolMembersMutex);
    return this->d->poolMembers;
}

int XenConnection::getCurrentPoolMemberIndex() const
{
    QMutexLocker locker(&this->d->poolMembersMutex);
    return this->d->poolMemberIndex;
}

void XenConnection::setCurrentPoolMemberIndex(int index)
{
    QMutexLocker locker(&this->d->poolMembersMutex);
    this->d->poolMemberIndex = index;
}

bool XenConnection::hasMorePoolMembers() const
{
    QMutexLocker locker(&this->d->poolMembersMutex);
    return this->d->poolMemberIndex < this->d->poolMembers.count();
}

QString XenConnection::getNextPoolMember()
{
    QMutexLocker locker(&this->d->poolMembersMutex);
    if (this->d->poolMemberIndex < this->d->poolMembers.count())
    {
        QString member = this->d->poolMembers[this->d->poolMemberIndex];
        this->d->poolMemberIndex++;
        return member;
    }
    return QString();
}

void XenConnection::resetPoolMemberIndex()
{
    QMutexLocker locker(&this->d->poolMembersMutex);
    this->d->poolMemberIndex = 0;
}

// Coordinator tracking methods
QString XenConnection::getLastCoordinatorHostname() const
{
    return this->d->lastCoordinatorHostname;
}

void XenConnection::setLastCoordinatorHostname(const QString& hostname)
{
    this->d->lastCoordinatorHostname = hostname;
}

bool XenConnection::isFindingNewCoordinator() const
{
    return this->d->findingNewCoordinator;
}

void XenConnection::setFindingNewCoordinator(bool finding)
{
    this->d->findingNewCoordinator = finding;
}

QDateTime XenConnection::getFindingNewCoordinatorStartedAt() const
{
    return this->d->findingNewCoordinatorStartedAt;
}

void XenConnection::setFindingNewCoordinatorStartedAt(const QDateTime& time)
{
    this->d->findingNewCoordinatorStartedAt = time;
}

// Failover state methods
bool XenConnection::getExpectDisruption() const
{
    return this->d->expectDisruption;
}

void XenConnection::setExpectDisruption(bool expect)
{
    this->d->expectDisruption = expect;
}

bool XenConnection::getCoordinatorMayChange() const
{
    return this->d->coordinatorMayChange;
}

void XenConnection::setCoordinatorMayChange(bool mayChange)
{
    this->d->coordinatorMayChange = mayChange;
}

// Cache access (each connection has its own cache)
XenCache* XenConnection::getCache() const
{
    return this->d->cache;
}
