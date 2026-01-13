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

#include "vlan.h"
#include "network/connection.h"
#include "../xencache.h"
#include "pif.h"

VLAN::VLAN(XenConnection* connection,
           const QString& opaqueRef,
           QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

QString VLAN::GetTaggedPIFRef() const
{
    return this->stringProperty("tagged_PIF");
}

QString VLAN::GetUntaggedPIFRef() const
{
    return this->stringProperty("untagged_PIF");
}

qint64 VLAN::GetTag() const
{
    return this->longProperty("tag", -1);
}

QSharedPointer<PIF> VLAN::GetTaggedPIF() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QSharedPointer<PIF>();
    
    XenCache* cache = connection->GetCache();
    if (!cache)
        return QSharedPointer<PIF>();
    
    QString ref = this->GetTaggedPIFRef();
    if (ref.isEmpty() || ref == "OpaqueRef:NULL")
        return QSharedPointer<PIF>();
    
    return cache->ResolveObject<PIF>("pif", ref);
}

QSharedPointer<PIF> VLAN::GetUntaggedPIF() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QSharedPointer<PIF>();
    
    XenCache* cache = connection->GetCache();
    if (!cache)
        return QSharedPointer<PIF>();
    
    QString ref = this->GetUntaggedPIFRef();
    if (ref.isEmpty() || ref == "OpaqueRef:NULL")
        return QSharedPointer<PIF>();
    
    return cache->ResolveObject<PIF>("pif", ref);
}

QString VLAN::GetObjectType() const
{
    return "vlan";
}
