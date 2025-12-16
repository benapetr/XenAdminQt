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

#include "eventpoller.h"
#include "api.h"
#include "connection.h"
#include "session.h"
#include "jsonrpcclient.h"
#include <QDebug>
#include <QThread>

class EventPoller::Private
{
    public:
        // Own connection stack for event polling (separate from main API connection)
        XenConnection* connection;
        XenSession* session;
        XenRpcAPI* api;

        QString token;       // Current event token
        QStringList classes; // Event classes to monitor
        bool running;
        bool initialCachePopulated;
        bool initialized;
        QTimer* pollTimer;
        int consecutiveErrors;

        static const int POLL_TIMEOUT = 30; // 30 seconds - proper long-poll timeout (EventPoller runs on dedicated thread with own connection)
        static const int MAX_CONSECUTIVE_ERRORS = 3;

        Private()
            : connection(nullptr), session(nullptr), api(nullptr),
              token(""), running(false), initialCachePopulated(false),
              initialized(false), pollTimer(nullptr), consecutiveErrors(0)
        {
        }

        ~Private()
        {
            if (pollTimer)
            {
                delete pollTimer;
            }
            // Clean up our own connection stack
            if (api)
                delete api;
            if (session)
                delete session;
            if (connection)
                delete connection;
        }
};

EventPoller::EventPoller(QObject* parent) : QObject(parent), d(new Private())
{
    // Connection stack will be created in initialize()
}

EventPoller::~EventPoller()
{
    stop();
    delete d;
}

void EventPoller::reset()
{
    // Drop duplicated stack so a fresh session can be created on next initialize()
    if (this->d->running)
    {
        this->d->pollTimer->stop();
        this->d->running = false;
    }

    if (this->d->pollTimer)
    {
        delete this->d->pollTimer;
        this->d->pollTimer = nullptr;
    }

    if (this->d->api)
    {
        delete this->d->api;
        this->d->api = nullptr;
    }

    if (this->d->session)
    {
        delete this->d->session;
        this->d->session = nullptr;
    }

    if (this->d->connection)
    {
        delete this->d->connection;
        this->d->connection = nullptr;
    }

    this->d->token.clear();
    this->d->classes.clear();
    this->d->initialized = false;
    this->d->initialCachePopulated = false;
    this->d->consecutiveErrors = 0;

    qDebug() << "EventPoller: Reset duplicated session/connection";
}

void EventPoller::initialize(XenSession* originalSession)
{
    if (!originalSession)
    {
        qWarning() << "EventPoller: Cannot initialize with null session";
        return;
    }

    if (this->d->initialized)
    {
        QString existing = this->d->session ? this->d->session->getSessionId().left(20) + "..." : "null";
        QString incoming = originalSession->getSessionId().left(20) + "...";
        if (existing != incoming)
        {
            qWarning() << "EventPoller: Reinitializing with new session. Old="
                       << existing << "new=" << incoming;
            this->reset();
        } else
        {
            qWarning() << "EventPoller: Already initialized with same session";
            return;
        }
    }

    qDebug() << "EventPoller: Duplicating session for dedicated event polling connection";

    // Create a duplicate session with its own connection stack
    // This is the C# XenAdmin pattern - separate TCP connection prevents blocking
    this->d->session = XenSession::duplicateSession(originalSession, this);

    if (!this->d->session)
    {
        qWarning() << "EventPoller: Failed to duplicate session";
        return;
    } else
    {
        qDebug() << "EventPoller: Using duplicated session"
                 << this->d->session->getSessionId().left(20) + "...";
    }

    // The duplicated session owns its own XenConnection internally
    // We don't need to store connection separately - it's managed by the session

    // Create API wrapper for the duplicated session
    this->d->api = new XenRpcAPI(this->d->session, this);

    this->d->initialized = true;

    qDebug() << "EventPoller: Initialized with dedicated connection stack";
}

void EventPoller::initialize(const QString& hostname, int port, const QString& sessionId)
{
    Q_UNUSED(hostname);
    Q_UNUSED(port);
    Q_UNUSED(sessionId);

    qWarning() << "EventPoller::initialize(hostname, port, sessionId) is deprecated";
    qWarning() << "Use initialize(XenSession*) instead";
}

void EventPoller::start(const QStringList& classes, const QString& initialToken)
{
    if (!this->d->initialized)
    {
        qWarning() << "EventPoller: Not initialized - call initialize() first";
        return;
    }

    if (this->d->running)
    {
        qDebug() << "EventPoller already running";
        return;
    }

    // Create QTimer here (on the thread where start() is called) to avoid thread affinity issues
    if (!this->d->pollTimer)
    {
        this->d->pollTimer = new QTimer();
        connect(this->d->pollTimer, &QTimer::timeout, this, &EventPoller::pollEvents);
        qDebug() << "EventPoller: QTimer created on thread" << QThread::currentThread();
    }

    this->d->classes = classes;
    this->d->token = initialToken; // Use token from cache population instead of resetting to ""
    this->d->running = true;
    this->d->initialCachePopulated = false;
    this->d->consecutiveErrors = 0;

    qDebug() << "EventPoller starting with classes:" << classes;
    if (!initialToken.isEmpty())
    {
        qDebug() << "EventPoller: Starting with token from cache population:" << initialToken.left(20) + "...";
    } else
    {
        qDebug() << "EventPoller: Starting with empty token (fresh cache)";
    }

    // Start polling immediately
    this->pollEvents();
}

