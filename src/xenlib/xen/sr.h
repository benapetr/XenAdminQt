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
    Q_PROPERTY(QString type READ type NOTIFY dataChanged)
    Q_PROPERTY(bool shared READ shared NOTIFY dataChanged)
    Q_PROPERTY(qint64 physicalSize READ physicalSize NOTIFY dataChanged)
    Q_PROPERTY(qint64 physicalUtilisation READ physicalUtilisation NOTIFY dataChanged)

public:
    explicit SR(XenConnection* connection,
                const QString& opaqueRef,
                QObject* parent = nullptr);
    ~SR() override = default;

    /**
     * @brief Get SR type
     * @return Type string (e.g., "nfs", "lvmoiscsi", "lvm", "ext", "iso")
     */
    QString type() const;

    /**
     * @brief Check if SR is shared
     * @return true if SR is shared across multiple hosts
     */
    bool shared() const;

    /**
     * @brief Get physical size (bytes)
     * @return Total physical size in bytes
     */
    qint64 physicalSize() const;

    /**
     * @brief Get physical utilisation (bytes)
     * @return Used physical space in bytes
     */
    qint64 physicalUtilisation() const;

    /**
     * @brief Get virtual allocation (bytes)
     * @return Total virtual allocation in bytes
     */
    qint64 virtualAllocation() const;

    /**
     * @brief Get free space (bytes)
     * @return Free physical space in bytes
     */
    qint64 freeSpace() const;

    /**
     * @brief Get list of VDI references
     * @return List of VDI opaque references
     */
    QStringList vdiRefs() const;

    /**
     * @brief Get list of PBD references
     * @return List of PBD opaque references
     */
    QStringList pbdRefs() const;

    /**
     * @brief Get content type
     * @return Content type string ("user", "iso", "system", etc.)
     */
    QString contentType() const;

    /**
     * @brief Get other_config dictionary
     * @return Map of additional configuration
     */
    QVariantMap otherConfig() const;

    /**
     * @brief Get SM (storage manager) config
     * @return Map of SM configuration
     */
    QVariantMap smConfig() const;

    /**
     * @brief Get tags
     * @return List of tag strings
     */
    QStringList tags() const;

    /**
     * @brief Get allowed operations
     * @return List of allowed operation strings
     */
    QStringList allowedOperations() const;

    /**
     * @brief Get current operations
     * @return Map of operation ID to operation type
     */
    QVariantMap currentOperations() const;

    /**
     * @brief Check if SR supports trim/unmap
     * @return true if SR supports trim
     */
    bool supportsTrim() const;

    /**
     * @brief Check if SR is local (not shared)
     * @return true if SR is local to single host
     */
    bool isLocal() const
    {
        return !shared();
    }

    /**
     * @brief Check if SR is an ISO library
     * @return true if content type is "iso"
     */
    bool isISOLibrary() const
    {
        return contentType() == "iso";
    }

    /**
     * @brief Get home host reference
     *
     * For local SRs, returns the host this SR is connected to.
     * For shared SRs, returns empty string or first connected host.
     *
     * @return Host opaque reference
     */
    QString homeRef() const;

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
    class Host* getFirstAttachedStorageHost() const;

protected:
    QString objectType() const override
    {
        return "SR";
    }
};

#endif // SR_H
