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

#ifndef OPERATIONLAUNCHER_H
#define OPERATIONLAUNCHER_H

#include "../xen/asyncoperation.h"
#include <QObject>
#include <QList>
#include <QString>

/**
 * @brief Launch operations with automatic connection-based grouping
 *
 * Qt equivalent of C# MultipleActionLauncher. Takes a list of operations
 * and intelligently groups them by connection, then launches them using
 * the appropriate strategy:
 * - Single operation: Run directly
 * - Multiple operations on same connection: Use MultipleOperation or ParallelOperation
 * - Operations across connections: Group by connection and run in parallel
 *
 * This ensures operations are synchronous per connection but asynchronous
 * across connections.
 *
 * Usage:
 *   QList<AsyncOperation*> ops = { op1, op2, op3, ... };
 *   OperationLauncher launcher(ops, "Bulk Operation", "Starting...",
 *                              "Complete", true);
 *   launcher.run();
 */
class OperationLauncher : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct an operation launcher
     * @param operations List of operations to launch
     * @param title Title for grouped operations
     * @param startDescription Start description for grouped operations
     * @param endDescription End description for grouped operations
     * @param runInParallel If true, use ParallelOperation; otherwise MultipleOperation
     * @param parent Parent QObject
     */
    explicit OperationLauncher(const QList<AsyncOperation*>& operations,
                               const QString& title,
                               const QString& startDescription,
                               const QString& endDescription,
                               bool runInParallel = true,
                               QObject* parent = nullptr);

    /**
     * @brief Launch all operations with automatic grouping
     *
     * Operations are grouped by connection and launched appropriately:
     * - Single ops run directly via runAsync()
     * - Multiple ops per connection use MultipleOperation or ParallelOperation
     * - Cross-connection ops are properly synchronized
     */
    void run();

private:
    QList<AsyncOperation*> m_operations;
    QString m_title;
    QString m_startDescription;
    QString m_endDescription;
    bool m_runInParallel;
};

#endif // OPERATIONLAUNCHER_H
