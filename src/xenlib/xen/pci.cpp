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

#include "pci.h"
#include "network/connection.h"
#include "../xencache.h"
#include "host.h"

PCI::PCI(XenConnection* connection, const QString& opaqueRef, QObject* parent) : XenObject(connection, opaqueRef, parent)
{
}


QString PCI::ClassName() const
{
    return this->stringProperty("class_name");
}

QString PCI::VendorName() const
{
    return this->stringProperty("vendor_name");
}

QString PCI::DeviceName() const
{
    return this->stringProperty("device_name");
}

QString PCI::GetHostRef() const
{
    return this->stringProperty("host");
}

QString PCI::PciId() const
{
    return this->stringProperty("pci_id");
}

QStringList PCI::DependencyRefs() const
{
    return this->property("dependencies").toStringList();
}

QString PCI::SubsystemVendorName() const
{
    return this->stringProperty("subsystem_vendor_name");
}

QString PCI::SubsystemDeviceName() const
{
    return this->stringProperty("subsystem_device_name");
}

QString PCI::DriverName() const
{
    return this->stringProperty("driver_name");
}

// Helper methods
bool PCI::HasDependencies() const
{
    return !this->DependencyRefs().isEmpty();
}

QString PCI::GetFullDeviceName() const
{
    QString vendor = this->VendorName();
    QString device = this->DeviceName();
    
    if (!vendor.isEmpty() && !device.isEmpty())
        return QString("%1 %2").arg(vendor, device);
    else if (!device.isEmpty())
        return device;
    else if (!vendor.isEmpty())
        return vendor;
    else
        return this->PciId();
}

QSharedPointer<Host> PCI::GetHost() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QSharedPointer<Host>();
    
    XenCache* cache = connection->GetCache();
    if (!cache)
        return QSharedPointer<Host>();
    
    QString ref = this->GetHostRef();
    if (ref.isEmpty() || ref == XENOBJECT_NULL)
        return QSharedPointer<Host>();
    
    return cache->ResolveObject<Host>(XenObjectType::Host, ref);
}
