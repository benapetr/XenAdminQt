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

#include "httpconnect.h"
#include <QSslSocket>
#include <QSslConfiguration>
#include <QTimer>
#include <QDebug>

HTTPConnect::HTTPConnect(QObject* parent)
    : QObject(parent),
      m_socket(nullptr),
      m_timeoutTimer(nullptr),
      m_state(Idle)
{
}

HTTPConnect::~HTTPConnect()
{
    cleanupSocket(false);

    if (m_timeoutTimer)
    {
        m_timeoutTimer->stop();
        m_timeoutTimer->deleteLater();
        m_timeoutTimer = nullptr;
    }
}

void HTTPConnect::connectToConsoleAsync(const QUrl& consoleUrl, const QString& sessionId)
{
    m_lastError.clear();

    // Validate input
    if (!consoleUrl.isValid())
    {
        failWithError("Invalid console URL");
        return;
    }

    if (sessionId.isEmpty())
    {
        failWithError("Session ID is required");
        return;
    }

    QString host = consoleUrl.host();
    int port = consoleUrl.port(443); // Default HTTPS port

    if (host.isEmpty())
    {
        failWithError("No host in console URL");
        return;
    }

    // Clean up any previous connection
    cleanupSocket();

    // Store connection parameters
    m_consoleUrl = consoleUrl;
    m_sessionId = sessionId;
    m_responseBuffer.clear();
    m_state = ConnectingSSL;

    qDebug() << "HTTPConnect: Connecting to" << host << ":" << port;
    qDebug() << "HTTPConnect: Console URL:" << consoleUrl.toString();

    // Create SSL socket
    m_socket = new QSslSocket(this);

    // Configure SSL to accept XenServer self-signed certificates
    QSslConfiguration sslConfig = m_socket->sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone); // Accept self-signed certs
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);
    m_socket->setSslConfiguration(sslConfig);

    // Set socket options
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    m_socket->setReadBufferSize(0); // Unlimited read buffer

    // Connect signals (async)
    connect(m_socket, &QSslSocket::encrypted,
            this, &HTTPConnect::onSslEncrypted);
    connect(m_socket, QOverload<const QList<QSslError>&>::of(&QSslSocket::sslErrors),
            this, &HTTPConnect::onSslError);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QSslSocket::errorOccurred),
            this, &HTTPConnect::onSocketError);
    connect(m_socket, &QSslSocket::readyRead,
            this, &HTTPConnect::onReadyRead);

    // Create timeout timer (30 seconds for SSL connection)
    if (!m_timeoutTimer)
    {
        m_timeoutTimer = new QTimer(this);
        m_timeoutTimer->setSingleShot(true);
        connect(m_timeoutTimer, &QTimer::timeout,
                this, &HTTPConnect::onConnectionTimeout);
    }
    m_timeoutTimer->start(30000); // 30 second timeout

    // Start async SSL connection
    m_socket->connectToHostEncrypted(host, port);
}

void HTTPConnect::onSslEncrypted()
{
    if (m_state != ConnectingSSL)
    {
        return;
    }

    if (!m_socket)
    {
        failWithError("Socket invalid after SSL handshake");
        return;
    }

    qDebug() << "HTTPConnect: SSL connection established";

    // Stop timeout timer
    if (m_timeoutTimer)
    {
        m_timeoutTimer->stop();
    }

    // Send HTTP CONNECT request
    m_state = SendingConnect;
    sendConnectRequest();
}

void HTTPConnect::sendConnectRequest()
{
    if (!m_socket)
    {
        failWithError("Socket is null before sending CONNECT");
        return;
    }

    // Build HTTP CONNECT request
    // Format based on XenAdmin C# implementation (HTTP.cs):
    // CONNECT /console?ref=OpaqueRef:xxx HTTP/1.0
    // Host: hostname
    // Cookie: session_id=sessionId
    // (blank line)

    QString pathAndQuery = m_consoleUrl.path();
    if (m_consoleUrl.hasQuery())
    {
        pathAndQuery += "?" + m_consoleUrl.query();
    }

    QString request;
    request += QString("CONNECT %1 HTTP/1.0\r\n").arg(pathAndQuery);
    request += QString("Host: %1\r\n").arg(m_consoleUrl.host());
    request += QString("Cookie: session_id=%1\r\n").arg(m_sessionId);
    request += "\r\n"; // Empty line to end headers

    qDebug() << "HTTPConnect: Sending CONNECT request:";
    qDebug().noquote() << request.trimmed();

    // Send the request (non-blocking)
    QByteArray requestData = request.toUtf8();
    qint64 written = m_socket->write(requestData);
    if (written != requestData.size())
    {
        failWithError("Failed to write complete CONNECT request");
        return;
    }

    m_socket->flush();

    // Start reading response
    m_state = ReadingResponse;
    m_responseBuffer.clear();

    // Restart timeout timer for response (30 seconds)
    if (m_timeoutTimer)
    {
        m_timeoutTimer->start(30000);
    }

    // If data is already available, process it
    if (m_socket->bytesAvailable() > 0)
    {
        onReadyRead();
    }
}

