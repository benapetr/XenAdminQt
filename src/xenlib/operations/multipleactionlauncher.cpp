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

#include "multipleactionlauncher.h"
#include "multipleaction.h"
#include "parallelaction.h"
#include "../xen/network/connection.h"
#include <QHash>

MultipleActionLauncher::MultipleActionLauncher(const QList<AsyncOperation*>& operations,
                                     const QString& title,
                                     const QString& startDescription,
                                     const QString& endDescription,
                                     bool runInParallel,
                                     QObject* parent)
    : QObject(parent), m_operations(operations), m_title(title), m_startDescription(startDescription), m_endDescription(endDescription), m_runInParallel(runInParallel)
{
}

void MultipleActionLauncher::Run()
{
    // Group operations by connection
    QHash<XenConnection*, QList<AsyncOperation*>> operationsByConnection;
    QList<AsyncOperation*> operationsWithNoConnection;

    for (AsyncOperation* op : m_operations)
    {
        if (op->GetConnection() != nullptr)
        {
            if (op->GetConnection()->IsConnected())
            {
                if (!operationsByConnection.contains(op->GetConnection()))
                {
                    operationsByConnection[op->GetConnection()] = QList<AsyncOperation*>();
                }
                operationsByConnection[op->GetConnection()].append(op);
            }
        } else
        {
            operationsWithNoConnection.append(op);
        }
    }

    // Launch operations grouped by connection
    for (XenConnection* connection : operationsByConnection.keys())
    {
        QList<AsyncOperation*>& ops = operationsByConnection[connection];

        if (ops.size() == 1)
        {
            // Single operation - run directly
            ops.first()->RunAsync();
        } else
        {
            // Multiple operations - use MultipleOperation or ParallelOperation
            if (m_runInParallel)
            {
                ParallelAction* parallel = new ParallelAction(
                    m_title,
                    m_startDescription,
                    m_endDescription,
                    ops,
                    connection,
                    false, // suppressHistory
                    false, // showSubOperationDetails
                    ParallelAction::DEFAULT_MAX_PARALLEL_OPERATIONS,
                    this);
                parallel->RunAsync();
            } else
            {
                MultipleAction* multiple = new MultipleAction(
                    connection,
                    m_title,
                    m_startDescription,
                    m_endDescription,
                    ops,
                    false, // suppressHistory
                    false, // showSubOperationDetails
                    false, // stopOnFirstException
                    this);
                multiple->RunAsync();
            }
        }
    }

    // Launch operations with no connection
    if (operationsWithNoConnection.size() == 1)
    {
        // Single operation - run directly
        operationsWithNoConnection.first()->RunAsync();
    } else if (operationsWithNoConnection.size() > 1)
    {
        // Multiple operations - use MultipleOperation or ParallelOperation
        if (m_runInParallel)
        {
            ParallelAction* parallel = new ParallelAction(
                m_title,
                m_startDescription,
                m_endDescription,
                operationsWithNoConnection,
                nullptr, // connection
                false,   // suppressHistory
                false,   // showSubOperationDetails
                ParallelAction::DEFAULT_MAX_PARALLEL_OPERATIONS,
                this);
            parallel->RunAsync();
        } else
        {
            MultipleAction* multiple = new MultipleAction(
                nullptr, // connection
                m_title,
                m_startDescription,
                m_endDescription,
                operationsWithNoConnection,
                false, // suppressHistory
                false, // showSubOperationDetails
                false, // stopOnFirstException
                this);
            multiple->RunAsync();
        }
    }
}
