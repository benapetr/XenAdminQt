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

#include "hostmetrics.h"

HostMetrics::HostMetrics(XenConnection* connection,
                        const QString& opaqueRef,
                        QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

QString HostMetrics::GetObjectType() const
{
    return QStringLiteral("host_metrics");
}

bool HostMetrics::live() const
{
    return this->boolProperty("live", false);
}

QString HostMetrics::GetUUID() const
{
    return this->stringProperty("uuid");
}

qint64 HostMetrics::memoryTotal() const
{
    return this->longProperty("memory_total", 0);
}

qint64 HostMetrics::memoryFree() const
{
    return this->longProperty("memory_free", 0);
}

QDateTime HostMetrics::lastUpdated() const
{
    QString dateStr = this->stringProperty("last_updated");
    if (dateStr.isEmpty())
        return QDateTime();
    
    QDateTime dt = QDateTime::fromString(dateStr, Qt::ISODate);
    if (!dt.isValid())
        dt = QDateTime::fromString(dateStr, Qt::ISODateWithMs);
    
    return dt;
}

QVariantMap HostMetrics::otherConfig() const
{
    return this->property("other_config").toMap();
}
