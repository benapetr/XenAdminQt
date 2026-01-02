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

#include "connectionworker.h"
#include "certificatemanager.h"
#include <QSslConfiguration>
#include <QElapsedTimer>
#include <QDebug>
#include <QDateTime>

namespace Xen
{
    ConnectionWorker::ConnectionWorker(const QString& hostname, int port, QObject* parent)
        : QThread(parent), m_hostname(hostname), m_port(port), m_socket(nullptr), m_stopped(0), m_nextRequestId(1)
    {
        this->m_certManager = XenCertificateManager::instance();
    }

    ConnectionWorker::~ConnectionWorker()
    {
        this->requestStop();

        // Wait for thread to finish (max 5 seconds)
        if (!this->wait(5000))
        {
            qWarning() << "ConnectionWorker: Thread did not finish in time, terminating";
            this->terminate();
            this->wait();
        }
    }

    void ConnectionWorker::requestStop()
    {
        this->m_stopped.storeRelaxed(1);

        // Wake up the worker thread if it's waiting on the request queue
        this->m_requestCondition.wakeAll();
    }

    int ConnectionWorker::queueRequest(const QByteArray& data, bool emitSignal)
    {
        QMutexLocker locker(&this->m_requestMutex);

        // Create request
        ApiRequest* request = new ApiRequest();
        request->id = this->m_nextRequestId.fetchAndAddRelaxed(1);
        request->payload = data;
        request->processed = false;
        request->emitSignal = emitSignal; // Set signal emission flag

        // Add to pending queue
        this->m_pendingQueue.enqueue(request);

        // Wake up worker thread to process request
        this->m_requestCondition.wakeOne();

        // qDebug() << "ConnectionWorker: Queued request" << request->id;

        return request->id;
    }

    QByteArray ConnectionWorker::waitForResponse(int requestId, int timeoutMs)
    {
        QElapsedTimer timer;
        timer.start();

        while (timer.elapsed() < timeoutMs)
        {
            QMutexLocker locker(&this->m_requestMutex);

            // Search for the request in the completed queue
            for (int i = 0; i < this->m_completedQueue.size(); ++i)
            {
                ApiRequest* req = this->m_completedQueue.at(i);
                if (req->id == requestId)
                {
                    // Found it! Remove from completed queue and return response
                    this->m_completedQueue.removeAt(i);
                    QByteArray response = req->response;
                    delete req;
                    return response;
                }
            }

            // Wait for signal with remaining timeout
            int remainingTimeout = timeoutMs - timer.elapsed();
            if (remainingTimeout > 0)
            {
                this->m_requestCondition.wait(&this->m_requestMutex, remainingTimeout);
            }
        }

        qWarning() << "ConnectionWorker: Timeout waiting for response to request" << requestId;
        return QByteArray();
    }

    void ConnectionWorker::handleSslErrors(const QList<QSslError>& errors)
    {
        if (!this->m_socket)
        {
            qWarning() << "ConnectionWorker::handleSslErrors called with null socket!";
            return;
        }

        // qDebug() << "ConnectionWorker: SSL errors occurred, count:" << errors.size();
        // qDebug() << "ConnectionWorker: Socket state:" << this->m_socket->state();
        // qDebug() << "ConnectionWorker: Socket encrypted:" << this->m_socket->isEncrypted();

        // for (const QSslError& error : errors)
        // {
        //     qDebug() << "  - Error type:" << error.error() << "Message:" << error.errorString();
        // }

        // For XenServer/XCP-ng with self-signed certificates, we accept them
        if (this->m_certManager->getAllowSelfSigned())
        {
            // qDebug() << "ConnectionWorker: Ignoring SSL errors due to allowSelfSigned policy";
            // qDebug() << "ConnectionWorker: Calling ignoreSslErrors()...";
            this->m_socket->ignoreSslErrors();
            // qDebug() << "ConnectionWorker: ignoreSslErrors() called, socket state:" << this->m_socket->state();
            return;
        }

        // Validate certificate if we're not accepting all self-signed
        if (!this->m_socket->peerCertificate().isNull())
        {
            if (this->m_certManager->validateCertificate(this->m_socket->peerCertificate(), this->m_hostname))
            {
                // qDebug() << "ConnectionWorker: Certificate validated, ignoring SSL errors";
                this->m_socket->ignoreSslErrors();
            } else
            {
                qDebug() << "ConnectionWorker: Certificate validation FAILED";
            }
        } else
        {
            // qDebug() << "ConnectionWorker: Peer certificate is NULL";
        }
    }

