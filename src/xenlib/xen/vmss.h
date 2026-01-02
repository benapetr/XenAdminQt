#ifndef VMSS_H
#define VMSS_H

#include "xenobject.h"
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QDateTime>

/*!
 * \brief VM Snapshot Schedule wrapper class
 * 
 * Represents a VM snapshot schedule configuration.
 * Provides access to snapshot scheduling, retention policies, and attached VMs.
 * First published in XenServer 7.2.
 */
class VMSS : public XenObject
{
    Q_OBJECT
    
    Q_PROPERTY(QString uuid READ Uuid)
    Q_PROPERTY(QString nameLabel READ NameLabel)
    Q_PROPERTY(QString nameDescription READ NameDescription)
    Q_PROPERTY(bool enabled READ Enabled)
    Q_PROPERTY(QString type READ Type)
    Q_PROPERTY(qlonglong retainedSnapshots READ RetainedSnapshots)
    Q_PROPERTY(QString frequency READ Frequency)
    Q_PROPERTY(QDateTime lastRunTime READ LastRunTime)
    
public:
    explicit VMSS(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
    
    QString GetObjectType() const override;
    
    // Basic properties
    QString Uuid() const;
    QString NameLabel() const;
    QString NameDescription() const;
    bool Enabled() const;
    QString Type() const;
    qlonglong RetainedSnapshots() const;
    QString Frequency() const;
    QVariantMap Schedule() const;
    QDateTime LastRunTime() const;
    QStringList VMRefs() const;
    
    // Helper methods
    bool IsEnabled() const;
    int VMCount() const;
};

#endif // VMSS_H
