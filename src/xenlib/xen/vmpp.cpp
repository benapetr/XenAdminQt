#include "vmpp.h"
#include "network/connection.h"
#include "../xencache.h"

VMPP::VMPP(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

QString VMPP::GetObjectType() const
{
    return "vmpp";
}

QString VMPP::Uuid() const
{
    return this->stringProperty("uuid");
}

QString VMPP::NameLabel() const
{
    return this->stringProperty("name_label");
}

QString VMPP::NameDescription() const
{
    return this->stringProperty("name_description");
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
        QDateTime dt = QDateTime::fromString(timeStr, Qt::ISODate);
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
        QDateTime dt = QDateTime::fromString(timeStr, Qt::ISODate);
        if (dt.isValid())
            return dt;
    }
    
    return QDateTime::fromSecsSinceEpoch(0);
}

// VM and alarm configuration
QStringList VMPP::VMRefs() const
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
    return this->VMRefs().count();
}

bool VMPP::HasBackupSchedule() const
{
    return !this->BackupSchedule().isEmpty();
}

bool VMPP::HasArchiveSchedule() const
{
    return !this->ArchiveSchedule().isEmpty();
}