    void ConnectionWorker::run()
    {
        // qDebug() << timestamp() << "ConnectionWorker: Thread started for" << this->m_hostname;

        // Create socket on this thread (important for thread affinity)
        this->m_socket = new QSslSocket();

        // Connect certificate validation with DIRECT connection to avoid queued signals after cleanup
        // This is critical because SSL errors can arrive after socket deletion if using queued connection
        connect(this->m_socket, QOverload<const QList<QSslError>&>::of(&QSslSocket::sslErrors), this, &ConnectionWorker::handleSslErrors, Qt::DirectConnection);

        // qDebug() << "ConnectionWorker: Certificate manager allowSelfSigned =" << this->m_certManager->getAllowSelfSigned();

        // Step 1: TCP connection
        if (!this->connectToHostSync())
        {
            emit connectionFailed("Failed to connect to " + this->m_hostname);
            goto cleanup;
        }

        /*if (this->m_stopped.loadRelaxed())
        {
            qDebug() << "ConnectionWorker: Stop requested after TCP connect";
            goto cleanup;
        }

        // Step 2: SSL handshake
        if (!this->sslHandshakeSync())
        {
            emit connectionFailed("SSL handshake failed");
            goto cleanup;
        }

        if (this->m_stopped.loadRelaxed())
        {
            qDebug() << "ConnectionWorker: Stop requested after SSL handshake";
            goto cleanup;
        }*/

        // Notify main thread that TCP/SSL connection is ready
        // The caller (XenLib) will now use XenSession to login
        // qDebug() << timestamp() << "ConnectionWorker: TCP/SSL connection established, ready for login";
        emit connectionEstablished();

        // Enter event polling loop - this processes queued API requests (including login from XenSession)
        // qDebug() << timestamp() << "ConnectionWorker: Entering event poll loop";
        this->eventPollLoop();

    cleanup:
        // qDebug() << "ConnectionWorker: Cleaning up";

        if (this->m_socket)
        {
            if (this->m_socket->state() == QAbstractSocket::ConnectedState)
            {
                this->m_socket->disconnectFromHost();
                this->m_socket->waitForDisconnected(1000);
            }
            this->m_socket->deleteLater();
            this->m_socket = nullptr;
        }

        emit workerFinished();
        // qDebug() << "ConnectionWorker: Thread finished";
    }

    bool ConnectionWorker::connectToHostSync()
    {
        emit connectionProgress("Connecting to " + this->m_hostname + ":" + QString::number(this->m_port) + "...");

        // Start encrypted connection
        this->m_socket->setPeerVerifyMode(QSslSocket::VerifyNone);
        this->m_socket->connectToHostEncrypted(this->m_hostname, this->m_port);

        // Block this thread (worker thread) until connected
        // Main thread is unaffected
        if (!this->m_socket->waitForConnected(30000))
        {
            qWarning() << "ConnectionWorker: TCP connection timeout -" << this->m_socket->errorString();
            return false;
        }

        // qDebug() << "ConnectionWorker: TCP connection established";
        return true;
    }

    bool ConnectionWorker::sslHandshakeSync()
    {
        emit connectionProgress("Performing SSL handshake...");

        // qDebug() << "ConnectionWorker: Waiting for SSL handshake, socket state:" << this->m_socket->state();
        // qDebug() << "ConnectionWorker: Socket encrypted before wait:" << this->m_socket->isEncrypted();

        // Wait for SSL handshake to complete
        // The socket is already doing the handshake from connectToHostEncrypted
        if (!this->m_socket->waitForEncrypted(30000))
        {
            qWarning() << "ConnectionWorker: SSL handshake timeout";
            qWarning() << "  - Socket state:" << this->m_socket->state();
            qWarning() << "  - Socket encrypted:" << this->m_socket->isEncrypted();
            qWarning() << "  - Error string:" << this->m_socket->errorString();
            qWarning() << "  - SSL errors:" << this->m_socket->sslHandshakeErrors();
            return false;
        }

        // qDebug() << "ConnectionWorker: SSL handshake complete";
        // qDebug() << "ConnectionWorker: Socket encrypted:" << this->m_socket->isEncrypted();
        return true;
    }

