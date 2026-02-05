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

#include "vusb.h"
#include "network/connection.h"
#include "../xencache.h"
#include "vm.h"
#include "usbgroup.h"

VUSB::VUSB(XenConnection* connection, const QString& opaqueRef, QObject* parent) : XenObject(connection, opaqueRef, parent)
{
}


QStringList VUSB::AllowedOperations() const
{
    return this->property("allowed_operations").toStringList();
}

QVariantMap VUSB::CurrentOperations() const
{
    return this->property("current_operations").toMap();
}

QString VUSB::GetVMRef() const
{
    return this->stringProperty("VM");
}

QString VUSB::USBGroupRef() const
{
    return this->stringProperty("USB_group");
}

bool VUSB::CurrentlyAttached() const
{
    return this->boolProperty("currently_attached");
}

// Helper methods
bool VUSB::IsAttached() const
{
    return this->CurrentlyAttached();
}

QSharedPointer<VM> VUSB::GetVM() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QSharedPointer<VM>();
    
    XenCache* cache = connection->GetCache();
    if (!cache)
        return QSharedPointer<VM>();
    
    QString ref = this->GetVMRef();
    if (ref.isEmpty() || ref == XENOBJECT_NULL)
        return QSharedPointer<VM>();
    
    return cache->ResolveObject<VM>(XenObjectType::VM, ref);
}

QSharedPointer<USBGroup> VUSB::GetUSBGroup() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QSharedPointer<USBGroup>();
    
    XenCache* cache = connection->GetCache();
    if (!cache)
        return QSharedPointer<USBGroup>();
    
    QString ref = this->USBGroupRef();
    if (ref.isEmpty() || ref == XENOBJECT_NULL)
        return QSharedPointer<USBGroup>();
    
    return cache->ResolveObject<USBGroup>(ref);
}
