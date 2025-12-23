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

class XenCertificateManager;
class XenSession;

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
        bool connectToHost(const QString& hostname, int port,
                           const QString& username, const QString& password);
        void disconnect();
        bool isConnected() const;

        QString getHostname() const;
        int getPort() const;
        QString getSessionId() const;

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
        QByteArray sendRequest(const QByteArray& data);

        /**
         * @brief Send an API request asynchronously (non-blocking)
         *
         * This method queues the request to the worker thread and returns immediately.
         * Connect to the apiResponse() signal to receive the response.
         *
         * @param data request body
         * @return Request ID (use to match with apiResponse signal)
         */
        int sendRequestAsync(const QByteArray& data);

        // Session association (for heartbeat and other operations)
        void setSession(XenSession* session);
        XenSession* getSession() const;

        // Pool member tracking for failover
        void setPoolMembers(const QStringList& members);
        QStringList getPoolMembers() const;
        int getCurrentPoolMemberIndex() const;
        void setCurrentPoolMemberIndex(int index);
        bool hasMorePoolMembers() const;
        QString getNextPoolMember();
        void resetPoolMemberIndex(); // Reset to try all members again

        // Coordinator tracking for failover
        QString getLastCoordinatorHostname() const;
        void setLastCoordinatorHostname(const QString& hostname);
        bool isFindingNewCoordinator() const;
        void setFindingNewCoordinator(bool finding);
        QDateTime getFindingNewCoordinatorStartedAt() const;
        void setFindingNewCoordinatorStartedAt(const QDateTime& time);

        // Failover state
        bool getExpectDisruption() const;
        void setExpectDisruption(bool expect);
        bool getCoordinatorMayChange() const;
        void setCoordinatorMayChange(bool mayChange);

        // Certificate management
        void setCertificateManager(XenCertificateManager* certManager);

        // Cache access (each connection owns its own cache, matching C# architecture)
        class XenCache* getCache() const;

    signals:
        void connected();
        void disconnected();
        void error(const QString& message);
        void progressUpdate(const QString& message);
        void cacheDataReceived(const QByteArray& data);

        /**
         * @brief Emitted when async API request completes
         * @param requestId The request ID returned by sendRequestAsync()
         * @param response API response body
         */
        void apiResponse(int requestId, const QByteArray& response);

    private slots:
        void onWorkerEstablished();
        void onWorkerFailed(const QString& error);
        void onWorkerFinished();
        void onWorkerProgress(const QString& message);
        void onWorkerCacheData(const QByteArray& data);
        void onWorkerApiResponse(int requestId, const QByteArray& response);

    private:
        class Private;
        Private* d;
};

#endif // XEN_CONNECTION_H
