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
class Console;
class VUSB;
class VTPM;
class Blob;
class PCI;
class SR;
class VMMetrics;

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

    public:
        static constexpr int DEFAULT_CORES_PER_SOCKET = 1;
        static constexpr long MAX_SOCKETS = 16;
        static constexpr long MAX_VCPUS_FOR_NON_TRUSTED_VMS = 32;

        explicit VM(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        ~VM() override = default;

        //! Get VM power state ("Running", "Halted", "Suspended", "Paused")
        QString GetPowerState() const;

        QString NameWithLocation() const override;
        QString LocationString() const override;

        //! Check if this is a template
        bool IsTemplate() const;

        bool IsLocked() const override;

        //! Check if this is a default template (C# DefaultTemplate)
        bool DefaultTemplate() const;

        //! Check if this is an internal template (C# InternalTemplate)
        bool InternalTemplate() const;

        //! Check if this object should be shown in the UI
        bool Show(bool showHiddenVMs) const;

        //! Check if this is a snapshot
        bool IsSnapshot() const;

        //! Get reference to host VM is resident on (empty if not running)
        QString GetResidentOnRef() const;

        QSharedPointer<Host> GetResidentOnHost();
        QSharedPointer<Pool> GetPool();

        //! Get reference to affinity host
        QString GetAffinityRef() const;

        //! Get list of VBD (virtual block device) references
        QStringList GetVBDRefs() const;
        QSharedPointer<VBD> FindVMCDROM() const;

        //! Get list of VBD objects
        QList<QSharedPointer<VBD>> GetVBDs() const;

        //! Get list of VIF (virtual network interface) references
        QStringList GetVIFRefs() const;

        //! Get list of VIF objects
        QList<QSharedPointer<VIF>> GetVIFs() const;

        //! Get list of console references
        QStringList GetConsoleRefs() const;

        //! Get suspend VDI reference (opaque reference to VDI for suspend image)
        QString GetSuspendVDIRef() const;

        // Object resolution getters
        QSharedPointer<Host> GetAffinityHost() const;
        QList<QSharedPointer<Console>> GetConsoles() const;
        QSharedPointer<VDI> GetSuspendVDI() const;
        QSharedPointer<Host> GetHome() const;
        QList<QSharedPointer<VUSB>> GetVUSBs() const;
        QList<QSharedPointer<VTPM>> GetVTPMs() const;
        QList<QSharedPointer<Blob>> GetBlobs() const;
        QSharedPointer<VM> GetParent() const;
        QList<QSharedPointer<PCI>> GetAttachedPCIDevices() const;
        QSharedPointer<SR> GetSuspendSR();

        //! Get snapshot parent reference (if this is a snapshot)
        QString SnapshotOfRef() const;

        QSharedPointer<VM> SnapshotOf() const;

        //! Get list of snapshot children (if this VM has snapshots)
        QStringList GetSnapshotRefs() const;

        //! Get memory target in bytes
        qint64 MemoryTarget() const;

        //! Get memory static max in bytes
        qint64 GetMemoryStaticMax() const;

        //! Get memory dynamic max in bytes
        qint64 GetMemoryDynamicMax() const;

        //! Get memory dynamic min in bytes
        qint64 GetMemoryDynamicMin() const;

        //! Get memory static min in bytes
        qint64 GetMemoryStaticMin() const;

        /**
         * @brief Check if VM supports memory ballooning
         * 
         * C# equivalent: VM.SupportsBallooning()
         * For templates: ballooning supported if dynamic_min != static_max
         * For VMs: ballooning supported if guest_metrics.other["feature-balloon"] exists
         * 
         * @return true if ballooning is supported
         */
        bool SupportsBallooning() const;

        /**
         * @brief Check if VM uses memory ballooning
         * 
         * C# equivalent: VM.UsesBallooning()
         * VM uses ballooning if dynamic_max != static_max AND ballooning is supported
         * 
         * @return true if ballooning is in use
         */
        bool UsesBallooning() const;

        //! Get maximum number of VCPUs
        int VCPUsMax() const;

        //! Get number of VCPUs at startup
        int VCPUsAtStartup() const;

        //! Check if VM is HVM
        bool IsHVM() const;

        //! Check if VM is Windows
        bool IsWindows() const;

        //! Check if vCPU hotplug is supported
        bool SupportsVCPUHotplug() const;

        //! Get maximum allowed VCPUs
        int MaxVCPUsAllowed() const;

        //! Get maximum allowed VBDs (virtual block devices)
        int MaxVBDsAllowed() const;

        //! Get minimum recommended VCPUs
        int MinVCPUs() const;

        //! Get vCPU weight from VCPUs_params
        int GetVCPUWeight() const;

        //! Get cores per socket from platform
        long GetCoresPerSocket() const;

        //! Get maximum cores per socket based on host capabilities
        long MaxCoresPerSocket() const;

        //! Validate vCPU configuration
        static QString ValidVCPUConfiguration(long noOfVcpus, long coresPerSocket);

        //! Get human readable topology string
        static QString GetTopology(long sockets, long cores);

        /**
         * @brief Parse the provisioning XML from other_config["disks"]
         * @return Root element or null element if missing/invalid
         */
        QDomElement ProvisionXml() const;

        //! Get platform configuration map
        QVariantMap Platform() const;

        //! Get allowed operations list
        QStringList GetAllowedOperations() const;

        //! Get current operations map (operation ID to operation type)
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

        //! Check if VM is running (power state is "Running")
        bool IsRunning() const
        {
            return GetPowerState() == "Running";
        }

        //! Check if VM is halted (power state is "Halted")
        bool IsHalted() const
        {
            return GetPowerState() == "Halted";
        }

        //! Check if VM is suspended (power state is "Suspended")
        bool IsSuspended() const
        {
            return GetPowerState() == "Suspended";
        }

        //! Check if VM is paused (power state is "Paused")
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

        //! Get user-defined version number for this VM
        qint64 UserVersion() const;

        //! Get host where VM is scheduled to start (memory reservation indicator)
        QString ScheduledToBeResidentOnRef() const;

        //! Get virtualization memory overhead in bytes
        qint64 MemoryOverhead() const;

        //! Get vCPU parameters dictionary (map of vCPU configuration parameters)
        QVariantMap VCPUsParams() const;

        //! Get action to take after soft reboot (e.g., "soft_reboot", "destroy")
        QString ActionsAfterSoftreboot() const;

        //! Get action to take after guest shutdown ("destroy", "restart", etc.)
        QString ActionsAfterShutdown() const;

        //! Get action to take after guest reboot
        QString ActionsAfterReboot() const;

        //! Get action to take if guest crashes ("destroy", "coredump_and_destroy", etc.)
        QString ActionsAfterCrash() const;

        //! Get virtual USB device references (list of VUSB opaque references)
        QStringList VUSBRefs() const;

        //! Get crash dump references (list of Crashdump opaque references)
        QStringList CrashDumpRefs() const;

        //! Get virtual TPM references (list of VTPM opaque references)
        QStringList VTPMRefs() const;

        //! Get PV bootloader path or name for paravirtualized VMs
        QString PVBootloader() const;

        //! Get PV kernel path for paravirtualized VMs
        QString PVKernel() const;

        //! Get PV ramdisk path (initrd) for paravirtualized VMs
        QString PVRamdisk() const;

        //! Get PV kernel command-line arguments
        QString PVArgs() const;

        //! Get PV bootloader arguments (miscellaneous bootloader arguments)
        QString PVBootloaderArgs() const;

        //! Get PV legacy arguments for Zurich guests (deprecated)
        QString PVLegacyArgs() const;

        //! Get HVM boot policy ("BIOS order", etc.)
        QString HVMBootPolicy() const;

        //! Get HVM boot parameters map (boot order, etc.)
        QVariantMap HVMBootParams() const;

        //! Get HVM shadow page multiplier for shadow page table allocation
        double HVMShadowMultiplier() const;

        //! Get PCI bus path for passthrough devices
        QString PCIBus() const;

        //! Get Xen domain ID (if VM is running), or -1
        qint64 Domid() const;

        //! Get domain architecture ("x86_64", "x86_32", etc.) or empty
        QString Domarch() const;

        //! Get last boot CPU flags (map of CPU flags VM was last booted with)
        QVariantMap LastBootCPUFlags() const;

        //! Check if this is a control domain (domain 0 or driver domain)
        bool IsControlDomain() const;

        //! Get VM metrics reference (opaque reference to VM_metrics object)
        QString MetricsRef() const;

        QSharedPointer<VMMetrics> GetMetrics();

        //! Get guest metrics reference (opaque reference to VM_guest_metrics object)
        QString GetGuestMetricsRef() const;

        //! Get last booted record (marshalled VM record from last boot)
        QString LastBootedRecord() const;

        //! Get resource recommendations (XML specification of recommended resource values)
        QString Recommendations() const;

        //! Get XenStore data (map of key-value pairs for /local/domain/<domid>/vm-data)
        QVariantMap XenstoreData() const;

        //! Check if HA always-run is enabled (system will attempt to keep VM running)
        bool HAAlwaysRun() const;

        //! Get HA restart priority ("restart", "best-effort", "")
        QString HARestartPriority() const;

        //! Get snapshot creation timestamp (date/time when snapshot was created)
        QDateTime SnapshotTime() const;

        //! Get transportable snapshot ID for XVA export
        QString TransportableSnapshotId() const;

        //! Get binary large objects (map of blob names to blob references)
        QVariantMap Blobs() const;

        //! Get blocked operations (map of blocked operations to error codes)
        QVariantMap BlockedOperations() const;

        //! Get snapshot information (map of human-readable snapshot metadata)
        QVariantMap SnapshotInfo() const;

        //! Get encoded snapshot metadata (encoded information about VM's metadata)
        QString SnapshotMetadata() const;

        //! Get parent VM reference (opaque reference to parent VM)
        QString ParentRef() const;

        //! Get child VM references (list of child VM opaque references)
        QStringList ChildrenRefs() const;

        //! Get BIOS strings (map of BIOS string identifiers to values)
        QVariantMap BIOSStrings() const;

        //! Get VM protection policy reference (opaque reference to VMPP)
        QString ProtectionPolicyRef() const;

        //! Check if snapshot was created by VMPP (protection policy)
        bool IsSnapshotFromVMPP() const;

        //! Get VM snapshot schedule reference (opaque reference to VMSS)
        QString SnapshotScheduleRef() const;

        //! Check if snapshot was created by VMSS (snapshot schedule)
        bool IsVMSSSnapshot() const;

        //! Get VM appliance reference (opaque reference to VM_appliance)
        QString ApplianceRef() const;

        //! Get appliance start delay in seconds before proceeding to next order
        qint64 StartDelay() const;

        //! Get appliance shutdown delay in seconds before proceeding to next order
        qint64 ShutdownDelay() const;

        //! Get appliance boot order (point in startup/shutdown sequence for this VM)
        qint64 Order() const;

        //! Get virtual GPU references (list of VGPU opaque references)
        QStringList VGPURefs() const;

        //! Get attached PCI device references (list of currently passed-through PCI device references)
        QStringList AttachedPCIRefs() const;

        //! Get suspend SR reference (opaque reference to SR where suspend image is stored)
        QString SuspendSRRef() const;

        //! Get VM version (number of times this VM has been recovered)
        qint64 Version() const;

        //! Get VM generation ID string (for AD domain controllers)
        QString GenerationId() const;

        //! Get hardware platform version (host virtual hardware platform version VM can run on)
        qint64 HardwarePlatformVersion() const;

        //! Check if vendor device is present (emulated C000 PCI device for Windows Update)
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

        //! Check if VM requires reboot to apply configuration changes
        bool RequiresReboot() const;

        //! Get immutable template reference label (textual reference to template used to create this VM)
        QString ReferenceLabel() const;

        //! Get domain type ("hvm", "pv", "pvh", "pv_in_pvh", "unspecified")
        QString DomainType() const;

        //! Get NVRAM data (map of NVRAM key-value pairs for UEFI variables, etc.)
        QVariantMap NVRAM() const;

        //! Get pending update guidances (list of guidance strings for pending updates)
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
        XenObjectType GetObjectType() const override { return XenObjectType::VM; }
};

#endif // VM_H