    void ConnectionWorker::eventPollLoop()
    {
        // qDebug() << "ConnectionWorker: Entering event polling loop";

        while (!this->m_stopped.loadRelaxed())
        {
            // Process any queued API requests
            this->processQueuedRequests();

            // TODO: Implement event.from() polling
            // For now, just sleep to avoid busy loop
            QThread::msleep(100);
        }

        // qDebug() << "ConnectionWorker: Exiting event polling loop";
    }

    void ConnectionWorker::processQueuedRequests()
    {
        QMutexLocker locker(&this->m_requestMutex);

        // Process all pending requests
        while (!this->m_pendingQueue.isEmpty())
        {
            // Take request from pending queue
            ApiRequest* request = this->m_pendingQueue.dequeue();
            locker.unlock(); // Unlock while processing

            // qDebug() << timestamp() << "ConnectionWorker: Processing request" << request->id;

            // Send request synchronously (safe on worker thread)
            QByteArray response = this->sendRequestSync(request->payload);

            // Store response and mark as processed
            locker.relock();
            request->response = response;
            request->processed = true;

            // Move to completed queue so waitForResponse() can retrieve it
            this->m_completedQueue.enqueue(request);

            // Emit signal to main thread (only if requested)
            // For blocking/sync calls, emitSignal is false to avoid spurious "Unknown request ID" warnings
            if (request->emitSignal)
            {
                emit apiResponse(request->id, response);
            }

            // Wake up any threads waiting for this response
            this->m_requestCondition.wakeAll();

            // qDebug() << timestamp() << "ConnectionWorker: Completed request" << request->id;
        }
    }

    QByteArray ConnectionWorker::sendRequestSync(const QByteArray& request)
    {
        // Build HTTP POST request
        QByteArray httpRequest;

        // Auto-detect content type and endpoint: JSON uses /jsonrpc, legacy XML uses /RPC2
        QString endpoint = "/RPC2";
        QString contentType = "text/xml";
        if (request.trimmed().startsWith("{") || request.trimmed().startsWith("["))
        {
            endpoint = "/jsonrpc";
            contentType = "application/json";
        }

        httpRequest += "POST " + endpoint.toLatin1() + " HTTP/1.1\r\n";
        httpRequest += "Host: " + this->m_hostname.toUtf8() + "\r\n";
        httpRequest += "User-Agent: XenAdminQt/1.0\r\n";
        httpRequest += "Content-Type: " + contentType.toLatin1() + "\r\n";
        httpRequest += "Content-Length: " + QByteArray::number(request.size()) + "\r\n";
        httpRequest += "Connection: keep-alive\r\n";
        httpRequest += "\r\n";
        httpRequest += request;

        // qDebug() << "ConnectionWorker: Sending request," << httpRequest.size() << "bytes - Method:" << methodName;

        // Write request to socket
        qint64 written = this->m_socket->write(httpRequest);
        if (written != httpRequest.size())
        {
            qWarning() << "ConnectionWorker: Failed to write complete request";
            return QByteArray();
        }

        // Wait for write to complete (synchronous - safe on worker thread)
        if (!this->m_socket->waitForBytesWritten(30000))
        {
            qWarning() << "ConnectionWorker: Timeout waiting for bytes written -" << this->m_socket->errorString();
            return QByteArray();
        }

        // qDebug() << "ConnectionWorker: Request sent, waiting for response...";

        // Wait for response data (synchronous - safe on worker thread)
        if (!this->m_socket->waitForReadyRead(30000))
        {
            qWarning() << "ConnectionWorker: Timeout waiting for response -" << this->m_socket->errorString();
            // Check if data arrived anyway (race condition handling)
            if (this->m_socket->bytesAvailable() == 0)
            {
                return QByteArray();
            }
            // qDebug() << "ConnectionWorker: Data available despite timeout";
        }

        // qDebug() << "ConnectionWorker: Response received," << this->m_socket->bytesAvailable() << "bytes available";

        // fprintf(stderr, "[WORKER] sendRequestSync: About to read HTTP response, %lld bytes available\n",
        //         this->m_socket->bytesAvailable());
        // fflush(stderr);

        // Read HTTP response
        QMap<QString, QString> headers;
        QByteArray responseBody = this->readHttpResponse(headers);

        // qDebug() << "ConnectionWorker: Response body size:" << responseBody.size();

        // fprintf(stderr, "[WORKER] sendRequestSync: Got response body, size=%lld\n",
        //         (long long)responseBody.size());
        // fflush(stderr);

        return responseBody;
    }

