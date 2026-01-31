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

#include "group.h"
#include "grouping.h"
#include "search.h"
#include "query.h"
#include "sort.h"
#include "iacceptgroups.h"
#include "../utils/misc.h"
#include "../xencache.h"
#include "../xen/network/connection.h"
#include "../xen/network/connectionsmanager.h"
#include "../xen/pool.h"
#include "../xen/host.h"
#include "../xen/vm.h"
#include "../xen/sr.h"
#include "../xen/network.h"
#include <QDebug>
#include <algorithm>

namespace XenSearch
{
    // ========================================================================
    // GroupKey implementation
    // ========================================================================

    GroupKey::GroupKey(Grouping* grouping, const QVariant& key)
        : grouping_(grouping), key_(key)
    {
    }

    bool GroupKey::operator==(const GroupKey& other) const
    {
        if (this->grouping_ != other.grouping_)
            return false;
        return this->key_ == other.key_;
    }

    // ========================================================================
    // Group implementation
    // ========================================================================

    Group::Group(Search* search)
        : search_(search)
    {
    }

    QSharedPointer<Group> Group::GetGrouped(Search* search)
    {
        QSharedPointer<Group> group = GetGroupFor(nullptr, search, search->GetEffectiveGrouping());

        GetGrouped(search, group);

        return group;
    }

    QSharedPointer<Group> Group::GetGroupFor(Grouping* grouping, Search* search, Grouping* subgrouping)
    {
        // TODO: Check if grouping is FolderGrouping when that class exists
        // if (dynamic_cast<FolderGrouping*>(grouping))
        //     return QSharedPointer<Group>(new FolderGroup(search, grouping));
        if (subgrouping == nullptr)
            return QSharedPointer<Group>(new LeafGroup(search));
        else
            return QSharedPointer<Group>(new NodeGroup(search, subgrouping));
    }

    void Group::GetGrouped(Search* search, QSharedPointer<Group> group)
    {
        search->SetItems(0);

        // Get all connections from ConnectionsManager
        Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
        QList<XenConnection*> connections = connMgr->GetAllConnections();

        foreach (XenConnection* connection, connections)
        {
            if (connection && connection->IsConnected())
            {
                XenCache* cache = connection->GetCache();
                if (!cache)
                    continue;

                // Get pool to check if connection is valid
                QString poolRef = cache->GetPoolRef();
                if (poolRef.isEmpty())
                    continue;

                // Process all objects in cache
                // VMs
                QList<QSharedPointer<XenObject>> vmObjects = cache->GetAll(XenObjectType::VM);
                foreach (const QSharedPointer<XenObject>& vmObj, vmObjects)
                {
                    if (!vmObj)
                        continue;
                    QString ref = vmObj->OpaqueRef();
                    QVariantMap vmData = vmObj->GetData();
                    vmData["__type"] = "vm";
                    if (!vmData.isEmpty() && !Hide("vm", ref, vmData, connection))
                        group->FilterAdd(search->GetQuery(), "vm", ref, vmData);
                }

                // Hosts
                QList<QSharedPointer<XenObject>> hostObjects = cache->GetAll(XenObjectType::Host);
                foreach (const QSharedPointer<XenObject>& hostObj, hostObjects)
                {
                    if (!hostObj)
                        continue;
                    QString ref = hostObj->OpaqueRef();
                    QVariantMap hostData = hostObj->GetData();
                    hostData["__type"] = "host";
                    if (!hostData.isEmpty() && !Hide("host", ref, hostData, connection))
                        group->FilterAdd(search->GetQuery(), "host", ref, hostData);
                }

                // SRs
                QList<QSharedPointer<XenObject>> srObjects = cache->GetAll(XenObjectType::SR);
                foreach (const QSharedPointer<XenObject>& srObj, srObjects)
                {
                    if (!srObj)
                        continue;
                    QString ref = srObj->OpaqueRef();
                    QVariantMap srData = srObj->GetData();
                    srData["__type"] = "sr";
                    if (!srData.isEmpty() && !Hide("sr", ref, srData, connection))
                        group->FilterAdd(search->GetQuery(), "sr", ref, srData);
                }

                // Networks
                QList<QSharedPointer<XenObject>> networkObjects = cache->GetAll(XenObjectType::Network);
                foreach (const QSharedPointer<XenObject>& networkObj, networkObjects)
                {
                    if (!networkObj)
                        continue;
                    QString ref = networkObj->OpaqueRef();
                    QVariantMap networkData = networkObj->GetData();
                    networkData["__type"] = "network";
                    if (!networkData.isEmpty() && !Hide("network", ref, networkData, connection))
                        group->FilterAdd(search->GetQuery(), "network", ref, networkData);
                }

                // Pools
                QList<QSharedPointer<XenObject>> poolObjects = cache->GetAll(XenObjectType::Pool);
                foreach (const QSharedPointer<XenObject>& poolObj, poolObjects)
                {
                    if (!poolObj)
                        continue;
                    QString ref = poolObj->OpaqueRef();
                    QVariantMap poolData = poolObj->GetData();
                    poolData["__type"] = "pool";
                    if (!poolData.isEmpty() && !Hide("pool", ref, poolData, connection))
                        group->FilterAdd(search->GetQuery(), "pool", ref, poolData);
                }

                // Folders (TODO: implement when folder support is added)
            }
            else
            {
                // TODO: Handle disconnected connections with fake host
            }
        }
    }

