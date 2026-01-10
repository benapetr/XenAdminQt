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

#ifndef PIF_H
#define PIF_H

#include "xenobject.h"
#include <QString>
#include <QStringList>
#include <QVariantMap>

class Host;
class Network;

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
    
    public:
        explicit PIF(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);

        QString GetObjectType() const override;
        QString GetName() const override;

        // Basic properties
        QString GetDevice() const;
        QString GetNetworkRef() const;
        QSharedPointer<Network> GetNetwork();
        QString GetHostRef() const;
        QSharedPointer<Host> GetHost();
        QString GetMAC() const;
        qlonglong GetMTU() const;
        qlonglong GetVLAN() const;
        QString MetricsRef() const;
        bool IsPhysical() const;
        bool IsCurrentlyAttached() const;

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
        bool IsVLAN() const;
        bool IsBondSlave() const;
        bool IsBondMaster() const;
        bool IsBondMember() const;
        bool IsTunnelAccessPIF() const;
        bool IsTunnelTransportPIF() const;
        bool IsSriovPhysicalPIF() const;
        bool IsSriovLogicalPIF() const;
        bool Show(bool showHiddenObjects) const;
};

#endif // PIF_H
