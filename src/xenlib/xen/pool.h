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

#ifndef POOL_H
#define POOL_H

#include "xenobject.h"

class Host;
class SR;

/**
 * @brief Pool - Pool-wide information
 *
 * Qt equivalent of C# XenAPI.Pool class. Represents a XenServer resource pool.
 *
 * Key properties (from C# Pool class):
 * - name_label, name_description
 * - master (reference to master host)
 * - default_SR (reference to default storage repository)
 * - ha_enabled, ha_configuration
 * - other_config
 */
class XENLIB_EXPORT Pool : public XenObject
{
    Q_OBJECT

    public:
        explicit Pool(XenConnection* connection,
                      const QString& opaqueRef,
                      QObject* parent = nullptr);
        ~Pool() override = default;

        /**
         * @brief Get reference to pool master host
         * @return Host opaque reference
         */
        QString GetMasterHostRef() const;

        /**
         * @brief Get reference to default SR
         * @return SR opaque reference
         */
        QString DefaultSRRef() const;

        /**
         * @brief Check if HA is enabled
         * @return true if HA is enabled
         */
        bool HAEnabled() const;

        /**
         * @brief Get HA configuration
         * @return Map of HA configuration keys/values
         */
        QVariantMap HAConfiguration() const;

        /**
         * @brief Get other_config dictionary
         * @return Map of additional configuration
         */
        QVariantMap OtherConfig() const;

        /**
         * @brief Get list of all host refs in this pool
         * @return List of host opaque references
         */
        QStringList HostRefs() const;

        /**
         * @brief Check if this is a pool-of-one (single host pool)
         * @return true if pool has only one host
         */
        bool IsPoolOfOne() const;

        /**
         * @brief Get tags
         * @return List of tag strings
         */
        QStringList Tags() const;

        /**
         * @brief Get WLB (Workload Balancing) enabled status
         * @return true if WLB is enabled
         */
        bool WLBEnabled() const;

        /**
         * @brief Get live patching disabled status
         * @return true if live patching is disabled
         */
        bool LivePatchingDisabled() const;

        /**
         * @brief Get the SR reference for suspend images
         * @return Opaque reference to SR where suspend images are stored
         */
        QString SuspendImageSRRef() const;

        /**
         * @brief Get the SR reference for crash dumps
         * @return Opaque reference to SR where crash dumps are stored
         */
        QString CrashDumpSRRef() const;

        /**
         * @brief Get HA statefile VDI paths
         * @return List of VDI paths used for HA statefiles
         */
        QStringList HAStatefiles() const;

        /**
         * @brief Get number of host failures to tolerate
         * @return Number of host failures pool can tolerate before being overcommitted
         */
        qint64 HAHostFailuresToTolerate() const;

        /**
         * @brief Get number of host failures plan exists for
         * @return Number of future host failures we have managed to find a plan for
         */
        qint64 HAPlanExistsFor() const;

        /**
         * @brief Check if HA overcommit is allowed
         * @return true if operations causing pool overcommit are allowed
         */
        bool HAAllowOvercommit() const;

        /**
         * @brief Check if pool is HA overcommitted
         * @return true if pool lacks resources to tolerate configured host failures
         */
        bool HAOvercommitted() const;

        /**
         * @brief Get HA cluster stack name
         * @return Name of the HA cluster stack (e.g., "xhad")
         */
        QString HAClusterStack() const;

        /**
         * @brief Check if redo log is enabled
         * @return true if redo log is enabled for this pool
         */
        bool RedoLogEnabled() const;

        /**
         * @brief Get redo log VDI reference
         * @return Opaque reference to VDI used for redo log
         */
        QString RedoLogVDIRef() const;

        /**
         * @brief Get GUI configuration
         * @return Map of GUI-specific configuration settings
         */
        QVariantMap GUIConfig() const;

        /**
         * @brief Get health check configuration
         * @return Map of health check feature settings
         */
        QVariantMap HealthCheckConfig() const;

        /**
         * @brief Get guest agent configuration
         * @return Map of guest agent configuration settings
         */
        QVariantMap GuestAgentConfig() const;

        /**
         * @brief Get pool-wide CPU information
         * @return Map containing CPU vendor, features, and capabilities
         */
        QVariantMap CPUInfo() const;
        
        /**
         * @brief Get binary large objects
         * @return Map of blob names to blob references
         */
        QVariantMap Blobs() const;

