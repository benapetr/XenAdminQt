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

#ifndef XENLIB_H
#define XENLIB_H

#include "xenlib_global.h"
#include <QtCore/QObject>
#include <QtCore/QVariant>

QT_FORWARD_DECLARE_CLASS(XenConnection)
QT_FORWARD_DECLARE_CLASS(XenRpcAPI)
QT_FORWARD_DECLARE_CLASS(XenAsyncOperations)
QT_FORWARD_DECLARE_CLASS(XenCertificateManager)
QT_FORWARD_DECLARE_CLASS(XenCache)
QT_FORWARD_DECLARE_CLASS(MetricUpdater)

namespace XenAPI
{
    class Session;
}

namespace Xen
{
    QT_FORWARD_DECLARE_CLASS(ConnectionsManager)
}

class XENLIB_EXPORT XenLib : public QObject
{
    Q_OBJECT
    public:
        XenLib(QObject* parent = nullptr);
        ~XenLib();

        // Core Xen API functionality
        bool initialize();

        // TODO - delete this
        bool isConnected() const;

        void setConnection(XenConnection* connection);
        // Cache is per-connection; each XenConnection owns its own XenCache instance.
        XenCache* GetCache() const;

        // Status and information
        QString getConnectionInfo() const;
        QString getLastError() const;
        bool hasError() const;

    signals:
        void connectionStateChanged(bool connected);
        void connectionError(const QString& error);
        void authenticationFailed(const QString& hostname, int port, const QString& username, const QString& errorMessage);
        void redirectedToMaster(const QString& masterAddress);
        void apiCallCompleted(const QString& method, const QVariant& result);
        void apiCallFailed(const QString& method, const QString& error);
        void certificateError(const QString& hostname, const QString& error);

        // Async API result signals (emitted when async requests complete)
        void virtualMachinesReceived(QVariantList vms);
        void hostsReceived(QVariantList hosts);
        void poolsReceived(QVariantList pools);
        void storageRepositoriesReceived(QVariantList srs);
        void networksReceived(QVariantList networks);
        void objectDataReceived(QString objectType, QString objectRef, QVariantMap data);

        // Task rehydration signals (forwarded from EventPoller for task monitoring)
        void taskAdded(XenConnection* connection, const QString& taskRef, const QVariantMap& taskData);
        void taskModified(XenConnection* connection, const QString& taskRef, const QVariantMap& taskData);
        void taskDeleted(XenConnection* connection, const QString& taskRef);

        // XenAPI Message signals (for alert system integration)
        void messageReceived(const QString& messageRef, const QVariantMap& messageData);
        void messageRemoved(const QString& messageRef);

    private slots:
        void handleConnectionStateChanged(bool connected);
        void handleConnectionError(const QString& error);
        void handleApiCallResult(const QString& method, const QVariant& result);
        void handleApiCallError(const QString& method, const QString& error);

        // Async connection handling (worker-based)
        void onConnectionEstablished();
        void onConnectionError(const QString& errorMessage);
        void onConnectionProgress(const QString& message);

        // Async API response handler
        void onConnectionApiResponse(int requestId, const QByteArray& response);

        // Pool member population for failover
        void onHostsReceivedForPoolMembers(const QVariantList& hosts);
        void onPoolsReceivedForHATracking(const QVariantList& pools);

        void onEventPollerConnectionLost();

    private:
        class Private;
        Private* d;

        void setupConnections();
        void clearError();
        void setError(const QString& error);
};

#endif // XENLIB_H
