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

#include "httpclient.h"
#include <QFile>
#include <QFileInfo>
#include <QUrlQuery>
#include <QSslConfiguration>
#include <QEventLoop>
#include <QTimer>
#include <QDateTime>
#include <QElapsedTimer>
#include <QDebug>
#include <QNetworkProxy>
#include <cerrno>

HttpClient::HttpClient(QObject* parent) : QObject(parent)
{
}

HttpClient::~HttpClient()
{
}

QUrl HttpClient::buildUri(const QString& hostname,
                          const QString& path,
                          const QMap<QString, QString>& queryParams)
{
    QUrl url;
    url.setScheme("https");
    url.setHost(hostname);
    url.setPort(443);
    url.setPath(path);

    QUrlQuery query;
    for (auto it = queryParams.constBegin(); it != queryParams.constEnd(); ++it)
    {
        if (!it.value().isEmpty())
            query.addQueryItem(it.key(), it.value());
    }
    
    url.setQuery(query);
    return url;
}

bool HttpClient::connectToHost(const QUrl& url, QSslSocket*& socket)
{
    socket = new QSslSocket(this);

    // Respect app-wide proxy settings (SettingsManager::ApplyProxySettings).
    const QNetworkProxy appProxy = QNetworkProxy::applicationProxy();
    if (appProxy.type() != QNetworkProxy::NoProxy)
        socket->setProxy(appProxy);
    
    // Configure SSL to accept all certificates (matching C# behavior)
    QSslConfiguration sslConfig = socket->sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    socket->setSslConfiguration(sslConfig);

    // Connect to host
    socket->connectToHostEncrypted(url.host(), url.port(443));
    
    if (!socket->waitForEncrypted(30000))
    {
        this->lastError_ = QString("Failed to establish SSL connection: %1").arg(socket->errorString());
        delete socket;
        socket = nullptr;
        return false;
    }

    return true;
}

bool HttpClient::sendHttpHeaders(QSslSocket* socket, const QStringList& headers)
{
    for (const QString& header : headers)
    {
        QString line = header + "\r\n";
        qint64 bytesWritten = socket->write(line.toLatin1());
        if (bytesWritten < 0)
        {
            qWarning() << "HttpClient: Failed to queue HTTP header"
                       << "state" << socket->state()
                       << "encrypted" << socket->isEncrypted()
                       << "socketError" << socket->error()
                       << "errorString" << socket->errorString();
            return false;
        }
    }

    qint64 bytesWritten = socket->write("\r\n");
    if (bytesWritten < 0)
    {
        qWarning() << "HttpClient: Failed to queue HTTP header terminator"
                   << "state" << socket->state()
                   << "encrypted" << socket->isEncrypted()
                   << "socketError" << socket->error()
                   << "errorString" << socket->errorString();
        return false;
    }

    socket->flush();
    return true;
}

bool HttpClient::readHttpResponse(QSslSocket* socket)
{
    // Wait for response
    if (!socket->waitForReadyRead(HTTP_TIMEOUT_MS))
    {
        this->lastError_ = "Timeout waiting for HTTP response";
        return false;
    }

    // Read status line
    QByteArray statusLine = socket->readLine();
    QString statusStr = QString::fromLatin1(statusLine).trimmed();
    
    qDebug() << "HTTP response:" << statusStr;
    
    // Read headers
    while (socket->canReadLine() || socket->waitForReadyRead(5000))
    {
        QByteArray line = socket->readLine();
        if (line.trimmed().isEmpty())
            break;
        qDebug() << "  Header:" << QString::fromLatin1(line).trimmed();
    }

    // Parse status code
    QStringList parts = statusStr.split(' ');
    if (parts.size() < 2)
    {
        this->lastError_ = QString("Invalid HTTP response: %1").arg(statusStr);
        return false;
    }

    int statusCode = parts[1].toInt();
    if (statusCode != 200)
    {
        this->lastError_ = QString("HTTP error %1: %2").arg(statusCode).arg(statusStr);
        return false;
    }

    return true;
}

bool HttpClient::waitForWriteBuffer(QAbstractSocket* socket,
                                    qint64 targetBytes,
                                    CancelCallback cancelCallback)
{
    QElapsedTimer stalledTimer;
    stalledTimer.start();
    qint64 previousBytesToWrite = socket->bytesToWrite();

    while (socket->bytesToWrite() > targetBytes)
    {
        if (cancelCallback && cancelCallback())
        {
            this->lastError_ = "Operation cancelled by user";
            return false;
        }

        if (socket->state() != QAbstractSocket::ConnectedState)
        {
            this->lastError_ = QString("Connection closed while sending data: %1")
                                   .arg(socket->errorString());
            return false;
        }

        const bool wroteBytes = socket->waitForBytesWritten(WRITE_WAIT_SLICE_MS);
        if (!wroteBytes &&
            socket->error() != QAbstractSocket::UnknownSocketError &&
            socket->error() != QAbstractSocket::SocketTimeoutError &&
            socket->error() != QAbstractSocket::TemporaryError)
        {
            this->lastError_ = QString("Failed while sending data: %1")
                                   .arg(socket->errorString());
            return false;
        }

        const qint64 bytesToWrite = socket->bytesToWrite();
        if (bytesToWrite < previousBytesToWrite)
        {
            previousBytesToWrite = bytesToWrite;
            stalledTimer.restart();
            continue;
        }

        if (stalledTimer.elapsed() >= HTTP_TIMEOUT_MS)
        {
            this->lastError_ = QString("Timeout sending data: %1").arg(socket->errorString());
            return false;
        }
    }

    return true;
}

