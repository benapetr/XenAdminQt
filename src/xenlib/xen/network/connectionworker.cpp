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
#include <QCoreApplication>
#include <QSslConfiguration>
#include <QElapsedTimer>
#include <QDebug>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <limits>
#include <stdexcept>
#include "../jsonrpcclient.h"
#include "../failure.h"

namespace Xen
{
    namespace
    {
        constexpr int requestTimeoutMs = 60000;

        QByteArray requestError(const QByteArray& request, const QString& detail, bool mayHaveExecuted)
        {
            const QString message = (mayHaveExecuted
                ? QString("Request outcome is unknown: ")
                : QString("Request was not sent: ")) + detail;
            const QJsonObject original = QJsonDocument::fromJson(request).object();
            const QJsonObject error{{"code", -32098}, {"message", "TRANSPORT_ERROR"},
                                    {"data", QJsonArray{message}}};
            return QJsonDocument(QJsonObject{{"jsonrpc", "2.0"}, {"id", original.value("id")},
                                            {"error", error}}).toJson(QJsonDocument::Compact);
        }
    }
    ConnectionWorker::ConnectionWorker(const QString& hostname, int port, QObject* parent) : QThread(parent), m_hostname(hostname), m_port(port)
    {
    }

    ConnectionWorker::~ConnectionWorker()
    {
        this->RequestStop();

        // Wait for thread to finish (max 5 seconds)
        if (!this->wait(5000))
        {
            qWarning() << "ConnectionWorker: Thread did not finish in time, terminating";
            this->terminate();
            this->wait();
        }
        qDeleteAll(this->m_pendingQueue);
        qDeleteAll(this->m_completedQueue);
    }

    void ConnectionWorker::RequestStop()
    {
        this->m_stopped.storeRelaxed(1);

        // Wake up the worker thread if it's waiting on the request queue
        this->m_requestCondition.wakeAll();
    }

    int ConnectionWorker::QueueRequest(const QByteArray& data, bool emitSignal)
    {
        QMutexLocker locker(&this->m_requestMutex);

        // Create request
        ApiRequest* request = new ApiRequest();
        request->queuedAt.start();
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

    QByteArray ConnectionWorker::WaitForResponse(int requestId, int timeoutMs)
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

        QMutexLocker locker(&this->m_requestMutex);
        // Resolve a completion racing with the deadline before abandoning it.
        for (int i = 0; i < this->m_completedQueue.size(); ++i)
        {
            if (this->m_completedQueue.at(i)->id == requestId)
            {
                ApiRequest* request = this->m_completedQueue.takeAt(i);
                const QByteArray response = request->response;
                delete request;
                return response;
            }
        }
        // A timed-out queued mutation must never be sent later.
        for (int i = 0; i < this->m_pendingQueue.size(); ++i)
        {
            if (this->m_pendingQueue.at(i)->id == requestId)
            {
                ApiRequest* request = this->m_pendingQueue.takeAt(i);
                const QByteArray response = requestError(request->payload, "Timed out in request queue", false);
                delete request;
                return response;
            }
        }
        if (this->m_activeRequest && this->m_activeRequest->id == requestId)
            this->m_activeRequest->abandoned.storeRelease(1);
        qWarning() << "ConnectionWorker: Timeout waiting for response to request" << requestId;
        return requestError(QByteArray(), "Timed out waiting for a response", true);
    }

    void ConnectionWorker::handleSslErrors(const QList<QSslError>& errors)
    {
        if (!this->m_socket)
        {
            qWarning() << "ConnectionWorker::handleSslErrors called with null socket!";
            return;
        }

        qDebug() << "ConnectionWorker: SSL errors occurred, count:" << errors.size();
        qDebug() << "ConnectionWorker: Socket state:" << this->m_socket->state();
        qDebug() << "ConnectionWorker: Socket encrypted:" << this->m_socket->isEncrypted();

        for (const QSslError& error : errors)
        {
             qDebug() << "  - Error type:" << error.error() << "Message:" << error.errorString();
         }

        // C# SSL.ValidateServerCertificate: trust on first use, may ask the user.
        // Runs inside waitForEncrypted() on this worker thread.
        if (XenCertificateManager::instance()->ValidateServerCertificate(this->m_socket->peerCertificate(),
                                                                        errors, this->m_hostname))
        {
            this->m_socket->ignoreSslErrors();
        } else
        {
            qWarning() << "ConnectionWorker: server certificate for" << this->m_hostname << "was rejected";
            this->m_certificateRejected.storeRelease(1);
        }
    }

    bool ConnectionWorker::IsTransportConnected() const
    {
        return this->m_transportConnected.loadAcquire() != 0;
    }

