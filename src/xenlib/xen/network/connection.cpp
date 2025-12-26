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
#include "connecttask.h"
#include "../session.h"
#include "../../xencache.h"
#include <QtCore/QMutex>
#include <QtCore/QDebug>
#include <QtCore/QDateTime>
#include <QtCore/QEventLoop>
#include <QtCore/QThread>
#include <QtCore/QTimer>

using namespace XenAPI;

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
        Session* session = nullptr;

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

        // C#-style connection flow scaffolding
        ConnectTask* connectTask = nullptr;
        bool saveDisconnected = false;
        bool expectPasswordIsCorrect = true;
        bool suppressErrors = false;
        bool preventResettingPasswordPrompt = false;
        bool fromDialog = false;
};

XenConnection::XenConnection(QObject* parent) : QObject(parent), d(new Private)
{
    // Each connection owns its own cache (matching C# architecture)
    this->d->cache = new XenCache(this);
}

XenConnection::~XenConnection()
{
    if (this->d->connectTask)
        this->EndConnect(false, true);
    this->Disconnect();
    delete this->d->cache;
    delete this->d;
}

bool XenConnection::ConnectToHost(const QString& host, int port, const QString& username, const QString& password)
{
    // qDebug() << "XenConnection: Connecting to" << host << ":" << port;

    // Disconnect any existing connection
    if (this->IsConnected())
        this->Disconnect();

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

void XenConnection::Disconnect()
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

bool XenConnection::IsConnected() const
{
    return this->d->connected;
}

QString XenConnection::GetHostname() const
{
    return this->d->host;
}

int XenConnection::GetPort() const
{
    return this->d->port;
}

QString XenConnection::GetUsername() const
{
    return this->d->username;
}

QString XenConnection::GetPassword() const
{
    return this->d->password;
}

QString XenConnection::GetSessionId() const
{
    return this->d->sessionId;
}

Session* XenConnection::GetNewSession(const QString& hostname,
                                         int port,
                                         const QString& username,
                                         const QString& password,
                                         bool isElevated,
                                         const PasswordPrompt& promptForNewPassword,
                                         QString* outError,
                                         QString* redirectHostname)
{
    static const int kMaxAttempts = 3;
    static const int kDelayMs = 250;

    QString currentUsername = username;
    QString currentPassword = password;

    for (int attempt = 0; attempt < kMaxAttempts; ++attempt)
    {
        XenConnection* newConn = new XenConnection(this);
        newConn->setCertificateManager(this->d->certManager);

        if (!newConn->ConnectToHost(hostname, port, currentUsername, currentPassword))
        {
            if (outError)
                *outError = "Failed to initiate connection";
            delete newConn;
            QThread::msleep(kDelayMs);
            continue;
        }

        if (!newConn->IsConnected())
        {
            QEventLoop loop;
            QTimer timer;
            timer.setSingleShot(true);
            QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
            QObject::connect(newConn, &XenConnection::connected, &loop, &QEventLoop::quit);
            QObject::connect(newConn, &XenConnection::error, &loop, &QEventLoop::quit);
            timer.start(10000);
            loop.exec();
        }

        if (!newConn->IsConnected())
        {
            if (outError)
                *outError = "Failed to establish transport connection";
            delete newConn;
            QThread::msleep(kDelayMs);
            continue;
        }

        Session* session = new Session(newConn, newConn);
        newConn->SetSession(session);

        QString redirectHost;
        QObject::connect(session, &Session::needsRedirectToMaster,
                         session, [&redirectHost](const QString& host) {
                             redirectHost = host;
                         });

        if (session->Login(currentUsername, currentPassword))
        {
            return session;
        }

        if (!redirectHost.isEmpty())
        {
            if (redirectHostname)
                *redirectHostname = redirectHost;
            if (outError)
                *outError = QString("HOST_IS_SLAVE:%1").arg(redirectHost);
            delete newConn;
            return nullptr;
        }

        const QString lastError = session->getLastError();
        if (!isElevated && promptForNewPassword)
        {
            QString newPassword;
            if (promptForNewPassword(currentPassword, &newPassword))
            {
                currentPassword = newPassword;
                if (!newPassword.isEmpty())
                    this->d->password = newPassword;
                attempt = -1;
                delete newConn;
                continue;
            }

            if (outError)
                *outError = "Authentication cancelled";
            delete newConn;
            return nullptr;
        }

        if (outError)
            *outError = lastError.isEmpty() ? "Authentication failed" : lastError;

        delete newConn;
        QThread::msleep(kDelayMs);
    }

    return nullptr;
}

void XenConnection::BeginConnect(bool initiateCoordinatorSearch,
                                 const PasswordPrompt& promptForNewPassword)
{
    if (this->d->connectTask)
        return;

    if (initiateCoordinatorSearch)
    {
        this->d->findingNewCoordinator = true;
        this->d->findingNewCoordinatorStartedAt = QDateTime::currentDateTimeUtc();
    }

    this->d->connectTask = new ConnectTask(this->d->host, this->d->port);

    QString error;
    QString redirectHost;
    Session* session = this->GetNewSession(this->d->host,
                                           this->d->port,
                                           this->d->username,
                                           this->d->password,
                                           false,
                                           promptForNewPassword,
                                           &error,
                                           &redirectHost);

    if (session)
    {
        this->d->connectTask->Session = session;
        this->d->connectTask->Connected = true;
        this->d->expectPasswordIsCorrect = true;
        emit this->connectionResult(true, QString());
    } else
    {
        if (!error.isEmpty())
            emit this->connectionResult(false, error);
        else
            emit this->connectionResult(false, "Connection failed");
    }

    emit this->connectionStateChanged();
}

void XenConnection::EndConnect(bool clearCache, bool exiting)
{
    Q_UNUSED(exiting);

    if (!this->d->connectTask)
        return;

    emit this->beforeConnectionEnd();

    this->d->connectTask->Cancelled = true;
    Session* session = this->d->connectTask->Session;
    if (session)
    {
        session->Logout();
        QObject* owner = session->parent();
        if (owner)
            delete owner;
        else
            delete session;
    }

    // TODO: mirror C# ClearCache path (ClearingCache event, event queue flush, cache clear on background thread).
    if (clearCache && this->d->cache)
    {
        this->d->cache->Clear();
    }

    // TODO: reset password prompt handler when PreventResettingPasswordPrompt is false (C#).

    delete this->d->connectTask;
    this->d->connectTask = nullptr;

    emit this->connectionClosed();
    emit this->connectionStateChanged();
}

void XenConnection::Interrupt()
{
    if (!this->d->connectTask)
        return;

    this->d->connectTask->Cancelled = true;
    // TODO: port HandleConnectionLost behavior (cancel actions, cache clear, reconnection scheduling).
    emit this->connectionLost();
    emit this->connectionStateChanged();
}

ConnectTask* XenConnection::GetConnectTask() const
{
    return this->d->connectTask;
}

bool XenConnection::InProgress() const
{
    return this->d->connectTask != nullptr;
}

bool XenConnection::IsConnectedNewFlow() const
{
    return this->d->connectTask && this->d->connectTask->Connected;
}

Session* XenConnection::GetConnectSession() const
{
    return this->d->connectTask ? this->d->connectTask->Session : nullptr;
}

bool XenConnection::getSaveDisconnected() const
{
    return this->d->saveDisconnected;
}

void XenConnection::setSaveDisconnected(bool save)
{
    this->d->saveDisconnected = save;
}

bool XenConnection::getExpectPasswordIsCorrect() const
{
    return this->d->expectPasswordIsCorrect;
}

void XenConnection::setExpectPasswordIsCorrect(bool expect)
{
    this->d->expectPasswordIsCorrect = expect;
}

bool XenConnection::getSuppressErrors() const
{
    return this->d->suppressErrors;
}

void XenConnection::setSuppressErrors(bool suppress)
{
    this->d->suppressErrors = suppress;
}

bool XenConnection::getPreventResettingPasswordPrompt() const
{
    return this->d->preventResettingPasswordPrompt;
}

void XenConnection::setPreventResettingPasswordPrompt(bool prevent)
{
    this->d->preventResettingPasswordPrompt = prevent;
}

bool XenConnection::getFromDialog() const
{
    return this->d->fromDialog;
}

void XenConnection::setFromDialog(bool fromDialog)
{
    this->d->fromDialog = fromDialog;
}

QByteArray XenConnection::SendRequest(const QByteArray& data)
{
    if (!this->IsConnected() || !this->d->worker)
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

int XenConnection::SendRequestAsync(const QByteArray& data)
{
    if (!this->IsConnected() || !this->d->worker)
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
void XenConnection::SetSession(Session* session)
{
    this->d->session = session;
}

Session* XenConnection::GetSession() const
{
    return this->d->session;
}

// Pool member tracking methods
void XenConnection::SetPoolMembers(const QStringList& members)
{
    QMutexLocker locker(&this->d->poolMembersMutex);
    this->d->poolMembers = members;
    this->d->poolMemberIndex = 0;
}

QStringList XenConnection::GetPoolMembers() const
{
    QMutexLocker locker(&this->d->poolMembersMutex);
    return this->d->poolMembers;
}

int XenConnection::GetCurrentPoolMemberIndex() const
{
    QMutexLocker locker(&this->d->poolMembersMutex);
    return this->d->poolMemberIndex;
}

void XenConnection::SetCurrentPoolMemberIndex(int index)
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
// TODO this functions is called all over the place and we verify if it didn't return NULL, this can never return NULL so we need to cleanup that code and remove all those redundant checks
XenCache* XenConnection::GetCache() const
{
    return this->d->cache;
}