    bool Group::Hide(const QString& objectType, const QString& objectRef, const QVariantMap& objectData, XenConnection* conn)
    {
        // TODO: Integrate with XenAdminConfigManager equivalent for hidden objects
        // if (XenAdminConfigManager::Provider::ObjectIsHidden(objectRef))
        //     return true;

        if (!conn)
            return true;

        XenCache* cache = conn->GetCache();
        if (!cache)
            return true;

        if (objectType == "vm")
        {
            bool isTemplate = objectData.value("is_a_template").toBool();
            bool isControlDomain = objectData.value("is_control_domain").toBool();
            bool isSnapshot = objectData.value("is_a_snapshot").toBool();

            // TODO: Check VTPM restriction when Helpers is ported
            // if (!Helpers::VtpmCanView(conn))
            //     return true;

            // Hide control domain VMs
            if (isControlDomain)
                return true;

            // Hide templates
            if (isTemplate)
                return true;

            // Hide snapshots (TODO: make this configurable via grouping settings)
            if (isSnapshot)
                return true;

            // TODO: Check VM visibility using VM.Show() when that method exists
            // QSharedPointer<VM> vm = cache->ResolveObject<VM>("vm", objectRef);
            // if (vm && !vm->Show(false)) // false = don't show hidden
            //     return true;

            // Check if VM is on non-live host
            QString residentOnRef = objectData.value("resident_on").toString();
            if (!residentOnRef.isEmpty() && residentOnRef != "OpaqueRef:NULL")
            {
                QSharedPointer<Host> host = cache->ResolveObject<Host>(XenObjectType::Host, residentOnRef);
                if (host && !host->IsLive())
                    return true;
            }
        }
        else if (objectType == "sr")
        {
            // Check if SR is tools SR
            QSharedPointer<SR> sr = cache->ResolveObject<SR>(XenObjectType::SR, objectRef);
            if (sr)
            {
                if (sr->IsToolsSR())
                    return true;

                // TODO: Check SR visibility using SR.Show() when that method exists
                // if (!sr->Show(false))
                //     return true;
            }

            // Check if SR is on non-live host (check all PBDs)
            QVariantList pbdRefs = objectData.value("PBDs").toList();
            bool hasLiveHost = false;

            for (const QVariant& pbdRefVar : pbdRefs)
            {
                QString pbdRef = pbdRefVar.toString();
                if (pbdRef.isEmpty())
                    continue;

                QVariantMap pbdData = cache->ResolveObjectData("pbd", pbdRef);
                QString hostRef = pbdData.value("host").toString();
                if (hostRef.isEmpty())
                    continue;

                QSharedPointer<Host> host = cache->ResolveObject<Host>(XenObjectType::Host, hostRef);
                if (host && host->IsLive())
                {
                    hasLiveHost = true;
                    break;
                }
            }

            if (!hasLiveHost)
                return true;
        }
        else if (objectType == "network")
        {
            // TODO: Check network visibility using Network.Show() when that method exists
            // QSharedPointer<Network> network = cache->ResolveObject<Network>("network", objectRef);
            // if (network && !network->Show(false))
            //     return true;
        }
        else if (objectType == "host")
        {
            // Hide offline hosts (TODO: make this configurable)
            QSharedPointer<Host> host = cache->ResolveObject<Host>(XenObjectType::Host, objectRef);
            if (host && !host->IsLive())
                return false; // Don't hide offline hosts by default - let them show
        }
        else if (objectType == "folder")
        {
            // Hide root folder
            bool isRootFolder = objectData.value("isRootFolder").toBool();
            return isRootFolder;
        }

        return false;
    }

