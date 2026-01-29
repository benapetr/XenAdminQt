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

#include "pif.h"
#include <QSharedPointer>
#include "clusterhost.h"
#include "network/connection.h"
#include "host.h"
#include "network.h"
#include "bond.h"
#include "pifmetrics.h"
#include "tunnel.h"
#include "vlan.h"
#include "../xencache.h"

PIF::PIF(XenConnection* connection, const QString& opaqueRef, QObject* parent) : XenObject(connection, opaqueRef, parent)
{
}

QString PIF::GetObjectType() const
{
    return "pif";
}

QString PIF::GetName() const
{
    QVariantMap pifData = this->GetData();
    if (pifData.isEmpty())
        return XenObject::GetName();

    XenCache* cache = this->GetCache();
    if (!cache)
        return pifData.value("device", "").toString();

    // Tunnel access PIFs: show the transport PIF's NIC name.
    QVariantList tunnelAccessPifOf = pifData.value("tunnel_access_PIF_of", QVariantList()).toList();
    if (!tunnelAccessPifOf.isEmpty())
    {
        QString tunnelRef = tunnelAccessPifOf.first().toString();
        QVariantMap tunnelData = cache->ResolveObjectData("tunnel", tunnelRef);
        QString transportPifRef = tunnelData.value("transport_PIF", "").toString();
        QSharedPointer<PIF> transportPif = cache->ResolveObject<PIF>("pif", transportPifRef);
        if (transportPif && transportPif->IsValid())
            return transportPif->GetName();
        return pifData.value("device", "").toString();
    }

    // SR-IOV logical PIFs: show the physical PIF's NIC name.
    QVariantList sriovLogicalPifOf = pifData.value("sriov_logical_PIF_of", QVariantList()).toList();
    if (!sriovLogicalPifOf.isEmpty())
    {
        QString sriovRef = sriovLogicalPifOf.first().toString();
        QVariantMap sriovData = cache->ResolveObjectData("network_sriov", sriovRef);
        QString physicalPifRef = sriovData.value("physical_PIF", "").toString();
        QSharedPointer<PIF> physicalPif = cache->ResolveObject<PIF>("pif", physicalPifRef);
        if (physicalPif && physicalPif->IsValid())
            return physicalPif->GetName();
        return pifData.value("device", "").toString();
    }

    // VLAN PIFs: show the tagged PIF's NIC name.
    qint64 vlan = pifData.value("VLAN", -1).toLongLong();
    if (vlan != -1)
    {
        QString vlanMasterOf = pifData.value("VLAN_master_of").toString();
        QVariantMap vlanData = cache->ResolveObjectData("VLAN", vlanMasterOf);
        QString taggedPifRef = vlanData.value("tagged_PIF", "").toString();
        QSharedPointer<PIF> taggedPif = cache->ResolveObject<PIF>("pif", taggedPifRef);
        if (taggedPif && taggedPif->IsValid())
            return taggedPif->GetName();
        return pifData.value("device", "").toString();
    }

    QVariantList bondMasterOfRefs = pifData.value("bond_master_of", QVariantList()).toList();
    if (bondMasterOfRefs.isEmpty())
    {
        QString device = pifData.value("device", "").toString();
        QString number = device;
        number.remove("eth");
        return QString("NIC %1").arg(number);
    }

    QString bondRef = bondMasterOfRefs.first().toString();
    QVariantMap bondData = cache->ResolveObjectData("bond", bondRef);
    QVariantList slaveRefs = bondData.value("slaves", QVariantList()).toList();

    QStringList slaveNumbers;
    for (const QVariant& slaveRefVar : slaveRefs)
    {
        QString slaveRef = slaveRefVar.toString();
        QVariantMap slavePif = cache->ResolveObjectData("pif", slaveRef);
        QString slaveDevice = slavePif.value("device", "").toString();
        QString number = slaveDevice;
        number.remove("eth");
        if (!number.isEmpty())
            slaveNumbers.append(number);
    }

    slaveNumbers.sort();
    return QString("Bond %1").arg(slaveNumbers.join("+"));
}

// Basic properties
QString PIF::GetDevice() const
{
    return this->stringProperty("device");
}

QString PIF::GetNetworkRef() const
{
    return this->stringProperty("network");
}

QSharedPointer<Network> PIF::GetNetwork()
{
    if (!this->GetCache())
        return QSharedPointer<Network>();
    return this->GetCache()->ResolveObject<Network>("network", this->GetNetworkRef());
}

QSharedPointer<Network> PIF::GetNetwork() const
{
    return const_cast<PIF*>(this)->GetNetwork();
}

QString PIF::GetHostRef() const
{
    return this->stringProperty("host");
}

QSharedPointer<Host> PIF::GetHost()
{
    if (!this->GetCache())
        return QSharedPointer<Host>();
    return this->GetCache()->ResolveObject<Host>("host", this->GetHostRef());
}

QString PIF::GetMAC() const
{
    return this->stringProperty("MAC");
}

qlonglong PIF::GetMTU() const
{
    return this->longProperty("MTU");
}

qlonglong PIF::GetVLAN() const
{
    return this->longProperty("VLAN");
}

QString PIF::MetricsRef() const
{
    return this->stringProperty("metrics");
}

bool PIF::IsPhysical() const
{
    return this->GetVLAN() == -1 && !this->IsTunnelAccessPIF() && !this->IsSriovLogicalPIF();
}

bool PIF::IsCurrentlyAttached() const
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

