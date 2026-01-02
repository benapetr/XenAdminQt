#ifndef VGPU_H
#define VGPU_H

#include "xenobject.h"
#include <QString>
#include <QVariantMap>

/*!
 * \brief Virtual GPU device wrapper class
 * 
 * Represents a virtual GPU (vGPU) device attached to a VM.
 * Provides access to GPU group, type, physical GPU assignment, and configuration.
 * First published in XenServer 6.0.
 */
class VGPU : public XenObject
{
    Q_OBJECT
    
    Q_PROPERTY(QString uuid READ Uuid)
    Q_PROPERTY(QString VM READ VMRef)
    Q_PROPERTY(QString gpuGroup READ GPUGroupRef)
    Q_PROPERTY(QString device READ Device)
    Q_PROPERTY(bool currentlyAttached READ CurrentlyAttached)
    Q_PROPERTY(QString type READ TypeRef)
    Q_PROPERTY(QString residentOn READ ResidentOnRef)
    Q_PROPERTY(QString scheduledToBeResidentOn READ ScheduledToBeResidentOnRef)
    Q_PROPERTY(QString extraArgs READ ExtraArgs)
    Q_PROPERTY(QString PCI READ PCIRef)
    
public:
    explicit VGPU(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
    
    QString GetObjectType() const override;
    
    // Basic properties
    QString Uuid() const;
    QString VMRef() const;
    QString GPUGroupRef() const;
    QString Device() const;
    bool CurrentlyAttached() const;
    QVariantMap OtherConfig() const;
    
    // GPU configuration
    QString TypeRef() const;
    QString ResidentOnRef() const;
    QString ScheduledToBeResidentOnRef() const;
    QVariantMap CompatibilityMetadata() const;
    QString ExtraArgs() const;
    QString PCIRef() const;
    
    // Helper methods
    bool IsAttached() const;
    bool IsResident() const;
    bool HasScheduledLocation() const;
};

#endif // VGPU_H
