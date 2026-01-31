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

#include "task.h"
#include "network/connection.h"
#include "../xencache.h"

Task::Task(XenConnection* connection, const QString& opaqueRef, QObject* parent) : XenObject(connection, opaqueRef, parent)
{
}


QString Task::GetStatus() const
{
    return this->stringProperty("status");
}

double Task::GetProgress() const
{
    return this->GetData().value("progress").toDouble();
}

QDateTime Task::GetCreated() const
{
    return this->GetData().value("created").toDateTime();
}

QDateTime Task::GetFinished() const
{
    return this->GetData().value("finished").toDateTime();
}

QString Task::GetResult() const
{
    return this->stringProperty("result");
}

QStringList Task::GetErrorInfo() const
{
    QVariant errorInfo = this->GetData().value("error_info");
    if (errorInfo.canConvert<QStringList>())
        return errorInfo.toStringList();
    return QStringList();
}

QString Task::ResidentOnRef() const
{
    return this->stringProperty("resident_on");
}

bool Task::IsPending() const
{
    return this->GetStatus() == "pending";
}

bool Task::IsSuccess() const
{
    return this->GetStatus() == "success";
}

bool Task::IsFailed() const
{
    return this->GetStatus() == "failure";
}

bool Task::IsCancelled() const
{
    QString status = this->GetStatus();
    return status == "cancelled" || status == "cancelling";
}

bool Task::IsRunning() const
{
    QString status = this->GetStatus();
    return status == "pending" || status == "cancelling";
}
