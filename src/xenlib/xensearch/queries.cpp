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

#include "queries.h"
#include "../xenlib.h"

// Helper function to get property value from QVariantMap
static QVariant getPropertyValue(const QVariantMap& objectData, PropertyNames property)
{
    switch (property)
    {
        case PropertyNames::label:
            return objectData.value("name_label");
        case PropertyNames::description:
            return objectData.value("name_description");
        case PropertyNames::uuid:
            return objectData.value("uuid");
        case PropertyNames::tags:
            return objectData.value("tags");
        case PropertyNames::type:
            return objectData.value("type");
        case PropertyNames::power_state:
            return objectData.value("power_state");
        case PropertyNames::virtualisation_status:
            return objectData.value("PV_drivers_detected");
        case PropertyNames::os_name:
            {
                QVariantMap guestMetrics = objectData.value("guest_metrics").toMap();
                QVariantMap osVersion = guestMetrics.value("os_version").toMap();
                return osVersion.value("name");
            }
        case PropertyNames::ha_restart_priority:
            return objectData.value("ha_restart_priority");
        case PropertyNames::start_time:
            return objectData.value("start_time");
        case PropertyNames::memory:
            return objectData.value("memory_dynamic_max");
        case PropertyNames::size:
            return objectData.value("virtual_size");
        case PropertyNames::shared:
            return objectData.value("shared");
        case PropertyNames::ha_enabled:
            return objectData.value("ha_enabled");
        case PropertyNames::sr_type:
            return objectData.value("type"); // For SR
        case PropertyNames::read_caching_enabled:
            return objectData.value("read_caching_enabled");
        case PropertyNames::ip_address:
            {
                QVariantMap guestMetrics = objectData.value("guest_metrics").toMap();
                QVariantMap networks = guestMetrics.value("networks").toMap();
                // Return first IP address found
                for (const QVariant& ip : networks)
                    return ip;
                return QVariant();
            }
    }
    return QVariant();
}

// Helper function to get property name string
static QString getPropertyName(PropertyNames property)
{
    switch (property)
    {
        case PropertyNames::label:
            return "Name";
        case PropertyNames::description:
            return "Description";
        case PropertyNames::uuid:
            return "UUID";
        case PropertyNames::tags:
            return "Tags";
        case PropertyNames::type:
            return "Type";
        case PropertyNames::power_state:
            return "Power State";
        case PropertyNames::virtualisation_status:
            return "Virtualization Status";
        case PropertyNames::os_name:
            return "OS Name";
        case PropertyNames::ha_restart_priority:
            return "HA Restart Priority";
        case PropertyNames::start_time:
            return "Start Time";
        case PropertyNames::memory:
            return "Memory";
        case PropertyNames::size:
            return "Size";
        case PropertyNames::shared:
            return "Shared";
        case PropertyNames::ha_enabled:
            return "HA Enabled";
        case PropertyNames::sr_type:
            return "SR Type";
        case PropertyNames::read_caching_enabled:
            return "Read Caching";
        case PropertyNames::ip_address:
            return "IP Address";
    }
    return "Unknown";
}


// ============================================================================
// DummyQuery
// ============================================================================

QVariant DummyQuery::Match(const QVariantMap& objectData, const QString& objectType, XenConnection *conn) const
{
    Q_UNUSED(objectData);
    Q_UNUSED(objectType);
    Q_UNUSED(conn);
    return QVariant(); // Indeterminate - used for placeholder
}

bool DummyQuery::Equals(const QueryFilter* other) const
{
    return dynamic_cast<const DummyQuery*>(other) != nullptr;
}

uint DummyQuery::HashCode() const
{
    return qHash("DummyQuery");
}

// ============================================================================
// GroupQuery
// ============================================================================

GroupQuery::GroupQuery(GroupQueryType type, const QList<QueryFilter*>& subQueries)
    : type_(type)
    , subQueries_(subQueries)
{
}

GroupQuery::~GroupQuery()
{
    qDeleteAll(this->subQueries_);
}

QVariant GroupQuery::Match(const QVariantMap& objectData, const QString& objectType, XenConnection* conn) const
{
    if (this->subQueries_.isEmpty())
        return true; // Empty group matches everything
    
    switch (this->type_)
    {
        case GroupQueryType::And:
        {
            // All must match
            for (QueryFilter* subQuery : this->subQueries_)
            {
                QVariant result = subQuery->Match(objectData, objectType, conn);
                if (!result.isValid() || !result.toBool())
                    return false;
            }
            return true;
        }
        
        case GroupQueryType::Or:
        {
            // At least one must match
            for (QueryFilter* subQuery : this->subQueries_)
            {
                QVariant result = subQuery->Match(objectData, objectType, conn);
                if (result.isValid() && result.toBool())
                    return true;
            }
            return false;
        }
        
        case GroupQueryType::Nor:
        {
            // None must match
            for (QueryFilter* subQuery : this->subQueries_)
            {
                QVariant result = subQuery->Match(objectData, objectType, conn);
                if (result.isValid() && result.toBool())
                    return false;
            }
            return true;
        }
    }
    
    return false;
}

