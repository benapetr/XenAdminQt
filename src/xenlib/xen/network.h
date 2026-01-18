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

#ifndef NETWORK_H
#define NETWORK_H

#include "xenobject.h"

class VIF;
class PIF;
class XenCache;

/**
 * @brief Network - A virtual network
 *
 * Qt equivalent of C# XenAPI.Network class. Represents a virtual network.
 *
 * Key properties (from C# Network class):
 * - name_label, name_description
 * - bridge (Linux bridge name)
 * - managed (whether bridge is managed by xapi)
 * - MTU (maximum transmission unit)
 * - VIFs (virtual network interfaces connected to this network)
 * - PIFs (physical network interfaces connected to this network)
 * - other_config, tags
 */
class XENLIB_EXPORT Network : public XenObject
{
    Q_OBJECT

    public:
        explicit Network(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        ~Network() override = default;

        //! Get bridge name (Linux bridge name, e.g., "xenbr0")
        QString GetBridge() const;

        //! Check if bridge is managed by xapi (false if external bridge)
        bool IsManaged() const;

        //! If network should be automatically added to new VMs
        bool IsAutomatic() const;

        // Bond helpers (C# parity)
        bool IsBond() const;
        bool IsMember() const;
        bool IsGuestInstallerNetwork() const;
        bool Show(bool showHiddenObjects) const;

        //! Get MTU (Maximum Transmission Unit) in bytes
        qint64 GetMTU() const;

        //! Get list of VIF references (list of VIF opaque references)
        QStringList GetVIFRefs() const;

        //! Get list of PIF references (list of PIF opaque references)
        QStringList GetPIFRefs() const;

        //! Get list of PIF objects
        QList<QSharedPointer<PIF>> GetPIFs() const;

        //! Get list of VIF objects
        QList<QSharedPointer<VIF>> GetVIFs() const;

        //! Get allowed operations (list of allowed network operations)
        QStringList AllowedOperations() const;

        //! Get current operations (map of task ID to operation type)
        QVariantMap CurrentOperations() const;

        //! Get binary blobs associated with this network (map of blob name to blob reference)
        QVariantMap GetBlobs() const;

        //! Get default locking mode for VIFs ("locked", "unlocked", or "disabled")
        QString GetDefaultLockingMode() const;

        //! Get IP addresses assigned to VIFs (map of VIF reference to assigned IP for xapi-managed DHCP networks)
        QVariantMap GetAssignedIPs() const;

        //! Get purposes for which the server will use this network (list of network purpose strings like "nbd", "insecure_nbd", etc.)
        QStringList GetPurpose() const;
        
        //! Get aggregated link status across all PIFs
        QString GetLinkStatusString() const;

        //! Check if network can use jumbo frames
        bool CanUseJumboFrames() const;

        // XenObject interface
        QString GetObjectType() const override { return "network"; }
};

#endif // NETWORK_H
