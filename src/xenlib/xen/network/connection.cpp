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
#include "../api.h"
#include "../eventpoller.h"
#include "../failure.h"
#include "../../utils/misc.h"
#include "../session.h"
#include "../../xencache.h"
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QQueue>
#include <QtCore/QPointer>
#include <QtCore/QDebug>
#include <QtCore/QDateTime>
#include <QtCore/QEventLoop>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtCore/QStringList>

using namespace XenAPI;

namespace
{
    const int kReconnectHostTimeoutMs = 120 * 1000;
    const int kReconnectShortTimeoutMs = 5 * 1000;
    const int kSearchNewCoordinatorTimeoutMs = 60 * 1000;
    const int kSearchNextSupporterTimeoutMs = 15 * 1000;
    const int kSearchNewCoordinatorStopAfterMs = 6 * 60 * 1000;

    QString valueForKeys(const QVariantMap& map, std::initializer_list<const char*> keys)
    {
        for (const char* key : keys)
        {
            const QString value = map.value(QString::fromLatin1(key)).toString();
            if (!value.isEmpty())
                return value;
        }
        return QString();
    }
}

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
        QString lastConnectionFullName;
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
        bool cacheIsPopulated = false;
        PasswordPrompt promptForNewPassword;
        QStringList lastFailureDescription;

        QThread* connectThread = nullptr;
        QThread* eventPollerThread = nullptr;
        EventPoller* eventPoller = nullptr;
        QString eventToken;

        QQueue<QVariantMap> eventQueue;
        QMutex eventQueueMutex;
        QTimer* cacheUpdateTimer = nullptr;
        bool cacheUpdaterRunning = false;
        bool updatesWaiting = false;

        QTimer* reconnectionTimer = nullptr;
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
    return this->d->connected || this->IsConnectedNewFlow();
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

void XenConnection::SetHostname(const QString& hostname)
{
    this->d->host = hostname;
}

void XenConnection::SetPort(int port)
{
    this->d->port = port;
}

void XenConnection::SetUsername(const QString& username)
{
    this->d->username = username;
}

void XenConnection::SetPassword(const QString& password)
{
    this->d->password = password;
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
            this->d->lastFailureDescription.clear();
            return session;
        }

        if (!redirectHost.isEmpty())
        {
            if (redirectHostname)
                *redirectHostname = redirectHost;
            if (outError)
                *outError = QString("HOST_IS_SLAVE:%1").arg(redirectHost);
            this->d->lastFailureDescription = QStringList()
                << Failure::HOST_IS_SLAVE << redirectHost;
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

        this->d->lastFailureDescription = session->getLastErrorDescription();

        delete newConn;
        QThread::msleep(kDelayMs);
    }

    return nullptr;
}

void XenConnection::BeginConnect(bool initiateCoordinatorSearch,
                                 const PasswordPrompt& promptForNewPassword)
{
    if (this->d->connectTask || (this->d->connectThread && this->d->connectThread->isRunning()))
        return;

    if (initiateCoordinatorSearch)
    {
        this->d->findingNewCoordinator = true;
        this->d->findingNewCoordinatorStartedAt = QDateTime::currentDateTimeUtc();
    }

    this->d->connectTask = new ConnectTask(this->d->host, this->d->port);
    this->d->promptForNewPassword = promptForNewPassword;
    this->d->cacheIsPopulated = false;
    emit this->connectionMessageChanged(QString("Attempting to connect to %1...").arg(this->d->host));

    this->d->connectThread = QThread::create([this]() {
        this->connectWorkerThread();
    });

    connect(this->d->connectThread, &QThread::finished, this, [this]() {
        if (this->d->connectThread)
        {
            this->d->connectThread->deleteLater();
            this->d->connectThread = nullptr;
        }
    });

    this->d->connectThread->start();
}

