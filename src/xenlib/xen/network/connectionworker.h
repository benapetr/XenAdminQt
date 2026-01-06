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

#ifndef CONNECTIONWORKER_H
#define CONNECTIONWORKER_H

#include <QThread>
#include <QTcpSocket>
#include <QSslSocket>
#include <QString>
#include <QAtomicInt>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>

namespace Xen
{
    /**
     * @brief Structure for queuing API requests to worker thread
     */
    struct ApiRequest
    {
        int id;                 // Unique request ID
        QByteArray payload;     // request body
        bool processed = false; // Whether request has been processed
        QByteArray response;    // Response data (filled by worker)
        bool emitSignal = true; // Whether to emit apiResponse signal when done
                                // Set to false for sync/blocking calls that use waitForResponse()
                                // to avoid "Unknown request ID" warnings in async handlers
    };

    /**
     * @brief Worker thread for XenServer connection handling
     *
     * This class runs on a dedicated background thread and performs all network I/O
     * synchronously. This matches the C# XenAdmin architecture where ConnectWorkerThread
     * handles all socket operations without blocking the UI thread.
     *
     * The worker performs these steps sequentially:
     * 1. TCP connection
     * 2. SSL handshake
     * 3. Enter event polling loop to process queued API requests
     *
     * Note: Login is handled separately by XenSession after connection is established.
     * This matches the C# pattern where Session.login_with_password() is called from
     * the worker thread context but uses the Session object.
     *
     * All socket operations use blocking waits (waitForConnected, waitForReadyRead, etc.)
     * which is safe because this thread is dedicated to I/O and doesn't handle UI events.
     */
    class ConnectionWorker : public QThread
    {
        Q_OBJECT

        public:
            /**
             * @brief Construct a new Connection Worker
             *
             * @param hostname Server hostname or IP address
             * @param port Server port (usually 443 for HTTPS)
             * @param certManager Certificate manager for SSL validation (optional)
             * @param parent Parent QObject
             */
            explicit ConnectionWorker(const QString& hostname, int port,
                                      QObject* parent = nullptr);

            ~ConnectionWorker() override;

            /**
             * @brief Request the worker thread to stop gracefully
             *
             * Sets the stop flag which will cause the event polling loop to exit.
             * The thread will finish its current operation and then terminate.
             */
            void RequestStop();

            /**
             * @brief Queue an API request to be processed by the worker thread
             *
             * This is thread-safe and can be called from the main thread.
             * The worker will process the request and emit apiResponse when complete.
             *
             * @param data request body
             * @param emitSignal Whether to emit apiResponse signal when done (default true)
             *                   Set to false for sync/blocking calls to avoid spurious signals
             * @return Request ID for tracking the response
             */
            int QueueRequest(const QByteArray& data, bool emitSignal = true);

            /**
             * @brief Wait for a specific request to complete (blocking)
             *
             * Blocks the calling thread until the request is processed.
             * Use with caution - prefer using apiResponse signal instead.
             *
             * @param requestId The request ID returned from queueRequest
             * @param timeoutMs Timeout in milliseconds (default 30 seconds)
             * @return Response data, or empty if timeout/error
             */
            QByteArray WaitForResponse(int requestId, int timeoutMs = 60000);

        signals:
            /**
             * @brief Emitted to report connection progress
             * @param message Human-readable progress message
             */
            void ConnectionProgress(const QString& message);

            /**
             * @brief Emitted when TCP/SSL connection is established
             *
             * After this signal, the caller should use XenSession to login.
             * The worker thread will process queued API requests (including login).
             */
            void ConnectionEstablished();

            /**
             * @brief Emitted when connection or login fails
             * @param error Error message
             */
            void ConnectionFailed(const QString& error);

            /**
             * @brief Emitted when cache data is available
             * @param data API response data containing object updates
             */
            void CacheDataReceived(const QByteArray& data);

            /**
             * @brief Emitted when the worker thread is about to exit
             */
            void WorkerFinished();

            /**
             * @brief Emitted when an API request has been processed
             * @param requestId The request ID that was queued
             * @param response The API response data
             */
            void ApiResponse(int requestId, const QByteArray& response);

        protected:
            /**
             * @brief Main thread entry point
             *
             * Performs the connection sequence:
             * 1. TCP connect
             * 2. SSL handshake
             * 3. Emit connectionEstablished signal
             * 4. Enter event polling loop (processes queued API requests)
             *
             * Note: Login is handled by XenSession after connection is established.
             */
            void run() override;

        private slots:
            /**
             * @brief Handle SSL errors during handshake
             * @param errors List of SSL errors that occurred
             */
            void handleSslErrors(const QList<QSslError>& errors);

        private:
            /**
             * @brief Establish TCP connection synchronously
             * @return true if connection successful, false otherwise
             */
            bool connectToHostSync();

            /**
             * @brief Perform SSL handshake synchronously
             * @return true if SSL handshake successful, false otherwise
             */
            bool sslHandshakeSync();

            /**
             * @brief Enter event polling loop
             *
             * Continuously calls event.from() with 60 second timeout until stopped.
             * Also processes queued API requests between event polls.
             * Emits cacheDataReceived for each event batch.
             */
            void eventPollLoop();

            /**
             * @brief Process queued API requests
             *
             * Checks the request queue and processes any pending requests.
             * Called from event polling loop.
             */
            void processQueuedRequests();

            /**
             * @brief Send API request and wait for response synchronously
             *
             * Performs:
             * 1. Write HTTP POST request with request body
             * 2. Wait for bytes written
             * 3. Wait for response data
             * 4. Read complete HTTP response
             * 5. Parse and return body
             *
             * This is safe to block because it runs on the worker thread.
             *
             * @param request API request body
             * @return HTTP response body (API response) or empty on error
             */
            QByteArray sendRequestSync(const QByteArray& request);

            /**
             * @brief Read HTTP response from socket
             *
             * Handles chunked encoding and Content-Length headers.
             *
             * @param headers Output parameter - filled with HTTP headers
             * @return HTTP response body
             */
            QByteArray readHttpResponse(QMap<QString, QString>& headers);

            // Connection parameters
            QString m_hostname;
            int m_port;

            // Connection state
            QSslSocket* m_socket = nullptr;

            // Thread control
            QAtomicInt m_stopped = 0; // Thread-safe stop flag

            // Request queues for API calls
            QQueue<ApiRequest*> m_pendingQueue;   // Requests waiting to be processed
            QQueue<ApiRequest*> m_completedQueue; // Completed requests waiting for retrieval
            QMutex m_requestMutex;
            QWaitCondition m_requestCondition;
            QAtomicInt m_nextRequestId = 1;
    };

} // namespace Xen

#endif // CONNECTIONWORKER_H
