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

#ifndef VDI_H
#define VDI_H

#include "xenobject.h"

class SR;
class VBD;

/**
 * @brief VDI - A virtual disk image
 *
 * Qt equivalent of C# XenAPI.VDI class. Represents a virtual disk image.
 *
 * Key properties (from C# VDI class):
 * - name_label, name_description
 * - virtual_size (size in bytes)
 * - physical_utilisation (actual space used)
 * - type (System, User, Ephemeral, Suspend, Crashdump, etc.)
 * - sharable (whether VDI can be attached to multiple VMs)
 * - read_only (whether VDI is read-only)
 * - SR (parent storage repository)
 * - VBDs (virtual block devices using this VDI)
 */
class XENLIB_EXPORT VDI : public XenObject
{
    Q_OBJECT
    Q_PROPERTY(qint64 virtualSize READ virtualSize NOTIFY dataChanged)
    Q_PROPERTY(qint64 physicalUtilisation READ physicalUtilisation NOTIFY dataChanged)
    Q_PROPERTY(QString type READ type NOTIFY dataChanged)
    Q_PROPERTY(bool sharable READ sharable NOTIFY dataChanged)
    Q_PROPERTY(bool readOnly READ readOnly NOTIFY dataChanged)

    public:
        explicit VDI(XenConnection* connection,
                     const QString& opaqueRef,
                     QObject* parent = nullptr);
        ~VDI() override = default;

        /**
         * @brief Get virtual size of VDI
         * @return Size in bytes
         */
        qint64 virtualSize() const;

        /**
         * @brief Get physical space used by VDI
         * @return Size in bytes
         */
        qint64 physicalUtilisation() const;

        /**
         * @brief Get VDI type
         * @return Type string: "System", "User", "Ephemeral", "Suspend", "Crashdump", etc.
         */
        QString type() const;

        /**
         * @brief Check if VDI is sharable
         * @return true if VDI can be attached to multiple VMs
         */
        bool sharable() const;

        /**
         * @brief Check if VDI is read-only
         * @return true if VDI is read-only
         */
        bool readOnly() const;

        /**
         * @brief Get reference to parent SR
         * @return SR opaque reference
         */
        QString srRef() const;

        /**
         * @brief Get list of VBD references using this VDI
         * @return List of VBD opaque references
         */
        QStringList vbdRefs() const;

        /**
         * @brief Check if VDI is in use (has attached VBDs)
         * @return true if VDI has any VBDs
         */
        bool isInUse() const;

        /**
         * @brief Get human-readable size string
         * @return Size formatted as "10 GB", "512 MB", etc.
         */
        QString sizeString() const;

        /**
         * @brief Get snapshot parent reference (if this is a snapshot)
         * @return VDI opaque reference of parent VDI
         */
        QString snapshotOfRef() const;

        /**
         * @brief Check if this is a snapshot
         * @return true if VDI is a snapshot
         */
        bool isSnapshot() const;

        /**
         * @brief Get list of allowed operations on this VDI
         * @return List of operation type strings
         */
        QStringList allowedOperations() const;

        /**
         * @brief Get currently running operations
         * @return Map of task reference to operation type
         */
        QVariantMap currentOperations() const;

        /**
         * @brief Check if VDI is locked at storage level
         * @return true if storage-level lock is active
         */
        bool storageLock() const;

        /**
         * @brief Get VDI location on SR
         * @return Location string (path or identifier on storage repository)
         */
        QString location() const;

        /**
         * @brief Check if VDI is managed by XAPI
         * @return true if XAPI manages this VDI
         */
        bool managed() const;

        /**
         * @brief Check if VDI is missing from storage
         * @return true if SR scan reported VDI not present on disk
         */
        bool missing() const;

        /**
         * @brief Get parent VDI reference
         * @return Opaque reference to parent VDI (for clones) - deprecated, always null
         */
        QString parentRef() const;

        /**
         * @brief Get crash dump references
         * @return List of crashdump opaque references
         */
        QStringList crashDumpRefs() const;

        /**
         * @brief Get XenStore data
         * @return Map of key-value pairs for /local/domain/0/backend/vbd/<domid>/<device-id>/sm-data
         */
        QVariantMap xenstoreData() const;

        /**
         * @brief Get Storage Manager configuration
         * @return Map of SM-dependent configuration data
         */
        QVariantMap smConfig() const;

        /**
         * @brief Get snapshot VDI references
         * @return List of snapshot VDI opaque references
         */
        QStringList snapshotRefs() const;

        /**
         * @brief Get snapshot creation timestamp
         * @return Date/time when snapshot was created
         */
        QDateTime snapshotTime() const;

        /**
         * @brief Get user-specified tags
         * @return List of tag strings for categorization
         */
        QStringList tags() const;

        /**
         * @brief Get additional configuration
         * @return Map of additional configuration key-value pairs
         */
        QVariantMap otherConfig() const;

        /**
         * @brief Check if VDI should be cached in local cache SR
         * @return true if caching is enabled
         */
        bool allowCaching() const;

        /**
         * @brief Get VDI behavior on VM boot
         * @return Behavior string ("persist", "reset")
         */
        QString onBoot() const;

        /**
         * @brief Get pool reference if this VDI contains pool metadata
         * @return Opaque reference to pool or null
         */
        QString metadataOfPoolRef() const;

        /**
         * @brief Check if this VDI contains latest pool metadata
         * @return true if this is the most recent accessible pool metadata
         */
        bool metadataLatest() const;

        /**
         * @brief Check if this VDI is a XenServer Tools ISO
         * @return true if this is a tools ISO image
         */
        bool isToolsIso() const;

        /**
         * @brief Check if Changed Block Tracking is enabled
         * @return true if CBT is tracking changed blocks for this VDI
         */
        bool cbtEnabled() const;

    protected:
        QString GetObjectType() const override;
};

#endif // VDI_H
