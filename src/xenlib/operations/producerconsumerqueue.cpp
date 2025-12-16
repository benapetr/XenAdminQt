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

#include "producerconsumerqueue.h"
#include <QDebug>

ProducerConsumerQueue::ProducerConsumerQueue(int workerCount, QObject* parent)
    : QObject(parent), m_stopping(false)
{
    // Create and start worker threads
    m_workers.reserve(workerCount);
    for (int i = 0; i < workerCount; ++i)
    {
        ProducerConsumerWorker* worker = new ProducerConsumerWorker(this);
        m_workers.append(worker);
        worker->start();
    }
}

ProducerConsumerQueue::~ProducerConsumerQueue()
{
    // Ensure workers are stopped
    cancelWorkers(true);

    // Workers are QObject children (parent = this), so they will be destroyed
    // automatically when this queue is destroyed. Avoid manual deletion to
    // prevent double-free during QObject::deleteChildren().
    m_workers.clear();
}

void ProducerConsumerQueue::enqueueTask(std::function<void()> task)
{
    QMutexLocker locker(&m_mutex);

    // Add task to queue
    m_taskQueue.enqueue(task);

    // Wake up one waiting worker
    m_waitCondition.wakeOne();
}

void ProducerConsumerQueue::stopWorkers(bool waitForWorkers)
{
    {
        QMutexLocker locker(&m_mutex);
        m_stopping = true;
    }

    // Enqueue null tasks to signal workers to stop (one per worker)
    for (int i = 0; i < m_workers.size(); ++i)
    {
        enqueueTask(nullptr);
    }

    // Wait for workers to finish if requested
    if (waitForWorkers)
    {
        for (ProducerConsumerWorker* worker : m_workers)
        {
            worker->wait();
        }
    }
}

void ProducerConsumerQueue::cancelWorkers(bool waitForWorkers)
{
    {
        QMutexLocker locker(&m_mutex);
        // Clear all pending tasks
        m_taskQueue.clear();
    }

    // Stop workers
    stopWorkers(waitForWorkers);
}

int ProducerConsumerQueue::pendingTaskCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_taskQueue.size();
}

// ProducerConsumerWorker implementation

ProducerConsumerWorker::ProducerConsumerWorker(ProducerConsumerQueue* queue)
    : QThread(queue), m_queue(queue)
{
}

void ProducerConsumerWorker::run()
{
    while (true)
    {
        std::function<void()> task;

        {
            QMutexLocker locker(&m_queue->m_mutex);

            // Wait while queue is empty
            while (m_queue->m_taskQueue.isEmpty() && !m_queue->m_stopping)
            {
                m_queue->m_waitCondition.wait(&m_queue->m_mutex);
            }

            // Check if we should stop
            if (m_queue->m_taskQueue.isEmpty())
            {
                return;
            }

            // Dequeue the next task
            task = m_queue->m_taskQueue.dequeue();
        }

        // Null task signals worker to stop
        if (!task)
        {
            return;
        }

        // Execute the task outside the lock
        try
        {
            task();
        } catch (...)
        {
            qWarning() << "ProducerConsumerWorker: Exception in task execution";
        }
    }
}
