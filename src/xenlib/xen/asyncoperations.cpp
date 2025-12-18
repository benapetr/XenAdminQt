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

#include "asyncoperations.h"
#include "session.h"
#include "api.h"
#include <QtCore/QTimer>
#include <QtCore/QUuid>
#include <QtCore/QHash>
#include <QtCore/QDateTime>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtNetwork/QNetworkRequest>

// Local state holder for async XenAPI tasks. Named differently from
// the public AsyncOperation class to avoid ODR/linker collisions.
struct AsyncOpState
{
    QString id;          // Our internal operation ID
    QString taskRef;     // XenServer task reference
    QString method;      // Original method name
    QVariantList params; // Original parameters
    double progress;     // Current progress (0-100)
    QString status;      // Current status string
    int retryCount;      // Number of retries attempted
    qint64 startTime;    // When the operation started
    bool completed;      // Whether the operation is done
};

class XenAsyncOperations::Private
{
    public:
        XenSession* session = nullptr;
        XenRpcAPI* api = nullptr;
        QHash<QString, AsyncOpState*> operations;
        QTimer* globalStatusTimer = nullptr;

        // Configuration
        int connectionTimeout = 300000; // 5 minutes for long operations
        int maxRetries = 3;
        int retryDelay = 1000;         // 1 second
        int statusPollInterval = 1000; // 1 second - poll frequently for responsive UI
};

XenAsyncOperations::XenAsyncOperations(XenSession* session, QObject* parent)
    : QObject(parent), d(new Private)
{
    this->d->session = session;
    this->d->api = new XenRpcAPI(session, this);

    // Global status polling timer
    this->d->globalStatusTimer = new QTimer(this);
    this->d->globalStatusTimer->setInterval(this->d->statusPollInterval);
    connect(this->d->globalStatusTimer, &QTimer::timeout,
            this, &XenAsyncOperations::pollOperationStatus);
}

XenAsyncOperations::~XenAsyncOperations()
{
    // Cancel all pending operations and destroy tasks
    QHashIterator<QString, AsyncOpState*> it(this->d->operations);
    while (it.hasNext())
    {
        it.next();
        AsyncOpState* op = it.value();
        if (!op->completed && !op->taskRef.isEmpty())
        {
            // Try to cancel and destroy the task
            this->d->api->cancelTask(op->taskRef);
            this->d->api->destroyTask(op->taskRef);
        }
        delete op;
    }

    delete this->d;
}

QString XenAsyncOperations::generateOperationId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString XenAsyncOperations::startAsyncOperation(const QString& method, const QVariantList& params)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->operationFailed("", "Not authenticated");
        return QString();
    }

    // Generate our internal operation ID
    QString operationId = this->generateOperationId();

    // Create operation tracking structure
    AsyncOpState* op = new AsyncOpState;
    op->id = operationId;
    op->method = method;
    op->params = params;
    op->progress = 0.0;
    op->status = "Starting";
    op->retryCount = 0;
    op->startTime = QDateTime::currentMSecsSinceEpoch();
    op->completed = false;
    op->taskRef = QString(); // Will be filled when task is created

    this->d->operations[operationId] = op;

    // Build the async method name (e.g., "VM.start" -> "Async.VM.start" or "async_vm_start")
    // XenServer async calls are typically named "Async.<Class>.<method>" or use "async_" prefix
    QString asyncMethod = method;
    if (!asyncMethod.startsWith("Async.") && !asyncMethod.startsWith("async_"))
    {
        // Try to convert to async format
        // Async methods are typically lowercase with underscores
        asyncMethod = "async_" + method.toLower().replace(".", "_");
    }

    // Prepare parameters: session ID + original parameters
    QVariantList fullParams;
    fullParams << this->d->session->getSessionId();
    fullParams.append(params);

    // Build and send the async JSON-RPC call
    QByteArray jsonRequest = this->d->api->buildJsonRpcCall(asyncMethod, fullParams);
    QByteArray response = this->d->session->sendApiRequest(jsonRequest);

    if (response.isEmpty())
    {
        op->status = "Failed to start";
        emit this->operationFailed(operationId, "Failed to send async request");
        this->d->operations.remove(operationId);
        delete op;
        return QString();
    }

    // Parse the response to get the task reference
    QVariant result = this->d->api->parseJsonRpcResponse(response);
    QString taskRef = result.toString();

    if (taskRef.isEmpty())
    {
        op->status = "Failed to get task reference";
        emit this->operationFailed(operationId, "Invalid task reference from server");
        this->d->operations.remove(operationId);
        delete op;
        return QString();
    }

    // Store the task reference
    op->taskRef = taskRef;
    op->status = "Running";

    emit this->operationStarted(operationId, method);

    // Start global polling if not already running
    if (!this->d->globalStatusTimer->isActive())
    {
        this->d->globalStatusTimer->start();
    }

    return operationId;
}

