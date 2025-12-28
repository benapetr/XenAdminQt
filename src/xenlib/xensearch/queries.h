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

#ifndef QUERIES_H
#define QUERIES_H

#include "queryfilter.h"
#include <QDateTime>
#include <QList>

// Property names enum (matching C# PropertyNames)
enum class PropertyNames
{
    // Basic properties (group 1)
    label,
    description,
    uuid,
    tags,
    type,
    
    // VM properties (group 3)
    power_state,
    virtualisation_status,
    os_name,
    ha_restart_priority,
    start_time,
    memory,
    read_caching_enabled,
    vendor_device_state,        // NEW: VM vendor device boolean
    in_any_appliance,           // NEW: VM appliance membership
    
    // Storage properties (group 4)
    size,
    shared,
    sr_type,
    
    // Pool properties (group 4)
    ha_enabled,
    isNotFullyUpgraded,         // NEW: Pool upgrade status
    
    // Network properties (group 3)
    ip_address,
    
    // Relationship properties (for future recursive queries)
    pool,                       // Parent pool reference
    host,                       // Parent/related host reference
    vm,                         // Related VM reference
    networks,                   // Network list
    storage,                    // Storage reference
    disks,                      // VDI list
    appliance,                  // VM_appliance reference
    folder,                     // Parent folder reference
    folders,                    // Ancestor folder list
    
    // Custom field support
    has_custom_fields           // Boolean for custom field presence
};

/**
 * Dummy query - used for "Select a filter..." placeholder
 */
class DummyQuery : public QueryFilter
{
    public:
        DummyQuery() {}

        QVariant Match(const QVariantMap& objectData, const QString& objectType, XenConnection *conn) const override;
        bool Equals(const QueryFilter* other) const override;
        uint HashCode() const override;
};

/**
 * Group query - combines multiple sub-queries with And/Or/Nor logic
 */
class GroupQuery : public QueryFilter
{
    public:
        enum class GroupQueryType
        {
            And,  // All sub-queries must match
            Or,   // At least one sub-query must match
            Nor   // No sub-queries must match
        };

        GroupQuery(GroupQueryType type, const QList<QueryFilter*>& subQueries);
        ~GroupQuery();

        QVariant Match(const QVariantMap& objectData, const QString& objectType, XenConnection *conn) const override;
        bool Equals(const QueryFilter* other) const override;
        uint HashCode() const override;

        GroupQueryType getType() const { return this->type_; }
        QList<QueryFilter*> getSubQueries() const { return this->subQueries_; }

    private:
        GroupQueryType type_;
        QList<QueryFilter*> subQueries_;
};

/**
 * String property query - matches string properties (name, description, etc.)
 */
class StringPropertyQuery : public QueryFilter
{
    public:
        enum class MatchType
        {
            Contains,
            NotContains,
            StartsWith,
            EndsWith,
            ExactMatch
        };

        StringPropertyQuery(PropertyNames property, const QString& query, MatchType matchType);

        QVariant Match(const QVariantMap& objectData, const QString& objectType, XenConnection *conn) const override;
        bool Equals(const QueryFilter* other) const override;
        uint HashCode() const override;

        PropertyNames getProperty() const { return this->property_; }
        QString getQuery() const { return this->query_; }
        MatchType getMatchType() const { return this->matchType_; }

    private:
        PropertyNames property_;
        QString query_;
        MatchType matchType_;
};

/**
 * Enum property query - matches enum properties (power state, OS, type, etc.)
 */
class EnumQuery : public QueryFilter
{
    public:
        EnumQuery(PropertyNames property, const QString& value, bool negated = false);

        QVariant Match(const QVariantMap& objectData, const QString& objectType, XenConnection *conn) const override;
        bool Equals(const QueryFilter* other) const override;
        uint HashCode() const override;

        PropertyNames getProperty() const { return this->property_; }
        QString getValue() const { return this->value_; }
        bool isNegated() const { return this->negated_; }

    private:
        PropertyNames property_;
        QString value_;
        bool negated_;
};

/**
 * Numeric property query - matches numeric properties (memory, disk size, CPU count, etc.)
 */
class NumericQuery : public QueryFilter
{
    public:
        enum class ComparisonType
        {
            LessThan,
            GreaterThan,
            Equal,
            NotEqual
        };

        NumericQuery(PropertyNames property, qint64 value, ComparisonType comparisonType);

        QVariant Match(const QVariantMap& objectData, const QString& objectType, XenConnection *conn) const override;
        bool Equals(const QueryFilter* other) const override;
        uint HashCode() const override;

        PropertyNames getProperty() const { return this->property_; }
        qint64 getValue() const { return this->value_; }
        ComparisonType getComparisonType() const { return this->comparisonType_; }

    private:
        PropertyNames property_;
        qint64 value_;
        ComparisonType comparisonType_;
};

/**
 * Date property query - matches date/time properties (start time, last boot, etc.)
 */
class DateQuery : public QueryFilter
{
    public:
        enum class ComparisonType
        {
            Before,
            After,
            Exact
        };

        DateQuery(PropertyNames property, const QDateTime& value, ComparisonType comparisonType);

        QVariant Match(const QVariantMap& objectData, const QString& objectType, XenConnection *conn) const override;
        bool Equals(const QueryFilter* other) const override;
        uint HashCode() const override;

        PropertyNames getProperty() const { return this->property_; }
        QDateTime getValue() const { return this->value_; }
        ComparisonType getComparisonType() const { return this->comparisonType_; }

    private:
        PropertyNames property_;
        QDateTime value_;
        ComparisonType comparisonType_;
};

/**
 * Boolean property query - matches boolean properties (HA enabled, shared, etc.)
 */
class BoolQuery : public QueryFilter
{
    public:
        BoolQuery(PropertyNames property, bool value);

        QVariant Match(const QVariantMap& objectData, const QString& objectType, XenConnection *conn) const override;
        bool Equals(const QueryFilter* other) const override;
        uint HashCode() const override;

        PropertyNames getProperty() const { return this->property_; }
        bool getValue() const { return this->value_; }

    private:
        PropertyNames property_;
        bool value_;
};

/**
 * Tag query - matches objects by tags
 */
class TagQuery : public QueryFilter
{
    public:
        TagQuery(const QString& tag, bool negated = false);

        QVariant Match(const QVariantMap& objectData, const QString& objectType, XenConnection *conn) const override;
        bool Equals(const QueryFilter* other) const override;
        uint HashCode() const override;

        QString getTag() const { return this->tag_; }
        bool isNegated() const { return this->negated_; }

    private:
        QString tag_;
        bool negated_;
};

#endif // QUERIES_H
