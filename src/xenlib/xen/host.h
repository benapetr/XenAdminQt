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
class SR;
class VM;

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
    Q_PROPERTY(QString GetHostname READ GetHostname NOTIFY dataChanged)
    Q_PROPERTY(QString GetAddress READ GetAddress NOTIFY dataChanged)
    Q_PROPERTY(bool IsEnabled READ IsEnabled NOTIFY dataChanged)

    public:
        explicit Host(XenConnection* connection,
                      const QString& opaqueRef,
                      QObject* parent = nullptr);
        ~Host() override = default;

        /**
         * @brief Get GetHostname
         * @return GetHostname string
         */
        QString GetHostname() const;

        /**
         * @brief Get IP GetAddress
         * @return IP GetAddress string
         */
        QString GetAddress() const;

        /**
         * @brief Check if host is IsEnabled (not in maintenance mode)
         * @return true if IsEnabled
         */
        bool IsEnabled() const;

        /**
         * @brief Get list of VMs resident on this host
         * @return List of VM opaque references
         */
        QStringList ResidentVMRefs() const;

        /**
         * @brief Get software version info
         * @return Map of version keys/values
         */
        QVariantMap softwareVersion() const;

        /**
         * @brief Get host Capabilities
         * @return List of capability strings
         */
        QStringList Capabilities() const;

        /**
         * @brief Get CPU info
         * @return Map of CPU information
         */
        QVariantMap CPUInfo() const;

        /**
         * @brief Get number of CPU sockets
         * @return Socket count or 0 if unknown
         */
        int cpuSockets() const;

        /**
         * @brief Get total CPU count
         * @return CPU count or 0 if unknown
         */
        int cpuCount() const;

        /**
         * @brief Get cores per socket
         * @return Cores per socket or 0 if unknown
         */
        int coresPerSocket() const;

        /**
         * @brief Get physical CPU count from host_CPUs list
         * @return Count of host CPUs
         */
        int hostCpuCount() const;

        /**
         * @brief Get other_config dictionary
         * @return Map of additional configuration
         */
        QVariantMap otherConfig() const;

        /**
         * @brief Get tags
         * @return List of tag strings
         */
        QStringList tags() const;

        /**
         * @brief Get suspend image SR reference
         * @return SR opaque reference
         */
        QString suspendImageSRRef() const;

        /**
         * @brief Get crash dump SR reference
         * @return SR opaque reference
         */
        QString crashDumpSRRef() const;

        /**
         * @brief Get list of PBD (storage connection) references
         * @return List of PBD opaque references
         */
        QStringList PBDRefs() const;

        /**
         * @brief Get list of PIF (network interface) references
         * @return List of PIF opaque references
         */
        QStringList PIFRefs() const;

        /**
         * @brief Check if host is pool master
         * @return true if this host is the pool master
         */
        bool IsMaster() const;

        /**
         * @brief Get pool reference this host belongs to
         *
         * Note: In XenAPI, hosts don't directly store pool ref,
         * we need to query the pool that references this host as master/member
         *
         * @return Pool opaque reference
         */
        QString PoolRef() const;

        /**
         * @brief Get memory overhead required by host
         * @return Memory overhead in bytes
         */
        qint64 MemoryOverhead() const;

        /**
         * @brief Get API version major number
         * @return Major version number
         */
        qint64 APIVersionMajor() const;

        /**
         * @brief Get API version minor number
         * @return Minor version number
         */
        qint64 APIVersionMinor() const;

        /**
         * @brief Get API version vendor string
         * @return Vendor name (e.g., "XenSource", "Citrix")
         */
        QString APIVersionVendor() const;

        /**
         * @brief Get vendor-specific API implementation details
         * @return Map of vendor implementation parameters
         */
        QVariantMap APIVersionVendorImplementation() const;

        /**
         * @brief Get CPU configuration parameters
         * @return Map of CPU configuration settings
         */
        QVariantMap CPUConfiguration() const;

        /**
         * @brief Get scheduling policy
         * @return Scheduling policy string (e.g., "credit", "credit2")
         */
        QString SchedPolicy() const;

        /**
         * @brief Get list of host CPU references
         * @return List of host_cpu opaque references
         */
        QStringList HostCPURefs() const;

        /**
         * @brief Get allowed operations for this host
         * @return List of allowed operation strings
         */
        QStringList AllowedOperations() const;

        /**
         * @brief Get current operations being performed on this host
         * @return Map of task ID to operation type
         */
        QVariantMap CurrentOperations() const;

        /**
         * @brief Get list of supported bootloaders
         * @return List of bootloader names (e.g., "pygrub", "eliloader")
         */
        QStringList SupportedBootloaders() const;

        /**
         * @brief Get logging configuration
         * @return Map of logging parameters
         */
        QVariantMap Logging() const;

        /**
         * @brief Get host metrics reference
         * @return Host_metrics opaque reference
         */
        QString MetricsRef() const;

        /**
         * @brief Get HA state file locations
         * @return List of state file paths for HA
         */
        QStringList HAStatefiles() const;

        /**
         * @brief Get HA network peer addresses
         * @return List of network addresses of HA peers
         */
        QStringList HANetworkPeers() const;

        /**
         * @brief Get BIOS strings from host
         * @return Map of BIOS version and other BIOS information
         */
        QVariantMap BIOSStrings() const;

        /**
         * @brief Get chipset information
         * @return Map of chipset details
         */
        QVariantMap ChipsetInfo() const;

        /**
         * @brief Get external authentication type
         * @return Authentication type string (e.g., "AD" for Active Directory)
         */
        QString ExternalAuthType() const;

        /**
         * @brief Get external authentication service name
         * @return Service name for external authentication
         */
        QString ExternalAuthServiceName() const;

        /**
         * @brief Get external authentication configuration
         * @return Map of authentication configuration parameters
         */
        QVariantMap ExternalAuthConfiguration() const;

        /**
         * @brief Get power-on mode
         * @return Power-on mode string (e.g., "wake-on-lan", "iLO", "DRAC")
         */
        QString PowerOnMode() const;

        /**
         * @brief Get power-on configuration
         * @return Map of power-on configuration parameters
         */
        QVariantMap PowerOnConfig() const;

        /**
         * @brief Get local cache SR reference
         * @return SR opaque reference used for local caching
         */
        QString LocalCacheSRRef() const;

        /**
         * @brief Get list of PCI device references
         * @return List of PCI opaque references
         */
        QStringList PCIRefs() const;

        /**
         * @brief Get list of physical GPU references
         * @return List of PGPU opaque references
         */
        QStringList PGPURefs() const;

        /**
         * @brief Get list of physical USB device references
         * @return List of PUSB opaque references
         */
        QStringList PUSBRefs() const;

        /**
         * @brief Get list of patch references (legacy)
         * @return List of pool_patch opaque references
         */
        QStringList PatchRefs() const;

        /**
         * @brief Get list of update references
         * @return List of pool_update opaque references
         */
        QStringList UpdateRefs() const;

        /**
         * @brief Get list of updates requiring host reboot
         * @return List of pool_update opaque references that require reboot
         */
        QStringList UpdatesRequiringRebootRefs() const;

        /**
         * @brief Get list of feature references
         * @return List of Feature opaque references
         */
        QStringList FeatureRefs() const;

        /**
         * @brief Get pending update guidances
         * @return List of guidance strings for pending updates
         */
        QStringList PendingGuidances() const;

        /**
         * @brief Check if SSL legacy support is enabled
         * @return true if legacy SSL/TLS versions are allowed
         */
        bool SSLLegacy() const;

        /**
         * @brief Check if TLS certificate verification is enabled
         * @return true if TLS verification is enabled
         */
        bool TLSVerificationEnabled() const;

        /**
         * @brief Check if HTTPS-only mode is enabled
         * @return true if only HTTPS connections are allowed
         */
        bool HTTPSOnly() const;

        /**
         * @brief Get guest VCPU parameters
         * @return Map of VCPU configuration parameters for guests
         */
        QVariantMap GuestVCPUsParams() const;

        /**
         * @brief Get display mode setting
         * @return Display configuration string
         */
        QString Display() const;

        /**
         * @brief Get supported virtual hardware platform versions
         * @return List of platform version numbers
         */
        QList<qint64> VirtualHardwarePlatformVersions() const;

        /**
         * @brief Get control domain (dom0) VM reference
         * @return VM opaque reference for domain 0
         */
        QString ControlDomainRef() const;

        /**
         * @brief Get iSCSI IQN (iSCSI Qualified Name)
         * @return iSCSI IQN string for this host
         */
        QString IscsiIQN() const;

        /**
         * @brief Check if multipathing is enabled
         * @return true if storage multipathing is enabled
         */
        bool Multipathing() const;

        /**
         * @brief Get UEFI certificates
         * @return UEFI certificate data
         */
        QString UEFICertificates() const;

        /**
         * @brief Get list of certificate references
         * @return List of Certificate opaque references
         */
        QStringList CertificateRefs() const;

        /**
         * @brief Get available product editions
         * @return List of edition strings available for this host
         */
        QStringList Editions() const;

        /**
         * @brief Get list of crash dump references
         * @return List of host_crashdump opaque references
         */
        QStringList CrashdumpRefs() const;

        /**
         * @brief Get timestamp of last software update
         * @return DateTime of last update installation
         */
        QDateTime LastSoftwareUpdate() const;

        /**
         * @brief Get latest synced updates applied state
         * @return String indicating latest synced update status
         */
        QString LatestSyncedUpdatesApplied() const;

        /**
         * @brief Get license parameters
         * @return Map of license configuration parameters
         */
        QVariantMap LicenseParams() const;

        /**
         * @brief Get current product edition
         * @return Edition string (e.g., "free", "per-socket", "xendesktop")
         */
        QString Edition() const;

        /**
         * @brief Get license server configuration
         * @return Map of license server address and port
         */
        QVariantMap LicenseServer() const;

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
         * @brief Get pool this host belongs to
         * @return Pool opaque reference (cached lookup)
         * 
         * C# equivalent: Helpers.GetPoolOfOne(connection)
         */
        QString GetPoolRef() const;

    protected:
        QString GetObjectType() const override;
};

#endif // HOST_H