    void ConnectionWorker::setTransportConnected(bool connected)
    {
        if (this->m_transportConnected.fetchAndStoreOrdered(connected ? 1 : 0) != (connected ? 1 : 0))
            emit this->TransportStateChanged(connected);
    }

    void ConnectionWorker::invalidateTransport()
    {
        this->setTransportConnected(false);
        if (this->m_socket)
        {
            this->m_socket->abort();
            delete this->m_socket;
            this->m_socket = nullptr;
        }
    }

    bool ConnectionWorker::ensureTransportConnected(int timeoutMs)
    {
        if (this->m_stopped.loadRelaxed() || timeoutMs <= 0)
            return false;

        if (this->m_socket && this->m_socket->state() == QAbstractSocket::ConnectedState && this->m_socket->isEncrypted())
        {
            // Process an already-arrived peer close without sending a network probe.
            // This is advisory: the peer can still close immediately afterwards.
            if (this->m_socket->bytesAvailable() == 0)
                this->m_socket->waitForReadyRead(1);
            if (this->m_socket->state() == QAbstractSocket::ConnectedState &&
                this->m_socket->isEncrypted() && this->m_socket->bytesAvailable() == 0)
                return true;
        }

        this->invalidateTransport();
        this->m_socket = new QSslSocket();
        connect(this->m_socket, QOverload<const QList<QSslError>&>::of(&QSslSocket::sslErrors),
                this, &ConnectionWorker::handleSslErrors, Qt::DirectConnection);
        connect(this->m_socket, &QSslSocket::disconnected, this, [this]() {
            this->setTransportConnected(false);
        }, Qt::DirectConnection);

        QElapsedTimer timer;
        timer.start();
        if (!this->connectToHostSync(timeoutMs) ||
            !this->sslHandshakeSync(qMax(0, timeoutMs - int(timer.elapsed()))))
        {
            this->invalidateTransport();
            return false;
        }
        this->setTransportConnected(true);
        return true;
    }

    void ConnectionWorker::run()
    {
        if (this->ensureTransportConnected(this->connectionTimeoutMs_()))
        {
            emit this->ConnectionEstablished();
            this->eventPollLoop();
        } else
        {
            emit this->ConnectionFailed(this->m_certificateRejected.loadAcquire()
                ? tr("The security certificate of %1 was not accepted").arg(this->m_hostname)
                : "Failed to connect to " + this->m_hostname);
        }
        this->invalidateTransport();

        // Wake callers whose requests were queued while the worker was stopping.
        QMutexLocker locker(&this->m_requestMutex);
        while (!this->m_pendingQueue.isEmpty())
        {
            ApiRequest* request = this->m_pendingQueue.dequeue();
            request->response = requestError(request->payload, "Connection worker stopped", false);
            request->processed = true;
            this->m_completedQueue.enqueue(request);
            if (request->emitSignal)
                emit this->ApiResponse(request->id, request->response);
        }
        this->m_requestCondition.wakeAll();
        locker.unlock();
        emit this->WorkerFinished();
    }

    bool ConnectionWorker::connectToHostSync(int timeoutMs)
    {
        emit this->ConnectionProgress("Connecting to " + this->m_hostname + ":" + QString::number(this->m_port) + "...");
        // Qt verifies the server certificate; handleSslErrors() decides about any errors
        this->m_certificateRejected.storeRelease(0);
        this->m_socket->connectToHostEncrypted(this->m_hostname, this->m_port);
        if (timeoutMs <= 0 || !this->m_socket->waitForConnected(timeoutMs))
        {
            qWarning() << "ConnectionWorker: TCP connection failed -" << this->m_socket->errorString();
            return false;
        }
        return true;
    }

    bool ConnectionWorker::sslHandshakeSync(int timeoutMs)
    {
        if (this->m_socket->isEncrypted())
            return true;
        if (timeoutMs <= 0 || !this->m_socket->waitForEncrypted(timeoutMs))
        {
            qWarning() << "ConnectionWorker: TLS handshake failed -" << this->m_socket->errorString();
            return false;
        }
        return true;
    }

    int ConnectionWorker::connectionTimeoutMs_() const
    {
        static constexpr int defaultTimeoutMs = 30000;
        static constexpr int minTimeoutMs = 1000;
        static constexpr int maxTimeoutMs = 300000;

        const QCoreApplication* app = QCoreApplication::instance();
        if (!app)
            return defaultTimeoutMs;

        bool ok = false;
        int value = app->property("ConnectionTimeoutMs").toInt(&ok);
        if (!ok)
            return defaultTimeoutMs;

        if (value < minTimeoutMs)
            value = minTimeoutMs;
        if (value > maxTimeoutMs)
            value = maxTimeoutMs;
        return value;
    }

