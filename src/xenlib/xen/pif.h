#ifndef PIF_H
#define PIF_H

#include "xenobject.h"
#include <QString>
#include <QStringList>
#include <QVariantMap>

/*!
 * \brief Physical network interface (PIF) wrapper class
 * 
 * Represents a physical or virtual network interface on a XenServer host.
 * Provides access to network configuration, IP settings, VLAN, bonding,
 * tunnel, and SR-IOV properties.
 */
class PIF : public XenObject
{
    Q_OBJECT
    
    Q_PROPERTY(QString uuid READ Uuid)
    Q_PROPERTY(QString device READ Device)
    Q_PROPERTY(QString network READ NetworkRef)
    Q_PROPERTY(QString host READ HostRef)
    Q_PROPERTY(QString MAC READ MAC)
    Q_PROPERTY(qlonglong MTU READ MTU)
    Q_PROPERTY(qlonglong VLAN READ VLAN)
    Q_PROPERTY(QString metrics READ MetricsRef)
    Q_PROPERTY(bool physical READ Physical)
    Q_PROPERTY(bool currentlyAttached READ CurrentlyAttached)
    Q_PROPERTY(QString ipConfigurationMode READ IpConfigurationMode)
    Q_PROPERTY(QString IP READ IP)
    Q_PROPERTY(QString netmask READ Netmask)
    Q_PROPERTY(QString gateway READ Gateway)
    Q_PROPERTY(QString DNS READ DNS)
    Q_PROPERTY(QString bondSlaveOf READ BondSlaveOfRef)
    Q_PROPERTY(bool management READ Management)
    Q_PROPERTY(bool disallowUnplug READ DisallowUnplug)
    Q_PROPERTY(QString ipv6ConfigurationMode READ Ipv6ConfigurationMode)
    Q_PROPERTY(QString ipv6Gateway READ Ipv6Gateway)
    Q_PROPERTY(QString primaryAddressType READ PrimaryAddressType)
    Q_PROPERTY(bool managed READ Managed)
    Q_PROPERTY(QString igmpSnoopingStatus READ IgmpSnoopingStatus)
    Q_PROPERTY(QString PCI READ PCIRef)
    
public:
    explicit PIF(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
    
    QString GetObjectType() const override;
    
    // Basic properties
    QString Uuid() const;
    QString Device() const;
    QString NetworkRef() const;
    QString HostRef() const;
    QString MAC() const;
    qlonglong MTU() const;
    qlonglong VLAN() const;
    QString MetricsRef() const;
    bool Physical() const;
    bool CurrentlyAttached() const;
    
    // IP configuration
    QString IpConfigurationMode() const;
    QString IP() const;
    QString Netmask() const;
    QString Gateway() const;
    QString DNS() const;
    
    // IPv6 configuration
    QString Ipv6ConfigurationMode() const;
    QStringList IPv6() const;
    QString Ipv6Gateway() const;
    QString PrimaryAddressType() const;
    
    // Bond configuration
    QString BondSlaveOfRef() const;
    QStringList BondMasterOfRefs() const;
    
    // VLAN configuration
    QString VLANMasterOfRef() const;
    QStringList VLANSlaveOfRefs() const;
    
    // Management
    bool Management() const;
    bool DisallowUnplug() const;
    bool Managed() const;
    
    // Tunnel configuration
    QStringList TunnelAccessPIFOfRefs() const;
    QStringList TunnelTransportPIFOfRefs() const;
    
    // SR-IOV
    QStringList SriovPhysicalPIFOfRefs() const;
    QStringList SriovLogicalPIFOfRefs() const;
    QString PCIRef() const;
    
    // Additional properties
    QVariantMap OtherConfig() const;
    QVariantMap Properties() const;
    QStringList Capabilities() const;
    QString IgmpSnoopingStatus() const;
    
    // Helper methods
    bool IsManagementInterface() const;
    bool IsPhysical() const;
    bool IsVLAN() const;
    bool IsBondSlave() const;
    bool IsBondMaster() const;
    bool IsTunnelAccessPIF() const;
    bool IsTunnelTransportPIF() const;
    bool IsSriovPhysicalPIF() const;
    bool IsSriovLogicalPIF() const;
};

#endif // PIF_H
