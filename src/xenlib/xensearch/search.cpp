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
#include <QDebug>
#include <QMetaType>
#include <algorithm>
#include "search.h"
#include "queryscope.h"
#include "queryfilter.h"
#include "queries.h"
#include "grouping.h"
#include "xen/network/connectionsmanager.h"
#include "../xencache.h"
#include "../network/comparableaddress.h"
#include "../utils/misc.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/xenobject.h"

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

    QueryFilter* filter = grouping->getSubquery(parent, group);
    if (!filter)
        filter = new NullQuery();

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

    QueryFilter* filter = grouping->getSubquery(parent, group);
    if (!filter)
        filter = new NullQuery();

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

static QString getObjectUuid(XenConnection* conn, const QString& objType, const QString& objRef)
{
    if (!conn || !conn->GetCache() || objRef.isEmpty())
        return QString();

    QSharedPointer<XenObject> object = conn->GetCache()->ResolveObject(objType, objRef);
    if (!object)
        return QString();

    return object->OpaqueRef();
}

static QString getPoolUuid(XenConnection* conn)
{
    if (!conn || !conn->GetCache())
        return QString();

    const QSharedPointer<Pool> pool = conn->GetCache()->GetPoolOfOne();
    if (!pool || !pool->IsValid())
        return QString();

    return pool->GetUUID();
}

static QString getHostAncestorRef(XenConnection* conn, const QString& objType, const QString& objRef)
{
    if (!conn || !conn->GetCache() || objRef.isEmpty())
        return QString();

    XenCache* cache = conn->GetCache();
    const QString type = objType.toLower();

    if (type == "host")
        return objRef;

    if (type == "vm")
    {
        QSharedPointer<VM> vm = cache->ResolveObject<VM>(XenObjectType::VM, objRef);
        if (vm)
            return vm->GetHomeRef();
    }

    if (type == "sr")
    {
        QSharedPointer<SR> sr = cache->ResolveObject<SR>(XenObjectType::SR, objRef);
        if (sr)
            return sr->HomeRef();
    }

    if (type == "vdi")
    {
        const QVariantMap vdiData = cache->ResolveObjectData(XenObjectType::VDI, objRef);
        const QString srRef = vdiData.value("SR").toString();
        if (!srRef.isEmpty() && srRef != XENOBJECT_NULL)
        {
            QSharedPointer<SR> sr = cache->ResolveObject<SR>(XenObjectType::SR, srRef);
            if (sr)
                return sr->HomeRef();
        }
    }

    if (type == "network")
    {
        const QVariantMap networkData = cache->ResolveObjectData(XenObjectType::Network, objRef);
        const QVariantList pifRefs = networkData.value("PIFs").toList();
        for (const QVariant& pifRefVar : pifRefs)
        {
            const QString pifRef = pifRefVar.toString();
            if (pifRef.isEmpty() || pifRef == XENOBJECT_NULL)
                continue;

            const QVariantMap pifData = cache->ResolveObjectData(XenObjectType::PIF, pifRef);
            const QString hostRef = pifData.value("host").toString();
            if (!hostRef.isEmpty() && hostRef != XENOBJECT_NULL)
                return hostRef;
        }
    }

    // Standalone-host fallback: when there is exactly one host, use it.
    const QStringList hostRefs = cache->GetAllRefs(XenObjectType::Host);
    if (hostRefs.size() == 1)
        return hostRefs.first();

    return QString();
}

static bool isRealVmData(const QVariantMap& data)
{
    const bool isTemplate = data.value("is_a_template").toBool();
    const bool isSnapshot = data.value("is_a_snapshot").toBool();
    const bool isControlDomain = data.value("is_control_domain").toBool();
    return !isTemplate && !isSnapshot && !isControlDomain;
}

static QString typeSortKey(const QString& objectType, const QVariantMap& data)
{
    const QString type = objectType.toLower();
    if (type == "folder")
        return "10";
    if (type == "pool")
        return "20";
    if (type == "host")
        return "30";
    if (type == "vm" && isRealVmData(data))
        return "40";
    return type;
}

