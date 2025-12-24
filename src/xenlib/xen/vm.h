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

        /**
         * @brief Get user-defined version number
         * @return User version number for this VM
         */
        qint64 userVersion() const;

        /**
         * @brief Get host where VM is scheduled to start
         * @return Opaque reference to host (memory reservation indicator)
         */
        QString scheduledToBeResidentOnRef() const;

        /**
         * @brief Get virtualization memory overhead
         * @return Memory overhead in bytes
         */
        qint64 memoryOverhead() const;

        /**
         * @brief Get vCPU parameters dictionary
         * @return Map of vCPU configuration parameters
         */
        QVariantMap vcpusParams() const;

        /**
         * @brief Get action to take after soft reboot
         * @return Action string (e.g., "soft_reboot", "destroy")
         */
        QString actionsAfterSoftreboot() const;

        /**
         * @brief Get action to take after guest shutdown
         * @return Action string ("destroy", "restart", etc.)
         */
        QString actionsAfterShutdown() const;

        /**
         * @brief Get action to take after guest reboot
         * @return Action string
         */
        QString actionsAfterReboot() const;

        /**
         * @brief Get action to take if guest crashes
         * @return Action string ("destroy", "coredump_and_destroy", etc.)
         */
        QString actionsAfterCrash() const;

        /**
         * @brief Get virtual USB device references
         * @return List of VUSB opaque references
         */
        QStringList vusbRefs() const;

        /**
         * @brief Get crash dump references
         * @return List of Crashdump opaque references
         */
        QStringList crashDumpRefs() const;

        /**
         * @brief Get virtual TPM references
         * @return List of VTPM opaque references
         */
        QStringList vtpmRefs() const;

        /**
         * @brief Get PV bootloader path or name
         * @return Path to bootloader for paravirtualized VMs
         */
        QString pvBootloader() const;

        /**
         * @brief Get PV kernel path
         * @return Path to kernel for paravirtualized VMs
         */
        QString pvKernel() const;

        /**
         * @brief Get PV ramdisk path
         * @return Path to initrd/ramdisk for paravirtualized VMs
         */
        QString pvRamdisk() const;

        /**
         * @brief Get PV kernel command-line arguments
         * @return Kernel boot arguments
         */
        QString pvArgs() const;

        /**
         * @brief Get PV bootloader arguments
         * @return Miscellaneous bootloader arguments
         */
        QString pvBootloaderArgs() const;

        /**
         * @brief Get PV legacy arguments
         * @return Legacy arguments for Zurich guests (deprecated)
         */
        QString pvLegacyArgs() const;

        /**
         * @brief Get HVM boot policy
         * @return HVM boot policy string ("BIOS order", etc.)
         */
        QString hvmBootPolicy() const;

        /**
         * @brief Get HVM boot parameters
         * @return Map of HVM boot configuration (boot order, etc.)
         */
        QVariantMap hvmBootParams() const;

        /**
         * @brief Get HVM shadow page multiplier
         * @return Multiplier for shadow page table allocation
         */
        double hvmShadowMultiplier() const;

        /**
         * @brief Get PCI bus path for passthrough devices
         * @return PCI bus configuration string
         */
        QString pciBus() const;

        /**
         * @brief Get domain ID
         * @return Xen domain ID (if VM is running), or -1
         */
        qint64 domid() const;

        /**
         * @brief Get domain architecture
         * @return Architecture string ("x86_64", "x86_32", etc.) or empty
         */
        QString domarch() const;

        /**
         * @brief Get last boot CPU flags
         * @return Map of CPU flags VM was last booted with
         */
        QVariantMap lastBootCPUFlags() const;

        /**
         * @brief Check if this is a control domain
         * @return true if this is domain 0 or a driver domain
         */
        bool isControlDomain() const;

        /**
         * @brief Get VM metrics reference
         * @return Opaque reference to VM_metrics object
         */
        QString metricsRef() const;

        /**
         * @brief Get guest metrics reference
         * @return Opaque reference to VM_guest_metrics object
         */
        QString guestMetricsRef() const;

        /**
         * @brief Get last booted record
         * @return Marshalled VM record from last boot
         */
        QString lastBootedRecord() const;

        /**
         * @brief Get resource recommendations
         * @return XML specification of recommended resource values
         */
        QString recommendations() const;

        /**
         * @brief Get XenStore data
         * @return Map of key-value pairs for /local/domain/<domid>/vm-data
         */
        QVariantMap xenstoreData() const;

        /**
         * @brief Check if HA always-run is enabled
         * @return true if system will attempt to keep VM running
         */
        bool haAlwaysRun() const;

        /**
         * @brief Get HA restart priority
         * @return Priority string ("restart", "best-effort", "")
         */
        QString haRestartPriority() const;

        /**
         * @brief Get snapshot creation timestamp
         * @return Date/time when snapshot was created
         */
        QDateTime snapshotTime() const;

        /**
         * @brief Get transportable snapshot ID
         * @return Snapshot ID for XVA export
         */
        QString transportableSnapshotId() const;

        /**
         * @brief Get binary large objects
         * @return Map of blob names to blob references
         */
        QVariantMap blobs() const;

        /**
         * @brief Get blocked operations
         * @return Map of blocked operations to error codes
         */
        QVariantMap blockedOperations() const;

        /**
         * @brief Get snapshot information
         * @return Map of human-readable snapshot metadata
         */
        QVariantMap snapshotInfo() const;

        /**
         * @brief Get encoded snapshot metadata
         * @return Encoded information about VM's metadata
         */
        QString snapshotMetadata() const;

        /**
         * @brief Get parent VM reference
         * @return Opaque reference to parent VM
         */
        QString parentRef() const;

        /**
         * @brief Get child VM references
         * @return List of child VM opaque references
         */
        QStringList childrenRefs() const;

        /**
         * @brief Get BIOS strings
         * @return Map of BIOS string identifiers to values
         */
        QVariantMap biosStrings() const;

        /**
         * @brief Get VM protection policy reference
         * @return Opaque reference to VMPP (VM Protection Policy)
         */
        QString protectionPolicyRef() const;

        /**
         * @brief Check if snapshot was created by VMPP
         * @return true if this snapshot was created by protection policy
         */
        bool isSnapshotFromVmpp() const;

        /**
         * @brief Get VM snapshot schedule reference
         * @return Opaque reference to VMSS (VM Snapshot Schedule)
         */
        QString snapshotScheduleRef() const;

        /**
         * @brief Check if snapshot was created by VMSS
         * @return true if this snapshot was created by snapshot schedule
         */
        bool isVmssSnapshot() const;

        /**
         * @brief Get VM appliance reference
         * @return Opaque reference to VM_appliance
         */
        QString applianceRef() const;

        /**
         * @brief Get appliance start delay
         * @return Delay in seconds before proceeding to next order in startup
         */
        qint64 startDelay() const;

        /**
         * @brief Get appliance shutdown delay
         * @return Delay in seconds before proceeding to next order in shutdown
         */
        qint64 shutdownDelay() const;

        /**
         * @brief Get appliance boot order
         * @return Point in startup/shutdown sequence for this VM
         */
        qint64 order() const;

        /**
         * @brief Get virtual GPU references
         * @return List of VGPU opaque references
         */
        QStringList vgpuRefs() const;

        /**
         * @brief Get attached PCI device references
         * @return List of currently passed-through PCI device references
         */
        QStringList attachedPCIRefs() const;

        /**
         * @brief Get suspend SR reference
         * @return Opaque reference to SR where suspend image is stored
         */
        QString suspendSRRef() const;

        /**
         * @brief Get VM version (recovery count)
         * @return Number of times this VM has been recovered
         */
        qint64 version() const;

        /**
         * @brief Get VM generation ID
         * @return Generation ID string (for AD domain controllers)
         */
        QString generationId() const;

        /**
         * @brief Get hardware platform version
         * @return Host virtual hardware platform version VM can run on
         */
        qint64 hardwarePlatformVersion() const;

        /**
         * @brief Check if vendor device is present
         * @return true if emulated C000 PCI device is enabled (for Windows Update)
         */
        bool hasVendorDevice() const;

        /**
         * @brief Check if VM requires reboot
         * @return true if VM needs reboot to apply configuration changes
         */
        bool requiresReboot() const;

        /**
         * @brief Get immutable template reference label
         * @return Textual reference to template used to create this VM
         */
        QString referenceLabel() const;

        /**
         * @brief Get domain type
         * @return Domain type string ("hvm", "pv", "pvh", "pv_in_pvh", "unspecified")
         */
        QString domainType() const;

        /**
         * @brief Get NVRAM data
         * @return Map of NVRAM key-value pairs (UEFI variables, etc.)
         */
        QVariantMap nvram() const;

        /**
         * @brief Get pending update guidances
         * @return List of guidance strings for pending updates
         */
        QStringList pendingGuidances() const;

    protected:
        QString objectType() const override;
};

#endif // VM_H
