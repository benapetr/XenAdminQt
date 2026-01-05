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

#include "hostpatch.h"
#include "network/connection.h"
#include <QDateTime>
#include <QMap>

HostPatch::HostPatch(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

HostPatch::~HostPatch()
{
}

QString HostPatch::NameLabel() const
{
    return this->stringProperty("name_label");
}

QString HostPatch::NameDescription() const
{
    return this->stringProperty("name_description");
}

QString HostPatch::Version() const
{
    return this->stringProperty("version");
}

QString HostPatch::HostRef() const
{
    return this->stringProperty("host");
}

bool HostPatch::Applied() const
{
    return this->boolProperty("applied", false);
}

QDateTime HostPatch::TimestampApplied() const
{
    QString dateStr = this->stringProperty("timestamp_applied");
    if (dateStr.isEmpty())
        return QDateTime();
    return QDateTime::fromString(dateStr, Qt::ISODate);
}

qint64 HostPatch::Size() const
{
    return this->longProperty("size", 0);
}

QString HostPatch::PoolPatchRef() const
{
    return this->stringProperty("pool_patch");
}

QMap<QString, QString> HostPatch::OtherConfig() const
{
    QVariantMap map = this->property("other_config").toMap();
    QMap<QString, QString> result;
    for (auto it = map.begin(); it != map.end(); ++it)
        result[it.key()] = it.value().toString();
    return result;
}
