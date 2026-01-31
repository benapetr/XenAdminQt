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

#ifndef VMMETRICS_H
#define VMMETRICS_H

#include "xenobject.h"
#include <QDateTime>
#include <QVariantMap>
#include <QStringList>

/**
 * @brief VM metrics XenObject wrapper
 *
 * Provides typed access to VM_metrics fields in XenCache.
 * Qt equivalent of C# XenAPI.VM_metrics class.
 *
 * The metrics associated with a VM.
 * First published in XenServer 4.0.
 *
 * Key properties (from C# VM_metrics class):
 * - uuid - Unique identifier
 * - memory_actual - Guest's actual memory (bytes)
 * - VCPUs_number - Current number of VCPUs
 * - VCPUs_utilisation - Utilisation for all of guest's current VCPUs (deprecated since 6.1)
 * - VCPUs_CPU - VCPU to PCPU map
 * - VCPUs_params - The live equivalent to VM.VCPUs_params
 * - VCPUs_flags - CPU flags (blocked, online, running)
 * - state - The state of the guest (blocked, dying, etc)
 * - start_time - Time at which this VM was last booted
 * - install_time - Time at which the VM was installed
 * - last_updated - Time at which this information was last updated
 * - other_config - Additional configuration (XenServer 5.0+)
 * - hvm - Hardware virtual machine (XenServer 7.1+)
 * - nested_virt - VM supports nested virtualisation (XenServer 7.1+)
 * - nomigrate - VM is immobile and can't migrate between hosts (XenServer 7.1+)
 * - current_domain_type - The current domain type of the VM (XenServer 7.5+)
 */
class XENLIB_EXPORT VMMetrics : public XenObject
{
    Q_OBJECT

    public:
        explicit VMMetrics(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        ~VMMetrics() override = default;

        /**
         * @brief Get guest's actual memory in bytes
         * @return Memory in bytes
         *
         * C# equivalent: VM_metrics.memory_actual
         */
        qint64 GetMemoryActual() const;

        /**
         * @brief Get current number of VCPUs
         * @return Number of VCPUs
         *
         * C# equivalent: VM_metrics.VCPUs_number
         */
        qint64 GetVCPUsNumber() const;

        /**
         * @brief Get VCPU to PCPU map
         * @return Map of virtual CPU to physical CPU assignments
         *
         * C# equivalent: VM_metrics.VCPUs_CPU
         */
        QVariantMap GetVCPUsCPU() const;

        /**
         * @brief Get live VCPU parameters
         * @return Map of VCPU parameters
         *
         * C# equivalent: VM_metrics.VCPUs_params
         */
        QVariantMap GetVCPUsParams() const;

        /**
         * @brief Get CPU flags (blocked, online, running)
         * @return Map of VCPU index to flags array
         *
         * C# equivalent: VM_metrics.VCPUs_flags
         */
        QVariantMap GetVCPUsFlags() const;

        /**
         * @brief Get the state of the guest (blocked, dying, etc)
         * @return List of state strings
         *
         * C# equivalent: VM_metrics.state
         */
        QStringList GetState() const;

        /**
         * @brief Get time at which this VM was last booted
         * @return Boot timestamp
         *
         * C# equivalent: VM_metrics.start_time
         */
        QDateTime GetStartTime() const;

        /**
         * @brief Get time at which the VM was installed
         * @return Install timestamp
         *
         * C# equivalent: VM_metrics.install_time
         */
        QDateTime GetInstallTime() const;

        /**
         * @brief Get timestamp of last update
         * @return Last updated timestamp
         *
         * C# equivalent: VM_metrics.last_updated
         */
        QDateTime GetLastUpdated() const;

        /**
         * @brief Check if this is a hardware virtual machine
         * @return true if HVM
         *
         * First published in XenServer 7.1.
         * C# equivalent: VM_metrics.hvm
         */
        bool IsHVM() const;

        /**
         * @brief Check if VM supports nested virtualisation
         * @return true if nested virt is supported
         *
         * First published in XenServer 7.1.
         * C# equivalent: VM_metrics.nested_virt
         */
        bool SupportsNestedVirt() const;

        /**
         * @brief Check if VM is immobile and can't migrate between hosts
         * @return true if VM cannot migrate
         *
         * First published in XenServer 7.1.
         * C# equivalent: VM_metrics.nomigrate
         */
        bool IsNoMigrate() const;

        /**
         * @brief Get the current domain type of the VM
         * @return Domain type string (e.g., "hvm", "pv", "pv_in_pvh", "unspecified")
         *
         * First published in XenServer 7.5.
         * C# equivalent: VM_metrics.current_domain_type
         */
        QString GetCurrentDomainType() const;

    protected:
        XenObjectType GetObjectType() const override { return XenObjectType::VMMetrics; }

    private:
        QDateTime parseDateTime(const QString& dateStr) const;
};

#endif // VMMETRICS_H
