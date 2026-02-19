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

#include "vmpp.h"
#include "network/connection.h"
#include "../xencache.h"
#include "../utils/misc.h"
#include "vm.h"

VMPP::VMPP(XenConnection* connection, const QString& opaqueRef, QObject* parent) : XenObject(connection, opaqueRef, parent)
{
}


bool VMPP::IsPolicyEnabled() const
{
    return this->boolProperty("is_policy_enabled");
}

// Backup configuration
QString VMPP::BackupType() const
{
    return this->stringProperty("backup_type");
}

qlonglong VMPP::BackupRetentionValue() const
{
    return this->longProperty("backup_retention_value");
}

QString VMPP::BackupFrequency() const
{
    return this->stringProperty("backup_frequency");
}

QVariantMap VMPP::BackupSchedule() const
{
    return this->property("backup_schedule").toMap();
}

bool VMPP::IsBackupRunning() const
{
    return this->boolProperty("is_backup_running");
}

QDateTime VMPP::BackupLastRunTime() const
{
    QVariant timeVariant = this->property("backup_last_run_time");
    if (timeVariant.canConvert<QDateTime>())
        return timeVariant.toDateTime();
    
    QString timeStr = timeVariant.toString();
    if (!timeStr.isEmpty())
    {
        QDateTime dt = Misc::ParseXenDateTime(timeStr);
        if (dt.isValid())
            return dt;
    }
    
    return QDateTime::fromSecsSinceEpoch(0);
}

// Archive configuration
QString VMPP::ArchiveTargetType() const
{
    return this->stringProperty("archive_target_type");
}

QVariantMap VMPP::ArchiveTargetConfig() const
{
    return this->property("archive_target_config").toMap();
}

QString VMPP::ArchiveFrequency() const
{
    return this->stringProperty("archive_frequency");
}

QVariantMap VMPP::ArchiveSchedule() const
{
    return this->property("archive_schedule").toMap();
}

bool VMPP::IsArchiveRunning() const
{
    return this->boolProperty("is_archive_running");
}

QDateTime VMPP::ArchiveLastRunTime() const
{
    QVariant timeVariant = this->property("archive_last_run_time");
    if (timeVariant.canConvert<QDateTime>())
        return timeVariant.toDateTime();
    
    QString timeStr = timeVariant.toString();
    if (!timeStr.isEmpty())
    {
        QDateTime dt = Misc::ParseXenDateTime(timeStr);
        if (dt.isValid())
            return dt;
    }
    
    return QDateTime::fromSecsSinceEpoch(0);
}

// VM and alarm configuration
QStringList VMPP::GetVMRefs() const
{
    return this->property("VMs").toStringList();
}

bool VMPP::IsAlarmEnabled() const
{
    return this->boolProperty("is_alarm_enabled");
}

QVariantMap VMPP::AlarmConfig() const
{
    return this->property("alarm_config").toMap();
}

QStringList VMPP::RecentAlerts() const
{
    return this->property("recent_alerts").toStringList();
}

// Helper methods
bool VMPP::IsEnabled() const
{
    return this->IsPolicyEnabled();
}

int VMPP::VMCount() const
{
    return this->GetVMRefs().count();
}

bool VMPP::HasBackupSchedule() const
{
    return !this->BackupSchedule().isEmpty();
}

bool VMPP::HasArchiveSchedule() const
{
    return !this->ArchiveSchedule().isEmpty();
}

QList<QSharedPointer<VM>> VMPP::GetVMs() const
{
    QList<QSharedPointer<VM>> result;
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return result;
    
    XenCache* cache = connection->GetCache();
    if (!cache)
        return result;
    
    QStringList refs = this->GetVMRefs();
    for (const QString& ref : refs)
    {
        if (!ref.isEmpty() && ref != XENOBJECT_NULL)
        {
            QSharedPointer<VM> obj = cache->ResolveObject<VM>(XenObjectType::VM, ref);
            if (obj)
                result.append(obj);
        }
    }
    return result;
}
