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

#ifndef SR_H
#define SR_H

#include "xenobject.h"

class Host;
class VDI;
class PBD;
class Blob;
class SM;

/**
 * @brief SR - A storage repository
 *
 * Qt equivalent of C# XenAPI.SR class. Represents a storage repository.
 *
 * Key properties (from C# SR class):
 * - name_label, name_description
 * - type (nfs, lvmoiscsi, lvm, etc.)
 * - physical_size, physical_utilisation, virtual_allocation
 * - PBDs (physical block device connections to hosts)
 * - VDIs (virtual disk images stored in this SR)
 * - shared (whether SR is shared across hosts)
 * - content_type (user, iso, system, etc.)
 */
class XENLIB_EXPORT SR : public XenObject
{
    Q_OBJECT

    public:
        explicit SR(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        ~SR() override = default;

        //! Get SR type (e.g., "nfs", "lvmoiscsi", "lvm", "ext", "iso")
        QString GetType() const;

        //! Check if SR is shared across multiple hosts
        bool IsShared() const;

        bool IsLocked() const override;

        //! Get physical size (total physical size in bytes)
        qint64 PhysicalSize() const;

        //! Get physical utilisation (used physical space in bytes)
        qint64 PhysicalUtilisation() const;

        //! Get virtual allocation (total virtual allocation in bytes)
        qint64 VirtualAllocation() const;

        //! Get free space (free physical space in bytes)
        qint64 FreeSpace() const;

        /**
         * @brief Get a friendly size summary string for this SR
         *
         * Mirrors C# SR.SizeString():
         * "<used> used of <total> (<allocated> allocated)"
         */
        QString SizeString() const;

        //! Get list of VDI references (list of VDI opaque references)
        QStringList GetVDIRefs() const;

        //! Get list of PBD references (list of PBD opaque references)
        QStringList GetPBDRefs() const;

        //! Get content type ("user", "iso", "system", etc.)
        QString ContentType() const;

        //! Get storage manager plugin for this SR type
        QSharedPointer<SM> GetSM() const;

        //! Get SR name without host suffix (C# NameWithoutHost)
        QString NameWithoutHost() const;

        //! Get SM (storage manager) config (map of SM configuration)
        QVariantMap SMConfig() const;

        /**
         * @brief Get SCSI identifier for this SR
         *
         * Mirrors C# SR.GetSCSIID():
         * - first tries PBD.device_config["SCSIid"]
         * - then falls back to sm_config["devserial"] without "scsi-" prefix
         * - trims trailing commas
         *
         * @return SCSI ID string, or empty if unavailable
         */
        QString GetSCSIID() const;

        static constexpr qint64 DISK_MAX_SIZE = 2LL * 1024 * 1024 * 1024 * 1024; // 2 TiB

        //! Get allowed operations (list of allowed operation strings)
        QStringList AllowedOperations() const;

        QStringList GetCapabilities() const;

        //! Get current operations (map of operation ID to operation type)
        QVariantMap CurrentOperations() const;

        //! Check if SR supports trim/unmap
        bool SupportsTrim() const;

        //! Get binary blobs associated with this SR (map of blob name to blob reference)
        QVariantMap Blobs() const;

        //! Get PBDs (physical block device connections to hosts)
        QList<QSharedPointer<PBD>> GetPBDs() const;

        //! Get VDIs (virtual disk images stored in this SR)
        QList<QSharedPointer<VDI>> GetVDIs() const;

        //! Get binary blobs associated with this SR (list of Blob objects)
        QList<QSharedPointer<Blob>> GetBlobs() const;

        //! Check if SR is assigned as local cache for its host
        bool LocalCacheEnabled() const;

        //! Get disaster recovery task which introduced this SR (DR_task opaque reference, or empty if none)
        QString IntroducedBy() const;

        //! Check if SR is using aggregated local storage (clustered local storage)
        bool Clustered() const;

        /**
         * @brief Check if this is the SR that contains the Tools ISO VDIs
         * @return true if this SR contains XenServer Tools ISOs
         * 
         * Checks multiple indicators:
         * - is_tools_sr API flag
         * - name_label == "XenServer Tools" (legacy)
         * - type == "udev" (alternative detection)
         * 
         * C# equivalent: SR.IsToolsSR() extension method
         */
        bool IsToolsSR() const;

        //! Check if SR supports storage migration
        bool SupportsStorageMigration() const;

        //! Check if SR is raw HBA LUN-per-VDI (SR type is rawhba)
        bool HBALunPerVDI() const;

        /**
         * @brief Get the host for this SR (for shared SRs, returns pool coordinator)
         * @return Shared pointer to Host, or null if none
         * 
         * C# equivalent: SR.Home() extension method
         */
        QSharedPointer<Host> GetHost() const;

        QString NameWithLocation() const override;
        QString LocationString() const override;

        //! Check if SR is local (not shared, local to single host)
        bool IsLocal() const
        {
            return !IsShared();
        }

        //! Check if SR is an ISO library (content type is "iso")
        bool IsISOLibrary() const
        {
            return ContentType() == "iso";
        }

        /**
         * @brief Get home host reference
         *
         * For local SRs, returns the host this SR is connected to.
         * For shared SRs, returns empty string or first connected host.
         *
         * @return Host opaque reference
         */
        QString HomeRef() const;

        /**
         * @brief Get first attached storage host
         *
         * Iterates through PBDs and returns the host of the first PBD
         * that is currently_attached. Returns nullptr if no PBDs are attached.
         *
         * This matches C# SR.GetFirstAttachedStorageHost()
         *
         * @return Pointer to first attached Host, or nullptr if none
         */
        class Host* GetFirstAttachedStorageHost() const;

        /**
         * @brief Check if SR has a driver domain VM
         *
         * Checks PBDs for a "storage_driver_domain" entry in other_config and
         * verifies the VM exists and is not dom0.
         *
         * C# reference: XenModel/XenAPI-Extensions/SR.cs HasDriverDomain
         *
         * @param outVMRef Optional output for driver domain VM reference
         * @return true if a driver domain VM exists for this SR
         */
        bool HasDriverDomain(QString* outVMRef = nullptr) const;

        //! Check if SR has any PBDs
        bool HasPBDs() const;

        /**
         * @brief Check if SR is broken
         *
         * Mirrors C# SR.IsBroken(checkAttached).
         */
        bool IsBroken(bool checkAttached = true) const;

        /**
         * @brief Check if multipath is healthy
         *
         * Mirrors C# SR.MultipathAOK().
         */
        bool MultipathAOK() const;

        /**
         * @brief Check if SR can be repaired after legacy storage upgrade
         *
         * Mirrors C# SR.CanRepairAfterUpgradeFromLegacySL().
         */
        bool CanRepairAfterUpgradeFromLegacySL() const;

        /**
         * @brief Check if SR is detached (no attached PBDs)
         *
         * Mirrors C# SR.IsDetached().
         */
        bool IsDetached() const;

        /**
         * @brief Check if SR has any running VMs
         *
         * Mirrors C# SR.HasRunningVMs().
         */
        bool HasRunningVMs() const;

    protected:
        XenObjectType GetObjectType() const override { return XenObjectType::SR; }
};

#endif // SR_H
