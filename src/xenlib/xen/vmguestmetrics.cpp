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

#include "vmguestmetrics.h"

VMGuestMetrics::VMGuestMetrics(XenConnection* connection, const QString& opaqueRef, QObject* parent) : XenObject(connection, opaqueRef, parent)
{
}

QVariantMap VMGuestMetrics::GetOSVersion() const
{
    return this->property("os_version").toMap();
}

QVariantMap VMGuestMetrics::GetPVDriversVersion() const
{
    return this->property("PV_drivers_version").toMap();
}

bool VMGuestMetrics::GetPVDriversUpToDate() const
{
    return this->boolProperty("PV_drivers_up_to_date", false);
}

QVariantMap VMGuestMetrics::GetMemory() const
{
    return this->property("memory").toMap();
}

QVariantMap VMGuestMetrics::GetDisks() const
{
    return this->property("disks").toMap();
}

QVariantMap VMGuestMetrics::GetNetworks() const
{
    return this->property("networks").toMap();
}

QVariantMap VMGuestMetrics::GetOther() const
{
    return this->property("other").toMap();
}

QDateTime VMGuestMetrics::GetLastUpdated() const
{
    QString dateStr = this->stringProperty("last_updated");
    return this->parseDateTime(dateStr);
}

QVariantMap VMGuestMetrics::GetOtherConfig() const
{
    return this->property("other_config").toMap();
}

bool VMGuestMetrics::IsLive() const
{
    return this->boolProperty("live", false);
}

QString VMGuestMetrics::GetCanUseHotplugVBD() const
{
    return this->stringProperty("can_use_hotplug_vbd", "unspecified");
}

QString VMGuestMetrics::GetCanUseHotplugVIF() const
{
    return this->stringProperty("can_use_hotplug_vif", "unspecified");
}

bool VMGuestMetrics::GetPVDriversDetected() const
{
    return this->boolProperty("PV_drivers_detected", false);
}

QDateTime VMGuestMetrics::parseDateTime(const QString& dateStr) const
{
    if (dateStr.isEmpty())
        return QDateTime();
    
    QDateTime dt = QDateTime::fromString(dateStr, Qt::ISODate);
    if (!dt.isValid())
        dt = QDateTime::fromString(dateStr, Qt::ISODateWithMs);
    
    return dt;
}
