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

#ifndef PIFMETRICS_H
#define PIFMETRICS_H

#include "xenobject.h"
#include <QDateTime>
#include <QMap>

/**
 * @brief PIFMetrics - The metrics associated with a physical network interface
 *
 * Qt equivalent of C# XenAPI.PIF_metrics class.
 * First published in XenServer 4.0.
 *
 * Key properties:
 * - uuid: Unique identifier
 * - io_read_kbs: Read bandwidth (KiB/s)
 * - io_write_kbs: Write bandwidth (KiB/s)
 * - carrier: Report if the PIF got a carrier or not
 * - vendor_id: Report vendor ID
 * - vendor_name: Report vendor name
 * - device_id: Report device ID
 * - device_name: Report device name
 * - speed: Speed of the link (if available)
 * - duplex: Full duplex capability of the link (if available)
 * - pci_bus_path: PCI bus path of the pif (if available)
 * - last_updated: Time at which this information was last updated
 * - other_config: Additional configuration
 */
class XENLIB_EXPORT PIFMetrics : public XenObject
{
    Q_OBJECT

    public:
        explicit PIFMetrics(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        ~PIFMetrics() override = default;

        QString GetObjectType() const override;

        //! Get read bandwidth (KiB/s)
        double IoReadKbs() const;

        //! Get write bandwidth (KiB/s)
        double IoWriteKbs() const;

        //! Report if the PIF got a carrier or not
        bool Carrier() const;

        //! Report vendor ID
        QString VendorId() const;

        //! Report vendor name
        QString VendorName() const;

        //! Report device ID
        QString DeviceId() const;

        //! Report device name
        QString DeviceName() const;

        //! Get speed of the link (if available)
        qint64 Speed() const;

        //! Get full duplex capability of the link (if available)
        bool Duplex() const;

        //! Get PCI bus path of the pif (if available)
        QString PciBusPath() const;

        //! Get time at which this information was last updated
        QDateTime LastUpdated() const;

        //! Get additional configuration
        QMap<QString, QString> OtherConfig() const;
};

#endif // PIFMETRICS_H
