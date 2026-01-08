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

// search.cpp - Implementation of Search
#include "search.h"
#include "queryscope.h"
#include "queryfilter.h"
#include "queries.h"
#include "grouping.h"
#include "xen/network/connectionsmanager.h"
#include "../xencache.h"
#include "../network/comparableaddress.h"
#include "../utils/misc.h"
#include <QDebug>
#include <QMetaType>

using namespace XenSearch;

Search::Search(Query* query, Grouping* grouping, const QString& name, const QString& uuid,
               bool defaultSearch, const QList<QPair<QString, int>>& columns, const QList<Sort>& sorting,
               bool ownsQuery, bool ownsGrouping)
    : m_query(query), m_grouping(grouping), m_ownsQuery(ownsQuery), m_ownsGrouping(ownsGrouping),
      m_name(name), m_uuid(uuid), m_defaultSearch(defaultSearch), m_connection(nullptr),
      m_items(0), m_columns(columns), m_sorting(sorting)
{
    // C# equivalent: Search constructor (lines 67-83 in Search.cs)

    // If query is null, create default query
    if (this->m_query == nullptr)
    {
        // C# code: this.query = new Query(null, null);
        this->m_query = new Query(nullptr, nullptr);
        this->m_ownsQuery = true;
    }

    // grouping can be null (no grouping)
}

Search::~Search()
{
    // Clean up owned pointers
    if (this->m_ownsQuery)
        delete this->m_query;
    if (this->m_ownsGrouping)
        delete this->m_grouping;
}

Grouping* Search::GetEffectiveGrouping() const
{
    // C# equivalent: EffectiveGrouping property (lines 120-126 in Search.cs)
    //
    // C# comment:
    // "The grouping we actually use internally. This is different because of CA-26708:
    //  if we show the folder navigator, we don't also show the ancestor folders in the
    //  main results, but we still pretend to the user that it's grouped by folder."
    //
    // C# code: return (FolderForNavigator == null ? Grouping : null);
    QString folder = this->GetFolderForNavigator();
    return (folder.isEmpty() ? this->m_grouping : nullptr);
}

QString Search::GetFolderForNavigator() const
{
    // C# equivalent: FolderForNavigator property (Search.cs lines 186-202)
    //
    // C# code:
    //   public string FolderForNavigator
    //   {
    //       get
    //       {
    //           if (Query == null || Query.QueryFilter == null)
    //               return null;
    //
    //           RecursiveXMOPropertyQuery<Folder> filter = Query.QueryFilter as RecursiveXMOPropertyQuery<Folder>;
    //           if (filter == null)
    //               return null;  // only show a folder for RecursiveXMOPropertyQuery<Folder>
    //
    //           StringPropertyQuery subFilter = filter.subQuery as StringPropertyQuery;
    //           if (subFilter == null || subFilter.property != PropertyNames.uuid)
    //               return null;  // also only show a folder if the subquery is "folder is"
    //
    //           return subFilter.query;
    //       }
    //   }

    if (!this->m_query || !this->m_query->getQueryFilter())
        return QString();

    // Check if the query filter is a RecursiveXMOPropertyQuery for the "folder" property
    RecursiveXMOPropertyQuery* recursiveQuery = dynamic_cast<RecursiveXMOPropertyQuery*>(this->m_query->getQueryFilter());
    if (!recursiveQuery)
        return QString();

    // Only show folder navigator for "folder" property (parent folder)
    if (recursiveQuery->getProperty() != PropertyNames::folder)
        return QString();

    // Check if the subquery is a StringPropertyQuery matching uuid
    StringPropertyQuery* stringQuery = dynamic_cast<StringPropertyQuery*>(recursiveQuery->getSubQuery());
    if (!stringQuery)
        return QString();

    if (stringQuery->getProperty() != PropertyNames::uuid)
        return QString();

    // Return the folder UUID/path from the subquery
    return stringQuery->getQuery();
}

