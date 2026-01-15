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

    public:
        explicit VBD(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        ~VBD() override = default;

        //! Get parent VM opaque reference
        QString GetVMRef() const;

        QSharedPointer<VM> GetVM();

        //! Get VDI opaque reference (empty if CD drive with no disc)
        QString GetVDIRef() const;

        //! Get VDI object (nullptr if CD drive with no disc)
        QSharedPointer<VDI> GetVDI();

        //! Check if this VBD is owned by its VM (other_config["owner"] present)
        bool IsOwner() const;

        //! Get device name in guest (e.g., "xvda", "hda", "xvdb")
        QString GetDevice() const;

        //! Get user device number (e.g., "0", "1", "2")
        QString GetUserdevice() const;

        //! Check if device is bootable
        bool IsBootable() const;

        //! Get device mode ("RO" or "RW")
        QString GetMode() const;

        //! Check if device is read-only
        bool IsReadOnly() const;

        //! Get device type ("Disk" or "CD")
        QString GetType() const;

        //! Check if device is a CD drive
        bool IsCD() const;

        /**
         * @brief Check if device is a floppy drive
         * @return false (floppy drives not yet supported in XenAPI)
         * @note C# version also returns false - floppy support is TODO
         */
        bool IsFloppyDrive() const;

        //! Check if device can be unplugged
        bool Unpluggable() const;

        //! Check if device is currently attached to VM
        bool CurrentlyAttached() const;

        //! Check if VDI is empty (CD with no disc)
        bool Empty() const;

        //! Get allowed operations on this VBD (e.g., "plug", "unplug")
        QStringList AllowedOperations() const;

        //! Check if plug operation is allowed
        bool CanPlug() const;

        //! Check if unplug operation is allowed
        bool CanUnplug() const;

        //! Get human-readable description (e.g., "Disk 0 (xvda)" or "CD Drive 1 (hdc)")
        QString Description() const;

        //! Get currently running operations (map of task reference to operation type)
        QVariantMap CurrentOperations() const;

        //! Check if VBD is locked at storage level
        bool StorageLock() const;

        //! Get status code from last attach operation (erased on reboot)
        qint64 StatusCode() const;

        //! Get status detail from last attach operation (erased on reboot)
        QString StatusDetail() const;

        //! Get device runtime properties
        QVariantMap RuntimeProperties() const;

        //! Get QoS algorithm type
        QString QosAlgorithmType() const;

        //! Get QoS algorithm parameters
        QVariantMap QosAlgorithmParams() const;

        //! Get supported QoS algorithms
        QStringList QosSupportedAlgorithms() const;

        //! Get VBD metrics opaque reference
        QString MetricsRef() const;

    protected:
        QString GetObjectType() const override;
};

#endif // VBD_H
