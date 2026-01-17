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

#include "vmmetrics.h"

VMMetrics::VMMetrics(XenConnection* connection, const QString& opaqueRef, QObject* parent) : XenObject(connection, opaqueRef, parent)
{
}

QString VMMetrics::GetObjectType() const
{
    return QStringLiteral("VM_metrics");
}

qint64 VMMetrics::GetMemoryActual() const
{
    return this->longProperty("memory_actual", 0);
}

qint64 VMMetrics::GetVCPUsNumber() const
{
    return this->longProperty("VCPUs_number", 0);
}

QVariantMap VMMetrics::GetVCPUsCPU() const
{
    return this->property("VCPUs_CPU").toMap();
}

QVariantMap VMMetrics::GetVCPUsParams() const
{
    return this->property("VCPUs_params").toMap();
}

QVariantMap VMMetrics::GetVCPUsFlags() const
{
    return this->property("VCPUs_flags").toMap();
}

QStringList VMMetrics::GetState() const
{
    return this->stringListProperty("state");
}

QDateTime VMMetrics::GetStartTime() const
{
    QString dateStr = this->stringProperty("start_time");
    return parseDateTime(dateStr);
}

QDateTime VMMetrics::GetInstallTime() const
{
    QString dateStr = this->stringProperty("install_time");
    return parseDateTime(dateStr);
}

QDateTime VMMetrics::GetLastUpdated() const
{
    QString dateStr = this->stringProperty("last_updated");
    return parseDateTime(dateStr);
}

bool VMMetrics::IsHVM() const
{
    return this->boolProperty("hvm", false);
}

bool VMMetrics::SupportsNestedVirt() const
{
    return this->boolProperty("nested_virt", false);
}

bool VMMetrics::IsNoMigrate() const
{
    return this->boolProperty("nomigrate", false);
}

QString VMMetrics::GetCurrentDomainType() const
{
    return this->stringProperty("current_domain_type", "unspecified");
}

QDateTime VMMetrics::parseDateTime(const QString& dateStr) const
{
    if (dateStr.isEmpty())
        return QDateTime();
    
    QDateTime dt = QDateTime::fromString(dateStr, Qt::ISODate);
    if (!dt.isValid())
        dt = QDateTime::fromString(dateStr, Qt::ISODateWithMs);
    
    return dt;
}