Search* Search::SearchForNonVappGroup(Grouping* grouping, const QVariant& parent, const QVariant& group)
{
    // C# equivalent: SearchForNonVappGroup (lines 647-650 in Search.cs)
    //
    // C# code:
    //   public static Search SearchForNonVappGroup(Grouping grouping, object parent, object v)
    //   {
    //       return new Search(new Query(new QueryScope(ObjectTypes.AllExcFolders), grouping.GetSubquery(parent, v)),
    //                         grouping.GetSubgrouping(v), grouping.GetGroupName(v), "", false);
    //   }

    // Create query scope (all objects except folders)
    QueryScope* scope = new QueryScope(ObjectTypes::AllExcFolders);

    // Get subquery filter from grouping
    // For TypeGrouping, this returns TypePropertyQuery("host") when clicking "Servers"
    // For other groupings, this may return null (no filtering)
    // C#: grouping.GetSubquery(parent, v)
    QueryFilter* filter = grouping->getSubquery(parent, group);

    // If no filter provided, use NullQuery (matches all)
    if (!filter)
        filter = new NullQuery();

    // Create query
    Query* query = new Query(scope, filter);

    // Get subgrouping from grouping
    // In C#: grouping.GetSubgrouping(v)
    Grouping* subgrouping = grouping->getSubgrouping(group);

    // Get group name
    // In C#: grouping.GetGroupName(v)
    QString groupName = grouping->getGroupName(group);

    // Create and return search
    // Note: In C#, the second parameter to Search constructor is grouping.GetSubgrouping(v),
    // not the original grouping. We pass ownership of subgrouping to Search.
    return new Search(query, subgrouping, groupName, "", false);
}

Search* Search::SearchForFolderGroup(Grouping* grouping, const QVariant& parent, const QVariant& group)
{
    // C# equivalent: SearchForFolderGroup (lines 641-644 in Search.cs)
    //
    // C# code:
    //   public static Search SearchForFolderGroup(Grouping grouping, object parent, object v)
    //   {
    //       return new Search(new Query(new QueryScope(ObjectTypes.AllIncFolders), grouping.GetSubquery(parent, v)),
    //                         grouping.GetSubgrouping(v), grouping.GetGroupName(v), "", false);
    //   }

    // Create query scope (all objects including folders)
    QueryScope* scope = new QueryScope(ObjectTypes::AllIncFolders);

    // Get subquery from grouping (usually null)
    QueryFilter* filter = new NullQuery();

    // Create query
    Query* query = new Query(scope, filter);

    // Get subgrouping and group name
    Grouping* subgrouping = grouping->getSubgrouping(group);
    QString groupName = grouping->getGroupName(group);

    return new Search(query, subgrouping, groupName, "", false);
}

Search* Search::SearchForVappGroup(Grouping* grouping, const QVariant& parent, const QVariant& group)
{
    // C# equivalent: SearchForVappGroup (lines 652-655 in Search.cs)
    //
    // C# code:
    //   public static Search SearchForVappGroup(Grouping grouping, object parent, object v)
    //   {
    //       return new Search(new Query(new QueryScope(ObjectTypes.VM), grouping.GetSubquery(parent, v)),
    //                         grouping.GetSubgrouping(v), grouping.GetGroupName(v), "", false);
    //   }

    // Create query scope (VM objects only)
    QueryScope* scope = new QueryScope(ObjectTypes::VM);

    // Get subquery from grouping (usually null)
    QueryFilter* filter = new NullQuery();

    // Create query
    Query* query = new Query(scope, filter);

    // Get subgrouping and group name
    Grouping* subgrouping = grouping->getSubgrouping(group);
    QString groupName = grouping->getGroupName(group);

    return new Search(query, subgrouping, groupName, "", false);
}

Search* Search::SearchFor(const QStringList& objectRefs, const QStringList& objectTypes, XenConnection *conn)
{
    // C# equivalent: SearchFor(IXenObject value) and SearchFor(IEnumerable<IXenObject> objects)
    // Lines 465-472, 398-460 in Search.cs

    return SearchFor(objectRefs, objectTypes, conn, GetOverviewScope());
}

