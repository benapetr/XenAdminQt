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
#include <QDebug>

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
    
    // Configure SSL to accept all certificates (matching C# behavior)
    QSslConfiguration sslConfig = socket->sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    socket->setSslConfiguration(sslConfig);

    // Connect to host
    socket->connectToHostEncrypted(url.host(), url.port(443));
    
    if (!socket->waitForEncrypted(30000))
    {
        this->lastError_ = QString("Failed to establish SSL connection: %1").arg(socket->errorString());
        socket->deleteLater();
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
        socket->write(line.toLatin1());
    }
    socket->write("\r\n");
    socket->flush();
    
    return socket->waitForBytesWritten(30000);
}

bool HttpClient::readHttpResponse(QSslSocket* socket)
{
    // Wait for response
    if (!socket->waitForReadyRead(30000))
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

qint64 HttpClient::copyStream(QIODevice* source, QIODevice* dest,
                              qint64 totalSize,
                              ProgressCallback progressCallback,
                              DataCopiedCallback dataCopiedCallback,
                              CancelCallback cancelCallback)
{
    qint64 bytesTransferred = 0;
    QByteArray buffer(BUFFER_SIZE, Qt::Uninitialized);
    QDateTime lastUpdate = QDateTime::currentDateTime();

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

        qint64 bytesWritten = dest->write(buffer.constData(), bytesRead);
        if (bytesWritten != bytesRead)
        {
            this->lastError_ = "Failed to write data";
            return -1;
        }

        bytesTransferred += bytesWritten;

        // Update progress every 500ms
        if (QDateTime::currentDateTime().msecsTo(lastUpdate) > 500)
        {
            if (progressCallback && totalSize > 0)
            {
                int percent = static_cast<int>((bytesTransferred * 100) / totalSize);
                progressCallback(percent);
            }
            
            if (dataCopiedCallback)
            {
                dataCopiedCallback(bytesTransferred);
            }
            
            lastUpdate = QDateTime::currentDateTime();
        }
    }

    // Final progress update
    if (progressCallback && totalSize > 0)
    {
        int percent = static_cast<int>((bytesTransferred * 100) / totalSize);
        progressCallback(percent);
    }
    
    if (dataCopiedCallback)
    {
        dataCopiedCallback(bytesTransferred);
    }

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
        socket->deleteLater();
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
        socket->deleteLater();
        return false;
    }

    socket->flush();
    socket->waitForBytesWritten(30000);

    // Read response
    bool success = this->readHttpResponse(socket);
    
    socket->disconnectFromHost();
    if (socket->state() != QAbstractSocket::UnconnectedState)
        socket->waitForDisconnected(5000);
    
    socket->deleteLater();

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
        socket->deleteLater();
        return false;
    }

    // Read response headers
    if (!this->readHttpResponse(socket))
    {
        emit this->error(this->lastError_);
        socket->deleteLater();
        return false;
    }

    // Create temporary file
    QString tmpFile = localFilePath + ".tmp";
    QFile file(tmpFile);
    if (!file.open(QIODevice::WriteOnly))
    {
        this->lastError_ = QString("Failed to create file: %1").arg(file.errorString());
        emit this->error(this->lastError_);
        socket->deleteLater();
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
    
    socket->deleteLater();

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
