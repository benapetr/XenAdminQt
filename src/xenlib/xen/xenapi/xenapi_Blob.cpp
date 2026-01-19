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

#include "xenapi_Blob.h"
#include "../session.h"
#include "../network/connection.h"
#include <QtCore/QEventLoop>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QSslConfiguration>
#include <QtNetwork/QSslSocket>
#include <stdexcept>

namespace XenAPI
{
    namespace
    {
        QUrl buildBlobUrl(Session* session, const QString& blobRef)
        {
            if (!session || !session->IsLoggedIn())
                return QUrl();

            XenConnection* connection = session->GetConnection();
            if (!connection)
                return QUrl();

            const QString sessionId = session->GetSessionID();
            if (sessionId.isEmpty() || blobRef.isEmpty())
                return QUrl();

            const int port = connection->GetPort();
            const bool useSSL = (port == 443);
            const QString escapedSession = QString::fromUtf8(QUrl::toPercentEncoding(sessionId));

            QUrl url;
            url.setScheme(useSSL ? "https" : "http");
            url.setHost(connection->GetHostname());
            url.setPort(port);
            url.setPath("/blob");
            url.setQuery(QString("ref=%1&session_id=%2").arg(blobRef, escapedSession));
            return url;
        }

        void configureSsl(QNetworkRequest& request, const QUrl& url)
        {
            if (!url.scheme().startsWith("https"))
                return;

            QSslConfiguration sslConfig = request.sslConfiguration();
            sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
            request.setSslConfiguration(sslConfig);
        }
    }

    void Blob::save(Session* session, const QString& blobRef, const QByteArray& data)
    {
        QUrl url = buildBlobUrl(session, blobRef);
        if (!url.isValid())
            throw std::runtime_error("Invalid session or blob reference");

        QNetworkAccessManager manager;
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, "XenAdmin-Qt/1.0");
        configureSsl(request, url);

        QNetworkReply* reply = manager.put(request, data);
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() != QNetworkReply::NoError)
        {
            QString error = reply->errorString();
            reply->deleteLater();
            throw std::runtime_error(error.toStdString());
        }

        reply->deleteLater();
    }

    QByteArray Blob::load(Session* session, const QString& blobRef)
    {
        QUrl url = buildBlobUrl(session, blobRef);
        if (!url.isValid())
            throw std::runtime_error("Invalid session or blob reference");

        QNetworkAccessManager manager;
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, "XenAdmin-Qt/1.0");
        configureSsl(request, url);

        QNetworkReply* reply = manager.get(request);
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() != QNetworkReply::NoError)
        {
            QString error = reply->errorString();
            reply->deleteLater();
            throw std::runtime_error(error.toStdString());
        }

        QByteArray data = reply->readAll();
        reply->deleteLater();
        return data;
    }
}