static Search* buildOverviewSearch(QueryScope* scopeToUse)
{
    Grouping* hostGrouping = new HostGrouping(nullptr);
    Grouping* poolGrouping = new PoolGrouping(hostGrouping);
    Query* query = new Query(scopeToUse, nullptr);
    return new Search(query, poolGrouping, "Overview", "", false);
}

Search* Search::SearchFor(const QStringList& objectRefs, const QStringList& objectTypes,
                          XenConnection* conn, QueryScope* scope)
{
    if (!scope)
        scope = GetOverviewScope();

    if (objectRefs.isEmpty())
    {
        return buildOverviewSearch(scope);
    }
    else if (objectRefs.count() == 1)
    {
        QString objRef = objectRefs.first();
        QString objType = objectTypes.isEmpty() ? QString() : objectTypes.first();

        QVariantMap objectData;
        if (!objType.isEmpty() && conn)
        {
            objectData = conn->GetCache()->ResolveObjectData(objType, objRef);
        }

        if (objType == "host")
        {
            Grouping* hostGrouping = new HostGrouping(nullptr);
            QueryFilter* uuidQuery = new StringPropertyQuery(PropertyNames::uuid, objRef,
                                                            StringPropertyQuery::MatchType::ExactMatch);
            QueryFilter* hostQuery = uuidQuery; // TODO: Implement RecursiveXMOListPropertyQuery

            Query* query = new Query(scope, hostQuery);
            QString name = QString("Host: %1").arg(objectData.value("name_label").toString());
            return new Search(query, hostGrouping, name, "", false);
        }
        else if (objType == "pool")
        {
            Grouping* hostGrouping = new HostGrouping(nullptr);
            Grouping* poolGrouping = new PoolGrouping(hostGrouping);

            QueryFilter* uuidQuery = new StringPropertyQuery(PropertyNames::uuid, objRef,
                                                            StringPropertyQuery::MatchType::ExactMatch);
            QueryFilter* poolQuery = uuidQuery; // TODO: Implement RecursiveXMOPropertyQuery

            Query* query = new Query(scope, poolQuery);
            QString name = QString("Pool: %1").arg(objectData.value("name_label").toString());
            return new Search(query, poolGrouping, name, "", false);
        }
        else
        {
            return buildOverviewSearch(scope);
        }
    }
    else
    {
        // TODO: Implement multi-object search with proper grouping
        return buildOverviewSearch(scope);
    }
}

Search* Search::SearchForAllTypes()
{
    // C# equivalent: SearchForAllTypes() - Line 606 in Search.cs
    //
    // C# code:
    //   public static Search SearchForAllTypes()
    //   {
    //       Query query = new Query(new QueryScope(ObjectTypes.AllExcFolders), null);
    //       return new Search(query, null, "", null, false);
    //   }

    // Default overview: Pool → Host grouping
    // Note: DockerVM grouping not yet implemented, using Host only
    Grouping* hostGrouping = new HostGrouping(nullptr);
    Grouping* poolGrouping = new PoolGrouping(hostGrouping);

    QueryScope* scope = GetOverviewScope();
    Query* query = new Query(scope, nullptr); // nullptr = NullQuery (match all)

    return new Search(query, poolGrouping, "Overview", "", false);
}

Search* Search::AddFullTextFilter(const QString& text) const
{
    if (text.isEmpty())
        return const_cast<Search*>(this);

    return AddFilter(FullQueryFor(text));
}

Search* Search::AddFilter(QueryFilter* addFilter) const
{
    QueryScope* scope = nullptr;
    if (this->m_query && this->m_query->getQueryScope())
        scope = new QueryScope(this->m_query->getQueryScope()->getObjectTypes());

    QueryFilter* filter = nullptr;
    if (!this->m_query || !this->m_query->getQueryFilter())
        filter = addFilter;
    else if (!addFilter)
        filter = this->m_query->getQueryFilter();
    else
        filter = new GroupQuery(GroupQuery::GroupQueryType::And, QList<QueryFilter*>() << this->m_query->getQueryFilter() << addFilter);

    return new Search(new Query(scope, filter), this->m_grouping, "", "", this->m_defaultSearch,
                      this->m_columns, this->m_sorting, true, false);
}

