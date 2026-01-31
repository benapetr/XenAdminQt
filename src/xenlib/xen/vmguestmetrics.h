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

#ifndef VMGUESTMETRICS_H
#define VMGUESTMETRICS_H

#include "xenobject.h"
#include <QDateTime>
#include <QVariantMap>

/**
 * @brief VM guest metrics XenObject wrapper
 *
 * Provides typed access to VM_guest_metrics fields in XenCache.
 * Qt equivalent of C# XenAPI.VM_guest_metrics class.
 *
 * The metrics reported by the guest (as opposed to inferred from outside).
 * First published in XenServer 4.0.
 *
 * Key properties (from C# VM_guest_metrics class):
 * - uuid - Unique identifier
 * - os_version - Version of the OS
 * - PV_drivers_version - Version of the PV drivers
 * - PV_drivers_up_to_date - Deprecated (logically equivalent to PV_drivers_detected)
 * - memory - Memory configuration (deprecated since 5.5)
 * - disks - Disk configuration (deprecated since 5.0)
 * - networks - Network configuration
 * - other - Anything else
 * - last_updated - Time at which this information was last updated
 * - other_config - Additional configuration (XenServer 5.0+)
 * - live - Guest is sending heartbeat via guest agent (XenServer 5.0+)
 * - can_use_hotplug_vbd - Guest supports VBD hotplug (XenServer 7.0+)
 * - can_use_hotplug_vif - Guest supports VIF hotplug (XenServer 7.0+)
 * - PV_drivers_detected - At least one device connected to backend (XenServer 7.0+)
 */
class XENLIB_EXPORT VMGuestMetrics : public XenObject
{
    Q_OBJECT

    public:
        explicit VMGuestMetrics(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        ~VMGuestMetrics() override = default;

        XenObjectType GetObjectType() const override
        {
            return XenObjectType::VMGuestMetrics;
        }

        /**
         * @brief Get version of the OS
         * @return Map of OS version information
         *
         * C# equivalent: VM_guest_metrics.os_version
         */
        QVariantMap GetOSVersion() const;

        /**
         * @brief Get version of the PV drivers
         * @return Map of PV driver version information
         *
         * C# equivalent: VM_guest_metrics.PV_drivers_version
         */
        QVariantMap GetPVDriversVersion() const;

        /**
         * @brief Check if PV drivers are up to date
         * @return True if up to date
         *
         * Deprecated since XenServer 7.0 (logically equivalent to PV_drivers_detected)
         * C# equivalent: VM_guest_metrics.PV_drivers_up_to_date
         */
        bool GetPVDriversUpToDate() const;

        /**
         * @brief Get memory configuration
         * @return Map of memory information
         *
         * Deprecated since XenServer 5.5
         * C# equivalent: VM_guest_metrics.memory
         */
        QVariantMap GetMemory() const;

        /**
         * @brief Get disk configuration
         * @return Map of disk information
         *
         * Deprecated since XenServer 5.0
         * C# equivalent: VM_guest_metrics.disks
         */
        QVariantMap GetDisks() const;

        /**
         * @brief Get network configuration
         * @return Map of network information
         *
         * C# equivalent: VM_guest_metrics.networks
         */
        QVariantMap GetNetworks() const;

        /**
         * @brief Get other guest information
         * @return Map of additional information
         *
         * C# equivalent: VM_guest_metrics.other
         */
        QVariantMap GetOther() const;

        /**
         * @brief Get time at which this information was last updated
         * @return Last updated timestamp
         *
         * C# equivalent: VM_guest_metrics.last_updated
         */
        QDateTime GetLastUpdated() const;

        /**
         * @brief Get other configuration
         * @return Map of other configuration
         *
         * First published in XenServer 5.0
         * C# equivalent: VM_guest_metrics.other_config
         */
        QVariantMap GetOtherConfig() const;

        /**
         * @brief Check if guest is sending heartbeat messages via guest agent
         * @return True if live
         *
         * First published in XenServer 5.0
         * C# equivalent: VM_guest_metrics.live
         */
        bool IsLive() const;

        /**
         * @brief Check if guest supports VBD hotplug
         * @return Tristate value (yes/no/unspecified)
         *
         * First published in XenServer 7.0
         * C# equivalent: VM_guest_metrics.can_use_hotplug_vbd
         */
        QString GetCanUseHotplugVBD() const;

        /**
         * @brief Check if guest supports VIF hotplug
         * @return Tristate value (yes/no/unspecified)
         *
         * First published in XenServer 7.0
         * C# equivalent: VM_guest_metrics.can_use_hotplug_vif
         */
        QString GetCanUseHotplugVIF() const;

        /**
         * @brief Check if PV drivers are detected
         * @return True if at least one device has successfully connected to backend
         *
         * First published in XenServer 7.0
         * C# equivalent: VM_guest_metrics.PV_drivers_detected
         */
        bool GetPVDriversDetected() const;

    private:
        /**
         * @brief Parse XenServer datetime string
         * @param dateStr ISO format datetime string
         * @return Parsed QDateTime
         */
        QDateTime parseDateTime(const QString& dateStr) const;
};

#endif // VMGUESTMETRICS_H
