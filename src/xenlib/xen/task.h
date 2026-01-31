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

#ifndef TASK_H
#define TASK_H

#include "xenobject.h"
#include <QDateTime>

/**
 * @brief Task - A long-running asynchronous task
 *
 * Qt equivalent of C# XenAPI.Task class. Represents a task object.
 *
 * Key properties (from C# Task class):
 * - name_label, name_description
 * - status (pending, success, failure, cancelling, cancelled)
 * - progress (0.0 to 1.0)
 * - created, finished (timestamps)
 * - resident_on (host where task is running)
 * - result (result value on success)
 * - error_info (error details on failure)
 */
class XENLIB_EXPORT Task : public XenObject
{
    Q_OBJECT

    public:
        explicit Task(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        ~Task() override = default;

        XenObjectType GetObjectType() const override { return XenObjectType::Task; }

        //! Get task status (Status string: "pending", "success", "failure", "cancelling", "cancelled")
        QString GetStatus() const;

        //! Get task progress (Progress value from 0.0 to 1.0)
        double GetProgress() const;

        //! Get creation timestamp (When task was created)
        QDateTime GetCreated() const;

        //! Get finish timestamp (When task finished or null if still running)
        QDateTime GetFinished() const;

        //! Get task result (Result value, valid only if status is "success")
        QString GetResult() const;

        //! Get error information (Error details, valid only if status is "failure")
        QStringList GetErrorInfo() const;

        //! Get resident host reference (Reference to host where task is running)
        QString ResidentOnRef() const;

        //! Check if task is pending
        bool IsPending() const;

        //! Check if task completed successfully
        bool IsSuccess() const;

        //! Check if task failed
        bool IsFailed() const;

        //! Check if task is cancelled or cancelling
        bool IsCancelled() const;

        //! Check if task is still running
        bool IsRunning() const;
};

#endif // TASK_H
