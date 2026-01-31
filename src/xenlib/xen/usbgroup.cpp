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

#include "usbgroup.h"
#include "network/connection.h"
#include "../xencache.h"
#include "pusb.h"
#include "vusb.h"

USBGroup::USBGroup(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

USBGroup::~USBGroup()
{
}

QString USBGroup::GetUuid() const
{
    return this->stringProperty("uuid");
}

QString USBGroup::GetNameLabel() const
{
    return this->stringProperty("name_label");
}

QString USBGroup::GetNameDescription() const
{
    return this->stringProperty("name_description");
}

QStringList USBGroup::GetPUSBRefs() const
{
    return this->property("PUSBs").toStringList();
}

QStringList USBGroup::GetVUSBRefs() const
{
    return this->property("VUSBs").toStringList();
}

QList<QSharedPointer<PUSB>> USBGroup::GetPUSBs() const
{
    QList<QSharedPointer<PUSB>> result;
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return result;
    
    XenCache* cache = connection->GetCache();
    if (!cache)
        return result;
    
    QStringList refs = this->GetPUSBRefs();
    for (const QString& ref : refs)
    {
        if (!ref.isEmpty() && ref != "OpaqueRef:NULL")
        {
            QSharedPointer<PUSB> obj = cache->ResolveObject<PUSB>(ref);
            if (obj)
                result.append(obj);
        }
    }
    return result;
}

QList<QSharedPointer<VUSB>> USBGroup::GetVUSBs() const
{
    QList<QSharedPointer<VUSB>> result;
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return result;
    
    XenCache* cache = connection->GetCache();
    if (!cache)
        return result;
    
    QStringList refs = this->GetVUSBRefs();
    for (const QString& ref : refs)
    {
        if (!ref.isEmpty() && ref != "OpaqueRef:NULL")
        {
            QSharedPointer<VUSB> obj = cache->ResolveObject<VUSB>(ref);
            if (obj)
                result.append(obj);
        }
    }
    return result;
}