    int Group::Compare(const QVariant& one, const QVariant& other, Search* search)
    {
        if (!one.isValid() && !other.isValid())
            return 0;
        if (!one.isValid())
            return -1;
        if (!other.isValid())
            return 1;

        // Check if both are object data maps (IXenObject equivalent)
        bool oneIsObject = one.canConvert<QVariantMap>();
        bool otherIsObject = other.canConvert<QVariantMap>();

        if (!oneIsObject && !otherIsObject)
        {
            // Neither is an object => compare as generic values

            // Try sorting if available
            // TODO: Sorting requires XenObject pointers, not QVariants
            // if (search && !search->GetSorting().isEmpty())
            // {
            //     QList<Sort> sorting = search->GetSorting();
            //     for (int i = 0; i < sorting.size(); ++i)
            //     {
            //         int r = sorting[i].Compare(one, other);
            //         if (r != 0)
            //             return r;
            //     }
            // }

            // Natural string comparison
            return QString::compare(one.toString(), other.toString(), Qt::CaseInsensitive);
        }

        if (!oneIsObject)
            return -1;
        if (!otherIsObject)
            return 1;

        // Both are objects
        QVariantMap oneData = one.toMap();
        QVariantMap otherData = other.toMap();

        // Try sorting if available
        // TODO: Sorting requires XenObject pointers, not QVariants with maps
        // if (search && !search->GetSorting().isEmpty())
        // {
        //     QList<Sort> sorting = search->GetSorting();
        //     for (int i = 0; i < sorting.size(); ++i)
        //     {
        //         int r2 = sorting[i].Compare(one, other);
        //         if (r2 != 0)
        //             return r2;
        //     }
        // }

        // Compare by type
        int r3 = CompareByType(oneData, otherData);
        if (r3 != 0)
            return r3;

        // Compare by name
        QString oneName = oneData.value("name_label").toString();
        QString otherName = otherData.value("name_label").toString();
        return QString::compare(oneName, otherName, Qt::CaseInsensitive);
    }

    int Group::CompareByType(const QVariantMap& oneData, const QVariantMap& otherData)
    {
        QString objectType1 = oneData.value("__type").toString();
        QString objectType2 = otherData.value("__type").toString();

        QString t1 = TypeOf(objectType1, oneData);
        QString t2 = TypeOf(objectType2, otherData);
        return t1.compare(t2);
    }

    QString Group::TypeOf(const QString& objectType, const QVariantMap& objectData)
    {
        if (objectType == "folder")
            return "10";
        if (objectType == "pool")
            return "20";
        if (objectType == "host")
            return "30";
        if (objectType == "vm")
        {
            // Check if real VM (not template, not snapshot)
            bool isTemplate = objectData.value("is_a_template").toBool();
            bool isSnapshot = objectData.value("is_a_snapshot").toBool();
            if (!isTemplate && !isSnapshot)
                return "40";
        }
        return objectType; // Other types sorted alphabetically
    }

    void Group::FilterAdd(Query* query, const QString& objectType, const QString& objectRef, const QVariantMap& objectData)
    {
        if (query && !query->match(objectData, objectType, nullptr))
            return;

        this->Add(objectType, objectRef, objectData);
    }

    bool Group::Populate(IAcceptGroups* adapter)
    {
        return this->Populate(adapter, 0, true);
    }

    int Group::Compare(const QVariant& one, const QVariant& other) const
    {
        return Compare(one, other, this->search_);
    }

    int Group::CompareGroupKeys(const GroupKey& one, const GroupKey& other) const
    {
        return Compare(one.GetKey(), other.GetKey());
    }

    // ========================================================================
    // AbstractNodeGroup implementation
    // ========================================================================

    AbstractNodeGroup::AbstractNodeGroup(Search* search, Grouping* grouping)
        : Group(search), grouping_(grouping)
    {
    }

    bool AbstractNodeGroup::Populate(IAcceptGroups* adapter)
    {
        return this->Populate(adapter, 0, false);
    }

