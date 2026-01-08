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

#ifndef XEN_API_H
#define XEN_API_H

#include "../xenlib_global.h"
#include <QtCore/QObject>
#include <QtCore/QVariant>

namespace XenAPI
{
    class Session;
}

class XENLIB_EXPORT XenRpcAPI : public QObject
{
    Q_OBJECT
    public:
        explicit XenRpcAPI(XenAPI::Session* session, QObject* parent = nullptr);
        ~XenRpcAPI();

        // Task operations
        QVariant GetTaskRecord(const QString& taskRef);
        QString GetTaskStatus(const QString& taskRef);
        double GetTaskProgress(const QString& taskRef);
        QString GetTaskResult(const QString& taskRef);
        QStringList GetTaskErrorInfo(const QString& taskRef);
        QVariantMap GetAllTaskRecords(); // Returns Dictionary<XenRef<Task>, Task> equivalent
        bool CancelTask(const QString& taskRef);
        bool DestroyTask(const QString& taskRef);

        // Task.other_config operations (for task rehydration)
        bool AddToTaskOtherConfig(const QString& taskRef, const QString& key, const QString& value);
        bool RemoveFromTaskOtherConfig(const QString& taskRef, const QString& key);
        QVariantMap GetTaskOtherConfig(const QString& taskRef);

        // Event operations
        // event.from - Get events since token (token="" for initial call, returns new token + events)
        QVariantMap EventFrom(const QStringList& classes, const QString& token, double timeout);
        // event.register - Register for specific event classes (legacy, not used in modern API)
        bool EventRegister(const QStringList& classes);
        // event.unregister - Unregister from event classes (legacy, not used in modern API)
        bool EventUnregister(const QStringList& classes);

        // JSON-RPC helper methods
        QByteArray BuildJsonRpcCall(const QString& method, const QVariantList& params);
        QVariant ParseJsonRpcResponse(const QByteArray& response);

    signals:
        void apiCallFailed(const QString& method, const QString& error);

    private:
        class Private;
        Private* d;
};

#endif // XEN_API_H