void EventPoller::stop()
{
    if (!this->d->running)
        return;

    qDebug() << "EventPoller stopping";

    this->d->running = false;
    this->d->pollTimer->stop();
    this->d->token = "";
    this->d->initialCachePopulated = false;
}

bool EventPoller::isRunning() const
{
    return this->d->running;
}

QString EventPoller::currentToken() const
{
    return this->d->token;
}

void EventPoller::pollEvents()
{
    if (!this->d->running)
        return;

    if (!this->d->api)
    {
        emit errorOccurred("XenAPI instance is null");
        this->stop();
        return;
    }

    // Call event.from with current token
    QVariantMap result = this->d->api->eventFrom(this->d->classes, this->d->token, this->d->POLL_TIMEOUT);

    if (result.isEmpty())
    {
        this->d->consecutiveErrors++;
        QString sessionIdPrefix = this->d->session ? this->d->session->getSessionId().left(12) + "..." : "null";
        QString tokenPrefix = this->d->token.left(16) + "...";
        // Surface more context, especially SESSION_INVALID occurrences
        qWarning() << "EventPoller: event.from returned empty result (error"
                   << this->d->consecutiveErrors << "of" << this->d->MAX_CONSECUTIVE_ERRORS << ")"
                   << "session" << sessionIdPrefix
                   << "token" << tokenPrefix
                   << "lastError" << Xen::JsonRpcClient::lastError();

        if (this->d->consecutiveErrors >= this->d->MAX_CONSECUTIVE_ERRORS)
        {
            qCritical() << "EventPoller: Too many consecutive errors, stopping";
            emit connectionLost();
            this->stop();
            return;
        }

        // Retry after a short delay
        this->d->pollTimer->start(5000); // 5 seconds
        return;
    }

    // Reset error counter on success
    this->d->consecutiveErrors = 0;

    // Extract new token
    if (result.contains("token"))
    {
        QString newToken = result["token"].toString();
        if (!newToken.isEmpty())
        {
            this->d->token = newToken;
        }
    }

    // Process events
    if (result.contains("events"))
    {
        QVariantList events = result["events"].toList();

        // Only log event count if there are actual events
        if (!events.isEmpty())
        {
            qDebug() << "EventPoller: Received" << events.size() << "events";
        }

        foreach (const QVariant& eventVar, events)
        {
            if (eventVar.type() == QVariant::Map)
            {
                QVariantMap eventData = eventVar.toMap();

                // Normalize JSON-RPC vs XML-RPC field naming so downstream code can use either
                if (!eventData.contains("class") && eventData.contains("class_"))
                    eventData.insert("class", eventData.value("class_"));
                if (!eventData.contains("class_") && eventData.contains("class"))
                    eventData.insert("class_", eventData.value("class"));
                if (!eventData.contains("opaqueRef") && eventData.contains("ref"))
                    eventData.insert("opaqueRef", eventData.value("ref"));
                if (!eventData.contains("ref") && eventData.contains("opaqueRef"))
                    eventData.insert("ref", eventData.value("opaqueRef"));

                // Emit all events - XenLib::onEventReceived will filter invalid ones
                emit eventReceived(eventData);

                // Emit task-specific signals for TaskRehydrationManager
                QString eventClass = eventData.value("class_", eventData.value("class")).toString();
                QString operation = eventData.value("operation").toString();
                QString opaqueRef = eventData.value("opaqueRef", eventData.value("ref")).toString();

                if (eventClass.toLower() == "task" && !opaqueRef.isEmpty())
                {
                    if (operation == "add")
                    {
                        QVariantMap snapshot = eventData.value("snapshot").toMap();
                        emit taskAdded(opaqueRef, snapshot);
                    } else if (operation == "mod")
                    {
                        QVariantMap snapshot = eventData.value("snapshot").toMap();
                        emit taskModified(opaqueRef, snapshot);
                    } else if (operation == "del")
                    {
                        emit taskDeleted(opaqueRef);
                    }
                }
            }
        }

        // Emit cache populated signal on first successful poll with events
        if (!this->d->initialCachePopulated && !events.isEmpty())
        {
            this->d->initialCachePopulated = true;
            qDebug() << "EventPoller: Initial cache populated";
            emit cachePopulated();
        }
    }

    // Continue polling immediately (event.from blocks until timeout or events arrive)
    if (this->d->running)
    {
        // Schedule next poll without delay - event.from already blocked for up to 30 seconds
        QTimer::singleShot(0, this, &EventPoller::pollEvents);
    }
}
