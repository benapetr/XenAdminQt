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

#ifndef XEN_CONNECTION_H
#define XEN_CONNECTION_H

#include "../../xenlib_global.h"
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QDateTime>
#include <QtCore/QSharedPointer>
#include <QtCore/QVariantMap>
#include <QtCore/QWaitCondition>
#include <functional>

class XenCertificateManager;
class ConnectTask;
class MetricUpdater;
class XenObject;

namespace XenAPI
{
    class Session;
}

namespace Xen
{
    class ConnectionWorker;
}

/**
 * @brief High-level connection management for XenServer
 *
 * This class now uses a background worker thread (ConnectionWorker) to handle
 * all network I/O, matching the C# XenAdmin architecture. The connection class
 * itself is a thin wrapper that creates the worker and routes signals.
 */
class XENLIB_EXPORT XenConnection : public QObject
{
    Q_OBJECT
    public:
        explicit XenConnection(QObject* parent = nullptr);
        ~XenConnection();

        // Connection management
        bool ConnectToHost(const QString& hostname, int port, const QString& username, const QString& password); // OBSOLETE: legacy direct connection flow (use BeginConnect)
        void DisconnectTransport();
        bool IsConnected() const;
        bool IsTransportConnected() const;

        QString GetHostname() const;
        int GetPort() const;
        QString GetUsername() const;
        QString GetPassword() const;
        QString GetSessionId() const;
        void SetHostname(const QString& hostname);
        void SetPort(int port);
        void SetUsername(const QString& username);
        void SetPassword(const QString& password);

        using PasswordPrompt = std::function<bool(const QString& oldPassword, QString* newPassword)>;

        XenAPI::Session* GetNewSession(const QString& hostname,
                                  int port,
                                  const QString& username,
                                  const QString& password,
                                  bool isElevated,
                                  const PasswordPrompt& promptForNewPassword = PasswordPrompt(),
                                  QString* outError = nullptr,
                                  QString* redirectHostname = nullptr);

        void BeginConnect(bool initiateCoordinatorSearch, const PasswordPrompt& promptForNewPassword = PasswordPrompt());
        void EndConnect(bool clearCache = true, bool exiting = false);
        void Interrupt();
        ConnectTask* GetConnectTask() const;
        bool InProgress() const;
        XenAPI::Session* GetConnectSession() const;
        QStringList GetLastFailureDescription() const;

        bool GetSaveDisconnected() const;
        void SetSaveDisconnected(bool save);
        bool GetExpectPasswordIsCorrect() const;
        void SetExpectPasswordIsCorrect(bool expect);
        bool GetSuppressErrors() const;
        void SetSuppressErrors(bool suppress);
        bool GetPreventResettingPasswordPrompt() const;
        void SetPreventResettingPasswordPrompt(bool prevent);
        bool GetFromDialog() const;
        void SetFromDialog(bool fromDialog);

        /**
         * @brief Send an API request and BLOCK waiting for response
         *
         * This method queues the request to the worker thread and waits for the response.
         * It blocks the calling thread, so avoid using from UI thread.
         * For non-blocking operation, use sendRequestAsync() instead.
         *
         * @param data request body
         * @return API response body (extracted from HTTP response)
         */
        QByteArray SendRequest(const QByteArray& data); // OBSOLETE: legacy direct request path

        /**
         * @brief Send an API request asynchronously (non-blocking)
         *
         * This method queues the request to the worker thread and returns immediately.
         * Connect to the apiResponse() signal to receive the response.
         *
         * @param data request body
         * @return Request ID (use to match with apiResponse signal)
         */
        int SendRequestAsync(const QByteArray& data); // OBSOLETE: legacy direct request path

        // Session association (for heartbeat and other operations)
        void SetSession(XenAPI::Session* session);
        XenAPI::Session* GetSession() const;

        // Pool member tracking for failover
        void SetPoolMembers(const QStringList& members);
        QStringList GetPoolMembers() const;
        int GetCurrentPoolMemberIndex() const;
        void SetCurrentPoolMemberIndex(int index);
        bool HasMorePoolMembers() const;
        QString GetNextPoolMember();
        void ResetPoolMemberIndex(); // Reset to try all members again