void XenConnection::EndConnect(bool clearCache, bool exiting)
{
    Q_UNUSED(exiting);
    ConnectTask* task = this->d->connectTask;
    this->d->connectTask = nullptr;

    emit this->beforeConnectionEnd();

    const bool allowBlocking = QThread::currentThread() != this->thread();
    const Qt::ConnectionType stopConnectionType = allowBlocking
        ? Qt::BlockingQueuedConnection
        : Qt::QueuedConnection;

    if (task)
    {
        task->Cancelled = true;
        Session* session = task->Session;
        task->Session = nullptr;
        if (session)
        {
            if (exiting)
            {
                session->LogoutWithoutDisconnect();
                session->deleteLater();
            } else
            {
                QPointer<Session> sessionPtr(session);
                QThread* logoutThread = QThread::create([sessionPtr]() {
                    if (!sessionPtr)
                        return;
                    sessionPtr->LogoutWithoutDisconnect();
                    QMetaObject::invokeMethod(sessionPtr, "deleteLater", Qt::QueuedConnection);
                });
                connect(logoutThread, &QThread::finished, logoutThread, &QObject::deleteLater);
                logoutThread->start();
            }
        }
    }

    if (this->d->eventPoller)
    {
        QMetaObject::invokeMethod(this->d->eventPoller, [this]() {
            this->d->eventPoller->stop();
            this->d->eventPoller->reset();
        }, stopConnectionType);
    }

    if (this->d->eventPollerThread && this->d->eventPollerThread->isRunning())
    {
        this->d->eventPollerThread->quit();
        if (allowBlocking)
            this->d->eventPollerThread->wait(5000);
    }

    if (this->d->eventPoller)
    {
        this->d->eventPoller->deleteLater();
        this->d->eventPoller = nullptr;
    }

    if (this->d->eventPollerThread)
    {
        this->d->eventPollerThread->deleteLater();
        this->d->eventPollerThread = nullptr;
    }

    if (this->d->connectThread && this->d->connectThread->isRunning())
    {
        this->d->connectThread->requestInterruption();
        this->d->connectThread->quit();
        if (allowBlocking)
            this->d->connectThread->wait(5000);
    }

    if (this->d->reconnectionTimer)
    {
        this->d->reconnectionTimer->stop();
    }

    QString poolName;
    bool haEnabled = false;
    QString coordinatorAddress;
    this->updatePoolMembersFromCache(&poolName, &haEnabled, &coordinatorAddress);

    if (clearCache)
    {
        emit this->clearingCache();
        {
            QMutexLocker locker(&this->d->eventQueueMutex);
            this->d->eventQueue.clear();
            this->d->cacheUpdaterRunning = false;
            this->d->updatesWaiting = false;
        }
        if (this->d->cacheUpdateTimer)
            this->d->cacheUpdateTimer->stop();

        if (this->d->cache)
        {
            QMetaObject::invokeMethod(this->d->cache, [cache = this->d->cache]() {
                cache->Clear();
            }, Qt::QueuedConnection);
        }
    }

    this->d->cacheIsPopulated = false;
    if (!this->d->preventResettingPasswordPrompt)
        this->d->promptForNewPassword = PasswordPrompt();

    if (task)
        delete task;

    this->d->connected = false;

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

void XenConnection::onCacheUpdateTimer()
{
    QList<QVariantMap> events;
    {
        QMutexLocker locker(&this->d->eventQueueMutex);
        if (this->d->cacheUpdaterRunning)
        {
            this->d->updatesWaiting = true;
            return;
        }

        this->d->cacheUpdaterRunning = true;
        this->d->updatesWaiting = false;
        while (!this->d->eventQueue.isEmpty())
            events.append(this->d->eventQueue.dequeue());
    }

    for (const QVariantMap& eventData : events)
    {
        QString eventClass = valueForKeys(eventData, {"class_", "class"});
        QString operation = eventData.value("operation").toString();
        QString ref = valueForKeys(eventData, {"opaqueRef", "ref"});

        if (eventClass.isEmpty() || operation.isEmpty() || ref.isEmpty())
            continue;

        QString cacheType = eventClass.toLower();

        if (operation == "del")
        {
            if (this->d->cache)
                this->d->cache->Remove(cacheType, ref);
        } else if (operation == "add" || operation == "mod")
        {
            QVariantMap snapshot = eventData.value("snapshot").toMap();
            if (!snapshot.isEmpty())
            {
                snapshot["ref"] = ref;
                snapshot["opaqueRef"] = ref;
                if (this->d->cache)
                    this->d->cache->Update(cacheType, ref, snapshot);
            } else
            {
                // TODO: fetch full record when snapshot is missing (mirrors XenLib::requestObjectData).
            }
        }
    }

    if (!this->d->cacheIsPopulated)
    {
        this->d->cacheIsPopulated = true;
        emit this->cachePopulated();
    }

    {
        QMutexLocker locker(&this->d->eventQueueMutex);
        this->d->cacheUpdaterRunning = false;
        if (this->d->updatesWaiting)
        {
            this->d->updatesWaiting = false;
            if (this->d->cacheUpdateTimer)
                this->d->cacheUpdateTimer->start(50);
        }
    }
}

void XenConnection::onEventPollerEventReceived(const QVariantMap& eventData)
{
    {
        QMutexLocker locker(&this->d->eventQueueMutex);
        this->d->eventQueue.enqueue(eventData);
    }

    if (!this->d->cacheUpdateTimer)
    {
        this->d->cacheUpdateTimer = new QTimer(this);
        this->d->cacheUpdateTimer->setSingleShot(true);
        connect(this->d->cacheUpdateTimer, &QTimer::timeout, this, &XenConnection::onCacheUpdateTimer);
    }

    if (!this->d->cacheUpdaterRunning)
        this->d->cacheUpdateTimer->start(50);
    else
        this->d->updatesWaiting = true;
}

void XenConnection::onEventPollerCachePopulated()
{
    if (!this->d->cacheIsPopulated)
    {
        this->d->cacheIsPopulated = true;
        emit this->cachePopulated();
    }
}

void XenConnection::onEventPollerConnectionLost()
{
    this->handleConnectionLostNewFlow();
}

void XenConnection::handleConnectionLostNewFlow()
{
    if (!this->d->connectTask)
        return;

    this->d->connectTask->Connected = false;

    QString poolName;
    bool haEnabled = false;
    QString coordinatorAddress;
    this->updatePoolMembersFromCache(&poolName, &haEnabled, &coordinatorAddress);

    const QString hostnameWithPort = QString("%1:%2").arg(this->d->host).arg(this->d->port);
    this->d->lastCoordinatorHostname = this->d->host;
    if (poolName.isEmpty())
        this->d->lastConnectionFullName = hostnameWithPort;
    else
        this->d->lastConnectionFullName = QString("'%1' (%2)").arg(poolName, hostnameWithPort);

    const QStringList members = this->GetPoolMembers();
    this->EndConnect(true, false);

    if (!members.isEmpty() && coordinatorAddress == members.first() && members.size() > 1)
        this->SetCurrentPoolMemberIndex(1);

    const bool searchCoordinator = (this->d->coordinatorMayChange || haEnabled) &&
                                   members.count() > 1;
    if (searchCoordinator)
    {
        this->d->findingNewCoordinator = true;
        this->d->findingNewCoordinatorStartedAt = QDateTime::currentDateTimeUtc();
        this->startReconnectCoordinatorTimer(kSearchNewCoordinatorTimeoutMs);
    } else
    {
        this->d->findingNewCoordinator = false;
        this->startReconnectSingleHostTimer();
    }

    emit this->connectionLost();
}

int XenConnection::reconnectHostTimeoutMs() const
{
    if (this->d->eventPollerThread && !this->d->eventPollerThread->isRunning())
        return kReconnectShortTimeoutMs;
    return kReconnectHostTimeoutMs;
}

void XenConnection::startReconnectSingleHostTimer()
{
    const int timeoutMs = this->reconnectHostTimeoutMs();
    if (!this->d->reconnectionTimer)
        this->d->reconnectionTimer = new QTimer(this);

    this->d->reconnectionTimer->setSingleShot(!this->d->expectDisruption);
    QObject::disconnect(this->d->reconnectionTimer, nullptr, nullptr, nullptr);
    connect(this->d->reconnectionTimer, &QTimer::timeout,
            this, &XenConnection::reconnectSingleHostTimer);

    const QString target = this->d->lastConnectionFullName.isEmpty()
        ? this->d->host
        : this->d->lastConnectionFullName;
    emit this->connectionMessageChanged(
        QString("Connection lost. Reconnecting to %1 in %2 seconds...")
            .arg(target)
            .arg(timeoutMs / 1000));

    this->d->reconnectionTimer->start(timeoutMs);
}

void XenConnection::startReconnectCoordinatorTimer(int timeoutMs)
{
    if (!this->d->reconnectionTimer)
        this->d->reconnectionTimer = new QTimer(this);

    this->d->reconnectionTimer->setSingleShot(true);
    QObject::disconnect(this->d->reconnectionTimer, nullptr, nullptr, nullptr);
    connect(this->d->reconnectionTimer, &QTimer::timeout,
            this, &XenConnection::reconnectCoordinatorTimer);

    const QString target = this->d->lastConnectionFullName.isEmpty()
        ? this->d->host
        : this->d->lastConnectionFullName;
    emit this->connectionMessageChanged(
        QString("Searching for pool coordinator for %1. Retrying in %2 seconds...")
            .arg(target)
            .arg(timeoutMs / 1000));

    this->d->reconnectionTimer->start(timeoutMs);
}

void XenConnection::reconnectSingleHostTimer()
{
    if (this->IsConnectedNewFlow() || this->InProgress())
        return;

    if (!this->d->expectDisruption && this->d->reconnectionTimer)
        this->d->reconnectionTimer->stop();

    emit this->connectionReconnecting();
    this->BeginConnect(false, this->d->promptForNewPassword);
}

void XenConnection::reconnectCoordinatorTimer()
{
    if (this->IsConnectedNewFlow() || this->InProgress())
        return;

    const QDateTime startedAt = this->d->findingNewCoordinatorStartedAt;
    if (startedAt.isValid())
    {
        const qint64 elapsedMs = startedAt.msecsTo(QDateTime::currentDateTimeUtc());
        if (!this->d->expectDisruption && elapsedMs > kSearchNewCoordinatorStopAfterMs)
        {
            this->d->findingNewCoordinator = false;
            if (!this->d->lastCoordinatorHostname.isEmpty())
                this->SetHostname(this->d->lastCoordinatorHostname);
            emit this->connectionReconnecting();
            this->BeginConnect(false, this->d->promptForNewPassword);
            return;
        }
    }

    if (this->poolMemberRemaining())
    {
        const QString nextMember = this->getNextPoolMember();
        if (!nextMember.isEmpty())
            this->SetHostname(nextMember);

        emit this->connectionMessageChanged(
            QString("Retrying pool member %1...").arg(this->d->host));

        emit this->connectionReconnecting();
        this->BeginConnect(false, this->d->promptForNewPassword);
        return;
    }

    this->resetPoolMemberIndex();
    if (this->poolMemberRemaining())
    {
        this->startReconnectCoordinatorTimer(kSearchNextSupporterTimeoutMs);
    } else
    {
        this->d->findingNewCoordinator = false;
    }
}

bool XenConnection::poolMemberRemaining() const
{
    QMutexLocker locker(&this->d->poolMembersMutex);
    return this->d->poolMemberIndex < this->d->poolMembers.count();
}

void XenConnection::updatePoolMembersFromCache(QString* poolName,
                                               bool* haEnabled,
                                               QString* coordinatorAddress)
{
    if (poolName)
        poolName->clear();
    if (haEnabled)
        *haEnabled = false;
    if (coordinatorAddress)
        coordinatorAddress->clear();

    if (!this->d->cache)
        return;

    QVariantMap poolData;
    const QList<QVariantMap> pools = this->d->cache->GetAllData("pool");
    if (!pools.isEmpty())
        poolData = pools.first();

    const QString resolvedPoolName = poolData.value("name_label").toString();
    if (poolName)
        *poolName = resolvedPoolName.isEmpty() ? poolData.value("name").toString() : resolvedPoolName;

    if (haEnabled)
        *haEnabled = poolData.value("ha_enabled", false).toBool();

    QVariant masterVar = poolData.value("master");
    QString masterRef;
    if (masterVar.type() == QVariant::List)
    {
        const QVariantList masterList = masterVar.toList();
        if (!masterList.isEmpty())
            masterRef = masterList.first().toString();
    } else
    {
        masterRef = masterVar.toString();
    }

    QString resolvedCoordinator;
    if (!masterRef.isEmpty())
    {
        const QVariantMap hostData = this->d->cache->ResolveObjectData("host", masterRef);
        resolvedCoordinator = hostData.value("address").toString();
    }

    if (coordinatorAddress)
        *coordinatorAddress = resolvedCoordinator;

    QStringList members;
    const QList<QVariantMap> hosts = this->d->cache->GetAllData("host");
    for (const QVariantMap& hostData : hosts)
    {
        const QString address = hostData.value("address").toString();
        if (!address.isEmpty())
            members.append(address);
    }

    if (!members.isEmpty() && !resolvedCoordinator.isEmpty())
    {
        members.removeAll(resolvedCoordinator);
        members.prepend(resolvedCoordinator);
    }

    if (!members.isEmpty())
        this->SetPoolMembers(members);
}

void XenConnection::connectWorkerThread()
{
    QString error;
    QString redirectHost;
    if (!this->d->connectTask || this->d->connectTask->Cancelled)
        return;

    Session* session = this->GetNewSession(this->d->host,
                                           this->d->port,
                                           this->d->username,
                                           this->d->password,
                                           false,
                                           this->d->promptForNewPassword,
                                           &error,
                                           &redirectHost);

    if (!session || !this->d->connectTask || this->d->connectTask->Cancelled)
    {
        const QString reason = !error.isEmpty() ? error : QString("Connection failed");
        emit this->connectionResult(false, reason);
        emit this->connectionStateChanged();
        return;
    }

    this->d->connectTask->Session = session;
    this->d->connectTask->Connected = true;
    this->d->expectPasswordIsCorrect = true;
    emit this->connectionMessageChanged(QString("Synchronizing with %1...").arg(this->d->host));

    XenRpcAPI api(session);
    QString token;

    // Populate cache using event.from
    if (this->d->cache)
        this->d->cache->Clear();

    // Preload roles (not delivered by event.from)
    try
    {
        qDebug() << "XenConnection: Preloading roles via role.get_all_records";
        QVariantList roleParams;
        roleParams.append(session->getSessionId());
        QByteArray roleRequest = api.buildJsonRpcCall("role.get_all_records", roleParams);
        QByteArray roleResponse = session->sendApiRequest(QString::fromUtf8(roleRequest));
        if (!roleResponse.isEmpty())
        {
            QVariant parsed = api.parseJsonRpcResponse(roleResponse);
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
                qDebug() << "XenConnection: Role records fetched:" << roles.size();
                for (auto it = roles.constBegin(); it != roles.constEnd(); ++it)
                {
                    QString roleRef = it.key();
                    QVariantMap roleRecord = it.value().toMap();
                    roleRecord["ref"] = roleRef;
                    roleRecord["opaqueRef"] = roleRef;
                    if (this->d->cache)
                        this->d->cache->Update("role", roleRef, roleRecord);
                }
            }
        }
    } catch (const std::exception& exn)
    {
        qWarning() << "XenLib::populateCache - Failed to fetch role records:" << exn.what();
    }

    qDebug() << "XenConnection: Calling event.from for initial cache population";
    QVariantMap eventBatch = api.eventFrom(QStringList() << "*", "", 30.0);
    if (eventBatch.contains("token"))
        token = eventBatch.value("token").toString();

    QVariantList events = eventBatch.value("events").toList();
    qDebug() << "XenConnection: event.from returned events:" << events.size();
    for (const QVariant& eventVar : events)
    {
        QVariantMap event = eventVar.toMap();
        const QString objectClass = valueForKeys(event, {"class_", "class"});
        const QString operation = event.value("operation").toString();
        const QString objectRef = valueForKeys(event, {"opaqueRef", "ref"});
        const QVariant snapshot = event.value("snapshot");

        if (objectClass.isEmpty() || objectRef.isEmpty())
            continue;

        if (objectClass == "session" || objectClass == "event" ||
            objectClass == "user" || objectClass == "secret")
        {
            continue;
        }

        if ((operation == "add" || operation == "mod") &&
            snapshot.isValid() && Misc::QVariantIsMap(snapshot))
        {
            QVariantMap objectData = snapshot.toMap();
            objectData["ref"] = objectRef;
            objectData["opaqueRef"] = objectRef;
            if (this->d->cache)
                this->d->cache->Update(objectClass.toLower(), objectRef, objectData);
        }
    }

    // Cache explicit console records
    try
    {
        qDebug() << "XenConnection: Preloading console.get_all_records";
        QVariantList consoleParams;
        consoleParams.append(session->getSessionId());
        QByteArray consoleRequest = api.buildJsonRpcCall("console.get_all_records", consoleParams);
        QByteArray consoleResponse = session->sendApiRequest(QString::fromUtf8(consoleRequest));
        if (!consoleResponse.isEmpty())
        {
            QVariant parsed = api.parseJsonRpcResponse(consoleResponse);
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
                qDebug() << "XenConnection: Console records fetched:" << consolesMap.size();
                for (auto it = consolesMap.constBegin(); it != consolesMap.constEnd(); ++it)
                {
                    QString consoleRef = it.key();
                    QVariantMap consoleData = it.value().toMap();
                    consoleData["ref"] = consoleRef;
                    consoleData["opaqueRef"] = consoleRef;
                    if (this->d->cache)
                        this->d->cache->Update("console", consoleRef, consoleData);
                }
            }
        }
    } catch (const std::exception& exn)
    {
        qWarning() << "XenLib::populateCache - Failed to fetch console records:" << exn.what();
    }

    this->d->cacheIsPopulated = true;
    qDebug() << "XenConnection: Cache populated, emitting cachePopulated";
    emit this->cachePopulated();

    if (!this->d->eventPollerThread)
    {
        this->d->eventPollerThread = new QThread();
        this->d->eventPollerThread->start();
    }

    if (!this->d->eventPoller)
    {
        this->d->eventPoller = new EventPoller();
        this->d->eventPoller->moveToThread(this->d->eventPollerThread);
        connect(this->d->eventPoller, &EventPoller::eventReceived, this, &XenConnection::onEventPollerEventReceived);
        connect(this->d->eventPoller, &EventPoller::cachePopulated, this, &XenConnection::onEventPollerCachePopulated);
        connect(this->d->eventPoller, &EventPoller::connectionLost, this, &XenConnection::onEventPollerConnectionLost);
        connect(this->d->eventPoller, &EventPoller::taskAdded, this, &XenConnection::taskAdded);
        connect(this->d->eventPoller, &EventPoller::taskModified, this, &XenConnection::taskModified);
        connect(this->d->eventPoller, &EventPoller::taskDeleted, this, &XenConnection::taskDeleted);
    }

    const QStringList classes = {
                                 "vm", "host", "pool", "sr", "vbd", "vdi", "vif",
                                 "network", "pbd", "pif", "task", "message", "console",
                                 "vm_guest_metrics", "host_metrics", "vm_metrics"
                                };

    QMetaObject::invokeMethod(this->d->eventPoller, [this, session, classes, token]()
    {
        this->d->eventPoller->reset();
        this->d->eventPoller->initialize(session);
        this->d->eventPoller->start(classes, token);
    }, Qt::QueuedConnection);

    emit this->connectionResult(true, QString());
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

QStringList XenConnection::GetLastFailureDescription() const
{
    return this->d->lastFailureDescription;
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