bool GroupQuery::Equals(const QueryFilter* other) const
{
    const GroupQuery* otherGroup = dynamic_cast<const GroupQuery*>(other);
    if (!otherGroup)
        return false;
    
    if (this->type_ != otherGroup->type_)
        return false;
    
    if (this->subQueries_.size() != otherGroup->subQueries_.size())
        return false;
    
    for (int i = 0; i < this->subQueries_.size(); ++i)
    {
        if (!this->subQueries_[i]->Equals(otherGroup->subQueries_[i]))
            return false;
    }
    
    return true;
}

uint GroupQuery::HashCode() const
{
    uint hash = qHash(static_cast<int>(this->type_));
    for (QueryFilter* subQuery : this->subQueries_)
    {
        hash ^= subQuery->HashCode();
    }
    return hash;
}

// ============================================================================
// StringPropertyQuery
// ============================================================================

StringPropertyQuery::StringPropertyQuery(PropertyNames property, const QString& query, MatchType matchType)
    : property_(property)
    , query_(query)
    , matchType_(matchType)
{
}

QVariant StringPropertyQuery::Match(const QVariantMap& objectData, const QString& objectType, XenConnection* conn) const
{
    Q_UNUSED(conn);
    Q_UNUSED(objectType);
    
    // Get property value
    QVariant propValue = getPropertyValue(objectData, this->property_);
    if (!propValue.isValid())
        return false;
    
    QString value = propValue.toString();
    
    // Perform comparison
    switch (this->matchType_)
    {
        case MatchType::Contains:
            return value.contains(this->query_, Qt::CaseInsensitive);
        
        case MatchType::NotContains:
            return !value.contains(this->query_, Qt::CaseInsensitive);
        
        case MatchType::StartsWith:
            return value.startsWith(this->query_, Qt::CaseInsensitive);
        
        case MatchType::EndsWith:
            return value.endsWith(this->query_, Qt::CaseInsensitive);
        
        case MatchType::ExactMatch:
            return value.compare(this->query_, Qt::CaseInsensitive) == 0;
    }
    
    return false;
}

bool StringPropertyQuery::Equals(const QueryFilter* other) const
{
    const StringPropertyQuery* otherQuery = dynamic_cast<const StringPropertyQuery*>(other);
    if (!otherQuery)
        return false;
    
    return this->property_ == otherQuery->property_ &&
           this->query_ == otherQuery->query_ &&
           this->matchType_ == otherQuery->matchType_;
}

uint StringPropertyQuery::HashCode() const
{
    return qHash(static_cast<int>(this->property_)) ^ 
           qHash(this->query_) ^
           qHash(static_cast<int>(this->matchType_));
}

// ============================================================================
// EnumQuery
// ============================================================================

EnumQuery::EnumQuery(PropertyNames property, const QString& value, bool negated)
    : property_(property)
    , value_(value)
    , negated_(negated)
{
}

QVariant EnumQuery::Match(const QVariantMap& objectData, const QString& objectType, XenConnection *conn) const
{
    Q_UNUSED(conn);
    Q_UNUSED(objectType);
    
    // Get property value
    QVariant propValue = getPropertyValue(objectData, this->property_);
    if (!propValue.isValid())
        return false;
    
    QString value = propValue.toString();
    bool matches = (value.compare(this->value_, Qt::CaseInsensitive) == 0);
    
    return this->negated_ ? !matches : matches;
}

bool EnumQuery::Equals(const QueryFilter* other) const
{
    const EnumQuery* otherQuery = dynamic_cast<const EnumQuery*>(other);
    if (!otherQuery)
        return false;
    
    return this->property_ == otherQuery->property_ &&
           this->value_ == otherQuery->value_ &&
           this->negated_ == otherQuery->negated_;
}

uint EnumQuery::HashCode() const
{
    return qHash(static_cast<int>(this->property_)) ^ 
           qHash(this->value_) ^
           qHash(this->negated_);
}

// ============================================================================
// NumericQuery
// ============================================================================

NumericQuery::NumericQuery(PropertyNames property, qint64 value, ComparisonType comparisonType)
    : property_(property)
    , value_(value)
    , comparisonType_(comparisonType)
{
}

QVariant NumericQuery::Match(const QVariantMap& objectData, const QString& objectType, XenConnection* conn) const
{
    Q_UNUSED(conn);
    Q_UNUSED(objectType);
    
    // Get property value
    QVariant propValue = getPropertyValue(objectData, this->property_);
    if (!propValue.isValid())
        return false;
    
    qint64 value = propValue.toLongLong();
    
    // Perform comparison
    switch (this->comparisonType_)
    {
        case ComparisonType::LessThan:
            return value < this->value_;
        
        case ComparisonType::GreaterThan:
            return value > this->value_;
        
        case ComparisonType::Equal:
            return value == this->value_;
        
        case ComparisonType::NotEqual:
            return value != this->value_;
    }
    
    return false;
}

