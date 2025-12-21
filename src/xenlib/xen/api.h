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

class XenSession;

class XENLIB_EXPORT XenRpcAPI : public QObject
{
    Q_OBJECT
    public:
        explicit XenRpcAPI(XenSession* session, QObject* parent = nullptr);
        ~XenRpcAPI();

        // Session info
        QString getSessionId() const;

        // Bond operations
        QString createBond(const QString& networkRef, const QStringList& pifRefs, const QString& mac = QString(), const QString& mode = "balance-slb");
        bool destroyBond(const QString& bondRef);

        // PIF operations
        bool reconfigurePIF(const QString& pifRef, const QString& mode, const QString& ip,
                            const QString& netmask, const QString& gateway, const QString& dns);
        bool reconfigurePIFDHCP(const QString& pifRef);

        // Task operations
        QVariant getTaskRecord(const QString& taskRef);
        QString getTaskStatus(const QString& taskRef);
        double getTaskProgress(const QString& taskRef);
        QString getTaskResult(const QString& taskRef);
        QStringList getTaskErrorInfo(const QString& taskRef);
        QVariantMap getAllTaskRecords(); // Returns Dictionary<XenRef<Task>, Task> equivalent
        bool cancelTask(const QString& taskRef);
        bool destroyTask(const QString& taskRef);

        // Task.other_config operations (for task rehydration)
        bool addToTaskOtherConfig(const QString& taskRef, const QString& key, const QString& value);
        bool removeFromTaskOtherConfig(const QString& taskRef, const QString& key);
        QVariantMap getTaskOtherConfig(const QString& taskRef);

        // Event operations
        // event.from - Get events since token (token="" for initial call, returns new token + events)
        QVariantMap eventFrom(const QStringList& classes, const QString& token, double timeout);
        // event.register - Register for specific event classes (legacy, not used in modern API)
        bool eventRegister(const QStringList& classes);
        // event.unregister - Unregister from event classes (legacy, not used in modern API)
        bool eventUnregister(const QStringList& classes);

        // Data source operations (performance monitoring)
        double queryVMDataSource(const QString& vmRef, const QString& dataSource);
        double queryHostDataSource(const QString& hostRef, const QString& dataSource);

        // JSON-RPC helper methods
        QByteArray buildJsonRpcCall(const QString& method, const QVariantList& params);
        QVariant parseJsonRpcResponse(const QByteArray& response);

    signals:
        void apiCallCompleted(const QString& method, const QVariant& result);
        void apiCallFailed(const QString& method, const QString& error);
        void eventReceived(const QVariant& event);
        void taskProgressChanged(const QString& taskRef, double progress);

    private slots:
        void handleEventPolling();

    private:
        class Private;
        Private* d;

        void makeAsyncCall(const QString& method, const QVariantList& params,
                           const QString& callId = QString());
};

#endif // XEN_API_H
