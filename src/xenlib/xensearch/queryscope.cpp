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

// queryscope.cpp - Implementation of QueryScope
#include "queryscope.h"
#include "xenlib.h"
#include <QDebug>

QueryScope::QueryScope(ObjectTypes types)
    : m_types(types)
{
    // C# equivalent: QueryScope constructor
}

bool QueryScope::wantType(const QVariantMap& objectData, const QString& objectType, XenConnection *conn) const
{
    // C# equivalent: WantType(IXenObject o)
    ObjectTypes type = objectTypeOf(objectData, objectType, conn);
    bool wants = wantType(type);

    // qDebug() << "      QueryScope::wantType() - type:" << objectType
    //          << "ObjectTypes:" << static_cast<int>(type)
    //          << "m_types:" << static_cast<int>(m_types)
    //          << "wants:" << wants;

    return wants;
}

bool QueryScope::wantType(ObjectTypes t) const
{
    // C# equivalent: WantType(ObjectTypes t)
    // Want type t: or if t is a bitwise OR, want *all* types in t
    // I.e., the types "this" includes are a superset of t
    return ((m_types & t) == t);
}

bool QueryScope::wantType(const QueryScope* q) const
{
    // C# equivalent: WantType(QueryScope q)
    return (q != nullptr && wantType(q->getObjectTypes()));
}

bool QueryScope::wantSubsetOf(ObjectTypes t) const
{
    // C# equivalent: WantSubsetOf(ObjectTypes t)
    // The types "this" includes are a subset of t
    return ((m_types & t) == m_types);
}

bool QueryScope::wantSubsetOf(const QueryScope* q) const
{
    // C# equivalent: WantSubsetOf(QueryScope q)
    return (q != nullptr && wantSubsetOf(q->getObjectTypes()));
}

bool QueryScope::wantAnyOf(ObjectTypes t) const
{
    // C# equivalent: WantAnyOf(ObjectTypes t)
    // Want any of the types in t: i.e., the types "this" includes
    // overlap with t
    return ((m_types & t) != ObjectTypes::None);
}

bool QueryScope::wantAnyOf(const QueryScope* q) const
{
    // C# equivalent: WantAnyOf(QueryScope q)
    return (q != nullptr && wantAnyOf(q->getObjectTypes()));
}

bool QueryScope::equals(ObjectTypes t) const
{
    // C# equivalent: Equals(ObjectTypes t)
    return m_types == static_cast<int>(t);
}

bool QueryScope::equals(const QueryScope* q) const
{
    // C# equivalent: Equals(QueryScope q)
    return (q != nullptr && equals(q->getObjectTypes()));
}

bool QueryScope::operator==(const QueryScope& other) const
{
    // C# equivalent: Equals(object obj)
    return equals(&other);
}

uint QueryScope::hashCode() const
{
    // C# equivalent: GetHashCode()
    return static_cast<uint>(static_cast<int>(m_types));
}

ObjectTypes QueryScope::objectTypeOf(const QVariantMap& objectData, const QString& objectType, XenConnection *conn) const
{
    // C# equivalent: ObjectTypeOf(IXenObject o)
    // Uses PropertyAccessors.Get(PropertyNames.type)
    // Returns the ObjectTypes enum value for the given object

    Q_UNUSED(conn); // May be needed for more complex type detection

    if (objectType == "pool")
    {
        return ObjectTypes::Pool;
    } else if (objectType == "host")
    {
        // Check if host is connected or disconnected
        // C# checks connection state
        // For now, assume all hosts are connected (we can refine this later)
        return ObjectTypes::Server;
    } else if (objectType == "vm")
    {
        // Distinguish between VM, Snapshot, UserTemplate, DefaultTemplate
        bool isTemplate = objectData.value("is_a_template", false).toBool();
        bool isSnapshot = objectData.value("is_a_snapshot", false).toBool();
        bool isDefaultTemplate = objectData.value("is_default_template", false).toBool();

        if (isSnapshot)
            return ObjectTypes::Snapshot;
        else if (isTemplate)
        {
            if (isDefaultTemplate)
                return ObjectTypes::DefaultTemplate;
            else
                return ObjectTypes::UserTemplate;
        } else
            return ObjectTypes::VM;
    } else if (objectType == "sr")
    {
        // Distinguish between RemoteSR and LocalSR
        QString srType = objectData.value("type", QString()).toString();
        bool shared = objectData.value("shared", false).toBool();

        // C# uses SR.IsLocalSR() which checks various conditions
        // Local SRs: local VHD, LVM, ISO (not shared)
        if (!shared || srType == "lvm" || srType == "udev" || srType == "iso")
            return ObjectTypes::LocalSR;
        else
            return ObjectTypes::RemoteSR;
    } else if (objectType == "vdi")
    {
        return ObjectTypes::VDI;
    } else if (objectType == "network")
    {
        return ObjectTypes::Network;
    } else if (objectType == "folder")
    {
        return ObjectTypes::Folder;
    } else if (objectType == "appliance" || objectType == "vm_appliance")
    {
        return ObjectTypes::Appliance;
    }

    // Default: None
    return ObjectTypes::None;
}
