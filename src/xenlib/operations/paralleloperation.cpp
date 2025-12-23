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

#include "paralleloperation.h"
#include "../xen/network/connection.h"
#include <QDebug>
#include <algorithm>

ParallelOperation::ParallelOperation(const QString& title,
                                     const QString& startDescription,
                                     const QString& endDescription,
                                     const QList<AsyncOperation*>& subOperations,
                                     XenConnection* connection,
                                     bool suppressHistory,
                                     bool showSubOperationDetails,
                                     int maxParallelOperations,
                                     QObject* parent)
    : MultipleOperation(connection, title, startDescription, endDescription,
                        subOperations, suppressHistory, showSubOperationDetails,
                        false, parent) // stopOnFirstException = false for parallel
      ,
      m_queueWithNoConnection(nullptr), m_maxParallelOperations(maxParallelOperations), m_totalOperationsCount(0), m_completedOperationsCount(0)
{
    // Organize operations by connection
    if (connection == nullptr)
    {
        // Cross-connection operation - group by each operation's connection
        for (AsyncOperation* op : subOperations)
        {
            if (op->connection() == nullptr)
            {
                m_operationsWithNoConnection.append(op);
                m_totalOperationsCount++;
            } else if (op->connection()->isConnected())
            {
                if (!m_operationsByConnection.contains(op->connection()))
                {
                    m_operationsByConnection[op->connection()] = QList<AsyncOperation*>();
                }
                m_operationsByConnection[op->connection()].append(op);
                m_totalOperationsCount++;
            }
        }
    } else
    {
        // Single connection operation - all ops use the same connection
        m_operationsByConnection[connection] = subOperations;
        m_totalOperationsCount = subOperations.size();
    }
}

ParallelOperation::~ParallelOperation()
{
    // Clean up queues
    for (ProducerConsumerQueue* queue : m_queuesByConnection.values())
    {
        delete queue;
    }
    m_queuesByConnection.clear();

    if (m_queueWithNoConnection)
    {
        delete m_queueWithNoConnection;
        m_queueWithNoConnection = nullptr;
    }
}

void ParallelOperation::runSubOperations(QStringList& exceptions)
{
    if (m_totalOperationsCount == 0)
    {
        return;
    }

    // Create queues for each connection
    for (XenConnection* conn : m_operationsByConnection.keys())
    {
        int operationCount = m_operationsByConnection[conn].size();
        int workerCount = std::min(m_maxParallelOperations, operationCount);

        ProducerConsumerQueue* queue = new ProducerConsumerQueue(workerCount, this);
        m_queuesByConnection[conn] = queue;

        // Enqueue all operations for this connection
        for (AsyncOperation* op : m_operationsByConnection[conn])
        {
            enqueueOperation(op, queue, exceptions);
        }
    }

    // Create queue for operations with no connection
    if (!m_operationsWithNoConnection.isEmpty())
    {
        int operationCount = m_operationsWithNoConnection.size();
        int workerCount = std::min(m_maxParallelOperations, operationCount);

        m_queueWithNoConnection = new ProducerConsumerQueue(workerCount, this);

        for (AsyncOperation* op : m_operationsWithNoConnection)
        {
            enqueueOperation(op, m_queueWithNoConnection, exceptions);
        }
    }

    // Wait for all operations to complete
    QMutexLocker locker(&m_completionMutex);

    // CA-379971: Make sure we don't hit a deadlock if all operations
    // have already completed before we hit the wait call
    if (m_completedOperationsCount != m_totalOperationsCount)
    {
        m_completionWaitCondition.wait(&m_completionMutex);
    }
}

void ParallelOperation::enqueueOperation(AsyncOperation* operation,
                                         ProducerConsumerQueue* queue,
                                         QStringList& exceptions)
{
    // Connect completion signal
    connect(operation, &AsyncOperation::completed,
            this, &ParallelOperation::onOperationCompleted);

    // Enqueue the operation
    queue->enqueueTask([this, operation, &exceptions]() {
        if (isCancelled())
        {
            return; // Don't start any more operations
        }

        try
        {
            operation->runSync(operation->session());
        } catch (const std::exception& e)
        {
            QString errorMsg = QString::fromStdString(e.what());

            QMutexLocker locker(&m_completionMutex);
            exceptions.append(errorMsg);

            // Record the first exception
            if (!hasError())
            {
                setError(errorMsg);
            }
        } catch (...)
        {
            QString errorMsg = tr("Unknown error occurred in parallel operation");

            QMutexLocker locker(&m_completionMutex);
            exceptions.append(errorMsg);

            if (!hasError())
            {
                setError(errorMsg);
            }
        }
    });
}

void ParallelOperation::onOperationCompleted()
{
    AsyncOperation* operation = qobject_cast<AsyncOperation*>(sender());
    if (operation)
    {
        disconnect(operation, &AsyncOperation::completed,
                   this, &ParallelOperation::onOperationCompleted);
    }

    QMutexLocker locker(&m_completionMutex);
    m_completedOperationsCount++;

    if (m_completedOperationsCount == m_totalOperationsCount)
    {
        m_completionWaitCondition.wakeOne();
        setPercentComplete(100);
    }
}

void ParallelOperation::recalculatePercentComplete()
{
    if (m_totalOperationsCount == 0)
    {
        return;
    }

    int total = 0;

    // Sum progress from all operations grouped by connection
    for (const QList<AsyncOperation*>& ops : m_operationsByConnection.values())
    {
        for (AsyncOperation* op : ops)
        {
            total += op->percentComplete();
        }
    }

    // Add progress from operations with no connection
    for (AsyncOperation* op : m_operationsWithNoConnection)
    {
        total += op->percentComplete();
    }

    int avgProgress = total / m_totalOperationsCount;
    setPercentComplete(avgProgress);
}

void ParallelOperation::onMultipleOperationCompleted()
{
    // Call base class implementation first
    MultipleOperation::onMultipleOperationCompleted();

    // Stop all worker queues
    for (ProducerConsumerQueue* queue : m_queuesByConnection.values())
    {
        queue->stopWorkers(false);
    }

    if (m_queueWithNoConnection)
    {
        m_queueWithNoConnection->stopWorkers(false);
    }
}
