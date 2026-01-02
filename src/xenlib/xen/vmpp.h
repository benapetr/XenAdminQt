#ifndef VMPP_H
#define VMPP_H

#include "xenobject.h"
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QDateTime>

/*!
 * \brief VM Protection Policy wrapper class
 * 
 * Represents a VM protection policy configuration for backup and archival.
 * Provides access to backup scheduling, archive settings, and alarm configuration.
 * First published in XenServer 5.6 FP1.
 */
class VMPP : public XenObject
{
    Q_OBJECT
    
    Q_PROPERTY(QString uuid READ Uuid)
    Q_PROPERTY(QString nameLabel READ NameLabel)
    Q_PROPERTY(QString nameDescription READ NameDescription)
    Q_PROPERTY(bool isPolicyEnabled READ IsPolicyEnabled)
    Q_PROPERTY(QString backupType READ BackupType)
    Q_PROPERTY(qlonglong backupRetentionValue READ BackupRetentionValue)
    Q_PROPERTY(QString backupFrequency READ BackupFrequency)
    Q_PROPERTY(bool isBackupRunning READ IsBackupRunning)
    Q_PROPERTY(QDateTime backupLastRunTime READ BackupLastRunTime)
    Q_PROPERTY(QString archiveTargetType READ ArchiveTargetType)
    Q_PROPERTY(QString archiveFrequency READ ArchiveFrequency)
    Q_PROPERTY(bool isArchiveRunning READ IsArchiveRunning)
    Q_PROPERTY(QDateTime archiveLastRunTime READ ArchiveLastRunTime)
    Q_PROPERTY(bool isAlarmEnabled READ IsAlarmEnabled)
    
    public:
        explicit VMPP(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);

        QString GetObjectType() const override;

        // Basic properties
        QString Uuid() const;
        QString NameLabel() const;
        QString NameDescription() const;
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
        QStringList VMRefs() const;
        bool IsAlarmEnabled() const;
        QVariantMap AlarmConfig() const;
        QStringList RecentAlerts() const;

        // Helper methods
        bool IsEnabled() const;
        int VMCount() const;
        bool HasBackupSchedule() const;
        bool HasArchiveSchedule() const;
};

#endif // VMPP_H
