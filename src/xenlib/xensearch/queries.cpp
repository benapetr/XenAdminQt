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

// Helper function to get object type from property name
static QString getObjectTypeFromPropertyName(PropertyNames property)
{
    switch (property)
    {
        case PropertyNames::pool:
            return "pool";
        case PropertyNames::host:
            return "host";
        case PropertyNames::vm:
            return "vm";
        case PropertyNames::networks:
            return "network";
        case PropertyNames::storage:
            return "sr";
        case PropertyNames::disks:
            return "vdi";
        case PropertyNames::appliance:
            return "vm_appliance";
        case PropertyNames::folder:
            return "folder";
        case PropertyNames::folders:
            return "folder";
        default:
            return QString();  // Unknown mapping
    }
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

// ============================================================================
// IPAddressQuery
// ============================================================================

IPAddressQuery::IPAddressQuery(PropertyNames property, const QString& address)
    : property_(property)
    , address_(address)
{
}

QVariant IPAddressQuery::Match(const QVariantMap& objectData, const QString& objectType, XenConnection* conn) const
{
    Q_UNUSED(objectType);
    Q_UNUSED(conn);
    
    QVariant value = getPropertyValue(objectData, this->property_);
    if (!value.isValid())
        return false;
    
    // Handle IP address matching
    // C# has full CIDR support with ComparableAddress, we use simple string comparison
    if (value.type() == QVariant::String)
    {
        QString ipAddress = value.toString();
        // Exact match or prefix match (e.g., "192.168." matches "192.168.1.1")
        return ipAddress == this->address_ || ipAddress.startsWith(this->address_);
    }
    else if (value.type() == QVariant::StringList)
    {
        // Check if any IP in the list matches
        QStringList ipList = value.toStringList();
        for (const QString& ip : ipList)
        {
            if (ip == this->address_ || ip.startsWith(this->address_))
                return true;
        }
        return false;
    }
    
    return false;
}

bool IPAddressQuery::Equals(const QueryFilter* other) const
{
    const IPAddressQuery* otherQuery = dynamic_cast<const IPAddressQuery*>(other);
    if (!otherQuery)
        return false;
    
    return this->property_ == otherQuery->property_ &&
           this->address_ == otherQuery->address_;
}

uint IPAddressQuery::HashCode() const
{
    return qHash(static_cast<int>(this->property_)) ^ qHash(this->address_);
}

// ============================================================================
// NullPropertyQuery
// ============================================================================

NullPropertyQuery::NullPropertyQuery(PropertyNames property, bool isNull)
    : property_(property)
    , isNull_(isNull)
{
}

QVariant NullPropertyQuery::Match(const QVariantMap& objectData, const QString& objectType, XenConnection* conn) const
{
    Q_UNUSED(objectType);
    Q_UNUSED(conn);
    
    QVariant value = getPropertyValue(objectData, this->property_);
    
    // Check if property value is null/empty
    // C# checks (o == null), we check for invalid/null/empty variants
    bool valueIsNull = !value.isValid() || 
                       value.isNull() || 
                       value.toString().isEmpty() ||
                       value.toString() == "OpaqueRef:NULL";
    
    // Return true if (isNull && valueIsNull) or (!isNull && !valueIsNull)
    // C# equivalent: return ((o == null) == query)
    return this->isNull_ == valueIsNull;
}

bool NullPropertyQuery::Equals(const QueryFilter* other) const
{
    const NullPropertyQuery* otherQuery = dynamic_cast<const NullPropertyQuery*>(other);
    if (!otherQuery)
        return false;
    
    return this->property_ == otherQuery->property_ &&
           this->isNull_ == otherQuery->isNull_;
}

uint NullPropertyQuery::HashCode() const
{
    return qHash(static_cast<int>(this->property_)) ^ qHash(this->isNull_);
}

// ============================================================================
// RecursivePropertyQuery - Base class for recursive queries
// ============================================================================

RecursivePropertyQuery::RecursivePropertyQuery(PropertyNames property, QueryFilter* subQuery)
    : property_(property)
    , subQuery_(subQuery)
{
}

RecursivePropertyQuery::~RecursivePropertyQuery()
{
    delete this->subQuery_;
}

// ============================================================================
// RecursiveXMOPropertyQuery - Single object recursive query
// ============================================================================

RecursiveXMOPropertyQuery::RecursiveXMOPropertyQuery(PropertyNames property, QueryFilter* subQuery)
    : RecursivePropertyQuery(property, subQuery)
{
}

QVariant RecursiveXMOPropertyQuery::Match(const QVariantMap& objectData, const QString& objectType, XenConnection* conn) const
{
    Q_UNUSED(objectType);
    
    // Get the property value (should be an opaque ref)
    QVariant propertyValue = getPropertyValue(objectData, this->property_);
    
    if (!propertyValue.isValid() || propertyValue.toString().isEmpty())
        return false;
    
    QString ref = propertyValue.toString();
    if (ref == "OpaqueRef:NULL")
        return false;
    
    // Get object data for the referenced object
    // We need to determine the object type from the property name
    QString refType = getObjectTypeFromPropertyName(this->property_);
    if (refType.isEmpty())
        return QVariant();  // Indeterminate
    
    // TODO: Get object data from cache when XenConnection cache API is complete
    // For now, return indeterminate since cache infrastructure isn't ready
    Q_UNUSED(conn);
    return QVariant();  // Indeterminate - cache not yet integrated
    
    #if 0  // Disabled until cache API is available
    // Get object data from cache
    QVariantMap refData;
    if (conn && conn->cache())
    {
        refData = conn->cache()->ResolveObjectData(refType, ref);
    }
    
    if (refData.isEmpty())
        return QVariant();  // Indeterminate - object not in cache
    
    // Evaluate subquery on the referenced object
    return this->subQuery_->Match(refData, refType, conn);
    #endif
}

bool RecursiveXMOPropertyQuery::Equals(const QueryFilter* other) const
{
    const RecursiveXMOPropertyQuery* otherQuery = dynamic_cast<const RecursiveXMOPropertyQuery*>(other);
    if (!otherQuery)
        return false;
    
    return this->property_ == otherQuery->property_ &&
           this->subQuery_->Equals(otherQuery->subQuery_);
}

uint RecursiveXMOPropertyQuery::HashCode() const
{
    return qHash(static_cast<int>(this->property_)) ^ this->subQuery_->HashCode();
}

// ============================================================================
// RecursiveXMOListPropertyQuery - List recursive query
// ============================================================================

RecursiveXMOListPropertyQuery::RecursiveXMOListPropertyQuery(PropertyNames property, QueryFilter* subQuery)
    : RecursivePropertyQuery(property, subQuery)
{
}

QVariant RecursiveXMOListPropertyQuery::Match(const QVariantMap& objectData, const QString& objectType, XenConnection* conn) const
{
    Q_UNUSED(objectType);
    
    // Get the property value (should be a list of opaque refs)
    QVariant propertyValue = getPropertyValue(objectData, this->property_);
    
    QVariantList refList;
    if (propertyValue.canConvert<QVariantList>())
    {
        refList = propertyValue.toList();
    }
    else if (propertyValue.canConvert<QStringList>())
    {
        QStringList strList = propertyValue.toStringList();
        for (const QString& str : strList)
            refList.append(str);
    }
    
    if (refList.isEmpty())
        return false;  // Empty list
    
    // TODO: Implement full recursive list matching when cache API is complete
    Q_UNUSED(conn);
    return QVariant();  // Indeterminate - cache not yet integrated
    
    #if 0  // Disabled until cache API is available
    // Determine object type from property name
    QString refType = getObjectTypeFromPropertyName(this->property_);
    if (refType.isEmpty())
        return QVariant();  // Indeterminate
    
    // Check if ANY item matches (C# logic)
    bool seenFalse = false;
    bool seenNull = false;
    
    for (const QVariant& refVariant : refList)
    {
        QString ref = refVariant.toString();
        if (ref.isEmpty() || ref == "OpaqueRef:NULL")
            continue;
        
        // Get object data from cache
        QVariantMap refData;
        if (conn && conn->cache())
        {
            refData = conn->cache()->ResolveObjectData(refType, ref);
        }
        
        if (refData.isEmpty())
        {
            seenNull = true;  // Indeterminate for this item
            continue;
        }
        
        // Evaluate subquery
        QVariant result = this->subQuery_->Match(refData, refType, conn);
        
        if (result.canConvert<bool>())
        {
            bool matchResult = result.toBool();
            if (matchResult)
                return true;  // ANY match means true
            else
                seenFalse = true;
        }
        else
        {
            seenNull = true;  // Indeterminate result
        }
    }
    
    // If we saw any false, return false
    if (seenFalse)
        return false;
    
    // If we saw any indeterminate, return indeterminate
    if (seenNull)
        return QVariant();
    
    // All items were skipped (null/empty)
    return false;
    #endif
}

bool RecursiveXMOListPropertyQuery::Equals(const QueryFilter* other) const
{
    const RecursiveXMOListPropertyQuery* otherQuery = dynamic_cast<const RecursiveXMOListPropertyQuery*>(other);
    if (!otherQuery)
        return false;
    
    return this->property_ == otherQuery->property_ &&
           this->subQuery_->Equals(otherQuery->subQuery_);
}

uint RecursiveXMOListPropertyQuery::HashCode() const
{
    return qHash(static_cast<int>(this->property_)) ^ this->subQuery_->HashCode();
}

// ============================================================================
// XenModelObjectPropertyQuery - Direct object UUID match
// ============================================================================

XenModelObjectPropertyQuery::XenModelObjectPropertyQuery(PropertyNames property, const QString& uuid, bool equals)
    : property_(property)
    , uuid_(uuid)
    , equals_(equals)
{
}

QVariant XenModelObjectPropertyQuery::Match(const QVariantMap& objectData, const QString& objectType, XenConnection* conn) const
{
    Q_UNUSED(objectType);
    Q_UNUSED(conn);
    
    if (this->uuid_.isEmpty() || this->uuid_ == "invalid")
        return false;
    
    // Get the property value (should be an opaque ref)
    QVariant propertyValue = getPropertyValue(objectData, this->property_);
    
    if (!propertyValue.isValid() || propertyValue.toString().isEmpty())
        return false;
    
    QString ref = propertyValue.toString();
    
    // Get UUID from the referenced object
    // Note: In C#, they look up the object and get its UUID
    // For simplicity, we'll compare the opaque ref directly
    // TODO: Implement proper UUID lookup when XenObject cache is complete
    
    bool matches = (ref == this->uuid_);
    return this->equals_ ? matches : !matches;
}

bool XenModelObjectPropertyQuery::Equals(const QueryFilter* other) const
{
    const XenModelObjectPropertyQuery* otherQuery = dynamic_cast<const XenModelObjectPropertyQuery*>(other);
    if (!otherQuery)
        return false;
    
    return this->property_ == otherQuery->property_ &&
           this->uuid_ == otherQuery->uuid_ &&
           this->equals_ == otherQuery->equals_;
}

uint XenModelObjectPropertyQuery::HashCode() const
{
    return qHash(static_cast<int>(this->property_)) ^ qHash(this->uuid_) ^ qHash(this->equals_);
}

// ============================================================================
// XenModelObjectListContainsQuery - List contains UUID
// ============================================================================

XenModelObjectListContainsQuery::XenModelObjectListContainsQuery(PropertyNames property, const QString& uuid, bool contains)
    : property_(property)
    , uuid_(uuid)
    , contains_(contains)
{
}

QVariant XenModelObjectListContainsQuery::Match(const QVariantMap& objectData, const QString& objectType, XenConnection* conn) const
{
    Q_UNUSED(objectType);
    Q_UNUSED(conn);
    
    // Get the property value (should be a list of opaque refs)
    QVariant propertyValue = getPropertyValue(objectData, this->property_);
    
    QVariantList refList;
    if (propertyValue.canConvert<QVariantList>())
    {
        refList = propertyValue.toList();
    }
    else if (propertyValue.canConvert<QStringList>())
    {
        QStringList strList = propertyValue.toStringList();
        for (const QString& str : strList)
            refList.append(str);
    }
    
    bool found = false;
    for (const QVariant& refVariant : refList)
    {
        QString ref = refVariant.toString();
        if (ref == this->uuid_)
        {
            found = true;
            break;
        }
    }
    
    return this->contains_ ? found : !found;
}

bool XenModelObjectListContainsQuery::Equals(const QueryFilter* other) const
{
    const XenModelObjectListContainsQuery* otherQuery = dynamic_cast<const XenModelObjectListContainsQuery*>(other);
    if (!otherQuery)
        return false;
    
    return this->property_ == otherQuery->property_ &&
           this->uuid_ == otherQuery->uuid_ &&
           this->contains_ == otherQuery->contains_;
}

uint XenModelObjectListContainsQuery::HashCode() const
{
    return qHash(static_cast<int>(this->property_)) ^ qHash(this->uuid_);
}

// ============================================================================
// XenModelObjectListContainsNameQuery - List contains name pattern
// ============================================================================

XenModelObjectListContainsNameQuery::XenModelObjectListContainsNameQuery(PropertyNames property, const QString& query, 
                                                                         StringPropertyQuery::MatchType type)
    : property_(property)
    , query_(query)
    , type_(type)
{
}

QVariant XenModelObjectListContainsNameQuery::Match(const QVariantMap& objectData, const QString& objectType, XenConnection* conn) const
{
    Q_UNUSED(objectType);
    
    // Get the property value (should be a list of opaque refs)
    QVariant propertyValue = getPropertyValue(objectData, this->property_);
    
    QVariantList refList;
    if (propertyValue.canConvert<QVariantList>())
    {
        refList = propertyValue.toList();
    }
    else if (propertyValue.canConvert<QStringList>())
    {
        QStringList strList = propertyValue.toStringList();
        for (const QString& str : strList)
            refList.append(str);
    }
    
    // TODO: Implement full name matching when cache API is complete
    Q_UNUSED(conn);
    return false;  // Not yet implemented - cache not available
    
    #if 0  // Disabled until cache API is available
    // Determine object type from property name
    QString refType = getObjectTypeFromPropertyName(this->property_);
    if (refType.isEmpty())
        return QVariant();  // Indeterminate
    
    // Check if any item's name matches
    for (const QVariant& refVariant : refList)
    {
        QString ref = refVariant.toString();
        if (ref.isEmpty() || ref == "OpaqueRef:NULL")
            continue;
        
        // Get object data from cache
        QVariantMap refData;
        if (conn && conn->cache())
        {
            refData = conn->cache()->ResolveObjectData(refType, ref);
        }
        
        if (refData.isEmpty())
            continue;
        
        // Get name from object (name_label or label)
        QString name = refData.value("name_label").toString();
        if (name.isEmpty())
            name = refData.value("label").toString();
        
        // Match name against query (inline string matching logic)
        bool matches = false;
        switch (this->type_)
        {
            case StringPropertyQuery::MatchType::Contains:
                matches = name.contains(this->query_, Qt::CaseInsensitive);
                break;
            case StringPropertyQuery::MatchType::StartsWith:
                matches = name.startsWith(this->query_, Qt::CaseInsensitive);
                break;
            case StringPropertyQuery::MatchType::EndsWith:
                matches = name.endsWith(this->query_, Qt::CaseInsensitive);
                break;
            case StringPropertyQuery::MatchType::ExactMatch:
                matches = name.compare(this->query_, Qt::CaseInsensitive) == 0;
                break;
            default:
                break;
        }
        
        if (matches)
            return true;  // Found a match
    }
    
    return false;  // No matches found
    #endif
}

bool XenModelObjectListContainsNameQuery::Equals(const QueryFilter* other) const
{
    const XenModelObjectListContainsNameQuery* otherQuery = dynamic_cast<const XenModelObjectListContainsNameQuery*>(other);
    if (!otherQuery)
        return false;
    
    return this->property_ == otherQuery->property_ &&
           this->query_ == otherQuery->query_ &&
           this->type_ == otherQuery->type_;
}

uint XenModelObjectListContainsNameQuery::HashCode() const
{
    return qHash(static_cast<int>(this->property_)) ^ qHash(this->query_);
}

// ============================================================================
// ListEmptyQuery - Check if list is empty
// ============================================================================

ListEmptyQuery::ListEmptyQuery(PropertyNames property, bool empty)
    : property_(property)
    , empty_(empty)
{
}

QVariant ListEmptyQuery::Match(const QVariantMap& objectData, const QString& objectType, XenConnection* conn) const
{
    Q_UNUSED(objectType);
    Q_UNUSED(conn);
    
    // Get the property value (should be a list)
    QVariant propertyValue = getPropertyValue(objectData, this->property_);
    
    int count = 0;
    if (propertyValue.canConvert<QVariantList>())
    {
        count = propertyValue.toList().count();
    }
    else if (propertyValue.canConvert<QStringList>())
    {
        count = propertyValue.toStringList().count();
    }
    
    bool isEmpty = (count == 0);
    return isEmpty == this->empty_;
}

bool ListEmptyQuery::Equals(const QueryFilter* other) const
{
    const ListEmptyQuery* otherQuery = dynamic_cast<const ListEmptyQuery*>(other);
    if (!otherQuery)
        return false;
    
    return this->property_ == otherQuery->property_ &&
           this->empty_ == otherQuery->empty_;
}

uint ListEmptyQuery::HashCode() const
{
    return qHash(static_cast<int>(this->property_)) ^ qHash(this->empty_);
}

// ============================================================================
// CustomFieldQueryBase - Base class for custom field queries
// ============================================================================

CustomFieldQueryBase::CustomFieldQueryBase(const QString& fieldName)
    : fieldName_(fieldName)
{
}

// ============================================================================
// CustomFieldQuery - String custom field matching
// ============================================================================

CustomFieldQuery::CustomFieldQuery(const QString& fieldName, const QString& query, 
                                   StringPropertyQuery::MatchType type)
    : CustomFieldQueryBase(fieldName)
    , query_(query)
    , type_(type)
{
}

QVariant CustomFieldQuery::Match(const QVariantMap& objectData, const QString& objectType, XenConnection* conn) const
{
    Q_UNUSED(objectType);
    Q_UNUSED(conn);
    
    // Custom fields are stored in other_config map with "XenCenter.CustomFields." prefix
    QVariantMap otherConfig = objectData.value("other_config").toMap();
    QString key = "XenCenter.CustomFields." + this->fieldName_;
    
    if (!otherConfig.contains(key))
        return false;  // Field not present
    
    QString value = otherConfig.value(key).toString();
    
    if (this->query_.isEmpty())
        return true;  // Just checking if field exists
    
    // Match value against query (inline string matching logic, case-insensitive)
    QString lowerValue = value.toLower();
    QString lowerQuery = this->query_.toLower();
    
    switch (this->type_)
    {
        case StringPropertyQuery::MatchType::Contains:
            return lowerValue.contains(lowerQuery);
        case StringPropertyQuery::MatchType::NotContains:
            return !lowerValue.contains(lowerQuery);
        case StringPropertyQuery::MatchType::StartsWith:
            return lowerValue.startsWith(lowerQuery);
        case StringPropertyQuery::MatchType::EndsWith:
            return lowerValue.endsWith(lowerQuery);
        case StringPropertyQuery::MatchType::ExactMatch:
            return lowerValue == lowerQuery;
    }
    
    return false;
}

bool CustomFieldQuery::Equals(const QueryFilter* other) const
{
    const CustomFieldQuery* otherQuery = dynamic_cast<const CustomFieldQuery*>(other);
    if (!otherQuery)
        return false;
    
    return this->fieldName_ == otherQuery->fieldName_ &&
           this->query_ == otherQuery->query_ &&
           this->type_ == otherQuery->type_;
}

uint CustomFieldQuery::HashCode() const
{
    return qHash(this->fieldName_) ^ qHash(this->query_);
}

// ============================================================================
// CustomFieldDateQuery - Date custom field matching
// ============================================================================

CustomFieldDateQuery::CustomFieldDateQuery(const QString& fieldName, const QDateTime& query, 
                                           DateQuery::ComparisonType type)
    : CustomFieldQueryBase(fieldName)
    , query_(query)
    , type_(type)
{
}

QVariant CustomFieldDateQuery::Match(const QVariantMap& objectData, const QString& objectType, XenConnection* conn) const
{
    Q_UNUSED(objectType);
    Q_UNUSED(conn);
    
    // Custom fields are stored in other_config map with "XenCenter.CustomFields." prefix
    QVariantMap otherConfig = objectData.value("other_config").toMap();
    QString key = "XenCenter.CustomFields." + this->fieldName_;
    
    if (!otherConfig.contains(key))
        return false;  // Field not present
    
    // Parse date from string (stored as yyyyMMdd format)
    QString value = otherConfig.value(key).toString();
    QDateTime fieldDate = QDateTime::fromString(value, "yyyyMMdd");
    
    if (!fieldDate.isValid())
        return false;  // Invalid date format
    
    // Match date using same logic as DateQuery
    switch (this->type_)
    {
        case DateQuery::ComparisonType::Before:
            return fieldDate < this->query_;
        
        case DateQuery::ComparisonType::After:
            return fieldDate > this->query_;
        
        case DateQuery::ComparisonType::Exact:
            // Match same date (ignoring time)
            return fieldDate.date() == this->query_.date();
    }
    
    return false;
}

bool CustomFieldDateQuery::Equals(const QueryFilter* other) const
{
    const CustomFieldDateQuery* otherQuery = dynamic_cast<const CustomFieldDateQuery*>(other);
    if (!otherQuery)
        return false;
    
    return this->fieldName_ == otherQuery->fieldName_ &&
           this->query_ == otherQuery->query_ &&
           this->type_ == otherQuery->type_;
}

uint CustomFieldDateQuery::HashCode() const
{
    return qHash(this->fieldName_) ^ qHash(this->query_.toString());
}