QueryFilter* Search::FullQueryFor(const QString& text)
{
    const QStringList parts = text.split(' ', Qt::SkipEmptyParts);
    QList<QueryFilter*> queries;

    for (const QString& part : parts)
    {
        if (part.isEmpty())
            continue;

        queries.append(new StringPropertyQuery(PropertyNames::label, part, StringPropertyQuery::MatchType::Contains));
        queries.append(new StringPropertyQuery(PropertyNames::description, part, StringPropertyQuery::MatchType::Contains));

        ComparableAddress address;
        if (ComparableAddress::TryParse(part, true, false, address))
        {
            queries.append(new IPAddressQuery(PropertyNames::ip_address, address.toString()));
        }
    }

    if (queries.isEmpty())
        queries.append(new StringPropertyQuery(PropertyNames::label, "", StringPropertyQuery::MatchType::Contains));

    return new GroupQuery(GroupQuery::GroupQueryType::Or, queries);
}

bool Search::PopulateAdapters(XenConnection* conn, const QList<IAcceptGroups*>& adapters)
{
    // C# equivalent: PopulateAdapters(params IAcceptGroups[] adapters) - Line 205 in Search.cs
    //
    // C# code:
    //   public bool PopulateAdapters(params IAcceptGroups[] adapters)
    //   {
    //       Group group = Group.GetGrouped(this);
    //       bool added = false;
    //       foreach (IAcceptGroups adapter in adapters)
    //           added |= group.Populate(adapter);
    //       return added;
    //   }
    //
    // This method:
    // 1. Filters all XenServer objects based on query scope & filter
    // 2. Groups filtered objects using the grouping algorithm
    // 3. Populates each adapter with the grouped hierarchy

    if (!this->m_query || !this->m_query->getQueryScope())
    {
        return false;
    }

    QList<XenConnection*> connections;
    Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
    if (connMgr)
        connections = connMgr->GetAllConnections();
    if (connections.isEmpty() && conn)
        connections.append(conn);

    if (connections.isEmpty())
        return false;

    auto setGroupingConnection = [](Grouping* grouping, XenConnection* connection) {
        while (grouping)
        {
            if (PoolGrouping* poolGrouping = dynamic_cast<PoolGrouping*>(grouping))
                poolGrouping->SetConnection(connection);
            if (HostGrouping* hostGrouping = dynamic_cast<HostGrouping*>(grouping))
                hostGrouping->SetConnection(connection);
            grouping = grouping->getSubgrouping(QVariant());
        }
    };

    int totalItems = 0;
    bool addedAny = false;

    for (XenConnection* connection : connections)
    {
        if (!connection)
            continue;

        QList<QPair<QString, QString>> matchedObjects;
        XenCache* cache = connection->GetCache();
        const QString hostname = connection->GetHostname();
        const QString hostRef = (hostname.isEmpty() || connection->GetPort() == 443)
            ? hostname
            : QString("%1:%2").arg(hostname).arg(connection->GetPort());

        if (connection->IsConnected() && cache && cache->Count("host") > 0)
        {
            const QStringList hostRefs = cache->GetAllRefs("host");
            for (const QString& ref : hostRefs)
            {
                const bool isOpaqueRef = ref.startsWith("OpaqueRef:");
                QVariantMap record = cache->ResolveObjectData("host", ref);
                const bool isDisconnected = record.value("is_disconnected").toBool();

                if (!isOpaqueRef || isDisconnected)
                    cache->Remove("host", ref);
            }
        }

        if (connection->IsConnected() && cache && cache->Count("pool") > 0)
        {
            matchedObjects = this->getMatchedObjects(connection);
        }
        else
        {
            if (hostname.isEmpty())
                continue;

            QVariantMap record;
            record["ref"] = hostRef;
            record["opaqueRef"] = hostRef;
            record["name_label"] = hostname;
            record["name_description"] = QString();
            record["hostname"] = hostname;
            record["address"] = hostname;
            record["enabled"] = false;
            record["is_disconnected"] = true;

            if (cache)
            {
                const QVariantMap existing = cache->ResolveObjectData("host", hostRef);
                if (existing.isEmpty() || existing != record)
                    cache->Update("host", hostRef, record);
            }

            if (!this->m_query || this->m_query->match(record, "host", connection))
                matchedObjects.append(qMakePair(QString("host"), hostRef));
        }

        if (matchedObjects.isEmpty())
            continue;

        totalItems += matchedObjects.count();
        setGroupingConnection(this->m_grouping, connection);

        if (!this->m_grouping)
        {
            for (IAcceptGroups* adapter : adapters)
            {
                for (const auto& objPair : matchedObjects)
                {
                    QString objType = objPair.first;
                    QString objRef = objPair.second;
                    QVariantMap objectData = connection->GetCache()->ResolveObjectData(objType, objRef);

                    IAcceptGroups* child = adapter->Add(nullptr, objRef, objType, objectData, 0, connection);
                    if (child)
                    {
                        child->FinishedInThisGroup(false);
                        addedAny = true;
                    }
                }
                adapter->FinishedInThisGroup(true);
            }
            continue;
        }

        for (IAcceptGroups* adapter : adapters)
        {
            addedAny |= this->populateGroupedObjects(adapter, this->m_grouping, matchedObjects, 0, connection);
            adapter->FinishedInThisGroup(true);
        }
    }

    this->m_items = totalItems;
    return addedAny;
}

