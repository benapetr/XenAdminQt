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

#include "xen/network/connection.h"
#include <QString>
#include <QVariantMap>

// Forward declaration
class XenConnection;

/**
 * @brief Base class for query filters that match objects based on properties
 *
 * C# equivalent: xenadmin/XenModel/XenSearch/QueryTypes.cs - abstract class QueryFilter
 *
 * NOTE: This is a simplified initial implementation.
 * C# has many filter types (StringPropertyQuery, BoolPropertyQuery, DatePropertyQuery, etc.)
 * For the overview panel, we only need NullQuery (no filtering).
 * More filter types can be added later as needed.
 */
class QueryFilter
{
    public:
        virtual ~QueryFilter()
        {}

        /**
         * @brief Check if an object matches this filter
         *
         * C# equivalent: Match(IXenObject o)
         *
         * @param objectData The object data
         * @param objectType The object type
         * @param xenLib XenLib instance for resolving references
         * @return QVariant true if Match, false if no Match, or invalid/Null if indeterminate
         */
        virtual QVariant Match(const QVariantMap& objectData, const QString& objectType, XenConnection* conn) const = 0;

        /**
         * @brief Equality comparison
         * @param other Other QueryFilter to compare
         * @return true if equal
         */
        virtual bool Equals(const QueryFilter* other) const = 0;

        /**
         * @brief Hash code for use in QHash
         * @return 32-bit hash value
         */
        virtual uint HashCode() const = 0;
};

// Hash function for QHash support
inline uint qHash(const QueryFilter& filter)
{
    return filter.HashCode();
}

/**
 * @brief A null filter that matches all objects (no filtering)
 *
 * This is used when clicking top-level grouping tags like "Servers" or "Templates"
 *
 * C# equivalent: DummyQuery in QueryTypes.cs (but DummyQuery returns null, NullQuery returns true)
 * Actually closer to the implicit "no filter" behavior in C# Search
 */
class NullQuery : public QueryFilter
{
    public:
        NullQuery()
        {}

        /**
         * @brief Always returns true (matches all objects)
         * @return QVariant true
         */
        QVariant Match(const QVariantMap& objectData, const QString& objectType, XenConnection* conn) const override;

        /**
         * @brief Equality comparison
         * @param other Other QueryFilter
         * @return true if other is also a NullQuery
         */
        bool Equals(const QueryFilter* other) const override;

        /**
         * @brief Hash code for use in QHash
         * @return 32-bit hash value
         */
        uint HashCode() const override;
};

/**
 * @brief Filter that matches objects by their type (e.g., "host", "vm", "sr")
 *
 * Used when clicking type grouping tags to show only objects of that type
 *
 * C# equivalent: EnumPropertyQuery<ObjectTypes> with property=PropertyNames.type
 */
class TypePropertyQuery : public QueryFilter
{
    public:
        /**
         * @brief Constructor
         * @param objectType The object type to match (e.g., "host", "vm", "sr")
         * @param equals If true, match objects that equal this type. If false, match objects that don't equal this type.
         */
        TypePropertyQuery(const QString& objectType, bool equals = true);

        /**
         * @brief Check if an object matches this type filter
         */
        QVariant Match(const QVariantMap& objectData, const QString& objectType, XenConnection* conn) const override;

        bool Equals(const QueryFilter* other) const override;

        uint HashCode() const override;

        /**
         * @brief Get the object type being filtered
         * @return Object type string
         */
        QString getObjectType() const
        {
            return m_objectType;
        }

        /**
         * @brief Get whether we're matching equals (true) or not-equals (false)
         * @return true if equals comparison, false if not-equals
         */
        bool getEquals() const
        {
            return m_equals;
        }

    private:
        QString m_objectType;
        bool m_equals;
};

#endif // QUERYFILTER_H
