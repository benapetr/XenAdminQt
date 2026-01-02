#include "pif.h"
#include "network/connection.h"
#include "../xencache.h"

PIF::PIF(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

QString PIF::GetObjectType() const
{
    return "pif";
}

// Basic properties
QString PIF::Uuid() const
{
    return this->stringProperty("uuid");
}

QString PIF::Device() const
{
    return this->stringProperty("device");
}

QString PIF::NetworkRef() const
{
    return this->stringProperty("network");
}

QString PIF::HostRef() const
{
    return this->stringProperty("host");
}

QString PIF::MAC() const
{
    return this->stringProperty("MAC");
}

qlonglong PIF::MTU() const
{
    return this->longProperty("MTU");
}

qlonglong PIF::VLAN() const
{
    return this->longProperty("VLAN");
}

QString PIF::MetricsRef() const
{
    return this->stringProperty("metrics");
}

bool PIF::Physical() const
{
    return this->boolProperty("physical");
}

bool PIF::CurrentlyAttached() const
{
    return this->boolProperty("currently_attached");
}

// IP configuration
QString PIF::IpConfigurationMode() const
{
    return this->stringProperty("ip_configuration_mode");
}

QString PIF::IP() const
{
    return this->stringProperty("IP");
}

QString PIF::Netmask() const
{
    return this->stringProperty("netmask");
}

QString PIF::Gateway() const
{
    return this->stringProperty("gateway");
}

QString PIF::DNS() const
{
    return this->stringProperty("DNS");
}

// IPv6 configuration
QString PIF::Ipv6ConfigurationMode() const
{
    return this->stringProperty("ipv6_configuration_mode");
}

QStringList PIF::IPv6() const
{
    return this->property("IPv6").toStringList();
}

QString PIF::Ipv6Gateway() const
{
    return this->stringProperty("ipv6_gateway");
}

QString PIF::PrimaryAddressType() const
{
    return this->stringProperty("primary_address_type");
}

// Bond configuration
QString PIF::BondSlaveOfRef() const
{
    return this->stringProperty("bond_slave_of");
}

QStringList PIF::BondMasterOfRefs() const
{
    return this->property("bond_master_of").toStringList();
}

// VLAN configuration
QString PIF::VLANMasterOfRef() const
{
    return this->stringProperty("VLAN_master_of");
}

QStringList PIF::VLANSlaveOfRefs() const
{
    return this->property("VLAN_slave_of").toStringList();
}

// Management
bool PIF::Management() const
{
    return this->boolProperty("management");
}

bool PIF::DisallowUnplug() const
{
    return this->boolProperty("disallow_unplug");
}

bool PIF::Managed() const
{
    return this->boolProperty("managed");
}

// Tunnel configuration
QStringList PIF::TunnelAccessPIFOfRefs() const
{
    return this->property("tunnel_access_PIF_of").toStringList();
}

QStringList PIF::TunnelTransportPIFOfRefs() const
{
    return this->property("tunnel_transport_PIF_of").toStringList();
}

// SR-IOV
QStringList PIF::SriovPhysicalPIFOfRefs() const
{
    return this->property("sriov_physical_PIF_of").toStringList();
}

QStringList PIF::SriovLogicalPIFOfRefs() const
{
    return this->property("sriov_logical_PIF_of").toStringList();
}

QString PIF::PCIRef() const
{
    return this->stringProperty("PCI");
}

// Additional properties
QVariantMap PIF::OtherConfig() const
{
    return this->property("other_config").toMap();
}

QVariantMap PIF::Properties() const
{
    return this->property("properties").toMap();
}

QStringList PIF::Capabilities() const
{
    return this->property("capabilities").toStringList();
}

QString PIF::IgmpSnoopingStatus() const
{
    return this->stringProperty("igmp_snooping_status");
}

// Helper methods
bool PIF::IsManagementInterface() const
{
    return this->Management();
}

bool PIF::IsPhysical() const
{
    return this->Physical();
}

bool PIF::IsVLAN() const
{
    return this->VLAN() != -1;
}

bool PIF::IsBondSlave() const
{
    return !this->BondSlaveOfRef().isEmpty() && 
           this->BondSlaveOfRef() != "OpaqueRef:NULL";
}

bool PIF::IsBondMaster() const
{
    return !this->BondMasterOfRefs().isEmpty();
}

bool PIF::IsTunnelAccessPIF() const
{
    return !this->TunnelAccessPIFOfRefs().isEmpty();
}

bool PIF::IsTunnelTransportPIF() const
{
    return !this->TunnelTransportPIFOfRefs().isEmpty();
}

bool PIF::IsSriovPhysicalPIF() const
{
    return !this->SriovPhysicalPIFOfRefs().isEmpty();
}

bool PIF::IsSriovLogicalPIF() const
{
    return !this->SriovLogicalPIFOfRefs().isEmpty();
}
