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

// grouping.cpp - Implementation of grouping algorithms
#include "grouping.h"
#include "queryfilter.h"
#include "xenlib.h"
#include "xencache.h"
#include <QDebug>

//==============================================================================
// Base Grouping class
//==============================================================================

Grouping::Grouping(Grouping* subgrouping)
    : m_subgrouping(subgrouping)
{
}

Grouping::~Grouping()
{
    // Note: We don't delete m_subgrouping - it's owned by the caller
}

QString Grouping::getGroupName(const QVariant& group) const
{
    // Default implementation: convert to string
    // C# equivalent: Grouping.GetGroupName() default implementation
    return group.toString();
}

QIcon Grouping::getGroupIcon(const QVariant& group) const
{
    Q_UNUSED(group);
    // Default implementation: return generic icon
    // C# equivalent: Grouping.GetGroupIcon() returns Icons.XenCenter
    return QIcon(":/resources/xenserver_16.png");
}

bool Grouping::belongsAsGroupNotMember(const QVariantMap& objectData, const QString& objectType) const
{
    Q_UNUSED(objectData);
    Q_UNUSED(objectType);
    // Default implementation: objects don't appear as groups
    // C# equivalent: Grouping.BelongsAsGroupNotMember() returns false
    return false;
}

Grouping* Grouping::getSubgrouping(const QVariant& group) const
{
    Q_UNUSED(group);
    // Default implementation: return the configured subgrouping
    // C# equivalent: Grouping.GetSubgrouping() returns null
    return m_subgrouping;
}

QueryFilter* Grouping::getSubquery(const QVariant& parent, const QVariant& group) const
{
    Q_UNUSED(parent);
    Q_UNUSED(group);
    // Default implementation: no filtering (return null)
    // C# equivalent: Grouping.GetSubquery() returns null
    // When null, Search will use NullQuery (no filtering)
    return nullptr;
}

//==============================================================================
// TypeGrouping - Group by object type (VM, Host, SR, etc.)
//==============================================================================

TypeGrouping::TypeGrouping(Grouping* subgrouping)
    : Grouping(subgrouping)
{
}

QString TypeGrouping::getGroupingName() const
{
    // C# equivalent: PropertyGrouping<ObjectTypes> with property=PropertyNames.type
    return "Type";
}

QString TypeGrouping::getGroupName(const QVariant& group) const
{
    // C# equivalent: Uses PropertyAccessors.GetName(ObjectTypes value)
    // Returns localized names like "Virtual Machines", "Servers", etc.

    QString objectType = group.toString();

    if (objectType == "vm")
        return "Virtual Machines";
    else if (objectType == "host")
        return "Servers";
    else if (objectType == "sr")
        return "Storage";
    else if (objectType == "network")
        return "Networks";
    else if (objectType == "pool")
        return "Pools";
    else if (objectType == "template")
        return "Templates";
    else
        return objectType;
}

QIcon TypeGrouping::getGroupIcon(const QVariant& group) const
{
    // C# equivalent: Uses PropertyAccessors.GetImage(ObjectTypes value)
    QString objectType = group.toString();

    if (objectType == "vm")
        return QIcon(":/resources/vm_16.png");
    else if (objectType == "host")
        return QIcon(":/resources/server_16.png");
    else if (objectType == "sr")
        return QIcon(":/resources/storage_16.png");
    else if (objectType == "network")
        return QIcon(":/resources/network_16.png");
    else if (objectType == "pool")
        return QIcon(":/resources/pool_16.png");
    else if (objectType == "template")
        return QIcon(":/resources/template_16.png");
    else
        return Grouping::getGroupIcon(group);
}

QVariant TypeGrouping::getGroup(const QVariantMap& objectData, const QString& objectType) const
{
    Q_UNUSED(objectData);

    // C# equivalent: PropertyAccessors.Get(PropertyNames.type)
    // Returns the ObjectTypes enum value

    // For templates, distinguish from regular VMs
    if (objectType == "vm")
    {
        bool isTemplate = objectData.value("is_a_template", false).toBool();
        if (isTemplate)
            return "template";
        else
            return "vm";
    }

    return objectType;
}

bool TypeGrouping::equals(const Grouping* other) const
{
    // All TypeGroupings are equivalent (like C# Equals implementation)
    return dynamic_cast<const TypeGrouping*>(other) != nullptr;
}

QueryFilter* TypeGrouping::getSubquery(const QVariant& parent, const QVariant& group) const
{
    Q_UNUSED(parent);

    // C# equivalent: PropertyGrouping<ObjectTypes>.GetSubquery()
    // Returns: new EnumPropertyQuery<ObjectTypes>(property, (ObjectTypes)val, true)
    //
    // This creates a filter that matches only objects of the specified type
    // For example, clicking "Servers" creates TypePropertyQuery("host", true)

    if (!group.isValid() || group.isNull())
        return nullptr;

    QString objectType = group.toString();

    // Create a TypePropertyQuery that matches this object type
    return new TypePropertyQuery(objectType, true);
}

//==============================================================================
// PoolGrouping - Group by pool membership
//==============================================================================

PoolGrouping::PoolGrouping(Grouping* subgrouping)
    : Grouping(subgrouping)
{
}

QString PoolGrouping::getGroupingName() const
{
    // C# equivalent: PropertyGrouping<Pool> with property=PropertyNames.pool
    return "Pool";
}

QString PoolGrouping::getGroupName(const QVariant& group) const
{
    // C# equivalent: Returns pool.Name()
    if (!m_xenLib || group.isNull())
        return "Unknown Pool";

    QString poolRef = group.toString();
    QVariantMap poolData = m_xenLib->getCache()->ResolveObjectData("pool", poolRef);

    if (poolData.isEmpty())
        return "Unknown Pool";

    return poolData.value("name_label", "Unknown Pool").toString();
}