    bool AbstractNodeGroup::Populate(IAcceptGroups* adapter, int indent, bool defaultExpand)
    {
        bool added = false;

        QList<GroupKey> groups;
        this->GetNextLevel(groups);

        // Sort groups
        std::sort(groups.begin(), groups.end(), [this](const GroupKey& a, const GroupKey& b) {
            return this->CompareGroupKeys(a, b) < 0;
        });

        foreach (const GroupKey& group, groups)
        {
            IAcceptGroups* subAdapter = adapter->Add(group.GetGrouping(), group.GetKey(), 
                                                      QString(), QVariantMap(), indent, nullptr);

            if (subAdapter == nullptr)
                continue;

            added = true;

            this->PopulateFor(subAdapter, group, indent + 1, defaultExpand);
        }

        adapter->FinishedInThisGroup(defaultExpand);

        return added;
    }

    void AbstractNodeGroup::PopulateFor(IAcceptGroups* adapter, const GroupKey& group, int indent, bool defaultExpand)
    {
        if (this->grouped_.contains(group))
        {
            this->grouped_.value(group)->Populate(adapter, indent, defaultExpand);
        }
        else if (this->ungrouped_)
        {
            this->ungrouped_->PopulateFor(adapter, group, indent, defaultExpand);
        }
    }

    void AbstractNodeGroup::GetNextLevel(QList<GroupKey>& nextLevel)
    {
        nextLevel.append(this->grouped_.keys());

        if (this->ungrouped_)
            this->ungrouped_->GetNextLevel(nextLevel);
    }

    QSharedPointer<Group> AbstractNodeGroup::FindOrAddSubgroup(Grouping* grouping, const QVariant& o, Grouping* subgrouping)
    {
        GroupKey key(grouping, o);
        if (!this->grouped_.contains(key))
            this->grouped_[key] = GetGroupFor(grouping, this->search_, subgrouping);
        return this->grouped_[key];
    }

    // ========================================================================
    // NodeGroup implementation
    // ========================================================================

    NodeGroup::NodeGroup(Search* search, Grouping* grouping)
        : AbstractNodeGroup(search, grouping)
    {
    }

    void NodeGroup::Add(const QString& objectType, const QString& objectRef, const QVariantMap& objectData)
    {
        if (this->grouping_->belongsAsGroupNotMember(objectData, objectType))
        {
            GroupKey key(this->grouping_, QVariant(objectRef));
            if (!this->grouped_.contains(key))
                this->grouped_[key] = GetGroupFor(this->grouping_, this->search_, this->grouping_->getSubgrouping(QVariant()));

            return;
        }

        QVariant group = this->grouping_->getGroup(objectData, objectType);

        if (!group.isValid())
        {
            this->AddUngrouped(objectType, objectRef, objectData);
            return;
        }

        // Check if it's a list
        if (group.canConvert<QVariantList>())
        {
            QVariantList groups = group.toList();

            if (groups.isEmpty())
            {
                this->AddUngrouped(objectType, objectRef, objectData);
                return;
            }

            foreach (const QVariant& g, groups)
            {
                if (!g.isValid())
                {
                    this->AddUngrouped(objectType, objectRef, objectData);
                    continue;
                }

                this->AddGrouped(objectType, objectRef, objectData, g);
            }
        }
        else
        {
            this->AddGrouped(objectType, objectRef, objectData, group);
        }
    }

    void NodeGroup::AddGrouped(const QString& objectType, const QString& objectRef, const QVariantMap& objectData, const QVariant& group)
    {
        // Check if group is a searchable object and if it's hidden
        if (group.canConvert<QVariantMap>())
        {
            QVariantMap groupData = group.toMap();
            QString groupRef = groupData.value("opaque_ref").toString();
            // TODO: Check if hidden
            // if (XenAdminConfigManager::Provider::ObjectIsHidden(groupRef))
            //     return;
        }

        // Apply filter to group if needed
        if (this->search_->GetQuery() && group.canConvert<QVariantMap>())
        {
            // TODO: Implement GetRelevantGroupQuery when needed
            // QueryFilter* subquery = this->grouping_->GetRelevantGroupQuery(this->search_);
            // QVariantMap groupData = group.toMap();
            // QString groupType = groupData.value("__type").toString();
            // if (subquery && !subquery->match(groupType, groupData))
            //     return;
        }

        QSharedPointer<Group> nextGroup;

        // Handle array groups (multi-level grouping)
        if (group.canConvert<QVariantList>())
        {
            QVariantList groupArray = group.toList();
            // Start with current group - we don't have shared_from_this, so use FindOrAddSubgroup
            nextGroup = QSharedPointer<Group>(this, [](Group*){});
            for (int i = 0; i < groupArray.size(); ++i)
            {
                Grouping* gr = (i == groupArray.size() - 1) ? this->grouping_->getSubgrouping(QVariant()) : this->grouping_;
                AbstractNodeGroup* nodeGroup = dynamic_cast<AbstractNodeGroup*>(nextGroup.data());
                if (nodeGroup)
                    nextGroup = nodeGroup->FindOrAddSubgroup(this->grouping_, groupArray.at(i), gr);
            }
        }
        else
        {
            nextGroup = this->FindOrAddSubgroup(this->grouping_, group, this->grouping_->getSubgrouping(QVariant()));
        }

        if (nextGroup)
            nextGroup->Add(objectType, objectRef, objectData);
    }

