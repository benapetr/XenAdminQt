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

// search.h - Search definition combining query, grouping, and metadata
// Port of C# xenadmin/XenModel/XenSearch/Search.cs (simplified for overview panel)
#ifndef SEARCH_H
#define SEARCH_H

#include "query.h"
#include "grouping.h"
#include <QString>
#include <QList>

// Forward declaration
class XenConnection;

/// <summary>
/// A search definition combining a query (what to match) with a grouping (how to organize results)
///
/// C# equivalent: xenadmin/XenModel/XenSearch/Search.cs
///
/// NOTE: This is a simplified initial implementation for the overview panel.
/// C# has additional features like columns, sorting, folder navigator, persistence, etc.
/// These can be added later as needed.
/// </summary>
class Search
{
public:
    /// <summary>
    /// Constructor with query, grouping, and name
    ///
    /// C# equivalent: Search(Query query, Grouping grouping, string name, string uuid, bool defaultSearch)
    /// </summary>
    /// <param name="query">The query (what objects to match). Takes ownership.</param>
    /// <param name="grouping">The grouping (how to organize results). Takes ownership.</param>
    /// <param name="name">The search name</param>
    /// <param name="uuid">Optional UUID for saved searches</param>
    /// <param name="defaultSearch">True if this is a built-in default search</param>
    Search(Query* query, Grouping* grouping, const QString& name,
           const QString& uuid = QString(), bool defaultSearch = false);

    ~Search();

    /// <summary>
    /// Get the query
    /// C# equivalent: Query property
    /// </summary>
    Query* getQuery() const
    {
        return m_query;
    }

    /// <summary>
    /// Get the grouping
    /// C# equivalent: Grouping property
    /// </summary>
    Grouping* getGrouping() const
    {
        return m_grouping;
    }

    /// <summary>
    /// Get the effective grouping (used internally)
    /// Different from Grouping when folder navigator is shown
    ///
    /// C# equivalent: EffectiveGrouping property
    /// </summary>
    Grouping* getEffectiveGrouping() const;

    /// <summary>
    /// Get the search name
    /// C# equivalent: Name property
    /// </summary>
    QString getName() const
    {
        return m_name;
    }

    /// <summary>
    /// Set the search name
    /// </summary>
    void setName(const QString& name)
    {
        m_name = name;
    }

    /// <summary>
    /// Get the search UUID (may be empty)
    /// C# equivalent: UUID property
    /// </summary>
    QString getUUID() const
    {
        return m_uuid;
    }

    /// <summary>
    /// Check if this is a default search
    /// C# equivalent: DefaultSearch property
    /// </summary>
    bool isDefaultSearch() const
    {
        return m_defaultSearch;
    }

    /// <summary>
    /// Get/Set the connection this search is associated with
    /// C# equivalent: Connection property
    /// </summary>
    XenConnection* getConnection() const
    {
        return m_connection;
    }
    void setConnection(XenConnection* connection)
    {
        m_connection = connection;
    }

    /// <summary>
    /// Get/Set the number of items matched by this search
    /// C# equivalent: Items property
    /// </summary>
    int getItems() const
    {
        return m_items;
    }
    void setItems(int items)
    {
        m_items = items;
    }

    /// <summary>
    /// Create a search for a non-vApp grouping tag
    /// This is the key method for overview panel - creates searches when clicking
    /// grouping nodes like "Servers", "Templates", etc.
    ///
    /// C# equivalent: SearchForNonVappGroup(Grouping grouping, object parent, object v)
    /// Line 647 in Search.cs
    /// </summary>
    /// <param name="grouping">The grouping algorithm</param>
    /// <param name="parent">Parent group value (may be null)</param>
    /// <param name="group">The group value</param>
    /// <returns>New Search instance configured for the group</returns>
    static Search* searchForNonVappGroup(Grouping* grouping, const QVariant& parent, const QVariant& group);

    /// <summary>
    /// Create a search for a folder grouping tag
    ///
    /// C# equivalent: SearchForFolderGroup(Grouping grouping, object parent, object v)
    /// Line 641 in Search.cs
    /// </summary>
    static Search* searchForFolderGroup(Grouping* grouping, const QVariant& parent, const QVariant& group);

    /// <summary>
    /// Create a search for a vApp grouping tag
    ///
    /// C# equivalent: SearchForVappGroup(Grouping grouping, object parent, object v)
    /// Line 652 in Search.cs
    /// </summary>
    static Search* searchForVappGroup(Grouping* grouping, const QVariant& parent, const QVariant& group);

private:
    Query* m_query;              // The query (what to match) - owned
    Grouping* m_grouping;        // The grouping (how to organize) - owned
    QString m_name;              // Search name
    QString m_uuid;              // UUID for saved searches
    bool m_defaultSearch;        // True if built-in default search
    XenConnection* m_connection; // Associated connection (not owned)
    int m_items;                 // Number of items matched
};

#endif // SEARCH_H
