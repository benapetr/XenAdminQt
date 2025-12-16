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

#ifndef DELEGATEDASYNCOPERATION_H
#define DELEGATEDASYNCOPERATION_H

#include "../asyncoperation.h"
#include <functional>

/**
 * @brief Lightweight wrapper for lambda-based async operations
 *
 * Qt equivalent of C# DelegatedAsyncAction. Allows creating quick
 * AsyncOperations without subclassing, using a std::function callback.
 *
 * Usage:
 *   auto op = new DelegatedAsyncOperation(connection, "Quick Task",
 *       "Running...", [](DelegatedAsyncOperation* self) {
 *           // Your operation logic here
 *           self->setPercentComplete(50);
 *           // ... do work ...
 *           self->setPercentComplete(100);
 *       });
 *   op->runAsync();
 */
class XENLIB_EXPORT DelegatedAsyncOperation : public AsyncOperation
{
    Q_OBJECT

public:
    using WorkCallback = std::function<void(DelegatedAsyncOperation*)>;

    /**
     * @brief Construct a delegated operation
     * @param connection XenAPI connection
     * @param title Operation title
     * @param description Description
     * @param workCallback Function to execute (receives 'this' as parameter)
     * @param parent Parent QObject
     */
    explicit DelegatedAsyncOperation(XenConnection* connection,
                                     const QString& title,
                                     const QString& description,
                                     WorkCallback workCallback,
                                     QObject* parent = nullptr);

    /**
     * @brief Construct without connection (for non-XenAPI work)
     */
    explicit DelegatedAsyncOperation(const QString& title,
                                     const QString& description,
                                     WorkCallback workCallback,
                                     QObject* parent = nullptr);

    ~DelegatedAsyncOperation() override = default;

protected:
    /**
     * @brief Execute the work callback
     */
    void run() override;

private:
    WorkCallback m_workCallback;
};

#endif // DELEGATEDASYNCOPERATION_H
