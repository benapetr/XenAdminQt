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

// query.h - Query combining scope and filter
// Port of C# xenadmin/XenModel/XenSearch/Query.cs
#ifndef QUERY_H
#define QUERY_H

#include "queryscope.h"
#include "queryfilter.h"
#include <QString>
#include <QVariantMap>

// Forward declaration
class XenLib;

/// <summary>
/// A query combining a scope (which object types) and a filter (which properties to match)
///
/// C# equivalent: xenadmin/XenModel/XenSearch/Query.cs
/// </summary>
class Query
{
public:
    /// <summary>
    /// Constructor with scope and optional filter
    ///
    /// C# equivalent: Query(QueryScope scope, QueryFilter filter)
    /// </summary>
    /// <param name="scope">The query scope (which object types to include). If null, defaults to AllExcFolders</param>
    /// <param name="filter">The query filter (which properties to match). Can be null for no filtering</param>
    Query(QueryScope* scope, QueryFilter* filter = nullptr);

    ~Query();

    /// <summary>
    /// Get the query scope
    ///
    /// C# equivalent: QueryScope property
    /// </summary>
    QueryScope* getQueryScope() const
    {
        return m_scope;
    }

    /// <summary>
    /// Get the query filter (may be null)
    ///
    /// C# equivalent: QueryFilter property
    /// </summary>
    QueryFilter* getQueryFilter() const
    {
        return m_filter;
    }

    /// <summary>
    /// Check if an object matches this query
    /// An object matches if:
    ///  1. Its type is in the scope
    ///  2. The filter matches (or there is no filter)
    ///
    /// C# equivalent: Match(IXenObject o)
    /// </summary>
    /// <param name="objectData">The object data</param>
    /// <param name="objectType">The object type</param>
    /// <param name="xenLib">XenLib instance for resolving references</param>
    /// <returns>true if object matches query, false otherwise</returns>
    bool match(const QVariantMap& objectData, const QString& objectType, XenLib* xenLib) const;

    /// <summary>
    /// Equality comparison
    ///
    /// C# equivalent: Equals(object obj)
    /// </summary>
    bool equals(const Query* other) const;

    /// <summary>
    /// Hash code for use in QHash
    ///
    /// C# equivalent: GetHashCode()
    /// </summary>
    uint hashCode() const;

private:
    QueryScope* m_scope;   // The query scope (which object types) - owned
    QueryFilter* m_filter; // The query filter (which properties) - owned, can be null
};

// Hash function for QHash support
inline uint qHash(const Query& query)
{
    return query.hashCode();
}

#endif // QUERY_H
