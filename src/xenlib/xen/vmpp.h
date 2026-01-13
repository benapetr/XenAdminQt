#ifndef VMPP_H
#define VMPP_H

#include "xenobject.h"
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QDateTime>
#include <QSharedPointer>

class VM;

/*!
 * \brief VM Protection Policy wrapper class
 * 
 * Represents a VM protection policy configuration for backup and archival.
 * Provides access to backup scheduling, archive settings, and alarm configuration.
 * First published in XenServer 5.6 FP1.
 */
class XENLIB_EXPORT VMPP : public XenObject
{
    Q_OBJECT

    public:
        explicit VMPP(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);

        QString GetObjectType() const override;

        // Basic properties
        bool IsPolicyEnabled() const;

        // Backup configuration
        QString BackupType() const;
        qlonglong BackupRetentionValue() const;
        QString BackupFrequency() const;
        QVariantMap BackupSchedule() const;
        bool IsBackupRunning() const;
        QDateTime BackupLastRunTime() const;

        // Archive configuration
        QString ArchiveTargetType() const;
        QVariantMap ArchiveTargetConfig() const;
        QString ArchiveFrequency() const;
        QVariantMap ArchiveSchedule() const;
        bool IsArchiveRunning() const;
        QDateTime ArchiveLastRunTime() const;

        // VM and alarm configuration
        QStringList GetVMRefs() const;
        bool IsAlarmEnabled() const;
        QVariantMap AlarmConfig() const;
        QStringList RecentAlerts() const;

        // Helper methods
        bool IsEnabled() const;
        int VMCount() const;
        bool HasBackupSchedule() const;
        bool HasArchiveSchedule() const;

        // Object resolution getters
        QList<QSharedPointer<VM>> GetVMs() const;
};

#endif // VMPP_H