        /**
         * @brief Get metadata VDI references
         * @return List of VDI references containing pool metadata
         */
        QStringList MetadataVDIRefs() const;

        /**
         * @brief Get Workload Balancing server URL
         * @return WLB server URL or empty string if not configured
         */
        QString WLBUrl() const;

        /**
         * @brief Get Workload Balancing username
         * @return WLB username or empty string
         */
        QString WLBUsername() const;

        /**
         * @brief Check if WLB certificate verification is enabled
         * @return true if WLB certificate should be verified
         */
        bool WLBVerifyCert() const;

        /**
         * @brief Get vSwitch controller address
         * @return vSwitch controller address or empty string (deprecated)
         */
        QString VswitchController() const;

        /**
         * @brief Get license restrictions
         * @return Map of license restriction keys and values
         */
        QVariantMap Restrictions() const;

        /**
         * @brief Check if vendor device policy is disabled
         * @return true if vendor device policy is set to deny
         */
        bool PolicyNoVendorDevice() const;

        /**
         * @brief Get list of allowed operations on this pool
         * @return List of operation type strings
         */
        QStringList AllowedOperations() const;

        /**
         * @brief Get currently running operations
         * @return Map of task reference to operation type
         */
        QVariantMap CurrentOperations() const;

        /**
         * @brief Check if IGMP snooping is enabled
         * @return true if IGMP snooping is enabled for networks
         */
        bool IGMPSnoopingEnabled() const;

        /**
         * @brief Get UEFI certificates
         * @return UEFI certificate data or empty string
         */
        QString UEFICertificates() const;

        /**
         * @brief Check if TLS verification is enabled
         * @return true if TLS certificate verification is enabled
         */
        bool TLSVerificationEnabled() const;

        /**
         * @brief Check if client certificate authentication is enabled
         * @return true if TLS client cert auth is enabled
         */
        bool ClientCertificateAuthEnabled() const;

        /**
         * @brief Get client certificate auth name requirement
         * @return CN/SAN that client certificates must have
         */
        QString ClientCertificateAuthName() const;

        /**
         * @brief Get enabled update repository references
         * @return List of repository opaque references
         */
        QStringList RepositoryRefs() const;

        /**
         * @brief Get repository proxy URL
         * @return Proxy URL for update repository access
         */
        QString RepositoryProxyUrl() const;

        /**
         * @brief Get repository proxy username
         * @return Username for proxy authentication
         */
        QString RepositoryProxyUsername() const;

        /**
         * @brief Get repository proxy password secret reference
         * @return Opaque reference to Secret containing proxy password
         */
        QString RepositoryProxyPasswordRef() const;

        /**
         * @brief Check if migration compression is enabled
         * @return true if VM migration should use stream compression
         */
        bool MigrationCompression() const;

        /**
         * @brief Check if coordinator bias is enabled
         * @return true if VM scheduling should avoid pool coordinator/master
         */
        bool CoordinatorBias() const;

        /**
         * @brief Get telemetry UUID secret reference
         * @return Opaque reference to Secret containing telemetry UUID
         */
        QString TelemetryUuidRef() const;

        /**
         * @brief Get telemetry collection frequency
         * @return Frequency string ("daily", "weekly", etc.)
         */
        QString TelemetryFrequency() const;

        /**
         * @brief Get next telemetry collection time
         * @return Timestamp when next telemetry collection may occur
         */
        QDateTime TelemetryNextCollection() const;

        /**
         * @brief Get last update synchronization time
         * @return Timestamp of last update sync from CDN
         */
        QDateTime LastUpdateSync() const;

        /**
         * @brief Get update synchronization frequency
         * @return Frequency string ("daily", "weekly")
         */
        QString UpdateSyncFrequency() const;

        /**
         * @brief Get update synchronization day of week
         * @return Day number (0-6, 0=Sunday) for weekly sync
         */
        qint64 UpdateSyncDay() const;

        /**
         * @brief Check if periodic update sync is enabled
         * @return true if automatic update synchronization is enabled
         */
        bool UpdateSyncEnabled() const;

        /**
         * @brief Check if PSR (Pooled Storage Repository) is pending
         * @return true if PSR operation is pending
         */
        bool IsPsrPending() const;

    protected:
        QString GetObjectType() const override;
};

#endif // POOL_H
