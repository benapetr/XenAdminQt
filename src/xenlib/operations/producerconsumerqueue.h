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

#ifndef PRODUCERCONSUMERQUEUE_H
#define PRODUCERCONSUMERQUEUE_H

#include <QObject>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>
#include <functional>

class ProducerConsumerWorker;

/**
 * @brief Thread-safe producer-consumer queue for executing tasks in parallel
 *
 * Qt equivalent of C# ProduceConsumerQueue. Manages a pool of worker threads
 * that consume and execute tasks from a shared queue. Tasks are executed
 * concurrently up to the configured worker count.
 *
 * Usage:
 *   ProducerConsumerQueue queue(4); // 4 worker threads
 *   queue.enqueueTask([](){ doWork(); });
 *   queue.stopWorkers(true); // stop and wait for completion
 */
class ProducerConsumerQueue : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct a new producer-consumer queue
     * @param workerCount Number of worker threads to create
     * @param parent Parent QObject
     */
    explicit ProducerConsumerQueue(int workerCount, QObject* parent = nullptr);

    /**
     * @brief Destructor - stops all workers
     */
    ~ProducerConsumerQueue();

    /**
     * @brief Enqueue a task for execution
     * @param task Function to execute (can be lambda, function pointer, etc.)
     */
    void enqueueTask(std::function<void()> task);

    /**
     * @brief Stop all worker threads gracefully
     * @param waitForWorkers If true, blocks until all workers finish
     */
    void stopWorkers(bool waitForWorkers);

    /**
     * @brief Cancel all pending tasks and stop workers
     * @param waitForWorkers If true, blocks until all workers finish
     */
    void cancelWorkers(bool waitForWorkers);

    /**
     * @brief Get the number of pending tasks in the queue
     * @return Number of tasks waiting to be executed
     */
    int pendingTaskCount() const;

private:
    QVector<ProducerConsumerWorker*> m_workers;
    QQueue<std::function<void()>> m_taskQueue;
    mutable QMutex m_mutex;
    QWaitCondition m_waitCondition;
    bool m_stopping;

    friend class ProducerConsumerWorker;
};

/**
 * @brief Worker thread that consumes tasks from the queue
 */
class ProducerConsumerWorker : public QThread
{
    Q_OBJECT

public:
    explicit ProducerConsumerWorker(ProducerConsumerQueue* queue);

protected:
    void run() override;

private:
    ProducerConsumerQueue* m_queue;
};

#endif // PRODUCERCONSUMERQUEUE_H
