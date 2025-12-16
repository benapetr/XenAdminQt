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

// queryfilter.h - Query filter for searching/filtering objects
// Port of C# xenadmin/XenModel/XenSearch/QueryTypes.cs (simplified version for overview panel)
#ifndef QUERYFILTER_H
#define QUERYFILTER_H

#include <QString>
#include <QVariantMap>

// Forward declaration
class XenLib;

/// <summary>
/// Base class for query filters that match objects based on properties
///
/// C# equivalent: xenadmin/XenModel/XenSearch/QueryTypes.cs - abstract class QueryFilter
///
/// NOTE: This is a simplified initial implementation.
/// C# has many filter types (StringPropertyQuery, BoolPropertyQuery, DatePropertyQuery, etc.)
/// For the overview panel, we only need NullQuery (no filtering).
/// More filter types can be added later as needed.
/// </summary>
class QueryFilter
{
public:
    virtual ~QueryFilter()
    {}

    /// <summary>
    /// Check if an object matches this filter
    ///
    /// C# equivalent: Match(IXenObject o)
    /// </summary>
    /// <param name="objectData">The object data</param>
    /// <param name="objectType">The object type</param>
    /// <param name="xenLib">XenLib instance for resolving references</param>
    /// <returns>true if match, false if no match, null if indeterminate</returns>
    virtual QVariant match(const QVariantMap& objectData, const QString& objectType, XenLib* xenLib) const = 0;

    /// <summary>
    /// Equality comparison
    /// </summary>
    virtual bool equals(const QueryFilter* other) const = 0;

    /// <summary>
    /// Hash code for use in QHash
    /// </summary>
    virtual uint hashCode() const = 0;
};

// Hash function for QHash support
inline uint qHash(const QueryFilter& filter)
{
    return filter.hashCode();
}

/// <summary>
/// A null filter that matches all objects (no filtering)
/// This is used when clicking top-level grouping tags like "Servers" or "Templates"
///
/// C# equivalent: DummyQuery in QueryTypes.cs (but DummyQuery returns null, NullQuery returns true)
/// Actually closer to the implicit "no filter" behavior in C# Search
/// </summary>
class NullQuery : public QueryFilter
{
public:
    NullQuery()
    {}

    /// <summary>
    /// Always returns true (matches all objects)
    /// </summary>
    QVariant match(const QVariantMap& objectData, const QString& objectType, XenLib* xenLib) const override;

    bool equals(const QueryFilter* other) const override;

    uint hashCode() const override;
};

/// <summary>
/// Filter that matches objects by their type (e.g., "host", "vm", "sr")
/// Used when clicking type grouping tags to show only objects of that type
///
/// C# equivalent: EnumPropertyQuery<ObjectTypes> with property=PropertyNames.type
/// </summary>
class TypePropertyQuery : public QueryFilter
{
public:
    /// <summary>
    /// Constructor
    /// </summary>
    /// <param name="objectType">The object type to match (e.g., "host", "vm", "sr")</param>
    /// <param name="equals">If true, match objects that equal this type. If false, match objects that don't equal this type.</param>
    TypePropertyQuery(const QString& objectType, bool equals = true);

    /// <summary>
    /// Check if an object matches this type filter
    /// </summary>
    QVariant match(const QVariantMap& objectData, const QString& objectType, XenLib* xenLib) const override;

    bool equals(const QueryFilter* other) const override;

    uint hashCode() const override;

    /// <summary>
    /// Get the object type being filtered
    /// </summary>
    QString getObjectType() const
    {
        return m_objectType;
    }

    /// <summary>
    /// Get whether we're matching equals (true) or not-equals (false)
    /// </summary>
    bool getEquals() const
    {
        return m_equals;
    }

private:
    QString m_objectType;
    bool m_equals;
};

#endif // QUERYFILTER_H
