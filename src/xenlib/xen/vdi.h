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

    public:
        explicit VDI(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        ~VDI() override = default;

        //! Get virtual size of VDI in bytes
        qint64 VirtualSize() const;

        //! Get physical space used by VDI in bytes
        qint64 PhysicalUtilisation() const;

        //! Get VDI type ("System", "User", "Ephemeral", "Suspend", "Crashdump", etc.)
        QString GetType() const;

        //! Check if VDI can be attached to multiple VMs
        bool Sharable() const;

        //! Check if VDI is read-only
        bool ReadOnly() const;

        //! Get parent SR opaque reference
        QString SRRef() const;

        //! Get list of VBD opaque references using this VDI
        QStringList GetVBDRefs() const;

        //! Check if VDI is in use (has attached VBDs)
        bool IsInUse() const;

        //! Get human-readable size string (e.g., "10 GB", "512 MB")
        QString SizeString() const;

        //! Get snapshot parent VDI opaque reference (if this is a snapshot)
        QString SnapshotOfRef() const;

        //! Check if this is a snapshot
        bool IsSnapshot() const;

        //! Get list of allowed operations on this VDI
        QStringList AllowedOperations() const;

        //! Get currently running operations (map of task reference to operation type)
        QVariantMap CurrentOperations() const;

        //! Check if VDI is locked at storage level
        bool StorageLock() const;

        //! Get VDI location on SR (path or identifier on storage repository)
        QString Location() const;

        //! Check if VDI is managed by XAPI
        bool Managed() const;

        //! Check if VDI is missing from storage (SR scan reported VDI not present on disk)
        bool Missing() const;

        //! Get parent VDI opaque reference for clones (deprecated, always null)
        QString ParentRef() const;

        //! Get crashdump opaque references
        QStringList CrashDumpRefs() const;

        //! Get XenStore data key-value pairs for /local/domain/0/backend/vbd/<domid>/<device-id>/sm-data
        QVariantMap XenstoreData() const;

        //! Get Storage Manager configuration (SM-dependent configuration data)
        QVariantMap SMConfig() const;

        //! Get snapshot VDI opaque references
        QStringList SnapshotRefs() const;

        //! Get snapshot creation timestamp
        QDateTime SnapshotTime() const;

        //! Check if VDI should be cached in local cache SR
        bool AllowCaching() const;

        //! Get VDI behavior on VM boot ("persist", "reset")
        QString OnBoot() const;

        //! Get pool opaque reference if this VDI contains pool metadata (or null)
        QString MetadataOfPoolRef() const;

        //! Check if this VDI contains latest pool metadata
        bool MetadataLatest() const;

        /**
         * @brief Check if this VDI is a XenServer Tools ISO
         * @return true if this is a tools ISO image
         *
         * Checks multiple indicators:
         * - is_tools_iso API flag (XenServer 7.3+)
         * - name_label matches known tools ISO names:
         *   "xswindrivers.iso", "xs-tools.iso", "guest-tools.iso" (legacy)
         *
         * C# equivalent: VDI.IsToolsIso() extension method
         */
        bool IsToolsIso() const;

        //! Check if Changed Block Tracking is enabled for this VDI
        bool IsCBTEnabled() const;

        // Object resolution getters
        QSharedPointer<SR> GetSR() const;
        QList<QSharedPointer<VBD>> GetVBDs() const;
        QString NameWithLocation() const override;

    protected:
        QString GetObjectType() const override;
};

#endif // VDI_H
