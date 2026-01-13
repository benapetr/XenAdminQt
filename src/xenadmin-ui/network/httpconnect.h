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

#ifndef HTTPCONNECT_H
#define HTTPCONNECT_H

#include <QObject>
#include <QSslSocket>
#include <QString>
#include <QUrl>
#include <QTimer>
#include <QPointer>

/**
 * @brief HTTPConnect class - Implements HTTP CONNECT tunneling for XenServer console access
 *
 * This class establishes an HTTP CONNECT tunnel through the XenServer's HTTPS proxy,
 * which is required for accessing VM consoles. Once the tunnel is established, the
 * socket can be used for VNC or RDP protocol communication.
 *
 * ASYNC DESIGN: This class uses non-blocking operations. Connection is established
 * asynchronously and the connectedToConsole() signal is emitted when ready.
 *
 * Based on XenAdmin C# implementation in XenModel/XenAPI/HTTP.cs
 */
class HTTPConnect : public QObject
{
    Q_OBJECT

    public:
        explicit HTTPConnect(QObject* parent = nullptr);
        ~HTTPConnect();

        /**
         * @brief Start async connection to console (non-blocking)
         * @param consoleUrl The console location URL (e.g., https://host:443/console?ref=...)
         * @param sessionId The XenServer session ID for authentication
         *
         * This method returns immediately. Connect to connectedToConsole() signal
         * to receive the ready socket, or error() signal on failure.
         */
        void connectToConsoleAsync(const QUrl& consoleUrl, const QString& sessionId);

        /**
         * @brief Get the last error message
         */
        QString lastError() const
        {
            return m_lastError;
        }

    signals:
        /**
         * @brief Emitted when console connection is ready
         * @param socket The QSslSocket ready for VNC/RDP communication
         *
         * The receiver takes ownership of the socket and must delete it when done.
         */
        void connectedToConsole(QSslSocket* socket);

        /**
         * @brief Emitted on connection error
         */
        void error(const QString& errorMessage);

    private slots:
        void onSslEncrypted();
        void onSslError(const QList<QSslError>& errors);
        void onSocketError(QAbstractSocket::SocketError socketError);
        void onReadyRead();
        void onConnectionTimeout();

    private:
        /**
         * @brief Send HTTP CONNECT request (async)
         */
        void sendConnectRequest();

        /**
         * @brief Read and parse HTTP response headers
         * @return HTTP status code (200 = success), or 0 if incomplete
         */
        int readHttpResponse();

        /**
         * @brief Cleanup and emit error
         */
        void failWithError(const QString& error);

        /**
         * @brief Safely tear down the socket; guarded by QPointer
         * @param closeConnection Whether to disconnectFromHost() first
         */
        void cleanupSocket(bool closeConnection = true);

        QString m_lastError;
        QPointer<QSslSocket> m_socket;
        QUrl m_consoleUrl;
        QString m_sessionId;
        QByteArray m_responseBuffer;
        QTimer* m_timeoutTimer = nullptr;

        enum State
        {
            Idle,
            ConnectingSSL,
            SendingConnect,
            ReadingResponse
        };
        State m_state;
};

#endif // HTTPCONNECT_H