QList<QPair<QString, QString>> Search::getMatchedObjects(XenConnection* connection) const
{
    // Get all objects from cache that match the query scope and filter
    QList<QPair<QString, QString>> matchedObjects;

    //if (!connection || !connection->GetCache())
    //    return matchedObjects;

    QueryScope* scope = this->m_query->getQueryScope();
    QueryFilter* filter = this->m_query->getQueryFilter();
    ObjectTypes types = scope->GetObjectTypes();

    // Get all objects from cache
    QList<QPair<QString, QString>> allCached = connection->GetCache()->GetXenSearchableObjects();

    for (const auto& pair : allCached)
    {
        QString objType = pair.first;
        QString objRef = pair.second;

        // Check if object type is in scope
        bool typeMatches = false;
        if (objType == "pool" && (types & ObjectTypes::Pool) != ObjectTypes::None)
            typeMatches = true;
        else if (objType == "host" && (types & ObjectTypes::Server) != ObjectTypes::None)
            typeMatches = true;
        else if (objType == "vm")
        {
            QVariantMap vmData = connection->GetCache()->ResolveObjectData(objType, objRef);
            bool isTemplate = vmData.value("is_a_template").toBool();
            bool isSnapshot = vmData.value("is_a_snapshot").toBool();
            bool isControlDomain = vmData.value("is_control_domain").toBool();

            if (isControlDomain)
                continue;

            if (!isTemplate && !isSnapshot && (types & ObjectTypes::VM) != ObjectTypes::None)
                typeMatches = true;
            else if (isTemplate && (types & ObjectTypes::UserTemplate) != ObjectTypes::None)
                typeMatches = true;
            else if (isSnapshot && (types & ObjectTypes::Snapshot) != ObjectTypes::None)
                typeMatches = true;
        }
        else if (objType == "sr" && ((types & ObjectTypes::RemoteSR) != ObjectTypes::None || 
                                      (types & ObjectTypes::LocalSR) != ObjectTypes::None))
            typeMatches = true;
        else if (objType == "network" && (types & ObjectTypes::Network) != ObjectTypes::None)
            typeMatches = true;

        if (!typeMatches)
            continue;

        // Apply query filter
        if (filter)
        {
            QVariantMap objectData = connection->GetCache()->ResolveObjectData(objType, objRef);
            QVariant matchResult = filter->Match(objectData, objType, connection);
            if (!matchResult.toBool())
                continue;
        }

        matchedObjects.append(pair);
    }

    return matchedObjects;
}

