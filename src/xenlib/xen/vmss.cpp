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

#include "vmss.h"
#include "network/connection.h"
#include "../xencache.h"

VMSS::VMSS(XenConnection* connection, const QString& opaqueRef, QObject* parent) : XenObject(connection, opaqueRef, parent)
{
}


bool VMSS::Enabled() const
{
    return this->boolProperty("enabled");
}

QString VMSS::Type() const
{
    return this->stringProperty("type");
}

qlonglong VMSS::RetainedSnapshots() const
{
    return this->longProperty("retained_snapshots");
}

QString VMSS::Frequency() const
{
    return this->stringProperty("frequency");
}

QVariantMap VMSS::Schedule() const
{
    return this->property("schedule").toMap();
}

QDateTime VMSS::LastRunTime() const
{
    QVariant timeVariant = this->property("last_run_time");
    if (timeVariant.canConvert<QDateTime>())
        return timeVariant.toDateTime();
    
    // Try parsing as string if not already a QDateTime
    QString timeStr = timeVariant.toString();
    if (!timeStr.isEmpty())
    {
        QDateTime dt = QDateTime::fromString(timeStr, Qt::ISODate);
        if (dt.isValid())
            return dt;
    }
    
    // Return epoch time if parsing fails
    return QDateTime::fromSecsSinceEpoch(0);
}

QStringList VMSS::GetVMRefs() const
{
    return this->property("VMs").toStringList();
}

// Helper methods
bool VMSS::IsEnabled() const
{
    return this->Enabled();
}

int VMSS::VMCount() const
{
    return this->GetVMRefs().count();
}
