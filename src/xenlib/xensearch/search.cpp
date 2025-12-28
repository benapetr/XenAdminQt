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
#include <QDebug>

Search::Search(Query* query, Grouping* grouping, const QString& name, const QString& uuid, 
               bool defaultSearch, const QList<QPair<QString, int>>& columns, const QList<Sort>& sorting)
    : m_query(query), m_grouping(grouping), m_name(name), m_uuid(uuid), m_defaultSearch(defaultSearch), 
      m_connection(nullptr), m_items(0), m_columns(columns), m_sorting(sorting)
{
    // C# equivalent: Search constructor (lines 67-83 in Search.cs)

    // If query is null, create default query
    if (this->m_query == nullptr)
    {
        // C# code: this.query = new Query(null, null);
        this->m_query = new Query(nullptr, nullptr);
    }

    // grouping can be null (no grouping)
}

Search::~Search()
{
    // Clean up owned pointers
    delete this->m_query;
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
    //
    // For now, we don't have FolderForNavigator, so just return the grouping
    return this->m_grouping;
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
