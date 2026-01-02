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

#ifndef PCI_H
#define PCI_H

#include "xenobject.h"
#include <QString>
#include <QStringList>
#include <QVariantMap>

/*!
 * \brief PCI device wrapper class
 * 
 * Represents a physical PCI device on a XenServer host.
 * Provides access to device identification, dependencies, and driver information.
 * First published in XenServer 6.0.
 */
class PCI : public XenObject
{
    Q_OBJECT
    
    Q_PROPERTY(QString uuid READ Uuid)
    Q_PROPERTY(QString className READ ClassName)
    Q_PROPERTY(QString vendorName READ VendorName)
    Q_PROPERTY(QString deviceName READ DeviceName)
    Q_PROPERTY(QString host READ HostRef)
    Q_PROPERTY(QString pciId READ PciId)
    Q_PROPERTY(QString subsystemVendorName READ SubsystemVendorName)
    Q_PROPERTY(QString subsystemDeviceName READ SubsystemDeviceName)
    Q_PROPERTY(QString driverName READ DriverName)
    
    public:
        explicit PCI(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);

        QString GetObjectType() const override;

        // Basic properties
        QString Uuid() const;
        QString ClassName() const;
        QString VendorName() const;
        QString DeviceName() const;
        QString HostRef() const;
        QString PciId() const;
        QStringList DependencyRefs() const;
        QVariantMap OtherConfig() const;

        // Extended properties
        QString SubsystemVendorName() const;
        QString SubsystemDeviceName() const;
        QString DriverName() const;

        // Helper methods
        bool HasDependencies() const;
        QString GetFullDeviceName() const;
};

#endif // PCI_H