bool XenAsyncOperations::cancelOperation(const QString& operationId)
{
    if (!this->d->operations.contains(operationId))
    {
        return false;
    }

    AsyncOpState* op = this->d->operations[operationId];

    // Try to cancel the task on the server
    if (!op->taskRef.isEmpty() && !op->completed)
    {
        this->d->api->cancelTask(op->taskRef);
        this->d->api->destroyTask(op->taskRef);
    }

    op->status = "Cancelled";
    op->completed = true;

    emit this->operationCancelled(operationId);

    // Cleanup
    this->d->operations.remove(operationId);
    delete op;

    // Stop global polling if no more operations
    if (this->d->operations.isEmpty())
    {
        this->d->globalStatusTimer->stop();
    }

    return true;
}

double XenAsyncOperations::getOperationProgress(const QString& operationId)
{
    if (!this->d->operations.contains(operationId))
    {
        return this->d->operations[operationId]->progress;
    }
    return -1.0;
}

QString XenAsyncOperations::getOperationStatus(const QString& operationId)
{
    if (this->d->operations.contains(operationId))
    {
        return this->d->operations[operationId]->status;
    }
    return QString();
}

void XenAsyncOperations::setConnectionTimeout(int timeoutMs)
{
    this->d->connectionTimeout = timeoutMs;
}

void XenAsyncOperations::setMaxRetries(int retries)
{
    this->d->maxRetries = retries;
}

void XenAsyncOperations::setRetryDelay(int delayMs)
{
    this->d->retryDelay = delayMs;
}

QVariant XenAsyncOperations::parseTaskResult(const QString& jsonResult)
{
    if (jsonResult.isEmpty())
    {
        return QVariant();
    }

    // Task results can be JSON-encoded values or simple strings
    // Try to parse as JSON first
    QJsonDocument doc = QJsonDocument::fromJson(jsonResult.toUtf8());

    if (!doc.isNull())
    {
        // Successfully parsed as JSON
        if (doc.isObject())
        {
            return doc.object().toVariantMap();
        } else if (doc.isArray())
        {
            return doc.array().toVariantList();
        } else
        {
            return doc.toVariant();
        }
    }

    // If JSON parsing failed, return the string as-is
    // It might be a simple reference like "OpaqueRef:..." or a plain value
    return jsonResult;
}

void XenAsyncOperations::pollOperationStatus()
{
    QList<QString> completedOps;

    // Poll status for all active operations
    QHashIterator<QString, AsyncOpState*> it(this->d->operations);
    while (it.hasNext())
    {
        it.next();
        AsyncOpState* op = it.value();

        if (op->completed || op->taskRef.isEmpty())
        {
            continue;
        }

        // Check for timeout
        qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - op->startTime;
        if (elapsed > this->d->connectionTimeout)
        {
            op->status = "Timed out";
            op->completed = true;
            emit this->operationFailed(op->id, "Operation timed out");

            // Try to cancel and destroy the task
            this->d->api->cancelTask(op->taskRef);
            this->d->api->destroyTask(op->taskRef);

            completedOps.append(op->id);
            continue;
        }

        // Get task status and progress from the server
        QString taskStatus = this->d->api->getTaskStatus(op->taskRef);
        double taskProgress = this->d->api->getTaskProgress(op->taskRef);

        if (taskStatus.isEmpty())
        {
            // Could not get status - maybe connection issue
            continue;
        }

        // Update operation progress
        op->progress = taskProgress * 100.0; // Convert 0-1 to 0-100

        // Check task status
        if (taskStatus == "success")
        {
            op->status = "Completed";
            op->progress = 100.0;
            op->completed = true;

            // Get the task result
            QString taskResult = this->d->api->getTaskResult(op->taskRef);

            // Parse the result to extract the actual value
            QVariant result = this->parseTaskResult(taskResult);

            emit this->operationCompleted(op->id, result);

            // Destroy the task on the server
            this->d->api->destroyTask(op->taskRef);

            completedOps.append(op->id);
        } else if (taskStatus == "failure")
        {
            op->status = "Failed";
            op->completed = true;

            // Get error information
            QStringList errorInfo = this->d->api->getTaskErrorInfo(op->taskRef);
            QString errorMsg = errorInfo.isEmpty() ? "Unknown error" : errorInfo.join(": ");

            emit this->operationFailed(op->id, errorMsg);

            // Destroy the task on the server
            this->d->api->destroyTask(op->taskRef);

            completedOps.append(op->id);
        } else if (taskStatus == "cancelled")
        {
            op->status = "Cancelled";
            op->completed = true;

            emit this->operationCancelled(op->id);

            // Destroy the task on the server
            this->d->api->destroyTask(op->taskRef);

            completedOps.append(op->id);
        } else if (taskStatus == "pending")
        {
            // Still running
            op->status = "Running";
            emit this->operationProgress(op->id, op->progress, op->status);
        }
    }

    // Clean up completed operations
    for (const QString& opId : completedOps)
    {
        AsyncOpState* op = this->d->operations.take(opId);
        delete op;
    }

    // Stop polling if no operations remain
    if (this->d->operations.isEmpty())
    {
        this->d->globalStatusTimer->stop();
    }
}
