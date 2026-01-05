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

#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include "../../xenlib_global.h"
#include <QObject>
#include <QString>
#include <QUrl>
#include <QSslSocket>
#include <functional>

class XenSession;

/**
 * @brief HTTP client for XenServer API file transfer operations (import/export)
 *
 * Implements HTTP PUT and GET operations for XenServer's HTTP endpoints,
 * matching C#'s HTTP and HTTP_actions classes.
 */
class XENLIB_EXPORT HttpClient : public QObject
{
    Q_OBJECT

    public:
        using ProgressCallback = std::function<void(int percent)>;
        using DataCopiedCallback = std::function<void(qint64 bytes)>;
        using CancelCallback = std::function<bool()>;

        static const int BUFFER_SIZE = 32 * 1024;
        static const int HTTP_TIMEOUT_MS = 30 * 60 * 1000; // 30 minutes

        explicit HttpClient(QObject* parent = nullptr);
        ~HttpClient();

        /**
        * @brief Upload a file via HTTP PUT
        * @param localFilePath Path to local file to upload
        * @param hostname Destination host
        * @param remotePath Remote HTTP path (e.g., "/import")
        * @param queryParams Query string parameters (task_id, session_id, etc.)
        * @param progressCallback Called with percent complete
        * @param cancelCallback Called to check if operation should be cancelled
        * @return true on success
        */
        bool putFile(const QString& localFilePath,
                    const QString& hostname,
                    const QString& remotePath,
                    const QMap<QString, QString>& queryParams,
                    ProgressCallback progressCallback = nullptr,
                    CancelCallback cancelCallback = nullptr);

        /**
        * @brief Download a file via HTTP GET
        * @param hostname Source host
        * @param remotePath Remote HTTP path (e.g., "/export")
        * @param queryParams Query string parameters
        * @param localFilePath Path where to save downloaded file
        * @param dataCopiedCallback Called with bytes transferred
        * @param cancelCallback Called to check if operation should be cancelled
        * @return true on success
        */
        bool getFile(const QString& hostname,
                    const QString& remotePath,
                    const QMap<QString, QString>& queryParams,
                    const QString& localFilePath,
                    DataCopiedCallback dataCopiedCallback = nullptr,
                    CancelCallback cancelCallback = nullptr);

        /**
        * @brief Build URI from hostname, path, and query parameters
        */
        static QUrl buildUri(const QString& hostname,
                            const QString& path,
                            const QMap<QString, QString>& queryParams);

        /**
        * @brief Get last error message
        */
        QString lastError() const { return this->lastError_; }

    signals:
        void error(const QString& message);

    private:
        bool connectToHost(const QUrl& url, QSslSocket*& socket);
        bool sendHttpHeaders(QSslSocket* socket, const QStringList& headers);
        bool readHttpResponse(QSslSocket* socket);
        qint64 copyStream(QIODevice* source, QIODevice* dest,
                        qint64 totalSize,
                        ProgressCallback progressCallback,
                        DataCopiedCallback dataCopiedCallback,
                        CancelCallback cancelCallback);

        QString lastError_;
};

#endif // HTTPCLIENT_H