QString PIF::GetLinkStatusString() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return "Unknown";

    XenCache* cache = connection->GetCache();
    if (!cache)
        return "Unknown";

    QStringList tunnelAccessRefs = this->TunnelAccessPIFOfRefs();
    if (!tunnelAccessRefs.isEmpty())
    {
        QSharedPointer<Tunnel> tunnel = cache->ResolveObject<Tunnel>("tunnel", tunnelAccessRefs.first());
        QVariantMap status = tunnel ? tunnel->GetStatus() : QVariantMap();
        bool active = status.value("active", "false").toString() == "true";
        return active ? "Connected" : "Disconnected";
    }

    QString metricsRef = this->MetricsRef();
    if (metricsRef.isEmpty() || metricsRef == XENOBJECT_NULL)
        return "Unknown";

    QSharedPointer<PIFMetrics> metrics = cache->ResolveObject<PIFMetrics>("pif_metrics", metricsRef);
    if (!metrics || !metrics->IsValid())
        return "Unknown";

    bool carrier = metrics->Carrier();

    if (this->IsSriovLogicalPIF() || this->IsVLAN())
    {
        QString networkSriovRef;

        if (this->IsSriovLogicalPIF())
        {
            QStringList sriovLogicalRefs = this->SriovLogicalPIFOfRefs();
            if (!sriovLogicalRefs.isEmpty())
                networkSriovRef = sriovLogicalRefs.first();
        } else
        {
            QString vlanRef = this->VLANMasterOfRef();
            if (!vlanRef.isEmpty())
            {
                QSharedPointer<VLAN> vlan = cache->ResolveObject<VLAN>("vlan", vlanRef);
                QSharedPointer<PIF> taggedPif = vlan ? vlan->GetTaggedPIF() : QSharedPointer<PIF>();
                if (taggedPif && taggedPif->IsValid())
                {
                    QStringList taggedSriovRefs = taggedPif->SriovLogicalPIFOfRefs();
                    if (!taggedSriovRefs.isEmpty())
                        networkSriovRef = taggedSriovRefs.first();
                }
            }
        }

        if (!networkSriovRef.isEmpty())
        {
            QVariantMap sriovData = cache->ResolveObjectData("network_sriov", networkSriovRef);
            QString configMode = sriovData.value("configuration_mode", "unknown").toString();
            bool requiresReboot = sriovData.value("requires_reboot", false).toBool();

            if (!carrier || configMode == "unknown" || requiresReboot)
                return "Disconnected";
        }
    }

    return carrier ? "Connected" : "Disconnected";
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
    return this->IsPrimaryManagementInterface() || this->IsSecondaryManagementInterface(true);
}

bool PIF::IsPrimaryManagementInterface() const
{
    if (!this->Management())
        return false;

    QSharedPointer<Network> network = this->GetNetwork();
    return network && !network->IsGuestInstallerNetwork();
}

bool PIF::IsSecondaryManagementInterface(bool showHiddenObjects) const
{
    if (this->Management())
        return false;

    const QString mode = this->IpConfigurationMode().trimmed().toLower();
    if (mode.isEmpty() || mode == "none" || mode == "unknown")
        return false;

    QSharedPointer<Network> network = this->GetNetwork();
    return network && network->Show(showHiddenObjects);
}

bool PIF::IsVLAN() const
{
    return this->GetVLAN() != -1;
}

bool PIF::IsBondSlave() const
{
    return !this->BondSlaveOfRef().isEmpty() && this->BondSlaveOfRef() != "OpaqueRef:NULL";
}

bool PIF::IsBondMaster() const
{
    return !this->BondMasterOfRefs().isEmpty();
}

bool PIF::IsBondMember() const
{
    return this->IsBondSlave();
}

bool PIF::IsInUseBondMember() const
{
    if (!this->IsBondMember())
        return false;

    XenConnection* connection = this->GetConnection();
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    if (!cache)
        return false;

    const QString bondRef = this->BondSlaveOfRef();
    if (bondRef.isEmpty() || bondRef == "OpaqueRef:NULL")
        return false;

    QSharedPointer<Bond> bond = cache->ResolveObject<Bond>("bond", bondRef);
    if (!bond || !bond->IsValid())
        return false;

    const QString masterRef = bond->MasterRef();
    if (masterRef.isEmpty())
        return false;

    QSharedPointer<PIF> bondInterface = cache->ResolveObject<PIF>("pif", masterRef);
    if (!bondInterface || !bondInterface->IsValid())
        return false;

    return bondInterface->IsCurrentlyAttached();
}

bool PIF::IsBondNIC() const
{
    return this->IsBondMaster();
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

bool PIF::IsUsedByClustering() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return false;

    XenCache* cache = connection->GetCache();
    if (!cache)
        return false;

    const QList<QSharedPointer<ClusterHost>> clusterHosts = cache->GetAll<ClusterHost>("cluster_host");
    for (const QSharedPointer<ClusterHost>& clusterHost : clusterHosts)
    {
        if (!clusterHost || !clusterHost->IsValid())
            continue;
        if (clusterHost->GetPIFRef() == this->OpaqueRef())
            return true;
    }
    return false;
}

bool PIF::SriovCapable() const
{
    return this->Capabilities().contains("sriov");
}

bool PIF::Show(bool showHiddenObjects) const
{
    if (!this->Managed())
        return false;

    if (showHiddenObjects)
        return true;

    return !this->IsHidden();
}
