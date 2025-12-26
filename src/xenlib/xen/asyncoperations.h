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

#ifndef XEN_ASYNCOPERATIONS_H
#define XEN_ASYNCOPERATIONS_H

#include "../xenlib_global.h"
#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QVariant>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

namespace XenAPI
{
    class Session;
}

class XENLIB_EXPORT XenAsyncOperations : public QObject
{
    Q_OBJECT
    public:
        explicit XenAsyncOperations(XenAPI::Session* session, QObject* parent = nullptr);
        ~XenAsyncOperations();

        // Async operation management
        QString startAsyncOperation(const QString& method, const QVariantList& params);
        bool cancelOperation(const QString& operationId);
        double getOperationProgress(const QString& operationId);
        QString getOperationStatus(const QString& operationId);

        // Network management
        void setConnectionTimeout(int timeoutMs);
        void setMaxRetries(int retries);
        void setRetryDelay(int delayMs);

    signals:
        void operationStarted(const QString& operationId, const QString& method);
        void operationProgress(const QString& operationId, double progress, const QString& status);
        void operationCompleted(const QString& operationId, const QVariant& result);
        void operationFailed(const QString& operationId, const QString& error);
        void operationCancelled(const QString& operationId);

    private slots:
        void pollOperationStatus();

    private:
        class Private;
        Private* d;

        QString generateOperationId();
        QVariant parseTaskResult(const QString& jsonResult);
};

#endif // XEN_ASYNCOPERATIONS_H
