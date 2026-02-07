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

#include "message.h"
#include "network/connection.h"
#include <QSet>

Message::Message(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}


QString Message::Name() const
{
    return this->stringProperty("name");
}

qint64 Message::Priority() const
{
    return this->property("priority").toLongLong();
}

QString Message::Cls() const
{
    return this->stringProperty("cls");
}

QString Message::ObjUuid() const
{
    return this->stringProperty("obj_uuid");
}

QDateTime Message::Timestamp() const
{
    return this->property("timestamp").toDateTime();
}

QDateTime Message::TimestampLocal() const
{
    QDateTime timestamp = this->Timestamp();
    if (!timestamp.isValid())
        return timestamp;

    XenConnection* connection = this->GetConnection();
    if (connection)
    {
        timestamp = timestamp.addSecs(connection->GetServerTimeOffsetSeconds());
    }

    return timestamp.toLocalTime();
}

QString Message::Body() const
{
    return this->stringProperty("body");
}

bool Message::ShowOnGraphs() const
{
    const QString type = this->Name().toUpper();
    static const QSet<QString> graphTypes =
    {
        QStringLiteral("VM_CLONED"),
        QStringLiteral("VM_CRASHED"),
        QStringLiteral("VM_REBOOTED"),
        QStringLiteral("VM_RESUMED"),
        QStringLiteral("VM_SHUTDOWN"),
        QStringLiteral("VM_STARTED"),
        QStringLiteral("VM_SUSPENDED")
    };

    return graphTypes.contains(type);
}

bool Message::IsSquelched() const
{
    return this->Name().toUpper() == QStringLiteral("HA_POOL_OVERCOMMITTED");
}
