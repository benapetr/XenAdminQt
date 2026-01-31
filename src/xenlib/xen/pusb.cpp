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

#include "pusb.h"
#include "network/connection.h"
#include "host.h"
#include "usbgroup.h"
#include "../xencache.h"
#include <QMap>

PUSB::PUSB(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

PUSB::~PUSB()
{
}

QString PUSB::GetUSBGroupRef() const
{
    return this->stringProperty("USB_group");
}

QString PUSB::GetHostRef() const
{
    return this->stringProperty("host");
}

QString PUSB::Path() const
{
    return this->stringProperty("path");
}

QString PUSB::VendorId() const
{
    return this->stringProperty("vendor_id");
}

QString PUSB::VendorDesc() const
{
    return this->stringProperty("vendor_desc");
}

QString PUSB::ProductId() const
{
    return this->stringProperty("product_id");
}

QString PUSB::ProductDesc() const
{
    return this->stringProperty("product_desc");
}

QString PUSB::Serial() const
{
    return this->stringProperty("serial");
}

QString PUSB::Version() const
{
    return this->stringProperty("version");
}

QString PUSB::Description() const
{
    return this->stringProperty("description");
}

bool PUSB::PassthroughEnabled() const
{
    return this->boolProperty("passthrough_enabled", false);
}

double PUSB::Speed() const
{
    return this->property("speed").toDouble();
}

QSharedPointer<USBGroup> PUSB::GetUSBGroup() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QSharedPointer<USBGroup>();
    
    XenCache* cache = connection->GetCache();
    if (!cache)
        return QSharedPointer<USBGroup>();
    
    QString ref = this->GetUSBGroupRef();
    if (ref.isEmpty() || ref == XENOBJECT_NULL)
        return QSharedPointer<USBGroup>();
    
    return cache->ResolveObject<USBGroup>(XenObjectType::USBGroup, ref);
}

QSharedPointer<Host> PUSB::GetHost() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QSharedPointer<Host>();
    
    XenCache* cache = connection->GetCache();
    if (!cache)
        return QSharedPointer<Host>();
    
    QString ref = this->GetHostRef();
    if (ref.isEmpty() || ref == "OpaqueRef:NULL")
        return QSharedPointer<Host>();
    
    return cache->ResolveObject<Host>(XenObjectType::Host, ref);
}