bool NumericQuery::Equals(const QueryFilter* other) const
{
    const NumericQuery* otherQuery = dynamic_cast<const NumericQuery*>(other);
    if (!otherQuery)
        return false;
    
    return this->property_ == otherQuery->property_ &&
           this->value_ == otherQuery->value_ &&
           this->comparisonType_ == otherQuery->comparisonType_;
}

uint NumericQuery::HashCode() const
{
    return qHash(static_cast<int>(this->property_)) ^ 
           qHash(this->value_) ^
           qHash(static_cast<int>(this->comparisonType_));
}

// ============================================================================
// DateQuery
// ============================================================================

DateQuery::DateQuery(PropertyNames property, const QDateTime& value, ComparisonType comparisonType)
    : property_(property)
    , value_(value)
    , comparisonType_(comparisonType)
{
}

QVariant DateQuery::Match(const QVariantMap& objectData, const QString& objectType, XenConnection *conn) const
{
    Q_UNUSED(conn);
    Q_UNUSED(objectType);
    
    // Get property value
    QVariant propValue = getPropertyValue(objectData, this->property_);
    if (!propValue.isValid())
        return false;
    
    QDateTime value = propValue.toDateTime();
    if (!value.isValid())
        return false;
    
    // Perform comparison
    switch (this->comparisonType_)
    {
        case ComparisonType::Before:
            return value < this->value_;
        
        case ComparisonType::After:
            return value > this->value_;
        
        case ComparisonType::Exact:
            // Match same date (ignoring time)
            return value.date() == this->value_.date();
    }
    
    return false;
}

bool DateQuery::Equals(const QueryFilter* other) const
{
    const DateQuery* otherQuery = dynamic_cast<const DateQuery*>(other);
    if (!otherQuery)
        return false;
    
    return this->property_ == otherQuery->property_ &&
           this->value_ == otherQuery->value_ &&
           this->comparisonType_ == otherQuery->comparisonType_;
}

uint DateQuery::HashCode() const
{
    return qHash(static_cast<int>(this->property_)) ^ 
           qHash(this->value_.toMSecsSinceEpoch()) ^
           qHash(static_cast<int>(this->comparisonType_));
}

// ============================================================================
// BoolQuery
// ============================================================================

BoolQuery::BoolQuery(PropertyNames property, bool value)
    : property_(property)
    , value_(value)
{
}

QVariant BoolQuery::Match(const QVariantMap& objectData, const QString& objectType, XenConnection *conn) const
{
    Q_UNUSED(conn);
    Q_UNUSED(objectType);
    
    // Get property value
    QVariant propValue = getPropertyValue(objectData, this->property_);
    if (!propValue.isValid())
        return false;
    
    bool value = propValue.toBool();
    return value == this->value_;
}

bool BoolQuery::Equals(const QueryFilter* other) const
{
    const BoolQuery* otherQuery = dynamic_cast<const BoolQuery*>(other);
    if (!otherQuery)
        return false;
    
    return this->property_ == otherQuery->property_ &&
           this->value_ == otherQuery->value_;
}

uint BoolQuery::HashCode() const
{
    return qHash(static_cast<int>(this->property_)) ^ qHash(this->value_);
}

// ============================================================================
// TagQuery
// ============================================================================

TagQuery::TagQuery(const QString& tag, bool negated)
    : tag_(tag)
    , negated_(negated)
{
}

QVariant TagQuery::Match(const QVariantMap& objectData, const QString& objectType, XenConnection* conn) const
{
    Q_UNUSED(conn);
    Q_UNUSED(objectType);
    
    // Get tags from object
    QVariant tagsValue = objectData.value("tags");
    if (!tagsValue.isValid())
        return this->negated_; // No tags = negated query matches
    
    QStringList tags = tagsValue.toStringList();
    bool hasTag = false;
    
    for (const QString& tag : tags)
    {
        if (tag.compare(this->tag_, Qt::CaseInsensitive) == 0)
        {
            hasTag = true;
            break;
        }
    }
    
    return this->negated_ ? !hasTag : hasTag;
}

bool TagQuery::Equals(const QueryFilter* other) const
{
    const TagQuery* otherQuery = dynamic_cast<const TagQuery*>(other);
    if (!otherQuery)
        return false;
    
    return this->tag_ == otherQuery->tag_ &&
           this->negated_ == otherQuery->negated_;
}

uint TagQuery::HashCode() const
{
    return qHash(this->tag_) ^ qHash(this->negated_);
}