bool Search::populateGroupedObjects(IAcceptGroups* adapter, Grouping* grouping, 
                                    const QList<QPair<QString, QString>>& objects,
                                    int indent, XenConnection* conn)
{
    // Group objects by the grouping algorithm
    // C# equivalent: Group.Populate(IAcceptGroups adapter) in GroupAlg.cs

    if (!grouping || objects.isEmpty())
        return false;

    // Organize objects by group value (use string keys for QHash)
    QHash<QString, QList<QPair<QString, QString>>> groupedObjects;
    QHash<QString, QVariant> groupValueLookup; // Map string key back to original group value
    QList<QPair<QString, QString>> ungroupedObjects;

    for (const auto& objPair : objects)
    {
        QString objType = objPair.first;
        QString objRef = objPair.second;
        QVariantMap objectData = conn->GetCache()->ResolveObjectData(objType, objRef);

        // Get group value for this object
        QVariant groupValue = grouping->getGroup(objectData, objType);

        if (!groupValue.isValid())
        {
            ungroupedObjects.append(objPair);
            continue; // Object doesn't belong to any group
        }

        QList<QVariant> groupValues;
        bool isVariantList = false;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        isVariantList = (groupValue.typeId() == QMetaType::QVariantList);
#else
        isVariantList = (groupValue.type() == QVariant::List);
#endif

        if (isVariantList)
        {
            const QVariantList values = groupValue.toList();
            for (const QVariant& value : values)
            {
                if (value.isValid())
                    groupValues.append(value);
            }
        }
        else if (groupValue.canConvert<QStringList>())
        {
            const QStringList values = groupValue.toStringList();
            for (const QString& value : values)
            {
                if (!value.isEmpty())
                    groupValues.append(value);
            }
        }
        else
        {
            groupValues.append(groupValue);
        }

        if (groupValues.isEmpty())
        {
            ungroupedObjects.append(objPair);
            continue;
        }

        for (const QVariant& value : groupValues)
        {
            const QString groupKey = value.toString();
            if (groupKey.isEmpty())
                continue;
            groupedObjects[groupKey].append(objPair);
            groupValueLookup[groupKey] = value; // Store original value
        }
    }

    bool addedAny = false;
    //qDebug() << "Search::populateGroupedObjects"
    //         << (grouping ? grouping->getGroupingName() : QString())
    //         << "groups=" << groupedObjects.size()
    //         << "ungrouped=" << ungroupedObjects.size();

    QString groupObjectType;
    if (dynamic_cast<PoolGrouping*>(grouping))
        groupObjectType = "pool";
    else if (dynamic_cast<HostGrouping*>(grouping))
        groupObjectType = "host";

    auto groupSortKey = [&](const QString& key) -> int {
        if (dynamic_cast<TypeGrouping*>(grouping))
        {
            static const QHash<QString, int> order = {
                {"pool", 0},
                {"host", 1},
                {"disconnected_host", 2},
                {"vm", 3},
                {"snapshot", 4},
                {"template", 5},
                {"sr", 6},
                {"vdi", 7},
                {"network", 8},
                {"folder", 9},
                {"appliance", 10},
                {"dockercontainer", 11},
            };
            return order.value(key, 100);
        }
        return 0;
    };

    // Add each group
    QList<QString> groupKeys = groupedObjects.keys();
    std::sort(groupKeys.begin(), groupKeys.end(), [&](const QString& a, const QString& b) {
        if (dynamic_cast<TypeGrouping*>(grouping))
        {
            int orderA = groupSortKey(a);
            int orderB = groupSortKey(b);
            if (orderA != orderB)
                return orderA < orderB;
        }
        return Misc::NaturalCompare(a, b) < 0;
    });

    for (const QString& groupKey : groupKeys)
    {
        QVariant groupValue = groupValueLookup.value(groupKey); // Retrieve original group value
        QList<QPair<QString, QString>> groupObjects = groupedObjects.value(groupKey);

        IAcceptGroups* childAdapter = nullptr;
        if (!groupObjectType.isEmpty())
        {
            QVariantMap groupObjectData = conn->GetCache()->ResolveObjectData(groupObjectType, groupValue.toString());
            if (!groupObjectData.isEmpty() && grouping->belongsAsGroupNotMember(groupObjectData, groupObjectType))
            {
                childAdapter = adapter->Add(grouping, groupValue, groupObjectType, groupObjectData, indent, conn);
                groupObjects.erase(std::remove_if(groupObjects.begin(), groupObjects.end(),
                    [&](const QPair<QString, QString>& objPair) {
                        return objPair.first == groupObjectType && objPair.second == groupValue.toString();
                    }), groupObjects.end());
            }
        }

        if (!childAdapter)
        {
            // Add group node to adapter (empty objectType/objectData = group header)
            childAdapter = adapter->Add(grouping, groupValue, QString(), QVariantMap(), indent, conn);
        }
        
        if (!childAdapter)
            continue;

        addedAny = true;

        // Get subgrouping for this group
        Grouping* subgrouping = grouping->getSubgrouping(groupValue);

        if (subgrouping)
        {
            // Recursively populate subgroups
            this->populateGroupedObjects(childAdapter, subgrouping, groupObjects, indent + 1, conn);
        }
        else
        {
            // Leaf level - add objects directly
            for (const auto& objPair : groupObjects)
            {
                QString objType = objPair.first;
                QString objRef = objPair.second;
                QVariantMap objectData = conn->GetCache()->ResolveObjectData(objType, objRef);

                IAcceptGroups* objAdapter = childAdapter->Add(nullptr, objRef, objType, objectData, indent + 1, conn);
                if (objAdapter)
                    objAdapter->FinishedInThisGroup(false);
            }
        }

        // Expand groups at top 2 levels by default
        bool defaultExpand = (indent < 2);
        childAdapter->FinishedInThisGroup(defaultExpand);
    }

    if (!ungroupedObjects.isEmpty())
    {
        Grouping* subgrouping = grouping ? grouping->getSubgrouping(QVariant()) : nullptr;
        if (subgrouping)
        {
            this->populateGroupedObjects(adapter, subgrouping, ungroupedObjects, indent, conn);
        }
        else
        {
            for (const auto& objPair : ungroupedObjects)
            {
                QString objType = objPair.first;
                QString objRef = objPair.second;
                QVariantMap objectData = conn->GetCache()->ResolveObjectData(objType, objRef);

                IAcceptGroups* objAdapter = adapter->Add(nullptr, objRef, objType, objectData, indent, conn);
                if (objAdapter)
                    objAdapter->FinishedInThisGroup(false);
            }
        }
    }

    return addedAny;
}