    QByteArray ConnectionWorker::readHttpResponse(QMap<QString, QString>& headers)
    {
        QByteArray responseData;

        /*fprintf(stderr, "[WORKER] readHttpResponse: Starting to read HTTP response\n");
        fflush(stderr);*/

        // Read until we have the headers
        while (this->m_socket->canReadLine() || this->m_socket->waitForReadyRead(5000))
        {
            QByteArray line = this->m_socket->readLine();
            responseData += line;

            /*fprintf(stderr, "[WORKER] readHttpResponse: Read line (%lld bytes): %s",
                    (long long)line.size(), line.left(80).constData());
            fflush(stderr);*/

            // Empty line marks end of headers
            if (line == "\r\n" || line == "\n")
            {
                /*fprintf(stderr, "[WORKER] readHttpResponse: Found end of headers\n");
                fflush(stderr);*/
                break;
            }

            // Parse header line
            int colonPos = line.indexOf(':');
            if (colonPos > 0)
            {
                QString headerName = QString::fromLatin1(line.left(colonPos)).trimmed();
                QString headerValue = QString::fromLatin1(line.mid(colonPos + 1)).trimmed();
                // Store with lowercase key for case-insensitive lookup
                headers[headerName.toLower()] = headerValue;
                /*fprintf(stderr, "[WORKER] readHttpResponse: Header: %s = %s\n",
                        headerName.toUtf8().constData(), headerValue.toUtf8().constData());
                fflush(stderr);*/
            }
        }

        // Check for Content-Length (case-insensitive)
        int contentLength = headers.value("content-length", "-1").toInt();
        /*fprintf(stderr, "[WORKER] readHttpResponse: Content-Length = %d (from headers[\"content-length\"])\n", contentLength);
        fflush(stderr);*/

        if (contentLength > 0)
        {
            /*fprintf(stderr, "[WORKER] readHttpResponse: Reading %d bytes of body\n", contentLength);
            fflush(stderr);*/

            // Read exact number of bytes
            QByteArray body;
            while (body.size() < contentLength)
            {
                if (this->m_socket->bytesAvailable() > 0 || this->m_socket->waitForReadyRead(5000))
                {
                    QByteArray chunk = this->m_socket->read(contentLength - body.size());
                    body += chunk;
                    /*fprintf(stderr, "[WORKER] readHttpResponse: Read %lld bytes, total %lld/%d\n",
                            (long long)chunk.size(), (long long)body.size(), contentLength);
                    fflush(stderr);*/
                } else
                {
                    // fprintf(stderr, "[WORKER] readHttpResponse: Timeout reading body\n");
                    // fflush(stderr);
                    qWarning() << "ConnectionWorker: Timeout reading response body";
                    break;
                }
            }

            /*fprintf(stderr, "[WORKER] readHttpResponse: Body complete, size=%lld\n",
                    (long long)body.size());
            fprintf(stderr, "[WORKER] readHttpResponse: Body content (first 200 chars): %s\n",
                    body.left(200).constData());
            fflush(stderr);*/

            return body;
        } else
        {
            // fprintf(stderr, "[WORKER] readHttpResponse: No Content-Length, reading until timeout\n");
            // fflush(stderr);

            // No Content-Length, read until connection closes or timeout
            // (should not happen with keep-alive)
            QByteArray body;
            while (this->m_socket->waitForReadyRead(1000))
            {
                body += this->m_socket->readAll();
            }

            /*fprintf(stderr, "[WORKER] readHttpResponse: Body complete (no CL), size=%lld\n",
                    (long long)body.size());
            fflush(stderr);*/

            return body;
        }
    }

} // namespace Xen