void HTTPConnect::onReadyRead()
{
    if (!m_socket)
    {
        failWithError("Socket is null during read");
        return;
    }

    if (m_state != ReadingResponse)
    {
        return;
    }

    // Read available data into buffer
    m_responseBuffer.append(m_socket->readAll());

    // Try to parse HTTP response
    int statusCode = readHttpResponse();

    if (statusCode == 200)
    {
        qDebug() << "HTTPConnect: Received HTTP 200 OK - tunnel established";

        // Stop timeout timer
        if (m_timeoutTimer)
        {
            m_timeoutTimer->stop();
        }

        m_state = Idle;

        // Transfer ownership of socket to caller
        QSslSocket* socket = m_socket;
        m_socket = nullptr; // Don't delete it in destructor

        // Disconnect our signals so caller can use the socket
        socket->disconnect(this);
        socket->setParent(nullptr);

        emit connectedToConsole(socket);
    } else if (statusCode > 0)
    {
        // Got a response but not 200 OK
        failWithError(QString("HTTP CONNECT failed with status code %1").arg(statusCode));
    }
    // else statusCode == 0 means incomplete response, wait for more data
}

int HTTPConnect::readHttpResponse()
{
    // Parse HTTP response headers from buffer
    // First line should be: HTTP/1.0 200 OK (or similar)
    // Read until we get \r\n\r\n (end of headers)

    int statusCode = 0;
    bool firstLine = true;
    int pos = 0;

    while (pos < m_responseBuffer.size())
    {
        // Find next line ending
        int lineEnd = m_responseBuffer.indexOf("\r\n", pos);
        if (lineEnd == -1)
        {
            // Incomplete line, need more data
            return 0;
        }

        // Extract line
        QByteArray line = m_responseBuffer.mid(pos, lineEnd - pos);
        pos = lineEnd + 2; // Skip \r\n

        // Check if this is the end of headers (blank line)
        if (line.isEmpty())
        {
            // End of headers - we're done
            qDebug() << "HTTPConnect: End of response headers";
            return statusCode;
        }

        QString lineStr = QString::fromUtf8(line);
        qDebug() << "HTTPConnect: Response header:" << lineStr;

        // Parse status line
        if (firstLine)
        {
            firstLine = false;

            // Format: HTTP/1.0 200 OK
            QStringList parts = lineStr.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 2)
            {
                statusCode = parts[1].toInt();
                qDebug() << "HTTPConnect: Status code:" << statusCode;
            }
        }
    }

    // Haven't found end of headers yet, need more data
    return 0;
}

void HTTPConnect::onSslError(const QList<QSslError>& errors)
{
    QString errorStr = "SSL errors: ";
    for (const QSslError& error : errors)
    {
        errorStr += error.errorString() + "; ";
    }
    qDebug() << "HTTPConnect:" << errorStr;

    // Since we're accepting self-signed certificates, ignore SSL errors
    if (m_socket)
    {
        m_socket->ignoreSslErrors();
    }
}

void HTTPConnect::onSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);

    if (m_socket)
    {
        QString error = m_socket->errorString();
        qDebug() << "HTTPConnect: Socket error:" << error;
        failWithError(QString("Socket error: %1").arg(error));
    }
}

void HTTPConnect::onConnectionTimeout()
{
    QString timeoutMsg;

    switch (m_state)
    {
    case ConnectingSSL:
        timeoutMsg = "Timeout waiting for SSL connection";
        break;
    case SendingConnect:
        timeoutMsg = "Timeout sending CONNECT request";
        break;
    case ReadingResponse:
        timeoutMsg = "Timeout waiting for CONNECT response";
        break;
    default:
        timeoutMsg = "Connection timeout";
        break;
    }

    qDebug() << "HTTPConnect:" << timeoutMsg;
    failWithError(timeoutMsg);
}

void HTTPConnect::failWithError(const QString& error)
{
    m_lastError = error;
    m_state = Idle;

    // Stop timeout timer
    if (m_timeoutTimer)
    {
        m_timeoutTimer->stop();
    }

    cleanupSocket();

    // Defer error signal emission to avoid crash if receiver calls deleteLater() on sender
    // This ensures failWithError() completes before object deletion
    QTimer::singleShot(0, this, [this]() {
        emit this->error(m_lastError);
    });
}

void HTTPConnect::cleanupSocket(bool closeConnection)
{
    if (!m_socket)
        return;

    // Disconnect to prevent signal callbacks after cleanup
    m_socket->disconnect(this);

    if (closeConnection)
    {
        m_socket->disconnectFromHost();
    }

    // Safe async delete; QPointer will null itself if already destroyed elsewhere
    m_socket->deleteLater();
    m_socket = nullptr;
}