QIcon PoolGrouping::getGroupIcon(const QVariant& group) const
{
    Q_UNUSED(group);
    return QIcon(":/resources/pool_16.png");
}

QVariant PoolGrouping::getGroup(const QVariantMap& objectData, const QString& objectType) const
{
    // C# equivalent: PropertyAccessors.Get(PropertyNames.pool)
    // Returns the pool reference for the object

    if (objectType == "pool")
    {
        // Pools belong to themselves
        return objectData.value("ref", QString()).toString();
    } else if (objectType == "host")
    {
        // Hosts have a pool reference
        QString poolRef = objectData.value("pool", QString()).toString();
        if (poolRef.isEmpty() || poolRef == "OpaqueRef:NULL")
            return QVariant(); // Standalone host (no pool)
        return poolRef;
    } else if (objectType == "vm" || objectType == "template")
    {
        // VMs/templates: get pool via resident_on host
        QString hostRef = objectData.value("resident_on", QString()).toString();
        if (hostRef.isEmpty() || hostRef == "OpaqueRef:NULL")
        {
            // Try affinity
            hostRef = objectData.value("affinity", QString()).toString();
        }

        if (hostRef.isEmpty() || hostRef == "OpaqueRef:NULL")
            return QVariant(); // No pool association

        if (!m_xenLib)
            return QVariant();

        QVariantMap hostData = m_xenLib->getCache()->ResolveObjectData("host", hostRef);
        if (hostData.isEmpty())
            return QVariant();

        QString poolRef = hostData.value("pool", QString()).toString();
        if (poolRef.isEmpty() || poolRef == "OpaqueRef:NULL")
            return QVariant();

        return poolRef;
    } else if (objectType == "sr")
    {
        // SRs: get pool via any connected host
        QVariantList pbds = objectData.value("PBDs", QVariantList()).toList();
        if (pbds.isEmpty() || !m_xenLib)
            return QVariant();

        // Get first PBD's host
        QString pbdRef = pbds.first().toString();
        QVariantMap pbdData = m_xenLib->getCache()->ResolveObjectData("pbd", pbdRef);
        if (pbdData.isEmpty())
            return QVariant();

        QString hostRef = pbdData.value("host", QString()).toString();
        if (hostRef.isEmpty() || hostRef == "OpaqueRef:NULL")
            return QVariant();

        QVariantMap hostData = m_xenLib->getCache()->ResolveObjectData("host", hostRef);
        if (hostData.isEmpty())
            return QVariant();

        QString poolRef = hostData.value("pool", QString()).toString();
        if (poolRef.isEmpty() || poolRef == "OpaqueRef:NULL")
            return QVariant();

        return poolRef;
    }

    return QVariant(); // Unknown type or no pool
}

bool PoolGrouping::equals(const Grouping* other) const
{
    // All PoolGroupings are equivalent
    return dynamic_cast<const PoolGrouping*>(other) != nullptr;
}

//==============================================================================
// HostGrouping - Group by host membership
//==============================================================================

HostGrouping::HostGrouping(Grouping* subgrouping)
    : Grouping(subgrouping)
{
}

QString HostGrouping::getGroupingName() const
{
    // C# equivalent: PropertyGrouping<Host> with property=PropertyNames.host
    return "Server";
}

QString HostGrouping::getGroupName(const QVariant& group) const
{
    // C# equivalent: Returns host.Name()
    if (!m_xenLib || group.isNull())
        return "Unknown Server";

    QString hostRef = group.toString();
    QVariantMap hostData = m_xenLib->getCache()->ResolveObjectData("host", hostRef);

    if (hostData.isEmpty())
        return "Unknown Server";

    return hostData.value("name_label", "Unknown Server").toString();
}

QIcon HostGrouping::getGroupIcon(const QVariant& group) const
{
    Q_UNUSED(group);
    return QIcon(":/resources/server_16.png");
}

QVariant HostGrouping::getGroup(const QVariantMap& objectData, const QString& objectType) const
{
    // C# equivalent: PropertyAccessors.Get(PropertyNames.host)
    // For VMs, this uses Common.HostProperty() which returns VM.Home()

    if (objectType == "host")
    {
        // Hosts belong to themselves
        return objectData.value("ref", QString()).toString();
    } else if (objectType == "vm" || objectType == "template")
    {
        // VMs: Use VMHelpers::getVMHome() logic
        // This matches C# VM.Home() method

        QString powerState = objectData.value("power_state", QString()).toString();

        // If running or paused, use resident_on
        if (powerState == "Running" || powerState == "Paused")
        {
            QString residentOn = objectData.value("resident_on", QString()).toString();
            if (!residentOn.isEmpty() && residentOn != "OpaqueRef:NULL")
                return residentOn;
        }

        // Try storage host (for local storage VMs)
        // TODO: Implement getVMStorageHost equivalent if needed

        // Try affinity
        QString affinity = objectData.value("affinity", QString()).toString();
        if (!affinity.isEmpty() && affinity != "OpaqueRef:NULL")
            return affinity;

        // No home (offline VM)
        return QVariant();
    } else if (objectType == "sr")
    {
        // SRs can belong to multiple hosts via PBDs
        // For grouping purposes, we might skip SRs or use first connected host
        return QVariant();
    }

    return QVariant(); // Unknown type or no host
}

bool HostGrouping::equals(const Grouping* other) const
{
    // All HostGroupings are equivalent
    return dynamic_cast<const HostGrouping*>(other) != nullptr;
}