        // Coordinator tracking for failover
        QString GetLastCoordinatorHostname() const;
        void SetLastCoordinatorHostname(const QString& hostname);
        bool IsFindingNewCoordinator() const;
        void SetFindingNewCoordinator(bool finding);
        QDateTime GetFindingNewCoordinatorStartedAt() const;
        void SetFindingNewCoordinatorStartedAt(const QDateTime& time);

        // Failover state
        bool GetExpectDisruption() const;
        void SetExpectDisruption(bool expect);
        bool GetCoordinatorMayChange() const;
        void SetCoordinatorMayChange(bool mayChange);

        qint64 GetServerTimeOffsetSeconds() const;
        void SetServerTimeOffsetSeconds(qint64 offsetSeconds);

        // Cache access (each connection owns its own cache, matching C# architecture)
        class XenCache* GetCache() const;
        MetricUpdater* GetMetricUpdater() const;
        void SetMetricUpdater(MetricUpdater* metricUpdater);
        QVariantMap WaitForCacheData(const QString& type,
                                     const QString& ref,
                                     int timeoutMs = 60000,
                                     const std::function<bool()>& cancelling = std::function<bool()>()) const;
        QSharedPointer<XenObject> WaitForCacheObject(const QString& type,
                                                     const QString& ref,
                                                     int timeoutMs = 60000,
                                                     const std::function<bool()>& cancelling = std::function<bool()>()) const;

        template <typename T>
        QSharedPointer<T> WaitForCacheObject(const QString& type,
                                             const QString& ref,
                                             int timeoutMs = 60000,
                                             const std::function<bool()>& cancelling = std::function<bool()>()) const
        {
            return qSharedPointerDynamicCast<T>(WaitForCacheObject(type, ref, timeoutMs, cancelling));
        }

    signals:
        void Connected();
        void Disconnected();
        void Error(const QString& message);
        void ProgressUpdate(const QString& message);
        void CacheDataReceived(const QByteArray& data);
        void CachePopulated();
        void ConnectionResult(bool connected, const QString& reason);
        void ConnectionStateChanged();
        void ConnectionLost();
        void ConnectionClosed();
        void ConnectionReconnecting();
        void BeforeConnectionEnd();
        void ClearingCache();
        void ConnectionMessageChanged(const QString& message);
        // These are just temporary for xenlib compat
        void TaskAdded(const QString& taskRef, const QVariantMap& taskData);
        void TaskModified(const QString& taskRef, const QVariantMap& taskData);
        void TaskDeleted(const QString& taskRef);
        void MessageReceived(const QString& messageRef, const QVariantMap& messageData);
        void MessageRemoved(const QString& messageRef);
        void XenObjectsUpdated();

        /**
         * @brief Emitted when async API request completes
         * @param requestId The request ID returned by sendRequestAsync()
         * @param response API response body
         */
        void ApiResponse(int requestId, const QByteArray& response);

    private slots:
        void onWorkerEstablished();
        void onWorkerFailed(const QString& error);
        void onWorkerFinished();
        void onWorkerProgress(const QString& message);
        void onWorkerCacheData(const QByteArray& data);
        void onWorkerApiResponse(int requestId, const QByteArray& response);
        void onCacheUpdateTimer();
        void onEventPollerEventReceived(const QVariantMap& eventData);
        void onEventPollerCachePopulated();
        void onEventPollerConnectionLost();

    private:
        //! Connection orchestration thread: login / cache warm / event loop
        void connectWorkerThread();
        void handleConnectionLostNewFlow();
        int reconnectHostTimeoutMs() const;
        QVariantMap fetchObjectRecord(const QString& cacheType, const QString& ref) const;
        void startReconnectSingleHostTimer();
        void startReconnectCoordinatorTimer(int timeoutMs);
        void reconnectSingleHostTimer();
        void reconnectCoordinatorTimer();
        bool poolMemberRemaining() const;
        void updatePoolMembersFromCache(QString* poolName,
                                        bool* haEnabled,
                                        QString* coordinatorAddress);

        class Private;
        Private* d;
};

#endif // XEN_CONNECTION_H
