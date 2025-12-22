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

#ifndef VM_H
#define VM_H

#include "xenobject.h"

class Host;
class VDI;
class VBD;
class VIF;

/**
 * @brief VM - A virtual machine (or 'guest')
 *
 * Qt equivalent of C# XenAPI.VM class. Represents a virtual machine.
 *
 * Key properties (from C# VM class):
 * - name_label, name_description
 * - power_state (Running, Halted, Suspended, Paused)
 * - is_a_template, is_a_snapshot
 * - resident_on, affinity (host placement)
 * - memory_*, VCPUs_* (resource allocation)
 * - VBDs, VIFs, consoles (virtual devices)
 * - snapshot_of, snapshot_time (snapshot relationships)
 */
class XENLIB_EXPORT VM : public XenObject
{
    Q_OBJECT
    Q_PROPERTY(QString powerState READ powerState NOTIFY dataChanged)
    Q_PROPERTY(bool isTemplate READ isTemplate NOTIFY dataChanged)
    Q_PROPERTY(bool isSnapshot READ isSnapshot NOTIFY dataChanged)
    Q_PROPERTY(QString residentOn READ residentOnRef NOTIFY dataChanged)

    public:
        static constexpr int DEFAULT_CORES_PER_SOCKET = 1;
        static constexpr long MAX_SOCKETS = 16;
        static constexpr long MAX_VCPUS_FOR_NON_TRUSTED_VMS = 32;

        explicit VM(XenConnection* connection,
                    const QString& opaqueRef,
                    QObject* parent = nullptr);
        ~VM() override = default;

        /**
         * @brief Get VM power state
         * @return Power state string: "Running", "Halted", "Suspended", "Paused"
         */
        QString powerState() const;

        /**
         * @brief Check if this is a template
         * @return true if template
         */
        bool isTemplate() const;

        /**
         * @brief Check if this is a default template
         * @return true if default template
         */
        bool isDefaultTemplate() const;

        /**
         * @brief Check if this is a snapshot
         * @return true if snapshot
         */
        bool isSnapshot() const;

        /**
         * @brief Get reference to host VM is resident on
         * @return Host opaque reference (empty if not running)
         */
        QString residentOnRef() const;

        /**
         * @brief Get reference to affinity host
         * @return Host opaque reference
         */
        QString affinityRef() const;

        /**
         * @brief Get list of VBD (virtual block device) references
         * @return List of VBD opaque references
         */
        QStringList vbdRefs() const;

        /**
         * @brief Get list of VIF (virtual network interface) references
         * @return List of VIF opaque references
         */
        QStringList vifRefs() const;

        /**
         * @brief Get list of console references
         * @return List of console opaque references
         */
        QStringList consoleRefs() const;

        /**
         * @brief Get snapshot parent reference (if this is a snapshot)
         * @return VM opaque reference of parent VM
         */
        QString snapshotOfRef() const;

        /**
         * @brief Get list of snapshot children (if this VM has snapshots)
         * @return List of VM opaque references
         */
        QStringList snapshotRefs() const;

        /**
         * @brief Get suspend VDI reference
         * @return VDI opaque reference
         */
        QString suspendVDIRef() const;

        /**
         * @brief Get memory target (bytes)
         * @return Memory target in bytes
         */
        qint64 memoryTarget() const;

        /**
         * @brief Get memory static max (bytes)
         * @return Memory static max in bytes
         */
        qint64 memoryStaticMax() const;

        /**
         * @brief Get memory dynamic max (bytes)
         * @return Memory dynamic max in bytes
         */
        qint64 memoryDynamicMax() const;

        /**
         * @brief Get memory dynamic min (bytes)
         * @return Memory dynamic min in bytes
         */
        qint64 memoryDynamicMin() const;

        /**
         * @brief Get memory static min (bytes)
         * @return Memory static min in bytes
         */
        qint64 memoryStaticMin() const;

        /**
         * @brief Get VCPUs max
         * @return Maximum number of VCPUs
         */
        int vcpusMax() const;

        /**
         * @brief Get VCPUs at startup
         * @return Number of VCPUs at startup
         */
        int vcpusAtStartup() const;

        /**
         * @brief Check if VM is HVM
         */
        bool isHvm() const;

        /**
         * @brief Check if VM is Windows
         */
        bool isWindows() const;

        /**
         * @brief Check if vCPU hotplug is supported
         */
        bool supportsVcpuHotplug() const;

        /**
         * @brief Get maximum allowed VCPUs
         */
        int maxVcpusAllowed() const;

        /**
         * @brief Get minimum recommended VCPUs
         */
        int minVcpus() const;

        /**
         * @brief Get vCPU weight from VCPUs_params
         */
        int getVcpuWeight() const;

        /**
         * @brief Get cores per socket from platform
         */
        long getCoresPerSocket() const;

        /**
         * @brief Get maximum cores per socket based on host capabilities
         */
        long maxCoresPerSocket() const;

        /**
         * @brief Validate vCPU configuration
         */
        static QString validVcpuConfiguration(long noOfVcpus, long coresPerSocket);

        /**
         * @brief Get human readable topology string
         */
        static QString getTopology(long sockets, long cores);

        /**
         * @brief Get other_config dictionary
         * @return Map of additional configuration
         */
        QVariantMap otherConfig() const;

        /**
         * @brief Get platform configuration
         * @return Map of platform settings
         */
        QVariantMap platform() const;

        /**
         * @brief Get tags
         * @return List of tag strings
         */
        QStringList tags() const;

        /**
         * @brief Get allowed operations
         * @return List of allowed operation strings
         */
        QStringList allowedOperations() const;

        /**
         * @brief Get current operations
         * @return Map of operation ID to operation type
         */
        QVariantMap currentOperations() const;

        /**
         * @brief Check if VM is running
         * @return true if power state is "Running"
         */
        bool isRunning() const
        {
            return powerState() == "Running";
        }

        /**
         * @brief Check if VM is halted
         * @return true if power state is "Halted"
         */
        bool isHalted() const
        {
            return powerState() == "Halted";
        }

        /**
         * @brief Check if VM is suspended
         * @return true if power state is "Suspended"
         */
        bool isSuspended() const
        {
            return powerState() == "Suspended";
        }

        /**
         * @brief Check if VM is paused
         * @return true if power state is "Paused"
         */
        bool isPaused() const
        {
            return powerState() == "Paused";
        }

        /**
         * @brief Get home host reference
         *
         * Returns the host this VM should preferably run on.
         * This is determined by affinity or current resident host.
         *
         * @return Host opaque reference
         */
        QString homeRef() const;

    protected:
        QString objectType() const override
        {
            return "VM";
        }
};

#endif // VM_H