    void ConnectionWorker::eventPollLoop()
    {
        // qDebug() << "ConnectionWorker: Entering event polling loop";

        while (!this->m_stopped.loadRelaxed())
        {
            // Process any queued API requests
            this->processQueuedRequests();

            // Events are polled by EventPoller; sleep to avoid a busy loop
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
            this->m_activeRequest = request;
            locker.unlock(); // Unlock while processing

            // qDebug() << timestamp() << "ConnectionWorker: Processing request" << request->id;

            // Send request synchronously (safe on worker thread)
            const int remaining = int(qMax<qint64>(0, requestTimeoutMs - request->queuedAt.elapsed()));
            const QByteArray response = this->sendRequestSync(request->payload, remaining, request->abandoned);

            // Store response and mark as processed
            locker.relock();
            this->m_activeRequest = nullptr;
            request->response = response;
            request->processed = true;

            if (request->abandoned.loadAcquire())
            {
                delete request;
                continue;
            }
            // Move to completed queue so waitForResponse() can retrieve it
            this->m_completedQueue.enqueue(request);

            // Emit signal to main thread (only if requested)
            // For blocking/sync calls, emitSignal is false to avoid spurious "Unknown request ID" warnings
            if (request->emitSignal)
            {
                emit ApiResponse(request->id, response);
            }

            // Wake up any threads waiting for this response
            this->m_requestCondition.wakeAll();

            // qDebug() << timestamp() << "ConnectionWorker: Completed request" << request->id;
        }
    }

    QByteArray ConnectionWorker::sendRequestSync(const QByteArray& request, int timeoutMs, const QAtomicInt& abandoned)
    {
        QElapsedTimer timer;
        timer.start();
        bool mayHaveExecuted = false;
        try
        {
            if (abandoned.loadAcquire())
                throw std::runtime_error("Request was abandoned before sending");
            if (!this->ensureTransportConnected(qMin(timeoutMs, this->connectionTimeoutMs_())))
                throw std::runtime_error(this->m_certificateRejected.loadAcquire()
                    ? tr("The security certificate of %1 was not accepted").arg(this->m_hostname).toStdString()
                    : std::string("Could not establish TCP/TLS transport"));
            if (abandoned.loadAcquire() || this->m_stopped.loadRelaxed() || timer.elapsed() >= timeoutMs)
                throw std::runtime_error("Request cancelled or timed out before sending");

            const QByteArray trimmed = request.trimmed();
            const bool json = trimmed.startsWith('{') || trimmed.startsWith('[');
            QByteArray httpRequest = json ? "POST /jsonrpc HTTP/1.1\r\n" : "POST /RPC2 HTTP/1.1\r\n";
            httpRequest += "Host: " + this->m_hostname.toUtf8() + "\r\n";
            httpRequest += "User-Agent: XenAdminQt/1.0\r\n";
            httpRequest += json ? "Content-Type: application/json\r\n" : "Content-Type: text/xml\r\n";
            httpRequest += "Content-Length: " + QByteArray::number(request.size()) + "\r\n";
            httpRequest += "Connection: keep-alive\r\n\r\n";
            httpRequest += request;

            // Once write is attempted, failure is ambiguous. Never replay this request.
            mayHaveExecuted = true;
            if (this->m_socket->write(httpRequest) != httpRequest.size())
                throw std::runtime_error("Failed to queue the complete HTTP request");
            while (this->m_socket->bytesToWrite() > 0 || this->m_socket->encryptedBytesToWrite() > 0)
            {
                const int remaining = qMax(0, timeoutMs - int(timer.elapsed()));
                if (this->m_stopped.loadRelaxed() || remaining == 0 || !this->m_socket->waitForBytesWritten(remaining))
                    throw std::runtime_error("Failed while writing HTTP request");
            }

            QMap<QString, QString> headers;
            const QByteArray response = this->readHttpResponse(headers, timer, timeoutMs);
            if (json)
            {
                try
                {
                    JsonRpcClient::parseJsonRpcResponseOrThrow(response);
                } catch (const Failure&)
                {
                    // A valid API rejection is not a broken transport. Pass it to the caller.
                }
            }
            if (headers.value("connection").toLower() == "close")
                this->invalidateTransport();
            return response;
        } catch (const std::exception& error)
        {
            qWarning() << "ConnectionWorker: Request failed on" << this->m_hostname << ':' << this->m_port
                       << error.what() << (mayHaveExecuted ? "(outcome unknown)" : "(not sent)");
            this->invalidateTransport();
            return requestError(request, QString::fromUtf8(error.what()), mayHaveExecuted);
        }
    }

