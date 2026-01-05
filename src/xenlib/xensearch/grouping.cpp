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
#include "xencache.h"
#include "xen/sr.h"
#include "xen/vm.h"
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
    else if (objectType == "disconnected_host")
        return "Disconnected Servers";
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
    else if (objectType == "disconnected_host")
        return QIcon(":/tree-icons/host_disconnected.png");
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

    if (objectType == "host")
    {
        if (objectData.value("is_disconnected").toBool())
            return "disconnected_host";
        return "host";
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
    if (!this->m_connection || group.isNull())
        return "Unknown Pool";

    QString poolRef = group.toString();
    QVariantMap poolData = this->m_connection->GetCache()->ResolveObjectData("pool", poolRef);

    if (poolData.isEmpty())
        return "Unknown Pool";

    return poolData.value("name_label", "Unknown Pool").toString();
}

QIcon PoolGrouping::getGroupIcon(const QVariant& group) const
{
    Q_UNUSED(group);
    return QIcon(":/resources/pool_16.png");
}

static QString valueForKeys(const QVariantMap& map, std::initializer_list<const char*> keys)
{
    for (const char* key : keys)
    {
        const QString value = map.value(QString::fromLatin1(key)).toString();
        if (!value.isEmpty())
            return value;
    }
    return QString();
}

QVariant PoolGrouping::getGroup(const QVariantMap& objectData, const QString& objectType) const
{
    // C# equivalent: PropertyAccessors.Get(PropertyNames.pool)
    // This always returns the pool of the connection (if any), regardless of object.
    if (!this->m_connection || !this->m_connection->GetCache())
        return QVariant();

    const QStringList poolRefs = this->m_connection->GetCache()->GetAllRefs("pool");
    if (poolRefs.isEmpty())
        return QVariant();

    return poolRefs.first();
}

bool PoolGrouping::belongsAsGroupNotMember(const QVariantMap& objectData, const QString& objectType) const
{
    Q_UNUSED(objectData);
    return objectType == "pool";
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
    if (!this->m_connection || group.isNull())
        return "Unknown Server";

    QString hostRef = group.toString();
    QVariantMap hostData = this->m_connection->GetCache()->ResolveObjectData("host", hostRef);

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
        return valueForKeys(objectData, {"ref", "opaqueRef", "opaque_ref"});
    } else if (objectType == "vm" || objectType == "template")
    {
        // VMs: Use VM.Home() logic (C# VM.Home)
        // 1) Snapshots use their source VM's home
        // 2) Templates (non-snapshots) have no home (pool-level)
        // 3) If running/paused, use resident_on
        // 4) Try storage host
        // 5) Try affinity

        if (objectData.value("is_a_snapshot").toBool())
        {
            const QString snapshotOf = objectData.value("snapshot_of").toString();
            if (!snapshotOf.isEmpty() && snapshotOf != "OpaqueRef:NULL" && this->m_connection)
            {
                QSharedPointer<VM> vm = this->m_connection->GetCache()->ResolveObject<VM>("vm", snapshotOf);
                if (vm)
                {
                    const QString home = vm->HomeRef();
                    if (!home.isEmpty() && home != "OpaqueRef:NULL")
                        return home;
                }
            }
        }

        if (objectData.value("is_a_template").toBool())
            return QVariant();

        QString powerState = objectData.value("power_state", QString()).toString();
        if (powerState == "Running" || powerState == "Paused")
        {
            QString residentOn = objectData.value("resident_on", QString()).toString();
            if (!residentOn.isEmpty() && residentOn != "OpaqueRef:NULL")
                return residentOn;
        }

        if (this->m_connection && this->m_connection->GetCache())
        {
            const QVariantList vbdRefs = objectData.value("VBDs").toList();
            for (const QVariant& vbdRefVar : vbdRefs)
            {
                const QString vbdRef = vbdRefVar.toString();
                if (vbdRef.isEmpty() || vbdRef == "OpaqueRef:NULL")
                    continue;

                const QVariantMap vbdData = this->m_connection->GetCache()->ResolveObjectData("vbd", vbdRef);
                if (vbdData.isEmpty())
                    continue;

                if (vbdData.value("type").toString().compare("Disk", Qt::CaseInsensitive) != 0)
                    continue;

                const QString vdiRef = vbdData.value("VDI").toString();
                if (vdiRef.isEmpty() || vdiRef == "OpaqueRef:NULL")
                    continue;

                const QVariantMap vdiData = this->m_connection->GetCache()->ResolveObjectData("vdi", vdiRef);
                if (vdiData.isEmpty())
                    continue;

                const QString srRef = vdiData.value("SR").toString();
                if (srRef.isEmpty() || srRef == "OpaqueRef:NULL")
                    continue;

                const QVariantMap srData = this->m_connection->GetCache()->ResolveObjectData("sr", srRef);
                if (srData.isEmpty())
                    continue;

                const QVariantList pbds = srData.value("PBDs").toList();
                for (const QVariant& pbdRefVar : pbds)
                {
                    const QString pbdRef = pbdRefVar.toString();
                    if (pbdRef.isEmpty() || pbdRef == "OpaqueRef:NULL")
                        continue;

                    const QVariantMap pbdData = this->m_connection->GetCache()->ResolveObjectData("pbd", pbdRef);
                    const QString hostRef = pbdData.value("host").toString();
                    if (!hostRef.isEmpty() && hostRef != "OpaqueRef:NULL")
                        return hostRef;
                }
            }
        }

        QString affinity = objectData.value("affinity", QString()).toString();
        if (!affinity.isEmpty() && affinity != "OpaqueRef:NULL")
            return affinity;

        return QVariant();
    } else if (objectType == "sr")
    {
        // SRs can belong to multiple hosts via PBDs
        if (!this->m_connection || !this->m_connection->GetCache())
            return QVariant();

        QString srRef = valueForKeys(objectData, {"ref", "opaqueRef", "opaque_ref"});
        if (srRef.isEmpty())
            return QVariant();

        QSharedPointer<SR> srObj = this->m_connection->GetCache()->ResolveObject<SR>("sr", srRef);
        if (!srObj)
            return QVariant();

        QString homeRef = srObj->HomeRef();
        if (!homeRef.isEmpty() && homeRef != "OpaqueRef:NULL")
            return homeRef;

        return QVariant();
    } else if (objectType == "network")
    {
        if (!this->m_connection || !this->m_connection->GetCache())
            return QVariant();

        const QVariantList pifRefs = objectData.value("PIFs").toList();
        if (pifRefs.isEmpty())
        {
            QVariantList hosts;
            const QStringList hostRefs = this->m_connection->GetCache()->GetAllRefs("host");
            for (const QString& hostRef : hostRefs)
                hosts.append(hostRef);
            return hosts;
        }
    }

    return QVariant(); // Unknown type or no host
}

bool HostGrouping::belongsAsGroupNotMember(const QVariantMap& objectData, const QString& objectType) const
{
    Q_UNUSED(objectData);
    return objectType == "host";
}

bool HostGrouping::equals(const Grouping* other) const
{
    // All HostGroupings are equivalent
    return dynamic_cast<const HostGrouping*>(other) != nullptr;
}
