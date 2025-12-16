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

#ifndef PARALLELOPERATION_H
#define PARALLELOPERATION_H

#include "multipleoperation.h"
#include "producerconsumerqueue.h"
#include <QHash>
#include <QMutex>
#include <QWaitCondition>

class XenConnection;

/**
 * @brief Run multiple operations in parallel with connection-based queuing
 *
 * Qt equivalent of C# ParallelAction. Takes a list of operations and runs
 * them concurrently, with a configurable maximum number of parallel operations
 * per connection. Operations are grouped by connection and each connection
 * gets its own producer-consumer queue.
 *
 * Usage:
 *   QList<AsyncOperation*> ops = { op1, op2, op3, ... };
 *   auto parallel = new ParallelOperation("Bulk Operation",
 *                                         "Starting...", "Complete", ops);
 *   parallel->execute();
 */
class ParallelOperation : public MultipleOperation
{
    Q_OBJECT

public:
    /**
     * @brief Default maximum number of parallel operations per connection
     */
    static const int DEFAULT_MAX_PARALLEL_OPERATIONS = 25;

    /**
     * @brief Construct a parallel operation
     * @param title Operation title
     * @param startDescription Description shown when starting
     * @param endDescription Description shown when complete
     * @param subOperations List of operations to run in parallel
     * @param connection Parent connection (nullptr for cross-connection ops)
     * @param suppressHistory If true, don't add to history
     * @param showSubOperationDetails If true, show details of sub-operations
     * @param maxParallelOperations Max concurrent operations per connection (default 25)
     * @param parent Parent QObject
     */
    explicit ParallelOperation(const QString& title,
                               const QString& startDescription,
                               const QString& endDescription,
                               const QList<AsyncOperation*>& subOperations,
                               XenConnection* connection = nullptr,
                               bool suppressHistory = false,
                               bool showSubOperationDetails = false,
                               int maxParallelOperations = DEFAULT_MAX_PARALLEL_OPERATIONS,
                               QObject* parent = nullptr);

    /**
     * @brief Destructor
     */
    ~ParallelOperation() override;

protected:
    /**
     * @brief Run sub-operations in parallel
     * @param exceptions List to collect exception messages
     */
    void runSubOperations(QStringList& exceptions) override;

    /**
     * @brief Recalculate progress from all sub-operations
     */
    void recalculatePercentComplete() override;

    /**
     * @brief Called when operation completes
     */
    void onMultipleOperationCompleted() override;

private:
    /**
     * @brief Enqueue a single operation
     */
    void enqueueOperation(AsyncOperation* operation,
                          ProducerConsumerQueue* queue,
                          QStringList& exceptions);

private slots:
    /**
     * @brief Handle sub-operation completion
     */
    void onOperationCompleted();

private:
    // Operations grouped by connection
    QHash<XenConnection*, QList<AsyncOperation*>> m_operationsByConnection;

    // Producer-consumer queues per connection
    QHash<XenConnection*, ProducerConsumerQueue*> m_queuesByConnection;

    // Operations with no connection
    QList<AsyncOperation*> m_operationsWithNoConnection;
    ProducerConsumerQueue* m_queueWithNoConnection;

    // Configuration
    int m_maxParallelOperations;
    int m_totalOperationsCount;

    // Completion tracking
    QMutex m_completionMutex;
    QWaitCondition m_completionWaitCondition;
    int m_completedOperationsCount;
};

#endif // PARALLELOPERATION_H