ObjectTypes Search::DefaultObjectTypes()
{
    // C# equivalent: DefaultObjectTypes() - Line 520 in Search.cs
    //
    // C# code:
    //   public static ObjectTypes DefaultObjectTypes()
    //   {
    //       ObjectTypes types = ObjectTypes.DisconnectedServer | ObjectTypes.Server | ObjectTypes.VM |
    //                           ObjectTypes.RemoteSR | ObjectTypes.DockerContainer;
    //       return types;
    //   }

    ObjectTypes types = ObjectTypes::DisconnectedServer | ObjectTypes::Server | ObjectTypes::VM |
                        ObjectTypes::RemoteSR | ObjectTypes::DockerContainer;

    return types;
}

QueryScope* Search::GetOverviewScope()
{
    // C# equivalent: GetOverviewScope() - Line 527 in Search.cs
    //
    // C# code:
    //   internal static QueryScope GetOverviewScope()
    //   {
    //       ObjectTypes types = DefaultObjectTypes();
    //       // To avoid excessive number of options in the search-for drop-down,
    //       // the search panel doesn't respond to the options on the View menu.
    //       types |= ObjectTypes.UserTemplate;
    //       return new QueryScope(types);
    //   }

    ObjectTypes types = DefaultObjectTypes();

    // Include user templates (but not default templates)
    types = types | ObjectTypes::UserTemplate;

    return new QueryScope(types);
}
