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

#ifndef EVENTPOLLER_H
#define EVENTPOLLER_H

#include <QObject>
#include <QTimer>
#include <QThread>
#include <QVariantMap>
#include <QVariantList>
#include <QStringList>

class XenAPI;
class XenConnection;
class XenSession;

/**
 * @brief Polls XenServer for events using event.from
 *
 * This class runs in a separate thread and continuously polls the XenServer
 * for events using the event.from API. Events are emitted as they arrive.
 *
 * IMPORTANT: EventPoller creates its own XenConnection/XenSession/XenAPI stack
 * to avoid blocking the main API request queue with long-poll event.from calls.
 *
 * The event.from API uses a token-based system:
 * - Initial call with empty token returns all current object state
 * - Subsequent calls return only changes since the last token
 * - Token must be preserved across calls
 */
class EventPoller : public QObject
{
    Q_OBJECT

public:
    explicit EventPoller(QObject* parent = nullptr);
    ~EventPoller();

    /**
     * @brief Initialize EventPoller by duplicating an existing session
     * Creates a separate connection stack to avoid blocking main API
     * @param originalSession Session to duplicate (must be logged in)
     */
    void initialize(XenSession* originalSession);

    /**
     * @brief Reset state and drop duplicated session/connection so a fresh session can be used.
     * Should be invoked on the EventPoller thread (use invokeMethod).
     */
    void reset();

    /**
     * @brief Initialize EventPoller with connection credentials (deprecated)
     * @deprecated Use initialize(XenSession*) instead
     */
    void initialize(const QString& hostname, int port, const QString& sessionId);

    /**
     * @brief Start polling for events
     * @param classes List of event classes to monitor (e.g., "VM", "host", "*" for all)
     * @param initialToken Token from initial event.from (empty to start fresh)
     */
    void start(const QStringList& classes = QStringList() << "*", const QString& initialToken = QString());

    /**
     * @brief Stop polling for events
     */
    void stop();

    /**
     * @brief Check if poller is currently running
     */
    bool isRunning() const;

    /**
     * @brief Get the current event token
     */
    QString currentToken() const;

signals:
    /**
     * @brief Emitted when an event is received
     * @param eventData Map containing: id, timestamp, class_, operation, opaqueRef, snapshot
     */
    void eventReceived(const QVariantMap& eventData);

    /**
     * @brief Emitted when a task is added (for task rehydration)
     * @param taskRef Task opaque reference
     * @param taskData Task snapshot data
     */
    void taskAdded(const QString& taskRef, const QVariantMap& taskData);

    /**
     * @brief Emitted when a task is modified (for task rehydration)
     * @param taskRef Task opaque reference
     * @param taskData Task snapshot data
     */
    void taskModified(const QString& taskRef, const QVariantMap& taskData);

    /**
     * @brief Emitted when a task is deleted (for task rehydration)
     * @param taskRef Task opaque reference
     */
    void taskDeleted(const QString& taskRef);

    /**
     * @brief Emitted when initial cache population is complete
     */
    void cachePopulated();

    /**
     * @brief Emitted when an error occurs
     */
    void errorOccurred(const QString& error);

    /**
     * @brief Emitted when connection is lost
     */
    void connectionLost();

private slots:
    void pollEvents();

private:
    class Private;
    Private* d;
};

#endif // EVENTPOLLER_H
