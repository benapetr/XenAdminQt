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
class VM;
class VDI;

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
        explicit Pool(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        ~Pool() override = default;

        QString GetName() const override;
        QString LocationString() const override;

        //! Get reference to pool master host (Host opaque reference)
        QString GetMasterHostRef() const;

        //! Get reference to default SR (SR opaque reference)
        QString GetDefaultSRRef() const;

        //! Check if HA is enabled (true if HA is enabled)
        bool HAEnabled() const;

        //! Get HA configuration (Map of HA configuration keys/values)
        QVariantMap GetHAConfiguration() const;

        //! Get list of all host refs in this pool (List of host opaque references)
        QStringList GetHostRefs() const;

        //! Check if this is a pool-of-one (true if pool has only one host)
        bool IsPoolOfOne() const;

        //! Check if this pool is visible (not a pool-of-one unless named)
        bool IsVisible() const;

        //! Get WLB (Workload Balancing) enabled status (true if WLB is enabled)
        bool IsWLBEnabled() const;

        //! Get live patching disabled status (true if live patching is disabled)
        bool IsLivePatchingDisabled() const;

        //! Get the SR reference for suspend images (Opaque reference to SR where suspend images are stored)
        QString GetSuspendImageSRRef() const;

        //! Get the SR reference for crash dumps (Opaque reference to SR where crash dumps are stored)
        QString GetCrashDumpSRRef() const;

        //! Get HA statefile VDI paths (List of VDI paths used for HA statefiles)
        QStringList HAStatefiles() const;

        //! Get number of host failures to tolerate (Number of host failures pool can tolerate before being overcommitted)
        qint64 HAHostFailuresToTolerate() const;

        //! Get number of host failures plan exists for (Number of future host failures we have managed to find a plan for)
        qint64 HAPlanExistsFor() const;

        //! Check if HA overcommit is allowed (true if operations causing pool overcommit are allowed)
        bool HAAllowOvercommit() const;

        //! Check if pool is HA overcommitted (true if pool lacks resources to tolerate configured host failures)
        bool HAOvercommitted() const;

        //! Get HA cluster stack name (Name of the HA cluster stack, e.g., "xhad")
        QString HAClusterStack() const;

        //! Check if redo log is enabled (true if redo log is enabled for this pool)
        bool RedoLogEnabled() const;

        //! Get redo log VDI reference (Opaque reference to VDI used for redo log)
        QString GetRedoLogVDIRef() const;

        //! Get GUI configuration (Map of GUI-specific configuration settings)
        QVariantMap GUIConfig() const;

        //! Get health check configuration (Map of health check feature settings)
        QVariantMap HealthCheckConfig() const;

        //! Get guest agent configuration (Map of guest agent configuration settings)
        QVariantMap GuestAgentConfig() const;

        //! Get pool-wide CPU information (Map containing CPU vendor, features, and capabilities)
        QVariantMap CPUInfo() const;
        
        //! Get binary large objects (Map of blob names to blob references)
        QVariantMap Blobs() const;

        //! Get metadata VDI references (List of VDI references containing pool metadata)
        QStringList GetMetadataVDIRefs() const;

        //! Get metadata VDI objects (List of VDI shared pointers containing pool metadata)
        QList<QSharedPointer<VDI>> GetMetadataVDIs() const;

        //! Get default SR object (Shared pointer to default storage repository)
        QSharedPointer<SR> GetDefaultSR() const;

        //! Get suspend image SR object (Shared pointer to SR where suspend images are stored)
        QSharedPointer<SR> GetSuspendImageSR() const;

        //! Get crash dump SR object (Shared pointer to SR where crash dumps are stored)
        QSharedPointer<SR> GetCrashDumpSR() const;

        //! Get redo log VDI object (Shared pointer to VDI used for redo log)
        QSharedPointer<VDI> GetRedoLogVDI() const;

        //! Get all host objects in this pool (List of Host shared pointers)
        QList<QSharedPointer<Host>> GetHosts() const;

        //! Get master host object (Shared pointer to pool master/coordinator host)
        QSharedPointer<Host> GetMasterHost() const;

        //! Get all VM objects in this pool (List of VM shared pointers across all hosts)
        QList<QSharedPointer<VM>> GetAllVMs() const;

        //! Get Workload Balancing server URL (WLB server URL or empty string if not configured)
        QString WLBUrl() const;

        //! Get Workload Balancing username (WLB username or empty string)
        QString WLBUsername() const;

        //! Check if WLB certificate verification is enabled (true if WLB certificate should be verified)
        bool WLBVerifyCert() const;

        //! Get vSwitch controller address (vSwitch controller address or empty string, deprecated)
        QString VswitchController() const;
        bool vSwitchController() const;
        bool HasSriovNic() const;

        //! Get license restrictions (Map of license restriction keys and values)
        QVariantMap Restrictions() const;

        //! Check if vendor device policy is disabled (true if vendor device policy is set to deny)
        bool PolicyNoVendorDevice() const;

        //! Get list of allowed operations on this pool (List of operation type strings)
        QStringList AllowedOperations() const;

        //! Get currently running operations (Map of task reference to operation type)
        QVariantMap CurrentOperations() const;

        //! Check if IGMP snooping is enabled (true if IGMP snooping is enabled for networks)
        bool IGMPSnoopingEnabled() const;

        //! Get UEFI certificates (UEFI certificate data or empty string)
        QString UEFICertificates() const;

        //! Check if TLS verification is enabled (true if TLS certificate verification is enabled)
        bool TLSVerificationEnabled() const;

        //! Check if client certificate authentication is enabled (true if TLS client cert auth is enabled)
        bool ClientCertificateAuthEnabled() const;

        //! Get client certificate auth name requirement (CN/SAN that client certificates must have)
        QString ClientCertificateAuthName() const;

        //! Get enabled update repository references (List of repository opaque references)
        QStringList RepositoryRefs() const;

        //! Get repository proxy URL (Proxy URL for update repository access)
        QString RepositoryProxyUrl() const;

        //! Get repository proxy username (Username for proxy authentication)
        QString RepositoryProxyUsername() const;

        //! Get repository proxy password secret reference (Opaque reference to Secret containing proxy password)
        QString RepositoryProxyPasswordRef() const;

        //! Check if migration compression is enabled (true if VM migration should use stream compression)
        bool MigrationCompression() const;

        //! Check if coordinator bias is enabled (true if VM scheduling should avoid pool coordinator/master)
        bool CoordinatorBias() const;

        //! Get telemetry UUID secret reference (Opaque reference to Secret containing telemetry UUID)
        QString TelemetryUuidRef() const;

        //! Get telemetry collection frequency (Frequency string: "daily", "weekly", etc.)
        QString TelemetryFrequency() const;

        //! Get next telemetry collection time (Timestamp when next telemetry collection may occur)
        QDateTime TelemetryNextCollection() const;

        //! Get last update synchronization time (Timestamp of last update sync from CDN)
        QDateTime LastUpdateSync() const;

        //! Get update synchronization frequency (Frequency string: "daily", "weekly")
        QString UpdateSyncFrequency() const;

        //! Get update synchronization day of week (Day number 0-6, 0=Sunday for weekly sync)
        qint64 UpdateSyncDay() const;

        //! Check if periodic update sync is enabled (true if automatic update synchronization is enabled)
        bool UpdateSyncEnabled() const;

        //! Check if PSR (Pooled Storage Repository) is pending (true if PSR operation is pending)
        bool IsPsrPending() const;

        // Property getters for search/query functionality
        // C# equivalent: PropertyAccessors dictionary in Common.cs

        /**
         * @brief Get all VMs in this pool (across all hosts)
         * @return List of VM opaque references
         * 
         * C# equivalent: PropertyAccessors VM property for Pool
         * Returns all VMs from connection.Cache.VMs
         */
        QStringList GetAllVMRefs() const;

        /**
         * @brief Check if pool is not fully upgraded
         * @return true if pool hosts have different versions
         * 
         * C# equivalent: PropertyNames.isNotFullyUpgraded
         * Used by search to find pools needing upgrades
         */
        bool IsNotFullyUpgraded() const;

    protected:
        XenObjectType GetObjectType() const override { return XenObjectType::Pool; }
};

#endif // POOL_H
