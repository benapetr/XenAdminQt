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
#include "queries.h"
#include "xencache.h"
#include "../folders/foldersmanager.h"
#include "xen/sr.h"
#include "xen/vm.h"
#include <QDebug>

//==============================================================================
// Base Grouping class
//==============================================================================

//! TODO move all icon stuff to UI layer our of library

Grouping::Grouping(Grouping* subgrouping) : m_subgrouping(subgrouping)
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
    else if (objectType == "snapshot")
        return "Snapshots";
    else if (objectType == "host")
        return "Servers";
    else if (objectType == "disconnected_host")
        return "Disconnected Servers";
    else if (objectType == "sr")
        return "Storage";
    else if (objectType == "vdi")
        return "Virtual Disks";
    else if (objectType == "network")
        return "Networks";
    else if (objectType == "pool")
        return "Pools";
    else if (objectType == "template")
        return "Templates";
    else if (objectType == "VM_appliance" || objectType == "vm_appliance")
        return "VM Appliance";
    else
        return objectType;
}

QIcon TypeGrouping::getGroupIcon(const QVariant& group) const
{
    // C# equivalent: Uses PropertyAccessors.GetImage(ObjectTypes value)
    QString objectType = group.toString();

    if (objectType == "vm")
        return QIcon(":/tree-icons/vm_generic.png");
    else if (objectType == "snapshot")
        return QIcon(":/tree-icons/snapshot.png");
    else if (objectType == "host")
        return QIcon(":/tree-icons/host.png");
    else if (objectType == "disconnected_host")
        return QIcon(":/tree-icons/host_disconnected.png");
    else if (objectType == "sr")
        return QIcon(":/tree-icons/storage.png");
    else if (objectType == "vdi")
        return QIcon(":/tree-icons/storage.png");
    else if (objectType == "network")
        return QIcon(":/tree-icons/network.png");
    else if (objectType == "pool")
        return QIcon(":/tree-icons/pool.png");
    else if (objectType == "template")
        return QIcon(":/tree-icons/template.png");
    else if (objectType == "VM_appliance" || objectType == "vm_appliance")
        return QIcon(":/tree-icons/vm_generic.png");
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
        if (objectData.value("is_a_snapshot", false).toBool())
            return "snapshot";

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
    QVariantMap poolData = this->m_connection->GetCache()->ResolveObjectData(XenObjectType::Pool, poolRef);

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
    Q_UNUSED(objectData);
    Q_UNUSED(objectType);

    // C# equivalent: PropertyAccessors.Get(PropertyNames.pool)
    // This always returns the pool of the connection (if any), regardless of object.
    if (!this->m_connection || !this->m_connection->GetCache())
        return QVariant();

    const QStringList poolRefs = this->m_connection->GetCache()->GetAllRefs(XenObjectType::Pool);
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
    QVariantMap hostData = this->m_connection->GetCache()->ResolveObjectData(XenObjectType::Host, hostRef);

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
            if (!snapshotOf.isEmpty() && snapshotOf != XENOBJECT_NULL && this->m_connection)
            {
                QSharedPointer<VM> vm = this->m_connection->GetCache()->ResolveObject<VM>(XenObjectType::VM, snapshotOf);
                if (vm)
                {
                    const QString home = vm->GetHomeRef();
                    if (!home.isEmpty() && home != XENOBJECT_NULL)
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
            if (!residentOn.isEmpty() && residentOn != XENOBJECT_NULL)
                return residentOn;
        }

        if (this->m_connection && this->m_connection->GetCache())
        {
            const QVariantList vbdRefs = objectData.value("VBDs").toList();
            for (const QVariant& vbdRefVar : vbdRefs)
            {
                const QString vbdRef = vbdRefVar.toString();
                if (vbdRef.isEmpty() || vbdRef == XENOBJECT_NULL)
                    continue;

                const QVariantMap vbdData = this->m_connection->GetCache()->ResolveObjectData(XenObjectType::VBD, vbdRef);
                if (vbdData.isEmpty())
                    continue;

                if (vbdData.value("type").toString().compare("Disk", Qt::CaseInsensitive) != 0)
                    continue;

                const QString vdiRef = vbdData.value("VDI").toString();
                if (vdiRef.isEmpty() || vdiRef == XENOBJECT_NULL)
                    continue;

                const QVariantMap vdiData = this->m_connection->GetCache()->ResolveObjectData(XenObjectType::VDI, vdiRef);
                if (vdiData.isEmpty())
                    continue;

                const QString srRef = vdiData.value("SR").toString();
                if (srRef.isEmpty() || srRef == XENOBJECT_NULL)
                    continue;

                const QVariantMap srData = this->m_connection->GetCache()->ResolveObjectData(XenObjectType::SR, srRef);
                if (srData.isEmpty())
                    continue;

                const bool srShared = srData.value("shared", false).toBool();
                const QVariantList pbds = srData.value("PBDs").toList();
                if (srShared || pbds.size() != 1)
                    continue;

                for (const QVariant& pbdRefVar : pbds)
                {
                    const QString pbdRef = pbdRefVar.toString();
                    if (pbdRef.isEmpty() || pbdRef == XENOBJECT_NULL)
                        continue;

                    const QVariantMap pbdData = this->m_connection->GetCache()->ResolveObjectData(XenObjectType::PBD, pbdRef);
                    const QString hostRef = pbdData.value("host").toString();
                    if (!hostRef.isEmpty() && hostRef != XENOBJECT_NULL)
                        return hostRef;
                }
            }
        }

        QString affinity = objectData.value("affinity", QString()).toString();
        if (!affinity.isEmpty() && affinity != XENOBJECT_NULL)
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

        QSharedPointer<SR> srObj = this->m_connection->GetCache()->ResolveObject<SR>(XenObjectType::SR, srRef);
        if (!srObj)
            return QVariant();

        QString homeRef = srObj->HomeRef();
        if (!homeRef.isEmpty() && homeRef != XENOBJECT_NULL)
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
            const QStringList hostRefs = this->m_connection->GetCache()->GetAllRefs(XenObjectType::Host);
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

//==============================================================================
// FolderGrouping - Group by folder ancestry
//==============================================================================

FolderGrouping::FolderGrouping(Grouping* subgrouping)
    : Grouping(subgrouping)
{
}

QString FolderGrouping::getGroupingName() const
{
    return "Folder";
}

QString FolderGrouping::getGroupName(const QVariant& group) const
{
    const QString path = group.toString();
    if (path == FoldersManager::PATH_SEPARATOR)
        return "Folders";

    const QStringList parts = FoldersManager::PointToPath(path);
    return parts.isEmpty() ? "Folders" : parts.last();
}

QIcon FolderGrouping::getGroupIcon(const QVariant& group) const
{
    Q_UNUSED(group);
    return QIcon(":/resources/folder_16.png");
}

QVariant FolderGrouping::getGroup(const QVariantMap& objectData, const QString& objectType) const
{
    if (objectType == "folder")
    {
        const QString folderRef = objectData.value("ref").toString();
        const QString parent = FoldersManager::GetParent(folderRef);
        if (parent.isEmpty() || parent == FoldersManager::PATH_SEPARATOR)
            return QVariant();
        QVariantList list;
        list.append(parent);
        return list;
    }

    const QString path = FoldersManager::FolderPathFromRecord(objectData);
    if (path.isEmpty())
        return QVariant();

    const QStringList ancestors = FoldersManager::AncestorFolders(path);
    if (ancestors.isEmpty())
        return QVariant();

    QVariantList nested;
    QVariantList chain;
    for (const QString& ancestor : ancestors)
        chain.append(ancestor);
    nested.append(chain);
    return nested;
}

bool FolderGrouping::belongsAsGroupNotMember(const QVariantMap& objectData, const QString& objectType) const
{
    if (objectType != "folder")
        return false;

    const QString folderRef = objectData.value("ref").toString();
    const QString parent = FoldersManager::GetParent(folderRef);
    return !parent.isEmpty() && parent == FoldersManager::PATH_SEPARATOR;
}

bool FolderGrouping::equals(const Grouping* other) const
{
    return dynamic_cast<const FolderGrouping*>(other) != nullptr;
}

//==============================================================================
// TagsGrouping
//==============================================================================

TagsGrouping::TagsGrouping(Grouping* subgrouping) : Grouping(subgrouping)
{
}

QString TagsGrouping::getGroupingName() const
{
    return "Tags";
}

QString TagsGrouping::getGroupName(const QVariant& group) const
{
    return group.toString();
}

QIcon TagsGrouping::getGroupIcon(const QVariant& group) const
{
    Q_UNUSED(group);
    return QIcon(":/resources/tag_16.png");
}

QVariant TagsGrouping::getGroup(const QVariantMap& objectData, const QString& objectType) const
{
    Q_UNUSED(objectType);
    const QVariant tagsValue = objectData.value("tags");
    if (tagsValue.canConvert<QStringList>())
    {
        QVariantList groups;
        const QStringList tags = tagsValue.toStringList();
        for (const QString& tag : tags)
            groups.append(tag);
        return groups;
    }
    if (tagsValue.canConvert<QVariantList>())
        return tagsValue.toList();
    return QVariant();
}

QueryFilter* TagsGrouping::getSubquery(const QVariant& parent, const QVariant& group) const
{
    Q_UNUSED(parent);
    if (!group.isValid())
        return nullptr;
    return new TagQuery(group.toString(), false);
}

bool TagsGrouping::equals(const Grouping* other) const
{
    return dynamic_cast<const TagsGrouping*>(other) != nullptr;
}

//==============================================================================
// VAppGrouping
//==============================================================================

VAppGrouping::VAppGrouping(Grouping* subgrouping) : Grouping(subgrouping)
{
}

QString VAppGrouping::getGroupingName() const
{
    return "vApps";
}

QString VAppGrouping::getGroupName(const QVariant& group) const
{
    if (!this->m_connection || !this->m_connection->GetCache())
        return group.toString();

    const QVariantMap applianceData = this->m_connection->GetCache()->ResolveObjectData(XenObjectType::VMAppliance, group.toString());
    if (applianceData.isEmpty())
        return group.toString();
    return applianceData.value("name_label").toString();
}

QIcon VAppGrouping::getGroupIcon(const QVariant& group) const
{
    Q_UNUSED(group);
    return QIcon(":/resources/vapp_16.png");
}

QVariant VAppGrouping::getGroup(const QVariantMap& objectData, const QString& objectType) const
{
    if (objectType != "vm")
        return QVariant();
    const QString applianceRef = objectData.value("appliance").toString();
    if (applianceRef.isEmpty() || applianceRef == XENOBJECT_NULL)
        return QVariant();
    return applianceRef;
}

QueryFilter* VAppGrouping::getSubquery(const QVariant& parent, const QVariant& group) const
{
    Q_UNUSED(parent);
    if (!group.isValid())
        return nullptr;
    return new XenModelObjectPropertyQuery(XenSearch::PropertyNames::appliance, group.toString(), true);
}

bool VAppGrouping::equals(const Grouping* other) const
{
    return dynamic_cast<const VAppGrouping*>(other) != nullptr;
}