static int compareByTypeAndName(const QString& typeA, const QVariantMap& dataA, const QString& refA,
                                const QString& typeB, const QVariantMap& dataB, const QString& refB)
{
    const QString keyA = typeSortKey(typeA, dataA);
    const QString keyB = typeSortKey(typeB, dataB);
    const int typeCmp = keyA.compare(keyB);
    if (typeCmp != 0)
        return typeCmp;

    const QString nameA = dataA.value("name_label").toString();
    const QString nameB = dataB.value("name_label").toString();
    const int nameCmp = Misc::NaturalCompare(nameA, nameB);
    if (nameCmp != 0)
        return nameCmp;

    return Misc::NaturalCompare(refA, refB);
}

Search* Search::SearchFor(const QStringList& objectRefs, const QStringList& objectTypes, XenConnection* conn, QueryScope* scope)
{
    if (!scope)
        scope = GetOverviewScope();

    if (objectRefs.isEmpty())
    {
        return buildOverviewSearch(scope);
    } else if (objectRefs.count() == 1)
    {
        QString objRef = objectRefs.first();
        QString objType = objectTypes.isEmpty() ? QString() : objectTypes.first();

        if (objType == "host")
        {
            Grouping* hostGrouping = new HostGrouping(nullptr);
            const QString resolvedHostUuid = getObjectUuid(conn, "host", objRef);
            const QString hostUuid = resolvedHostUuid.isEmpty() ? objRef : resolvedHostUuid;
            QueryFilter* uuidQuery = new StringPropertyQuery(PropertyNames::uuid, hostUuid, StringPropertyQuery::MatchType::ExactMatch);
            QueryFilter* hostQuery = new RecursiveXMOListPropertyQuery(PropertyNames::host, uuidQuery);

            Query* query = new Query(scope, hostQuery);
            QString nameLabel;
            if (conn && conn->GetCache())
            {
                QSharedPointer<Host> host = conn->GetCache()->ResolveObject<Host>(XenObjectType::Host, objRef);
                if (host && host->IsValid())
                    nameLabel = host->GetName();
            }
            QString name = QString("Host: %1").arg(nameLabel);
            return new Search(query, hostGrouping, name, "", false);
        } else if (objType == "pool")
        {
            Grouping* hostGrouping = new HostGrouping(nullptr);
            Grouping* poolGrouping = new PoolGrouping(hostGrouping);

            const QString resolvedPoolUuid = getObjectUuid(conn, "pool", objRef);
            const QString poolUuid = resolvedPoolUuid.isEmpty() ? objRef : resolvedPoolUuid;
            QueryFilter* uuidQuery = new StringPropertyQuery(PropertyNames::uuid, poolUuid, StringPropertyQuery::MatchType::ExactMatch);
            QueryFilter* poolQuery = new RecursiveXMOPropertyQuery(PropertyNames::pool, uuidQuery);

            Query* query = new Query(scope, poolQuery);
            QString nameLabel;
            if (conn && conn->GetCache())
            {
                QSharedPointer<Pool> pool = conn->GetCache()->GetPoolOfOne();
                if (pool && pool->IsValid())
                    nameLabel = pool->GetName();
            }
            QString name = QString("Pool: %1").arg(nameLabel);
            return new Search(query, poolGrouping, name, "", false);
        } else
        {
            return buildOverviewSearch(scope);
        }
    } else
    {
        bool containsHost = false;
        bool containsPool = false;
        QList<QueryFilter*> queryFilters;

        const QString poolUuid = getPoolUuid(conn);

        for (int i = 0; i < objectRefs.size(); ++i)
        {
            const QString ref = objectRefs.at(i);
            const QString type = (i < objectTypes.size()) ? objectTypes.at(i).toLower() : QString();

            if (!poolUuid.isEmpty())
            {
                containsPool = true;
                QueryFilter* uuidQuery = new StringPropertyQuery(PropertyNames::uuid, poolUuid, StringPropertyQuery::MatchType::ExactMatch);
                queryFilters.append(new RecursiveXMOPropertyQuery(PropertyNames::pool, uuidQuery));
                continue;
            }

            const QString hostRef = getHostAncestorRef(conn, type, ref);
            const QString resolvedHostUuid = getObjectUuid(conn, "host", hostRef);
            const QString hostUuid = resolvedHostUuid.isEmpty() ? hostRef : resolvedHostUuid;
            if (hostUuid.isEmpty())
                continue;

            containsHost = true;
            QueryFilter* uuidQuery = new StringPropertyQuery(PropertyNames::uuid, hostUuid, StringPropertyQuery::MatchType::ExactMatch);
            queryFilters.append(new RecursiveXMOListPropertyQuery(PropertyNames::host, uuidQuery));
        }

        Grouping* grouping = nullptr;
        if (containsPool)
        {
            Grouping* hostGrouping = new HostGrouping(nullptr);
            grouping = new PoolGrouping(hostGrouping);
        } else if (containsHost)
        {
            grouping = new HostGrouping(nullptr);
        }

        QueryFilter* filter = queryFilters.isEmpty() ? nullptr : static_cast<QueryFilter*>(new GroupQuery(GroupQuery::GroupQueryType::Or, queryFilters));
        Query* query = new Query(scope, filter);
        return new Search(query, grouping, "Overview", "", false);
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

Search* Search::SearchForTag(const QString& tag)
{
    Query* query = new Query(nullptr, new TagQuery(tag, false));
    return new Search(query, nullptr, QString("Objects with tag '%1'").arg(tag), "", false);
}

Search* Search::SearchForFolder(const QString& path)
{
    QueryScope* scope = new QueryScope(ObjectTypes::AllIncFolders);
    QueryFilter* innerFilter = new StringPropertyQuery(PropertyNames::uuid, path, StringPropertyQuery::MatchType::ExactMatch);
    QueryFilter* filter = new RecursiveXMOPropertyQuery(PropertyNames::folder, innerFilter);
    Query* query = new Query(scope, filter);
    Grouping* grouping = new FolderGrouping(nullptr);

    const QStringList pathParts = path.split('/', Qt::SkipEmptyParts);
    const QString name = pathParts.isEmpty() ? QStringLiteral("Folders") : pathParts.last();
    return new Search(query, grouping, name, "", false);
}

Search* Search::SearchForAllFolders()
{
    Query* query = new Query(new QueryScope(ObjectTypes::Folder), nullptr);
    Grouping* grouping = new FolderGrouping(nullptr);
    QList<Sort> sorts;
    sorts.append(Sort("name", true));
    return new Search(query, grouping, "", "", false, QList<QPair<QString, int>>(), sorts);
}

Search* Search::SearchForTags()
{
    Query* query = new Query(new QueryScope(ObjectTypes::AllIncFolders), new ListEmptyQuery(PropertyNames::tags, false));
    return new Search(query, nullptr, "", "", false);
}

Search* Search::SearchForFolders()
{
    Query* query = new Query(new QueryScope(ObjectTypes::AllIncFolders), new NullPropertyQuery(PropertyNames::folder, false));
    return new Search(query, nullptr, "", "", false);
}

Search* Search::SearchForCustomFields()
{
    Query* query = new Query(new QueryScope(ObjectTypes::AllIncFolders), new BoolQuery(PropertyNames::has_custom_fields, true));
    return new Search(query, nullptr, "", "", false);
}

Search* Search::SearchForVapps()
{
    Query* query = new Query(new QueryScope(ObjectTypes::AllIncFolders), new BoolQuery(PropertyNames::in_any_appliance, true));
    return new Search(query, nullptr, "", "", false);
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
            if (VAppGrouping* vappGrouping = dynamic_cast<VAppGrouping*>(grouping))
                vappGrouping->SetConnection(connection);
            grouping = grouping->getSubgrouping(QVariant());
        }
    };

    int totalItems = 0;
    bool addedAny = false;

    for (XenConnection* connection : connections)
    {
        if (!connection)
            continue;

        QList<QPair<XenObjectType, QString>> matchedObjects;
        XenCache* cache = connection->GetCache();
        const QString hostname = connection->GetHostname();
        const QString hostRef = (hostname.isEmpty() || connection->GetPort() == 443)
            ? hostname
            : QString("%1:%2").arg(hostname).arg(connection->GetPort());

        if (connection->IsConnected() && cache && cache->Count(XenObjectType::Host) > 0)
        {
            QList<QSharedPointer<Host>> hosts = cache->GetAll<Host>(XenObjectType::Host);
            foreach (QSharedPointer<Host> host, hosts)
            {
                const bool isOpaqueRef = host->OpaqueRef().startsWith("OpaqueRef:");
                const bool isDisconnected = !host->IsConnected();

                if (!isOpaqueRef || isDisconnected)
                    cache->Remove(XenObjectType::Host, host->OpaqueRef());
            }
        }

        if (connection->IsConnected() && cache && cache->Count(XenObjectType::Pool) > 0)
        {
            matchedObjects = this->getMatchedObjects(connection);
        } else
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

            const QVariantMap existing = cache->ResolveObjectData(XenObjectType::Host, hostRef);
            if (existing.isEmpty() || existing != record)
                cache->Update(XenObjectType::Host, hostRef, record);

            if (!this->m_query || this->m_query->match(record, "host", connection))
                matchedObjects.append(qMakePair(XenObjectType::Host, hostRef));
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
                    XenObjectType objType = objPair.first;
                    QString objRef = objPair.second;
                    const QString objTypeName = XenObject::TypeToString(objType);
                    QVariantMap objectData = connection->GetCache()->ResolveObjectData(objTypeName, objRef);

                    IAcceptGroups* child = adapter->Add(nullptr, objRef, objTypeName, objectData, 0, connection);
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

QList<QPair<XenObjectType, QString>> Search::getMatchedObjects(XenConnection* connection) const
{
    // Get all objects from cache that match the query scope and filter
    QList<QPair<XenObjectType, QString>> matchedObjects;

    //if (!connection || !connection->GetCache())
    //    return matchedObjects;

    QueryScope* scope = this->m_query->getQueryScope();
    QueryFilter* filter = this->m_query->getQueryFilter();
    ObjectTypes types = scope->GetObjectTypes();

    // Get all objects from cache
    QList<QPair<XenObjectType, QString>> allCached = connection->GetCache()->GetXenSearchableObjects();

    for (const auto& pair : allCached)
    {
        XenObjectType objType = pair.first;
        QString objRef = pair.second;

        // Check if object type is in scope
        bool typeMatches = false;
        if (objType == XenObjectType::Pool && (types & ObjectTypes::Pool) != ObjectTypes::None)
        {
            typeMatches = true;
        } else if (objType == XenObjectType::Host && (types & ObjectTypes::Server) != ObjectTypes::None)
        {
            typeMatches = true;
        } else if (objType == XenObjectType::VM)
        {
            QSharedPointer<VM> vm = connection->GetCache()->ResolveObject<VM>(objRef);
            if (!vm || !vm->IsValid())
                continue;

            if (vm->IsControlDomain())
                continue;

            bool isTemplate = vm->IsTemplate();
            bool isSnapshot = vm->IsSnapshot();
            bool isDefaultTemplate = vm->DefaultTemplate();

            if (!isTemplate && !isSnapshot && (types & ObjectTypes::VM) != ObjectTypes::None)
                typeMatches = true;
            else if (isTemplate)
            {
                if (isDefaultTemplate && (types & ObjectTypes::DefaultTemplate) != ObjectTypes::None)
                    typeMatches = true;
                else if (!isDefaultTemplate && (types & ObjectTypes::UserTemplate) != ObjectTypes::None)
                    typeMatches = true;
            }
            else if (isSnapshot && (types & ObjectTypes::Snapshot) != ObjectTypes::None)
            {
                typeMatches = true;
            }
        } else if (objType == XenObjectType::SR)
        {
            if ((types & ObjectTypes::RemoteSR) != ObjectTypes::None || (types & ObjectTypes::LocalSR) != ObjectTypes::None)
            {
                QSharedPointer<SR> sr = connection->GetCache()->ResolveObject<SR>(objRef);
                if (!sr || !sr->IsValid())
                    continue;

                const QString srType = sr->GetType();
                const bool isLocal = !sr->IsShared() || srType == "lvm" || srType == "udev" || srType == "iso";
                if (isLocal && (types & ObjectTypes::LocalSR) != ObjectTypes::None)
                    typeMatches = true;
                else if (!isLocal && (types & ObjectTypes::RemoteSR) != ObjectTypes::None)
                    typeMatches = true;
            }
        } else if (objType == XenObjectType::Network && (types & ObjectTypes::Network) != ObjectTypes::None)
        {
            typeMatches = true;
        } else if (objType == XenObjectType::VDI && (types & ObjectTypes::VDI) != ObjectTypes::None)
        {
            typeMatches = true;
        } else if (objType == XenObjectType::Folder && (types & ObjectTypes::Folder) != ObjectTypes::None)
        {
            typeMatches = true;
        } else if (objType == XenObjectType::VMAppliance && (types & ObjectTypes::Appliance) != ObjectTypes::None)
        {
            typeMatches = true;
        } else if (objType == XenObjectType::DockerContainer && (types & ObjectTypes::DockerContainer) != ObjectTypes::None)
        {
            typeMatches = true;
        }

        if (!typeMatches)
            continue;

        // Apply query filter
        if (filter)
        {
            const QString objTypeName = XenObject::TypeToString(objType);
            QVariantMap objectData = connection->GetCache()->ResolveObjectData(objTypeName, objRef);
            QVariant matchResult = filter->Match(objectData, objTypeName, connection);
            if (!matchResult.toBool())
                continue;
        }

        matchedObjects.append(pair);
    }

    return matchedObjects;
}

bool Search::populateGroupedObjects(IAcceptGroups* adapter, Grouping* grouping, 
                                    const QList<QPair<XenObjectType, QString>>& objects,
                                    int indent, XenConnection* conn)
{
    // Group objects by the grouping algorithm
    // C# equivalent: Group.Populate(IAcceptGroups adapter) in GroupAlg.cs

    if (!grouping || objects.isEmpty())
        return false;

    // Organize objects by group value (use string keys for QHash)
    QHash<QString, QList<QPair<XenObjectType, QString>>> groupedObjects;
    QHash<QString, QVariant> groupValueLookup; // Map string key back to original group value
    QList<QPair<XenObjectType, QString>> ungroupedObjects;

    for (const auto& objPair : objects)
    {
        XenObjectType objType = objPair.first;
        QString objRef = objPair.second;
        const QString objTypeName = XenObject::TypeToString(objType);
        QVariantMap objectData = conn->GetCache()->ResolveObjectData(objTypeName, objRef);

        // Get group value for this object
        QVariant groupValue = grouping->getGroup(objectData, objTypeName);

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
        } else if (groupValue.canConvert<QStringList>())
        {
            const QStringList values = groupValue.toStringList();
            for (const QString& value : values)
            {
                if (!value.isEmpty())
                    groupValues.append(value);
            }
        } else
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

    XenObjectType groupObjectType = XenObjectType::Null;
    if (dynamic_cast<PoolGrouping*>(grouping))
        groupObjectType = XenObjectType::Pool;
    else if (dynamic_cast<HostGrouping*>(grouping))
        groupObjectType = XenObjectType::Host;
    else if (dynamic_cast<FolderGrouping*>(grouping))
        groupObjectType = XenObjectType::Folder;
    else if (dynamic_cast<VAppGrouping*>(grouping))
        groupObjectType = XenObjectType::VMAppliance;

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

        const QVariant valueA = groupValueLookup.value(a);
        const QVariant valueB = groupValueLookup.value(b);

        if (groupObjectType != XenObjectType::Null)
        {
            QSharedPointer<XenObject> objA = conn->GetCache()->ResolveObject(groupObjectType, valueA.toString());
            QSharedPointer<XenObject> objB = conn->GetCache()->ResolveObject(groupObjectType, valueB.toString());
            QString nameA = objA ? objA->GetName() : QString();
            QString nameB = objB ? objB->GetName() : QString();

            if (!nameA.isEmpty() || !nameB.isEmpty())
            {
                if (nameA.isEmpty())
                    return false;
                if (nameB.isEmpty())
                    return true;
                return Misc::NaturalCompare(nameA, nameB) < 0;
            }
        }

        return Misc::NaturalCompare(a, b) < 0;
    });

    for (const QString& groupKey : groupKeys)
    {
        QVariant groupValue = groupValueLookup.value(groupKey); // Retrieve original group value
        QList<QPair<XenObjectType, QString>> groupObjects = groupedObjects.value(groupKey);

        IAcceptGroups* childAdapter = nullptr;
        if (groupObjectType != XenObjectType::Null)
        {
            const QString groupObjectTypeName = XenObject::TypeToString(groupObjectType);
            QVariantMap groupObjectData = conn->GetCache()->ResolveObjectData(groupObjectTypeName, groupValue.toString());
            if (!groupObjectData.isEmpty() && grouping->belongsAsGroupNotMember(groupObjectData, groupObjectTypeName))
            {
                childAdapter = adapter->Add(grouping, groupValue, groupObjectTypeName, groupObjectData, indent, conn);
                groupObjects.erase(std::remove_if(groupObjects.begin(), groupObjects.end(),
                    [&](const QPair<XenObjectType, QString>& objPair) {
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
            std::sort(groupObjects.begin(), groupObjects.end(), [&](const QPair<XenObjectType, QString>& a,
                                                                   const QPair<XenObjectType, QString>& b) {
                const QString typeA = XenObject::TypeToString(a.first);
                const QString typeB = XenObject::TypeToString(b.first);
                QVariantMap dataA = conn->GetCache()->ResolveObjectData(typeA, a.second);
                QVariantMap dataB = conn->GetCache()->ResolveObjectData(typeB, b.second);
                dataA["__type"] = typeA;
                dataB["__type"] = typeB;
                return compareByTypeAndName(typeA, dataA, a.second, typeB, dataB, b.second) < 0;
            });

            for (const auto& objPair : groupObjects)
            {
                XenObjectType objType = objPair.first;
                QString objRef = objPair.second;
                const QString objTypeName = XenObject::TypeToString(objType);
                QVariantMap objectData = conn->GetCache()->ResolveObjectData(objTypeName, objRef);

                IAcceptGroups* objAdapter = childAdapter->Add(nullptr, objRef, objTypeName, objectData, indent + 1, conn);
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
            std::sort(ungroupedObjects.begin(), ungroupedObjects.end(),
                [&](const QPair<XenObjectType, QString>& a, const QPair<XenObjectType, QString>& b) {
                    const QString typeA = XenObject::TypeToString(a.first);
                    const QString typeB = XenObject::TypeToString(b.first);
                    QVariantMap dataA = conn->GetCache()->ResolveObjectData(typeA, a.second);
                    QVariantMap dataB = conn->GetCache()->ResolveObjectData(typeB, b.second);
                    dataA["__type"] = typeA;
                    dataB["__type"] = typeB;
                    return compareByTypeAndName(typeA, dataA, a.second, typeB, dataB, b.second) < 0;
                });

            for (const auto& objPair : ungroupedObjects)
            {
                XenObjectType objType = objPair.first;
                QString objRef = objPair.second;
                const QString objTypeName = XenObject::TypeToString(objType);
                QVariantMap objectData = conn->GetCache()->ResolveObjectData(objTypeName, objRef);

                IAcceptGroups* objAdapter = adapter->Add(nullptr, objRef, objTypeName, objectData, indent, conn);
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
