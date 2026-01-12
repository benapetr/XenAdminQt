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
#include "network/comparableaddress.h"
#include <QDomElement>

class Host;
class VDI;
class VBD;
class VIF;
class Pool;

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
    Q_PROPERTY(QString powerState READ GetPowerState NOTIFY dataChanged)
    Q_PROPERTY(bool IsTemplate READ IsTemplate NOTIFY dataChanged)
    Q_PROPERTY(bool isSnapshot READ IsSnapshot NOTIFY dataChanged)
    Q_PROPERTY(QString residentOn READ ResidentOnRef NOTIFY dataChanged)

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
        QString GetPowerState() const;

        /**
         * @brief Check if this is a template
         * @return true if template
         */
        bool IsTemplate() const;

        /**
         * @brief Check if this is a default template
         * @return true if default template
         */
        bool IsDefaultTemplate() const;

        /**
         * @brief Check if this is a default template (C# DefaultTemplate)
         */
        bool DefaultTemplate() const;

        /**
         * @brief Check if this is an internal template (C# InternalTemplate)
         */
        bool InternalTemplate() const;

        /**
         * @brief Check if this object should be shown in the UI
         */
        bool Show(bool showHiddenVMs) const;

        /**
         * @brief Check if this is a snapshot
         * @return true if snapshot
         */
        bool IsSnapshot() const;

        /**
         * @brief Get reference to host VM is resident on
         * @return Host opaque reference (empty if not running)
         */
        QString ResidentOnRef() const;

        QSharedPointer<Host> GetHost();
        QSharedPointer<Pool> GetPool();

        /**
         * @brief Get reference to affinity host
         * @return Host opaque reference
         */
        QString AffinityRef() const;

        /**
         * @brief Get list of VBD (virtual block device) references
         * @return List of VBD opaque references
         */
        QStringList GetVBDRefs() const;
        QSharedPointer<VBD> FindVMCDROM() const;

        /**
         * @brief Get list of VIF (virtual network interface) references
         * @return List of VIF opaque references
         */
        QStringList GetVIFRefs() const;

        /**
         * @brief Get list of console references
         * @return List of console opaque references
         */
        QStringList GetConsoleRefs() const;

        /**
         * @brief Get snapshot parent reference (if this is a snapshot)
         * @return VM opaque reference of parent VM
         */
        QString SnapshotOfRef() const;

        QSharedPointer<VM> SnapshotOf();

        /**
         * @brief Get list of snapshot children (if this VM has snapshots)
         * @return List of VM opaque references
         */
        QStringList GetSnapshotRefs() const;

        /**
         * @brief Get suspend VDI reference
         * @return VDI opaque reference
         */
        QString GetSuspendVDIRef() const;

        /**
         * @brief Get memory target (bytes)
         * @return Memory target in bytes
         */
        qint64 MemoryTarget() const;

        /**
         * @brief Get memory static max (bytes)
         * @return Memory static max in bytes
         */
        qint64 MemoryStaticMax() const;

        /**
         * @brief Get memory dynamic max (bytes)
         * @return Memory dynamic max in bytes
         */
        qint64 MemoryDynamicMax() const;

        /**
         * @brief Get memory dynamic min (bytes)
         * @return Memory dynamic min in bytes
         */
        qint64 MemoryDynamicMin() const;

        /**
         * @brief Get memory static min (bytes)
         * @return Memory static min in bytes
         */
        qint64 MemoryStaticMin() const;

        /**
         * @brief Get VCPUs max
         * @return Maximum number of VCPUs
         */
        int VCPUsMax() const;

        /**
         * @brief Get VCPUs at startup
         * @return Number of VCPUs at startup
         */
        int VCPUsAtStartup() const;

        /**
         * @brief Check if VM is HVM
         */
        bool IsHVM() const;

        /**
         * @brief Check if VM is Windows
         */
        bool IsWindows() const;

        /**
         * @brief Check if vCPU hotplug is supported
         */
        bool SupportsVCPUHotplug() const;

        /**
         * @brief Get maximum allowed VCPUs
         */
        int MaxVCPUsAllowed() const;

        /**
         * @brief Get maximum allowed VBDs (virtual block devices)
         */
        int MaxVBDsAllowed() const;

        /**
         * @brief Get minimum recommended VCPUs
         */
        int MinVCPUs() const;

        /**
         * @brief Get vCPU weight from VCPUs_params
         */
        int GetVCPUWeight() const;

        /**
         * @brief Get cores per socket from platform
         */
        long GetCoresPerSocket() const;

        /**
         * @brief Get maximum cores per socket based on host capabilities
         */
        long MaxCoresPerSocket() const;

        /**
         * @brief Validate vCPU configuration
         */
        static QString ValidVCPUConfiguration(long noOfVcpus, long coresPerSocket);

        /**
         * @brief Get human readable topology string
         */
        static QString GetTopology(long sockets, long cores);

        /**
         * @brief Get other_config dictionary
         * @return Map of additional configuration
         */
        QVariantMap OtherConfig() const;

        /**
         * @brief Parse the provisioning XML from other_config["disks"]
         * @return Root element or null element if missing/invalid
         */
        QDomElement ProvisionXml() const;

        /**
         * @brief Get platform configuration
         * @return Map of platform settings
         */
        QVariantMap Platform() const;

        /**
         * @brief Get tags
         * @return List of tag strings
         */
        QStringList Tags() const;

        /**
         * @brief Get allowed operations
         * @return List of allowed operation strings
         */
        QStringList GetAllowedOperations() const;

        /**
         * @brief Get current operations
         * @return Map of operation ID to operation type
         */
        QVariantMap CurrentOperations() const;

        /**
         * @brief Check if VM can migrate to a host
         *
         * Mirrors basic C# migrate prechecks (allowed_operations, same-host).
         *
         * @param hostRef Destination host opaque reference
         * @param error Optional output for failure reason
         * @return true if migration is allowed
         */
        bool CanMigrateToHost(const QString& hostRef, QString* error = nullptr) const;

        /**
         * @brief Check if VM can be moved within the pool (VDI copy + destroy)
         *
         * Matches C# VM.CanBeMoved().
         *
         * @return true if VM is eligible for an intra-pool move
         */
        bool CanBeMoved() const;

        /**
         * @brief Check if any disk supports fast clone on its SR
         *
         * Matches C# VM.AnyDiskFastClonable().
         *
         * @return true if any disk is fast-clonable
         */
        bool AnyDiskFastClonable() const;

        /**
         * @brief Check if VM has at least one disk VBD
         *
         * Matches C# VM.HasAtLeastOneDisk().
         *
         * @return true if VM has at least one disk VBD
         */
        bool HasAtLeastOneDisk() const;

        /**
         * @brief Check if VM is running
         * @return true if power state is "Running"
         */
        bool IsRunning() const
        {
            return GetPowerState() == "Running";
        }

        /**
         * @brief Check if VM is halted
         * @return true if power state is "Halted"
         */
        bool IsHalted() const
        {
            return GetPowerState() == "Halted";
        }

        /**
         * @brief Check if VM is suspended
         * @return true if power state is "Suspended"
         */
        bool IsSuspended() const
        {
            return GetPowerState() == "Suspended";
        }

        /**
         * @brief Check if VM is paused
         * @return true if power state is "Paused"
         */
        bool IsPaused() const
        {
            return GetPowerState() == "Paused";
        }

        /**
         * @brief Get home host reference
         *
         * Returns the host this VM should preferably run on.
         * This is determined by affinity or current resident host.
         *
         * @return Host opaque reference
         */
        QString GetHomeRef() const;

        /**
         * @brief Get user-defined version number
         * @return User version number for this VM
         */
        qint64 UserVersion() const;

        /**
         * @brief Get host where VM is scheduled to start
         * @return Opaque reference to host (memory reservation indicator)
         */
        QString ScheduledToBeResidentOnRef() const;

        /**
         * @brief Get virtualization memory overhead
         * @return Memory overhead in bytes
         */
        qint64 MemoryOverhead() const;

        /**
         * @brief Get vCPU parameters dictionary
         * @return Map of vCPU configuration parameters
         */
        QVariantMap VCPUsParams() const;

        /**
         * @brief Get action to take after soft reboot
         * @return Action string (e.g., "soft_reboot", "destroy")
         */
        QString ActionsAfterSoftreboot() const;

        /**
         * @brief Get action to take after guest shutdown
         * @return Action string ("destroy", "restart", etc.)
         */
        QString ActionsAfterShutdown() const;

        /**
         * @brief Get action to take after guest reboot
         * @return Action string
         */
        QString ActionsAfterReboot() const;

        /**
         * @brief Get action to take if guest crashes
         * @return Action string ("destroy", "coredump_and_destroy", etc.)
         */
        QString ActionsAfterCrash() const;

        /**
         * @brief Get virtual USB device references
         * @return List of VUSB opaque references
         */
        QStringList VUSBRefs() const;

        /**
         * @brief Get crash dump references
         * @return List of Crashdump opaque references
         */
        QStringList CrashDumpRefs() const;

        /**
         * @brief Get virtual TPM references
         * @return List of VTPM opaque references
         */
        QStringList VTPMRefs() const;

        /**
         * @brief Get PV bootloader path or name
         * @return Path to bootloader for paravirtualized VMs
         */
        QString PVBootloader() const;

        /**
         * @brief Get PV kernel path
         * @return Path to kernel for paravirtualized VMs
         */
        QString PVKernel() const;

        /**
         * @brief Get PV ramdisk path
         * @return Path to initrd/ramdisk for paravirtualized VMs
         */
        QString PVRamdisk() const;

        /**
         * @brief Get PV kernel command-line arguments
         * @return Kernel boot arguments
         */
        QString PVArgs() const;

        /**
         * @brief Get PV bootloader arguments
         * @return Miscellaneous bootloader arguments
         */
        QString PVBootloaderArgs() const;

        /**
         * @brief Get PV legacy arguments
         * @return Legacy arguments for Zurich guests (deprecated)
         */
        QString PVLegacyArgs() const;

        /**
         * @brief Get HVM boot policy
         * @return HVM boot policy string ("BIOS order", etc.)
         */
        QString HVMBootPolicy() const;

        /**
         * @brief Get HVM boot parameters
         * @return Map of HVM boot configuration (boot order, etc.)
         */
        QVariantMap HVMBootParams() const;

        /**
         * @brief Get HVM shadow page multiplier
         * @return Multiplier for shadow page table allocation
         */
        double HVMShadowMultiplier() const;

        /**
         * @brief Get PCI bus path for passthrough devices
         * @return PCI bus configuration string
         */
        QString PCIBus() const;

        /**
         * @brief Get domain ID
         * @return Xen domain ID (if VM is running), or -1
         */
        qint64 Domid() const;

        /**
         * @brief Get domain architecture
         * @return Architecture string ("x86_64", "x86_32", etc.) or empty
         */
        QString Domarch() const;

        /**
         * @brief Get last boot CPU flags
         * @return Map of CPU flags VM was last booted with
         */
        QVariantMap LastBootCPUFlags() const;

        /**
         * @brief Check if this is a control domain
         * @return true if this is domain 0 or a driver domain
         */
        bool IsControlDomain() const;

        /**
         * @brief Get VM metrics reference
         * @return Opaque reference to VM_metrics object
         */
        QString MetricsRef() const;

        /**
         * @brief Get guest metrics reference
         * @return Opaque reference to VM_guest_metrics object
         */
        QString GetGuestMetricsRef() const;

        /**
         * @brief Get last booted record
         * @return Marshalled VM record from last boot
         */
        QString LastBootedRecord() const;

        /**
         * @brief Get resource recommendations
         * @return XML specification of recommended resource values
         */
        QString Recommendations() const;

        /**
         * @brief Get XenStore data
         * @return Map of key-value pairs for /local/domain/<domid>/vm-data
         */
        QVariantMap XenstoreData() const;

        /**
         * @brief Check if HA always-run is enabled
         * @return true if system will attempt to keep VM running
         */
        bool HAAlwaysRun() const;

        /**
         * @brief Get HA restart priority
         * @return Priority string ("restart", "best-effort", "")
         */
        QString HARestartPriority() const;

        /**
         * @brief Get snapshot creation timestamp
         * @return Date/time when snapshot was created
         */
        QDateTime SnapshotTime() const;

        /**
         * @brief Get transportable snapshot ID
         * @return Snapshot ID for XVA export
         */
        QString TransportableSnapshotId() const;

        /**
         * @brief Get binary large objects
         * @return Map of blob names to blob references
         */
        QVariantMap Blobs() const;

        /**
         * @brief Get blocked operations
         * @return Map of blocked operations to error codes
         */
        QVariantMap BlockedOperations() const;

        /**
         * @brief Get snapshot information
         * @return Map of human-readable snapshot metadata
         */
        QVariantMap SnapshotInfo() const;

        /**
         * @brief Get encoded snapshot metadata
         * @return Encoded information about VM's metadata
         */
        QString SnapshotMetadata() const;

        /**
         * @brief Get parent VM reference
         * @return Opaque reference to parent VM
         */
        QString ParentRef() const;

        /**
         * @brief Get child VM references
         * @return List of child VM opaque references
         */
        QStringList ChildrenRefs() const;

        /**
         * @brief Get BIOS strings
         * @return Map of BIOS string identifiers to values
         */
        QVariantMap BIOSStrings() const;

        /**
         * @brief Get VM protection policy reference
         * @return Opaque reference to VMPP (VM Protection Policy)
         */
        QString ProtectionPolicyRef() const;

        /**
         * @brief Check if snapshot was created by VMPP
         * @return true if this snapshot was created by protection policy
         */
        bool IsSnapshotFromVMPP() const;

        /**
         * @brief Get VM snapshot schedule reference
         * @return Opaque reference to VMSS (VM Snapshot Schedule)
         */
        QString SnapshotScheduleRef() const;

        /**
         * @brief Check if snapshot was created by VMSS
         * @return true if this snapshot was created by snapshot schedule
         */
        bool IsVMSSSnapshot() const;

        /**
         * @brief Get VM appliance reference
         * @return Opaque reference to VM_appliance
         */
        QString ApplianceRef() const;

        /**
         * @brief Get appliance start delay
         * @return Delay in seconds before proceeding to next order in startup
         */
        qint64 StartDelay() const;

        /**
         * @brief Get appliance shutdown delay
         * @return Delay in seconds before proceeding to next order in shutdown
         */
        qint64 ShutdownDelay() const;

        /**
         * @brief Get appliance boot order
         * @return Point in startup/shutdown sequence for this VM
         */
        qint64 Order() const;

        /**
         * @brief Get virtual GPU references
         * @return List of VGPU opaque references
         */
        QStringList VGPURefs() const;

        /**
         * @brief Get attached PCI device references
         * @return List of currently passed-through PCI device references
         */
        QStringList AttachedPCIRefs() const;

        /**
         * @brief Get suspend SR reference
         * @return Opaque reference to SR where suspend image is stored
         */
        QString SuspendSRRef() const;

        /**
         * @brief Get VM version (recovery count)
         * @return Number of times this VM has been recovered
         */
        qint64 Version() const;

        /**
         * @brief Get VM generation ID
         * @return Generation ID string (for AD domain controllers)
         */
        QString GenerationId() const;

        /**
         * @brief Get hardware platform version
         * @return Host virtual hardware platform version VM can run on
         */
        qint64 HardwarePlatformVersion() const;

        /**
         * @brief Check if vendor device is present
         * @return true if emulated C000 PCI device is enabled (for Windows Update)
         */
        bool HasVendorDevice() const;

        /**
         * @brief Check if vendor device state is present (Windows Update capable)
         * @return true if VM has vendor device and is Windows
         * 
         * C# reference: XenModel/XenAPI-Extensions/VM.cs WindowsUpdateCapable()
         */
        bool HasVendorDeviceState() const;

        /**
         * @brief Check if read caching is enabled on any VDI
         * @return true if any attached VDI has read caching enabled
         * 
         * C# reference: XenModel/XenAPI-Extensions/VM.cs ReadCachingEnabled()
         */
        bool ReadCachingEnabled() const;

        /**
         * @brief Check if VM requires reboot
         * @return true if VM needs reboot to apply configuration changes
         */
        bool RequiresReboot() const;

        /**
         * @brief Get immutable template reference label
         * @return Textual reference to template used to create this VM
         */
        QString ReferenceLabel() const;

        /**
         * @brief Get domain type
         * @return Domain type string ("hvm", "pv", "pvh", "pv_in_pvh", "unspecified")
         */
        QString DomainType() const;

        /**
         * @brief Get NVRAM data
         * @return Map of NVRAM key-value pairs (UEFI variables, etc.)
         */
        QVariantMap NVRAM() const;

        /**
         * @brief Get pending update guidances
         * @return List of guidance strings for pending updates
         */
        QStringList PendingGuidances() const;

        // Property getters for search/query functionality
        // C# equivalent: PropertyAccessors dictionary in Common.cs

        /**
         * @brief Check if this is a real VM (not template, not snapshot, not control domain)
         * @return true if real VM
         *
         * C# equivalent: VM.IsRealVM() extension method
         */
        bool IsRealVM() const;

        /**
         * @brief Get operating system name from guest_metrics
         * @return OS name string (e.g., "Ubuntu 20.04", "Windows Server 2019")
         * 
         * C# equivalent: VM.GetOSName() extension method
         * Used by PropertyAccessors.Get(PropertyNames.os_name)
         */
        QString GetOSName() const;

        /**
         * @brief Get virtualization status (PV drivers state)
         * @return Virtualization status flags
         * 
         * Flags:
         * - 0 = NotInstalled
         * - 1 = Unknown
         * - 2 = PvDriversOutOfDate
         * - 4 = IoDriversInstalled
         * - 8 = ManagementInstalled
         * 
         * C# equivalent: VM.GetVirtualizationStatus() extension method
         * Returns VM.VirtualizationStatus enum
         */
        int GetVirtualizationStatus() const;

        /**
         * @brief Get IP addresses from guest_metrics
         * @return List of IP addresses (IPv4/IPv6)
         * 
         * C# equivalent: PropertyAccessors IP address property
         * Returns ComparableList<ComparableAddress>
         */
        QList<ComparableAddress> GetIPAddresses() const;

        /**
         * @brief Get start time from guest_metrics
         * @return Start time (epoch seconds), or 0 if not available
         * 
         * C# equivalent: VM.GetStartTime() extension method
         */
        qint64 GetStartTime() const;

    protected:
        QString GetObjectType() const override;
};

#endif // VM_H
