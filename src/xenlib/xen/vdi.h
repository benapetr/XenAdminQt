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

protected:
    QString objectType() const override;
};

#endif // VDI_H