    QByteArray ConnectionWorker::readHttpResponse(QMap<QString, QString>& headers, QElapsedTimer& timer, int timeoutMs)
    {
        const auto waitForData = [this, &timer, timeoutMs]() {
            const int remaining = qMax(0, timeoutMs - int(timer.elapsed()));
            return !this->m_stopped.loadRelaxed() && remaining > 0 && this->m_socket->waitForReadyRead(remaining);
        };
        const auto readLine = [this, &waitForData]() {
            while (!this->m_socket->canReadLine())
            {
                if (this->m_socket->bytesAvailable() > 65536 ||
                    (!waitForData() && !this->m_socket->canReadLine()))
                    throw std::runtime_error("Incomplete HTTP response headers or chunk framing");
            }
            const QByteArray line = this->m_socket->readLine();
            if (line.size() > 65536)
                throw std::runtime_error("HTTP response header line too long");
            return line;
        };
        const auto readExact = [this, &waitForData](qint64 length) {
            if (length < 0 || length > std::numeric_limits<int>::max())
                throw std::runtime_error("Invalid HTTP response length");
            QByteArray data;
            while (data.size() < length)
            {
                if (this->m_socket->bytesAvailable() == 0 && !waitForData() && this->m_socket->bytesAvailable() == 0)
                    throw std::runtime_error("Incomplete HTTP response body");
                const QByteArray chunk = this->m_socket->read(length - data.size());
                if (chunk.isEmpty())
                    throw std::runtime_error("Incomplete HTTP response body");
                data += chunk;
            }
            return data;
        };

        int status = 0;
        QByteArray protocol;
        do
        {
            headers.clear();
            const QList<QByteArray> statusLine = readLine().trimmed().split(' ');
            if (statusLine.size() < 2 || (statusLine[0] != "HTTP/1.1" && statusLine[0] != "HTTP/1.0"))
                throw std::runtime_error("Invalid HTTP response status line");
            protocol = statusLine[0];
            bool validStatus = false;
            status = statusLine[1].toInt(&validStatus);
            if (!validStatus || status < 100 || status > 599 || status == 101)
                throw std::runtime_error("Invalid HTTP response status");
            int headerBytes = 0;
            while (true)
            {
                const QByteArray line = readLine();
                headerBytes += line.size();
                if (headerBytes > 65536)
                    throw std::runtime_error("HTTP response headers too long");
                if (line == "\r\n" || line == "\n")
                    break;
                const int colon = line.indexOf(':');
                if (colon <= 0)
                    throw std::runtime_error("Invalid HTTP response header");
                const QString name = QString::fromLatin1(line.left(colon)).trimmed().toLower();
                const QString value = QString::fromLatin1(line.mid(colon + 1)).trimmed();
                if (headers.contains(name) && (name == "content-length" || name == "transfer-encoding"))
                    throw std::runtime_error("Ambiguous HTTP response framing");
                headers.insert(name, value);
            }
        } while (status < 200);

        if (status < 200 || status >= 300)
            throw std::runtime_error(QString("HTTP request failed with status %1").arg(status).toStdString());

        if (headers.contains("transfer-encoding"))
        {
            if (headers.value("transfer-encoding").toLower() != "chunked" || headers.contains("content-length"))
                throw std::runtime_error("Unsupported or ambiguous HTTP transfer encoding");
            QByteArray body;
            while (true)
            {
                bool ok = false;
                const qint64 size = readLine().trimmed().split(';').first().toLongLong(&ok, 16);
                if (!ok || size < 0 || size > std::numeric_limits<int>::max() - body.size())
                    throw std::runtime_error("Invalid HTTP chunk size");
                if (size == 0)
                {
                    int trailerBytes = 0;
                    while (true)
                    {
                        const QByteArray trailer = readLine();
                        trailerBytes += trailer.size();
                        if (trailerBytes > 65536)
                            throw std::runtime_error("HTTP trailers too long");
                        if (trailer == "\r\n" || trailer == "\n")
                            return body;
                    }
                }
                body += readExact(size);
                if (readExact(2) != "\r\n")
                    throw std::runtime_error("Invalid HTTP chunk terminator");
            }
        }
        if (headers.contains("content-length"))
        {
            bool ok = false;
            const qint64 length = headers.value("content-length").toLongLong(&ok);
            if (!ok)
                throw std::runtime_error("Invalid HTTP Content-Length");
            return readExact(length);
        }
        if (protocol == "HTTP/1.0" || headers.value("connection").toLower() == "close")
        {
            QByteArray body;
            while (true)
            {
                body += this->m_socket->readAll();
                if (this->m_socket->state() == QAbstractSocket::UnconnectedState)
                    return body;
                if (!waitForData() && this->m_socket->state() != QAbstractSocket::UnconnectedState)
                    throw std::runtime_error("Timed out reading close-delimited HTTP response");
            }
        }
        throw std::runtime_error("HTTP response has no supported body framing");
    }

} // namespace Xen
