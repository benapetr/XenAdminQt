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

#ifndef HOST_H
#define HOST_H

#include "xenobject.h"
#include "network/comparableaddress.h"

class Pool;
class PoolUpdate;
class SR;
class VM;
class PBD;
class PIF;
class HostMetrics;
class PGPU;

/**
 * @brief Host - A physical host
 *
 * Qt equivalent of C# XenAPI.Host class. Represents a physical XenServer host.
 *
 * Key properties (from C# Host class):
 * - name_label, name_description, hostname, address
 * - enabled (maintenance mode status)
 * - resident_VMs (VMs running on this host)
 * - PIFs (physical network interfaces)
 * - PBDs (physical block devices / storage connections)
 * - software_version, capabilities
 * - memory_overhead, cpu_info
 */
class XENLIB_EXPORT Host : public XenObject
{
    Q_OBJECT

    public:
        explicit Host(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        ~Host() override = default;

        //! Get hostname string
        QString GetHostname() const;

        //! Get IP address string
        QString GetAddress() const;

        //! Check if host is enabled (not in maintenance mode)
        bool IsEnabled() const;

        /**
         * @brief Check if host is live (pool master sees it as live)
         * @return true if host_metrics.live is true
         *
         * C# equivalent: Host.IsLive()
         */
        bool IsLive() const;

        /**
         * @brief Check if vTPM is restricted by licensing
         *
         * C# equivalent: Host.RestrictVtpm (BoolKeyPreferTrue on license_params)
         */
        bool RestrictVtpm() const;

        /**
         * @brief Check if intra-pool migration is restricted by licensing
         *
         * C# equivalent: Host.RestrictIntraPoolMigrate (BoolKey on license_params)
         */
        bool RestrictIntraPoolMigrate() const;
        bool RestrictGpu() const;
        bool RestrictVgpu() const;
        bool RestrictIntegratedGpuPassthrough() const;
        bool RestrictVSwitchController() const;
        bool RestrictSriovNetwork() const;
        bool RestrictManagementOnVLAN() const;
        bool RestrictPooling() const;
        bool RestrictPoolSize() const;
        bool RestrictPoolSecretRotation() const;
        bool SriovNetworkDisabled() const;
        bool vSwitchNetworkBackend() const;

        QSharedPointer<PGPU> SystemDisplayDevice() const;
        bool CanEnableDisableIntegratedGpu() const;

        //! Get list of VMs resident on this host (list of VM opaque references)
        QStringList GetResidentVMRefs() const;

        //! Get software version info (map of version keys/values)
        QVariantMap SoftwareVersion() const;

        //! Get capabilities (list of capability strings)
        QStringList GetCapabilities() const;

        //! Get CPU info (map of CPU information)
        QVariantMap GetCPUInfo() const;

        //! Get number of CPU sockets (or 0 if unknown)
        int GetCPUSockets() const;

        //! Get total CPU count (or 0 if unknown)
        int GetCPUCount() const;

        //! Get cores per socket (or 0 if unknown)
        int GetCoresPerSocket() const;

        //! Get physical CPU count from host_CPUs list
        int GetHostCpuCount() const;
        
        /**
         * @brief Get boot time from other_config
         * @return Unix timestamp of host boot time, or 0.0 if not available
         * 
         * C# equivalent: Host.BootTime() extension method
         */
        double BootTime() const;

        /**
         * @brief Get host uptime in seconds
         * @return Uptime in seconds, or -1 if not available/invalid
         *
         * C# equivalent: Host.Uptime() extension method
         */
        qint64 GetUptime() const;

        //! Get suspend image SR reference (SR opaque reference)
        QString GetSuspendImageSRRef() const;

        //! Get crash dump SR reference (SR opaque reference)
        QString GetCrashDumpSRRef() const;

        //! Get list of PBD (storage connection) references
        QStringList GetPBDRefs() const;

        //! Get list of PIF (network interface) references
        QStringList GetPIFRefs() const;

        //! Get resident VMs (VMs running on this host)
        QList<QSharedPointer<VM>> GetResidentVMs() const;

        /**
         * @brief Check if host has any running VMs.
         *
         * C# equivalent: Host.HasRunningVMs()
         */
        bool HasRunningVMs() const;

        //! Get PBDs (physical block devices/storage connections)
        QList<QSharedPointer<PBD>> GetPBDs() const;

        //! Get PIFs (physical network interfaces)
        QList<QSharedPointer<PIF>> GetPIFs() const;

        //! Check if host is pool master
        bool IsMaster() const;

        /**
         * @brief Get pool reference this host belongs to
         *
         * Note: In XenAPI, hosts don't directly store pool ref,
         * we need to query the pool that references this host as master/member
         *
         * @return Pool opaque reference
         */
        QString GetPoolRef() const;

        QSharedPointer<Pool> GetPool();
        QSharedPointer<Pool> GetPoolOfOne();

        //! Get memory overhead required by host in bytes
        qint64 MemoryOverhead() const;

        //! Get API version major number
        qint64 APIVersionMajor() const;

        //! Get API version minor number
        qint64 APIVersionMinor() const;

        //! Get API version vendor string (e.g., "XenSource", "Citrix")
        QString APIVersionVendor() const;

        //! Get vendor-specific API implementation details (map of vendor implementation parameters)
        QVariantMap APIVersionVendorImplementation() const;

        //! Get CPU configuration parameters (map of CPU configuration settings)
        QVariantMap CPUConfiguration() const;

        //! Get scheduling policy (e.g., "credit", "credit2")
        QString SchedPolicy() const;

        //! Get list of host CPU references (list of host_cpu opaque references)
        QStringList GetHostCPURefs() const;

        //! Get allowed operations for this host (list of allowed operation strings)
        QStringList AllowedOperations() const;

        //! Get current operations being performed on this host (map of task ID to operation type)
        QVariantMap CurrentOperations() const;

        //! Get list of supported bootloaders (e.g., "pygrub", "eliloader")
        QStringList SupportedBootloaders() const;

        //! Get logging configuration (map of logging parameters)
        QVariantMap Logging() const;

        //! Get host metrics reference (Host_metrics opaque reference)
        QString GetMetricsRef() const;

        //! Get host metrics object (null if not available)
        QSharedPointer<HostMetrics> GetMetrics() const;

        //! Get HA state file locations (list of state file paths for HA)
        QStringList HAStatefiles() const;

        //! Get HA network peer addresses (list of network addresses of HA peers)
        QStringList HANetworkPeers() const;

        //! Get BIOS strings from host (map of BIOS version and other BIOS information)
        QVariantMap BIOSStrings() const;

        //! Get chipset information (map of chipset details)
        QVariantMap ChipsetInfo() const;

        //! Get external authentication type (e.g., "AD" for Active Directory)
        QString ExternalAuthType() const;

        //! Get external authentication service name for external authentication
        QString ExternalAuthServiceName() const;

        //! Get external authentication configuration (map of authentication configuration parameters)
        QVariantMap ExternalAuthConfiguration() const;

        //! Check if host license is a free/express/trial edition
        bool IsFreeLicense() const;

        //! Check if Linux pack is present (software_version contains xs:linux)
        bool LinuxPackPresent() const;

        //! Get power-on mode (e.g., "wake-on-lan", "iLO", "DRAC")
        QString PowerOnMode() const;

        //! Get power-on configuration (map of power-on configuration parameters)
        QVariantMap PowerOnConfig() const;

        //! Get local cache SR reference used for local caching
        QString LocalCacheSRRef() const;

        //! Get list of PCI device references (list of PCI opaque references)
        QStringList PCIRefs() const;

        //! Get list of physical GPU references (list of PGPU opaque references)
        QStringList PGPURefs() const;

        //! Get list of physical USB device references (list of PUSB opaque references)
        QStringList PUSBRefs() const;

        //! Get list of patch references (legacy, list of pool_patch opaque references)
        QStringList PatchRefs() const;

        //! Get list of update references (list of pool_update opaque references)
        QStringList UpdateRefs() const;

        //! Get list of updates requiring host reboot (list of pool_update opaque references that require reboot)
        QStringList UpdatesRequiringRebootRefs() const;

        //! Get list of feature references (list of Feature opaque references)
        QStringList FeatureRefs() const;

        //! Get pending update guidances (list of guidance strings for pending updates)
        QStringList PendingGuidances() const;

        //! Check if SSL legacy support is enabled (legacy SSL/TLS versions are allowed)
        bool SSLLegacy() const;

        //! Check if TLS certificate verification is enabled
        bool TLSVerificationEnabled() const;

        //! Check if HTTPS-only mode is enabled (only HTTPS connections are allowed)
        bool HTTPSOnly() const;

        //! Get guest VCPU parameters (map of VCPU configuration parameters for guests)
        QVariantMap GuestVCPUsParams() const;

        //! Get display mode setting
        QString Display() const;

        //! Get supported virtual hardware platform versions (list of platform version numbers)
        QList<qint64> VirtualHardwarePlatformVersions() const;

        //! Get control domain (dom0) VM reference
        QString ControlDomainRef() const;

        //! Get iSCSI IQN (iSCSI Qualified Name) for this host
        QString IscsiIQN() const;

        //! Check if multipathing is enabled (storage multipathing is enabled)
        bool Multipathing() const;

        //! Get UEFI certificates data
        QString UEFICertificates() const;

        //! Get list of certificate references (list of Certificate opaque references)
        QStringList CertificateRefs() const;

        //! Get available product editions (list of edition strings available for this host)
        QStringList Editions() const;

        //! Get list of crash dump references (list of host_crashdump opaque references)
        QStringList CrashdumpRefs() const;

        //! Get timestamp of last software update
        QDateTime LastSoftwareUpdate() const;

        //! Get latest synced updates applied state
        QString LatestSyncedUpdatesApplied() const;

        //! Get license parameters (map of license configuration parameters)
        QVariantMap LicenseParams() const;

        //! Get current product edition (e.g., "free", "per-socket", "xendesktop")
        QString Edition() const;

        //! Get license server configuration (map of license server address and port)
        QVariantMap LicenseServer() const;

        //! Get raw build number from software_version (may be empty)
        QString BuildNumberRaw() const;

        //! Get platform version string from software_version
        QString PlatformVersion() const;

        //! Get product brand string from software_version
        QString ProductBrand() const;

        //! Get database schema string from software_version
        QString GetDatabaseSchema() const;

        //! Get list of applied pool updates (Pool_update objects)
        QList<QSharedPointer<PoolUpdate>> AppliedUpdates() const;

        struct SuppPack
        {
            QString Originator;
            QString Name;
            QString Description;
            QString Version;
            QString Build;
            bool Homogeneous = false;
            bool IsValid = false;

            QString OriginatorAndName() const { return Originator + ":" + Name; }
        };

        //! Get supplemental packs parsed from software_version
        QList<SuppPack> SuppPacks() const;

        // Property getters for search/query functionality
        // C# equivalent: PropertyAccessors dictionary in Common.cs

        /**
         * @brief Get IP addresses from PIFs (physical interfaces)
         * @return List of IP addresses from all PIFs
         * 
         * C# equivalent: PropertyAccessors IP address property for Host
         * Iterates through host.PIFs and collects IP addresses
         */
        QList<ComparableAddress> GetIPAddresses() const;

        /**
         * @brief Calculate free memory on host
         * 
         * C# equivalent: Host.memory_free_calc()
         * Uses conservative calculation from xapi: memory_total - (host_overhead + sum of VM overheads + VM memory_actual)
         */
        qint64 MemoryFreeCalc() const;

        /**
         * @brief Calculate total dynamic minimum of all resident VMs
         * 
         * C# equivalent: Host.tot_dyn_min()
         * Sum of memory_dynamic_min for ballooning VMs, memory_static_max for non-ballooning VMs (excluding dom0)
         */
        qint64 TotDynMin() const;

        /**
         * @brief Calculate total dynamic maximum of all resident VMs
         * 
         * C# equivalent: Host.tot_dyn_max()
         * Sum of memory_dynamic_max for ballooning VMs, memory_static_max for non-ballooning VMs (excluding dom0)
         */
        qint64 TotDynMax() const;

        /**
         * @brief Calculate available memory on host
         * 
         * C# equivalent: Host.memory_available_calc()
         * Memory that could be available if all VMs were reduced to their dynamic_minimum
         */
        qint64 MemoryAvailableCalc() const;

        /**
         * @brief Calculate memory used by Xen (hypervisor + control domain)
         * 
         * C# equivalent: Host.xen_memory_calc()
         * Includes host overhead + VM overheads + control domain memory_actual
         */
        qint64 XenMemoryCalc() const;

        /**
         * @brief Get control domain (dom0) memory
         * 
         * C# equivalent: Host.dom0_memory()
         * Returns memory_actual from dom0 VM_metrics, or memory_dynamic_min if metrics unavailable
         */
        qint64 Dom0Memory() const;

        /**
         * @brief Get control domain zero VM
         * 
         * C# equivalent: Host.ControlDomainZero()
         * Finds the first resident VM where is_control_domain is true
         */
        QSharedPointer<VM> ControlDomainZero() const;

    protected:
        XenObjectType GetObjectType() const override { return XenObjectType::Host; }
};

#endif // HOST_H