qint64 HttpClient::copyStream(QIODevice* source, QIODevice* dest,
                              qint64 totalSize,
                              ProgressCallback progressCallback,
                              DataCopiedCallback dataCopiedCallback,
                              CancelCallback cancelCallback)
{
    qint64 bytesTransferred = 0;
    QByteArray buffer(BUFFER_SIZE, Qt::Uninitialized);
    QDateTime lastUpdate = QDateTime::currentDateTime();
    QAbstractSocket* destSocket = qobject_cast<QAbstractSocket*>(dest);

    auto reportProgress = [&]() {
        if (progressCallback && totalSize > 0)
        {
            int percent = static_cast<int>((bytesTransferred * 100) / totalSize);
            progressCallback(percent);
        }

        if (dataCopiedCallback)
            dataCopiedCallback(bytesTransferred);
    };

    auto writeBytes = [&](qint64 bytesRead) -> bool {
        qint64 offset = 0;
        while (offset < bytesRead)
        {
            if (cancelCallback && cancelCallback())
            {
                this->lastError_ = "Operation cancelled by user";
                return false;
            }

            const qint64 bytesWritten = dest->write(buffer.constData() + offset,
                                                    bytesRead - offset);
            if (bytesWritten < 0)
            {
                // Detect disk-full on POSIX systems (mirrors C# ERROR_DISK_FULL check)
#ifdef Q_OS_UNIX
                if (errno == ENOSPC)
                {
                    this->isDiskFull_ = true;
                    this->lastError_ = "The target disk is full.";
                    return false;
                }
#endif
                // Generic write error — include the destination error where available
                QFile* destFile = qobject_cast<QFile*>(dest);
                this->lastError_ = destFile
                    ? QString("Failed to write data: %1").arg(destFile->errorString())
                    : destSocket
                        ? QString("Failed to write data: %1").arg(destSocket->errorString())
                        : QString("Failed to write data");
                return false;
            }

            if (bytesWritten == 0)
            {
                this->lastError_ = destSocket
                    ? QString("Socket accepted no data: %1").arg(destSocket->errorString())
                    : QString("Destination accepted no data");
                return false;
            }

            offset += bytesWritten;
            bytesTransferred += bytesWritten;

            if (destSocket &&
                destSocket->bytesToWrite() >= WRITE_HIGH_WATER_MARK &&
                !this->waitForWriteBuffer(destSocket,
                                          WRITE_LOW_WATER_MARK,
                                          cancelCallback))
            {
                return false;
            }

            // Update progress every 500ms
            QDateTime now = QDateTime::currentDateTime();
            if (lastUpdate.msecsTo(now) > 500)
            {
                reportProgress();
                lastUpdate = now;
            }
        }

        return true;
    };

    QAbstractSocket* sourceSocket = qobject_cast<QAbstractSocket*>(source);
    if (sourceSocket)
    {
        while (sourceSocket->state() == QAbstractSocket::ConnectedState ||
               sourceSocket->bytesAvailable() > 0)
        {
            // Check for cancellation
            if (cancelCallback && cancelCallback())
            {
                this->lastError_ = "Operation cancelled by user";
                return -1;
            }

            if (sourceSocket->bytesAvailable() <= 0)
            {
                if (!sourceSocket->waitForReadyRead(HTTP_TIMEOUT_MS))
                {
                    if (sourceSocket->bytesAvailable() > 0)
                        continue;

                    if (sourceSocket->state() == QAbstractSocket::UnconnectedState ||
                        sourceSocket->error() == QAbstractSocket::RemoteHostClosedError)
                    {
                        break;
                    }

                    this->lastError_ = QString("Timeout waiting for data: %1").arg(sourceSocket->errorString());
                    return -1;
                }
            }

            while (sourceSocket->bytesAvailable() > 0)
            {
                qint64 bytesRead = sourceSocket->read(buffer.data(), buffer.size());
                if (bytesRead < 0)
                {
                    this->lastError_ = QString("Failed to read data: %1").arg(sourceSocket->errorString());
                    return -1;
                }

                if (bytesRead == 0)
                    break;

                if (!writeBytes(bytesRead))
                    return -1;
            }
        }
    } else
    {
        while (!source->atEnd())
        {
            // Check for cancellation
            if (cancelCallback && cancelCallback())
            {
                this->lastError_ = "Operation cancelled by user";
                return -1;
            }

            qint64 bytesRead = source->read(buffer.data(), buffer.size());
            if (bytesRead <= 0)
                break;

            if (!writeBytes(bytesRead))
                return -1;
        }
    }

    // Final progress update
    reportProgress();

    return bytesTransferred;
}

