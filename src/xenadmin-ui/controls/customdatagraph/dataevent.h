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

#ifndef CUSTOMDATAGRAPH_DATAEVENT_H
#define CUSTOMDATAGRAPH_DATAEVENT_H

#include <QString>
#include <QDateTime>

namespace CustomDataGraph
{
    struct DataEvent
    {
        qint64 TimestampTicks = 0;
        int Severity = 0;
        QString Message;
        QString ObjectUuid;
        QString ObjectName;

        DataEvent() = default;

        DataEvent(qint64 timestampTicks, int severity, const QString& message, const QString& objectUuid,
                  const QString& objectName = QString())
            : TimestampTicks(timestampTicks), Severity(severity), Message(message), ObjectUuid(objectUuid), ObjectName(objectName)
        {
        }

        bool operator==(const DataEvent& other) const
        {
            return this->TimestampTicks == other.TimestampTicks
                   && this->Message == other.Message
                   && this->ObjectUuid == other.ObjectUuid;
        }
    };
}

#endif // CUSTOMDATAGRAPH_DATAEVENT_H
