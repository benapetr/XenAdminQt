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

#ifndef XEN_HEARTBEAT_H
#define XEN_HEARTBEAT_H

#include "../../xenlib_global.h"
#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QThread>
#include <QtCore/QMutex>
#include <QtCore/QDateTime>
#include <QtCore/QString>

QT_FORWARD_DECLARE_CLASS(XenConnection)

namespace XenAPI
{
    class Session;
}

class XENLIB_EXPORT XenHeartbeat : public QObject
{
    Q_OBJECT

    public:
        explicit XenHeartbeat(XenConnection* connection, int connectionTimeout, QObject* parent = nullptr);
        ~XenHeartbeat();

        // Control methods
        void start();
        void stop();
        bool isRunning() const;

        // Server time offset calculation
        QDateTime getServerTimeOffset() const;

    signals:
        void connectionLost();
        void serverTimeUpdated(const QDateTime& serverTime, const QDateTime& localTime);

    private slots:
        void performHeartbeat();
        void onHeartbeatTimer();

    private:
        void getServerTime();
        void handleConnectionLoss();
        void dropSession();
        bool createSession();

        XenConnection* m_connection;
        XenAPI::Session* m_session;
        QTimer* m_heartbeatTimer;

        int m_connectionTimeout;
        bool m_running;
        bool m_retrying;
        QDateTime m_serverTimeOffset;
        QString m_masterHostRef; // Track pool master for heartbeat

        mutable QMutex m_mutex;

        // Configuration constants
        static const int HEARTBEAT_INTERVAL_MS = 15000;    // 15 seconds
        static const int MIN_CONNECTION_TIMEOUT_MS = 5000; // 5 seconds minimum
};

#endif // XEN_HEARTBEAT_H
