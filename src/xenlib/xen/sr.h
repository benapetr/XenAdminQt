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
    Q_PROPERTY(QString type READ GetType NOTIFY dataChanged)
    Q_PROPERTY(bool shared READ IsShared NOTIFY dataChanged)
    Q_PROPERTY(qint64 physicalSize READ PhysicalSize NOTIFY dataChanged)
    Q_PROPERTY(qint64 physicalUtilisation READ PhysicalUtilisation NOTIFY dataChanged)

    public:
        explicit SR(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        ~SR() override = default;

        /**
         * @brief Get SR type
         * @return Type string (e.g., "nfs", "lvmoiscsi", "lvm", "ext", "iso")
         */
        QString GetType() const;

        /**
         * @brief Check if SR is shared
         * @return true if SR is shared across multiple hosts
         */
        bool IsShared() const;

        /**
         * @brief Get physical size (bytes)
         * @return Total physical size in bytes
         */
        qint64 PhysicalSize() const;

        /**
         * @brief Get physical utilisation (bytes)
         * @return Used physical space in bytes
         */
        qint64 PhysicalUtilisation() const;

        /**
         * @brief Get virtual allocation (bytes)
         * @return Total virtual allocation in bytes
         */
        qint64 VirtualAllocation() const;

        /**
         * @brief Get free space (bytes)
         * @return Free physical space in bytes
         */
        qint64 FreeSpace() const;

        /**
         * @brief Get list of VDI references
         * @return List of VDI opaque references
         */
        QStringList VDIRefs() const;

        /**
         * @brief Get list of PBD references
         * @return List of PBD opaque references
         */
        QStringList PBDRefs() const;

        /**
         * @brief Get content type
         * @return Content type string ("user", "iso", "system", etc.)
         */
        QString ContentType() const;

        /**
         * @brief Get other_config dictionary
         * @return Map of additional configuration
         */
        QVariantMap OtherConfig() const;

        /**
         * @brief Get SM (storage manager) config
         * @return Map of SM configuration
         */
        QVariantMap SMConfig() const;

        /**
         * @brief Get tags
         * @return List of tag strings
         */
        QStringList Tags() const;

        /**
         * @brief Get allowed operations
         * @return List of allowed operation strings
         */
        QStringList AllowedOperations() const;

        /**
         * @brief Get current operations
         * @return Map of operation ID to operation type
         */
        QVariantMap CurrentOperations() const;

        /**
         * @brief Check if SR supports trim/unmap
         * @return true if SR supports trim
         */
        bool SupportsTrim() const;

        /**
         * @brief Get binary blobs associated with this SR
         * @return Map of blob name to blob reference
         */
        QVariantMap Blobs() const;

        /**
         * @brief Check if SR is assigned as local cache for its host
         * @return true if SR is assigned to be the local cache for its host
         */
        bool LocalCacheEnabled() const;

        /**
         * @brief Get disaster recovery task which introduced this SR
         * @return DR_task opaque reference, or empty string if none
         */
        QString IntroducedBy() const;

        /**
         * @brief Check if SR is using aggregated local storage
         * @return true if SR is using clustered local storage
         */
        bool Clustered() const;

        /**
         * @brief Check if this is the SR that contains the Tools ISO VDIs
         * @return true if this SR contains XenServer Tools ISOs
         */
        bool IsToolsSR() const;

        /**
         * @brief Check if SR supports storage migration
         * @return true if SR supports storage migration
         */
        bool SupportsStorageMigration() const;

        /**
         * @brief Check if SR is raw HBA LUN-per-VDI
         * @return true if SR type is rawhba
         */
        bool HBALunPerVDI() const;

        /**
         * @brief Check if SR is local (not shared)
         * @return true if SR is local to single host
         */
        bool IsLocal() const
        {
            return !IsShared();
        }

        /**
         * @brief Check if SR is an ISO library
         * @return true if content type is "iso"
         */
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

    protected:
        QString GetObjectType() const override;
};

#endif // SR_H