    void NodeGroup::AddUngrouped(const QString& objectType, const QString& objectRef, const QVariantMap& objectData)
    {
        // TODO: Check if hidden
        // if (XenAdminConfigManager::Provider::ObjectIsHidden(objectRef))
        //     return;

        if (!this->ungrouped_)
            this->ungrouped_ = GetGroupFor(this->grouping_, this->search_, this->grouping_->getSubgrouping(QVariant()));

        this->ungrouped_->Add(objectType, objectRef, objectData);
    }

    // ========================================================================
    // FolderGroup implementation
    // ========================================================================

    FolderGroup::FolderGroup(Search* search, Grouping* grouping)
        : AbstractNodeGroup(search, grouping)
    {
    }

    void FolderGroup::Add(const QString& objectType, const QString& objectRef, const QVariantMap& objectData)
    {
        if (objectType == "folder")
        {
            GroupKey key(this->grouping_, QVariant(objectRef));
            if (!this->grouped_.contains(key))
                this->grouped_[key] = QSharedPointer<Group>(new FolderGroup(this->search_, this->grouping_));
        }
        else
        {
            if (!this->ungrouped_)
                this->ungrouped_ = QSharedPointer<Group>(new LeafGroup(this->search_));
            this->ungrouped_->Add(objectType, objectRef, objectData);
        }
    }

    // ========================================================================
    // LeafGroup implementation
    // ========================================================================

    LeafGroup::LeafGroup(Search* search)
        : Group(search)
    {
    }

    void LeafGroup::Add(const QString& objectType, const QString& objectRef, const QVariantMap& objectData)
    {
        // Check for duplicate folders (one folder can appear on several connections)
        if (objectType == "folder")
        {
            for (const Item& item : this->items_)
            {
                if (item.objectRef == objectRef)
                    return; // Already added
            }
        }

        this->search_->SetItems(this->search_->GetItems() + 1);

        Item item;
        item.objectType = objectType;
        item.objectRef = objectRef;
        item.objectData = objectData;
        this->items_.append(item);
    }

    bool LeafGroup::Populate(IAcceptGroups* adapter, int indent, bool defaultExpand)
    {
        bool added = false;

        // Sort items
        QList<Item> sortedItems = this->items_;
        std::sort(sortedItems.begin(), sortedItems.end(), [this](const Item& a, const Item& b) {
            QVariant va = QVariant::fromValue(a.objectData);
            QVariant vb = QVariant::fromValue(b.objectData);
            return this->Compare(va, vb) < 0;
        });

        foreach (const Item& item, sortedItems)
        {
            IAcceptGroups* subAdapter = adapter->Add(nullptr, QVariant(item.objectRef), 
                                                      item.objectType, item.objectData, indent, nullptr);

            if (subAdapter != nullptr)
            {
                added = true;
                subAdapter->FinishedInThisGroup(defaultExpand);
            }
        }

        adapter->FinishedInThisGroup(defaultExpand);

        return added;
    }

    void LeafGroup::GetNextLevel(QList<GroupKey>& nextLevel)
    {
        foreach (const Item& item, this->items_)
            nextLevel.append(GroupKey(nullptr, QVariant(item.objectRef)));
    }

    void LeafGroup::PopulateFor(IAcceptGroups* adapter, const GroupKey& group, int indent, bool defaultExpand)
    {
        adapter->FinishedInThisGroup(defaultExpand);
    }

} // namespace XenSearch