bool HttpClient::putFile(const QString& localFilePath,
                         const QString& hostname,
                         const QString& remotePath,
                         const QMap<QString, QString>& queryParams,
                         ProgressCallback progressCallback,
                         CancelCallback cancelCallback)
{
    // Open local file
    QFile file(localFilePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        this->lastError_ = QString("Failed to open file: %1").arg(file.errorString());
        emit this->error(this->lastError_);
        return false;
    }

    qint64 fileSize = file.size();
    QUrl url = buildUri(hostname, remotePath, queryParams);

    qDebug() << "HTTP PUT:" << url.toString();
    qDebug() << "File size:" << fileSize << "bytes";

    // Connect to server
    QSslSocket* socket = nullptr;
    if (!this->connectToHost(url, socket))
    {
        emit this->error(this->lastError_);
        file.close();
        return false;
    }

    // Send HTTP PUT headers
    QStringList headers;
    headers << QString("PUT %1 HTTP/1.1").arg(url.path() + "?" + url.query());
    headers << QString("Host: %1").arg(url.host());
    headers << QString("Content-Length: %1").arg(fileSize);
    headers << "Connection: close";

    if (!this->sendHttpHeaders(socket, headers))
    {
        this->lastError_ = "Failed to send HTTP headers";
        emit this->error(this->lastError_);
        delete socket;
        file.close();
        return false;
    }

    // Upload file content
    qint64 bytesTransferred = this->copyStream(&file, socket, fileSize,
                                               progressCallback, nullptr, cancelCallback);
    
    file.close();

    if (bytesTransferred < 0)
    {
        emit this->error(this->lastError_);
        delete socket;
        return false;
    }

    socket->flush();
    if (!this->waitForWriteBuffer(socket, 0, cancelCallback))
    {
        emit this->error(this->lastError_);
        delete socket;
        return false;
    }

    // Read response
    bool success = this->readHttpResponse(socket);
    
    socket->disconnectFromHost();
    if (socket->state() != QAbstractSocket::UnconnectedState)
        socket->waitForDisconnected(5000);
    
    delete socket;

    if (!success)
        emit this->error(this->lastError_);

    return success;
}

bool HttpClient::getFile(const QString& hostname,
                         const QString& remotePath,
                         const QMap<QString, QString>& queryParams,
                         const QString& localFilePath,
                         DataCopiedCallback dataCopiedCallback,
                         CancelCallback cancelCallback)
{
    this->isDiskFull_ = false;   // Reset per-call flag before each download
    QUrl url = buildUri(hostname, remotePath, queryParams);

    qDebug() << "HTTP GET:" << url.toString();

    // Connect to server
    QSslSocket* socket = nullptr;
    if (!this->connectToHost(url, socket))
    {
        emit this->error(this->lastError_);
        return false;
    }

    // Send HTTP GET headers
    QStringList headers;
    headers << QString("GET %1 HTTP/1.1").arg(url.path() + "?" + url.query());
    headers << QString("Host: %1").arg(url.host());
    headers << "Connection: close";

    if (!this->sendHttpHeaders(socket, headers))
    {
        this->lastError_ = "Failed to send HTTP headers";
        emit this->error(this->lastError_);
        delete socket;
        return false;
    }

    // Read response headers
    if (!this->readHttpResponse(socket))
    {
        emit this->error(this->lastError_);
        delete socket;
        return false;
    }

    // Create temporary file
    QString tmpFile = localFilePath + ".tmp";
    QFile file(tmpFile);
    if (!file.open(QIODevice::WriteOnly))
    {
        this->lastError_ = QString("Failed to create file: %1").arg(file.errorString());
        emit this->error(this->lastError_);
        delete socket;
        return false;
    }

    // Download file content
    qint64 bytesTransferred = this->copyStream(socket, &file, 0,
                                               nullptr, dataCopiedCallback, cancelCallback);
    
    file.flush();
    file.close();

    socket->disconnectFromHost();
    if (socket->state() != QAbstractSocket::UnconnectedState)
        socket->waitForDisconnected(5000);
    
    delete socket;

    if (bytesTransferred < 0)
    {
        QFile::remove(tmpFile);
        emit this->error(this->lastError_);
        return false;
    }

    // Rename temp file to final name
    if (QFile::exists(localFilePath))
        QFile::remove(localFilePath);
    
    if (!QFile::rename(tmpFile, localFilePath))
    {
        this->lastError_ = "Failed to rename temporary file";
        QFile::remove(tmpFile);
        emit this->error(this->lastError_);
        return false;
    }

    return true;
}
