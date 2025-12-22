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

#ifndef HOSTMETRICS_H
#define HOSTMETRICS_H

#include "xenobject.h"
#include <QDateTime>
#include <QVariantMap>

/**
 * @brief Host metrics XenObject wrapper
 *
 * Provides typed access to host_metrics fields in XenCache.
 */
class XENLIB_EXPORT HostMetrics : public XenObject
{
    Q_OBJECT

public:
    explicit HostMetrics(XenConnection* connection,
                         const QString& opaqueRef,
                         QObject* parent = nullptr)
        : XenObject(connection, opaqueRef, parent)
    {
    }

    bool live() const
    {
        return boolProperty("live", false);
    }

    QString uuid() const
    {
        return stringProperty("uuid");
    }

    qint64 memoryTotal() const
    {
        return longProperty("memory_total", 0);
    }

    qint64 memoryFree() const
    {
        return longProperty("memory_free", 0);
    }

    QDateTime lastUpdated() const
    {
        QVariant value = property("last_updated");
        if (value.canConvert<QDateTime>())
            return value.toDateTime();

        QString text = value.toString();
        if (text.isEmpty())
            return QDateTime();

        QDateTime parsed = QDateTime::fromString(text, Qt::ISODate);
        if (parsed.isValid())
            return parsed;

        return QDateTime::fromString(text, Qt::ISODateWithMs);
    }

    QVariantMap otherConfig() const
    {
        QVariant value = property("other_config");
        if (value.canConvert<QVariantMap>())
            return value.toMap();
        return QVariantMap();
    }

protected:
    QString objectType() const override
    {
        return QStringLiteral("host_metrics");
    }
};

#endif // HOSTMETRICS_H
