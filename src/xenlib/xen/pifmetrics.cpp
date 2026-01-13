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

#include "pifmetrics.h"

PIFMetrics::PIFMetrics(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

QString PIFMetrics::GetObjectType() const
{
    return "pif_metrics";
}

double PIFMetrics::IoReadKbs() const
{
    return this->property("io_read_kbs").toDouble();
}

double PIFMetrics::IoWriteKbs() const
{
    return this->property("io_write_kbs").toDouble();
}

bool PIFMetrics::Carrier() const
{
    return this->property("carrier").toBool();
}

QString PIFMetrics::VendorId() const
{
    return this->stringProperty("vendor_id");
}

QString PIFMetrics::VendorName() const
{
    return this->stringProperty("vendor_name");
}

QString PIFMetrics::DeviceId() const
{
    return this->stringProperty("device_id");
}

QString PIFMetrics::DeviceName() const
{
    return this->stringProperty("device_name");
}

qint64 PIFMetrics::Speed() const
{
    return this->property("speed").toLongLong();
}

bool PIFMetrics::Duplex() const
{
    return this->property("duplex").toBool();
}

QString PIFMetrics::PciBusPath() const
{
    return this->stringProperty("pci_bus_path");
}

QDateTime PIFMetrics::LastUpdated() const
{
    return this->property("last_updated").toDateTime();
}

QMap<QString, QString> PIFMetrics::OtherConfig() const
{
    QVariantMap map = this->property("other_config").toMap();
    QMap<QString, QString> result;
    for (auto it = map.begin(); it != map.end(); ++it)
        result[it.key()] = it.value().toString();
    return result;
}
