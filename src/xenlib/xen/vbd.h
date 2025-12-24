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

#ifndef VBD_H
#define VBD_H

#include "xenobject.h"

class VM;
class VDI;

/**
 * @brief VBD - A virtual block device
 *
 * Qt equivalent of C# XenAPI.VBD class. Represents a virtual block device
 * (disk attachment between a VM and a VDI).
 *
 * Key properties (from C# VBD class):
 * - VM (parent virtual machine)
 * - VDI (virtual disk image)
 * - device (device name in guest, e.g., "xvda", "hda")
 * - userdevice (device number, e.g., "0", "1", "2")
 * - bootable (whether this device is bootable)
 * - mode (RO or RW)
 * - type (Disk, CD)
 * - unpluggable (whether device can be hot-unplugged)
 * - currently_attached (whether device is currently plugged)
 * - empty (whether VDI is empty - for CD drives)
 */
class XENLIB_EXPORT VBD : public XenObject
{
    Q_OBJECT
    Q_PROPERTY(QString device READ device NOTIFY dataChanged)
    Q_PROPERTY(QString userdevice READ userdevice NOTIFY dataChanged)
    Q_PROPERTY(bool bootable READ bootable NOTIFY dataChanged)
    Q_PROPERTY(QString mode READ mode NOTIFY dataChanged)
    Q_PROPERTY(QString type READ type NOTIFY dataChanged)
    Q_PROPERTY(bool currentlyAttached READ currentlyAttached NOTIFY dataChanged)

public:
    explicit VBD(XenConnection* connection,
                 const QString& opaqueRef,
                 QObject* parent = nullptr);
    ~VBD() override = default;

    /**
     * @brief Get parent VM reference
     * @return VM opaque reference
     */
    QString vmRef() const;

    /**
     * @brief Get VDI reference
     * @return VDI opaque reference (empty if CD drive with no disc)
     */
    QString vdiRef() const;

    /**
     * @brief Get device name in guest
     * @return Device name like "xvda", "hda", "xvdb", etc.
     */
    QString device() const;

    /**
     * @brief Get user device number
     * @return Device number like "0", "1", "2", etc.
     */
    QString userdevice() const;

    /**
     * @brief Check if device is bootable
     * @return true if bootable
     */
    bool bootable() const;

    /**
     * @brief Get device mode
     * @return "RO" (read-only) or "RW" (read-write)
     */
    QString mode() const;

    /**
     * @brief Check if device is read-only
     * @return true if mode is "RO"
     */
    bool isReadOnly() const;

    /**
     * @brief Get device type
     * @return "Disk" or "CD"
     */
    QString type() const;

    /**
     * @brief Check if device is a CD drive
     * @return true if type is "CD"
     */
    bool isCD() const;

    /**
     * @brief Check if device can be unplugged
     * @return true if unpluggable
     */
    bool unpluggable() const;

    /**
     * @brief Check if device is currently attached
     * @return true if currently attached to VM
     */
    bool currentlyAttached() const;

    /**
     * @brief Check if VDI is empty (CD with no disc)
     * @return true if empty
     */
    bool empty() const;

    /**
     * @brief Get allowed operations on this VBD
     * @return List of allowed operation strings (e.g., "plug", "unplug")
     */
    QStringList allowedOperations() const;

    /**
     * @brief Check if plug operation is allowed
     * @return true if "plug" is in allowed operations
     */
    bool canPlug() const;

    /**
     * @brief Check if unplug operation is allowed
     * @return true if "unplug" is in allowed operations
     */
    bool canUnplug() const;

    /**
     * @brief Get human-readable description
     * @return String like "Disk 0 (xvda)" or "CD Drive 1 (hdc)"
     */
    QString description() const;

    /**
     * @brief Get currently running operations
     * @return Map of task reference to operation type
     */
    QVariantMap currentOperations() const;

    /**
     * @brief Check if VBD is locked at storage level
     * @return true if storage-level lock is active
     */
    bool storageLock() const;

    /**
     * @brief Get additional configuration
     * @return Map of additional configuration key-value pairs
     */
    QVariantMap otherConfig() const;

    /**
     * @brief Get status code from last attach operation
     * @return Error/success code (erased on reboot)
     */
    qint64 statusCode() const;

    /**
     * @brief Get status detail from last attach operation
     * @return Error/success information string (erased on reboot)
     */
    QString statusDetail() const;

    /**
     * @brief Get device runtime properties
     * @return Map of runtime property key-value pairs
     */
    QVariantMap runtimeProperties() const;

    /**
     * @brief Get QoS algorithm type
     * @return QoS algorithm identifier string
     */
    QString qosAlgorithmType() const;

    /**
     * @brief Get QoS algorithm parameters
     * @return Map of parameters for chosen QoS algorithm
     */
    QVariantMap qosAlgorithmParams() const;

    /**
     * @brief Get supported QoS algorithms
     * @return List of supported QoS algorithm identifiers
     */
    QStringList qosSupportedAlgorithms() const;

    /**
     * @brief Get VBD metrics reference
     * @return Opaque reference to VBD_metrics object
     */
    QString metricsRef() const;

protected:
    QString objectType() const override;
};

#endif // VBD_H
