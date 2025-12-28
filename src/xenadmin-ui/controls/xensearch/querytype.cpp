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

#include "querytype.h"
#include "queryelement.h"
#include "../../../xenlib/xensearch/queries.h"

// Constants for binary sizes
static const qint64 BINARY_MEGA = 1024 * 1024;
static const qint64 BINARY_GIGA = 1024 * 1024 * 1024;


// Helper function to get property name string
static QString getPropertyName(PropertyNames property)
{
    switch (property)
    {
        // Basic properties
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
        
        // VM properties
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
        case PropertyNames::read_caching_enabled:
            return "Read Caching";
        case PropertyNames::vendor_device_state:
            return "Vendor Device";
        case PropertyNames::in_any_appliance:
            return "In Appliance";
        
        // Storage properties
        case PropertyNames::size:
            return "Size";
        case PropertyNames::shared:
            return "Shared";
        case PropertyNames::sr_type:
            return "SR Type";
        
        // Pool properties
        case PropertyNames::ha_enabled:
            return "HA Enabled";
        case PropertyNames::isNotFullyUpgraded:
            return "Upgrade Status";
        
        // Network properties
        case PropertyNames::ip_address:
            return "IP Address";
        
        // Relationship properties
        case PropertyNames::pool:
            return "Pool";
        case PropertyNames::host:
            return "Host";
        case PropertyNames::vm:
            return "VM";
        case PropertyNames::networks:
            return "Networks";
        case PropertyNames::storage:
            return "Storage";
        case PropertyNames::disks:
            return "Disks";
        case PropertyNames::appliance:
            return "Appliance";
        case PropertyNames::folder:
            return "Folder";
        case PropertyNames::folders:
            return "Folders";
        
        // Custom fields
        case PropertyNames::has_custom_fields:
            return "Has Custom Fields";
    }
    return "Unknown";
}


// ============================================================================
// QueryType base class
// ============================================================================

QueryType::QueryType(int group, ObjectTypes appliesTo)
    : group_(group)
    , appliesTo_(appliesTo)
{
}

QueryType::~QueryType()
{
}

// ============================================================================
// DummyQueryType - "Select a filter..."
// ============================================================================

DummyQueryType::DummyQueryType(int group, ObjectTypes appliesTo)
    : QueryType(group, appliesTo)
{
}

bool DummyQueryType::ForQuery(QueryFilter* query) const
{
    return dynamic_cast<DummyQuery*>(query) != nullptr;
}

void DummyQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    Q_UNUSED(query);
    Q_UNUSED(queryElement);
    // Nothing to populate
}

QueryFilter* DummyQueryType::GetQuery(QueryElement* queryElement) const
{
    Q_UNUSED(queryElement);
    return new DummyQuery();
}

QString DummyQueryType::toString() const
{
    return "Select a filter...";
}

// ============================================================================
// GroupQueryType - And/Or/Nor
// ============================================================================

GroupQueryType::GroupQueryType(int group, ObjectTypes appliesTo, GroupType type)
    : QueryType(group, appliesTo)
    , type_(type)
{
}

QueryScope* GroupQueryType::getSubQueryScope() const
{
    return nullptr; // Uses parent scope
}

bool GroupQueryType::ForQuery(QueryFilter* query) const
{
    GroupQuery* groupQuery = dynamic_cast<GroupQuery*>(query);
    if (!groupQuery)
        return false;
    
    switch (this->type_)
    {
        case GroupType::And:
            return groupQuery->getType() == GroupQuery::GroupQueryType::And;
        case GroupType::Or:
            return groupQuery->getType() == GroupQuery::GroupQueryType::Or;
        case GroupType::Nor:
            return groupQuery->getType() == GroupQuery::GroupQueryType::Nor;
    }
    return false;
}

void GroupQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    GroupQuery* groupQuery = dynamic_cast<GroupQuery*>(query);
    if (!groupQuery)
        return;
    
    // QueryElement will handle populating sub-elements
    queryElement->setSubQueries(groupQuery->getSubQueries());
}

QueryFilter* GroupQueryType::GetQuery(QueryElement* queryElement) const
{
    QList<QueryFilter*> subQueries = queryElement->getSubQueries();
    
    GroupQuery::GroupQueryType type;
    switch (this->type_)
    {
        case GroupType::And:
            type = GroupQuery::GroupQueryType::And;
            break;
        case GroupType::Or:
            type = GroupQuery::GroupQueryType::Or;
            break;
        case GroupType::Nor:
            type = GroupQuery::GroupQueryType::Nor;
            break;
        default:
            type = GroupQuery::GroupQueryType::And;
    }
    
    return new GroupQuery(type, subQueries);
}

QString GroupQueryType::toString() const
{
    switch (this->type_)
    {
        case GroupType::And:
            return "All of the following";
        case GroupType::Or:
            return "Any of the following";
        case GroupType::Nor:
            return "None of the following";
    }
    return "Group";
}

// ============================================================================
// StringPropertyQueryType - name, description, etc.
// ============================================================================

StringPropertyQueryType::StringPropertyQueryType(int group, ObjectTypes appliesTo, PropertyNames property)
    : QueryType(group, appliesTo)
    , property_(property)
    , customName_()
{
}

StringPropertyQueryType::StringPropertyQueryType(int group, ObjectTypes appliesTo, PropertyNames property, const QString& customName)
    : QueryType(group, appliesTo)
    , property_(property)
    , customName_(customName)
{
}

bool StringPropertyQueryType::ForQuery(QueryFilter* query) const
{
    StringPropertyQuery* stringQuery = dynamic_cast<StringPropertyQuery*>(query);
    if (!stringQuery)
        return false;
    
    return stringQuery->getProperty() == this->property_;
}

void StringPropertyQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    StringPropertyQuery* stringQuery = dynamic_cast<StringPropertyQuery*>(query);
    if (!stringQuery)
        return;
    
    queryElement->setMatchTypeSelection(matchTypeToString(stringQuery->getMatchType()));
    queryElement->setTextBoxValue(stringQuery->getQuery());
}

QueryFilter* StringPropertyQueryType::GetQuery(QueryElement* queryElement) const
{
    QString matchTypeStr = queryElement->getMatchTypeSelection();
    MatchType matchType = matchTypeFromString(matchTypeStr);
    QString text = queryElement->getTextBoxValue();
    
    return new StringPropertyQuery(this->property_, text, matchType);
}

QString StringPropertyQueryType::toString() const
{
    if (!this->customName_.isEmpty())
        return this->customName_;
    
    return getPropertyName(this->property_);
}

QStringList StringPropertyQueryType::getMatchTypeComboButtonEntries() const
{
    return QStringList() 
        << matchTypeToString(MatchType::Contains)
        << matchTypeToString(MatchType::NotContains)
        << matchTypeToString(MatchType::StartsWith)
        << matchTypeToString(MatchType::EndsWith)
        << matchTypeToString(MatchType::ExactMatch);
}

StringPropertyQueryType::MatchType StringPropertyQueryType::matchTypeFromString(const QString& str)
{
    if (str == "contains" || str == "Contains")
        return MatchType::Contains;
    if (str == "not contains" || str == "Not Contains")
        return MatchType::NotContains;
    if (str == "starts with" || str == "Starts With")
        return MatchType::StartsWith;
    if (str == "ends with" || str == "Ends With")
        return MatchType::EndsWith;
    if (str == "exact match" || str == "Exact Match")
        return MatchType::ExactMatch;
    
    return MatchType::Contains; // Default
}

QString StringPropertyQueryType::matchTypeToString(MatchType type)
{
    switch (type)
    {
        case MatchType::Contains:
            return "Contains";
        case MatchType::NotContains:
            return "Not Contains";
        case MatchType::StartsWith:
            return "Starts With";
        case MatchType::EndsWith:
            return "Ends With";
        case MatchType::ExactMatch:
            return "Exact Match";
    }
    return "Contains";
}

// ============================================================================
// EnumPropertyQueryType - power state, virtualization status, etc.
// ============================================================================

EnumPropertyQueryType::EnumPropertyQueryType(int group, ObjectTypes appliesTo, PropertyNames property)
    : QueryType(group, appliesTo)
    , property_(property)
{
}

bool EnumPropertyQueryType::ForQuery(QueryFilter* query) const
{
    EnumQuery* enumQuery = dynamic_cast<EnumQuery*>(query);
    if (!enumQuery)
        return false;
    
    return enumQuery->getProperty() == this->property_;
}

void EnumPropertyQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    EnumQuery* enumQuery = dynamic_cast<EnumQuery*>(query);
    if (!enumQuery)
        return;
    
    queryElement->setMatchTypeSelection(enumQuery->isNegated() ? "Is not" : "Is");
    queryElement->setComboBoxSelection(enumQuery->getValue());
}

QueryFilter* EnumPropertyQueryType::GetQuery(QueryElement* queryElement) const
{
    QString matchType = queryElement->getMatchTypeSelection();
    bool negated = (matchType == "Is not");
    QString value = queryElement->getComboBoxSelection();
    
    return new EnumQuery(this->property_, value, negated);
}

QString EnumPropertyQueryType::toString() const
{
    return getPropertyName(this->property_);
}

bool EnumPropertyQueryType::showComboButton(QueryElement* queryElement) const
{
    Q_UNUSED(queryElement);
    return true;
}

QStringList EnumPropertyQueryType::getMatchTypeComboButtonEntries() const
{
    return QStringList() << "Is" << "Is not";
}

QVariantList EnumPropertyQueryType::getComboButtonEntries(QueryElement* queryElement) const
{
    Q_UNUSED(queryElement);
    return this->getEnumValues();
}

QVariantList EnumPropertyQueryType::getEnumValues() const
{
    QVariantList values;
    
    switch (this->property_)
    {
        case PropertyNames::power_state:
            values << "Running" << "Halted" << "Suspended" << "Paused";
            break;
        
        case PropertyNames::virtualisation_status:
            values << "Not installed" << "Out of date" << "Up to date" << "Unknown";
            break;
        
        case PropertyNames::type:
            values << "Pool" << "Server" << "VM" << "Storage" << "Network" << "vApp";
            break;
        
        case PropertyNames::sr_type:
            values << "ISO" << "NFS" << "LVM" << "EXT" << "CIFS";
            break;
        
        case PropertyNames::ha_restart_priority:
            values << "Restart" << "Best effort" << "Do not restart";
            break;
        
        default:
            break;
    }
    
    return values;
}

// ============================================================================
// NumericPropertyQueryType - memory, disk size, CPU count, etc.
// ============================================================================

NumericPropertyQueryType::NumericPropertyQueryType(int group, ObjectTypes appliesTo, 
                                                   PropertyNames property, const QString& name,
                                                   qint64 multiplier, const QString& units)
    : QueryType(group, appliesTo)
    , property_(property)
    , name_(name)
    , multiplier_(multiplier)
    , units_(units)
{
}

bool NumericPropertyQueryType::ForQuery(QueryFilter* query) const
{
    NumericQuery* numericQuery = dynamic_cast<NumericQuery*>(query);
    if (!numericQuery)
        return false;
    
    return numericQuery->getProperty() == this->property_;
}

void NumericPropertyQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    NumericQuery* numericQuery = dynamic_cast<NumericQuery*>(query);
    if (!numericQuery)
        return;
    
    queryElement->setMatchTypeSelection(comparisonTypeToString(numericQuery->getComparisonType()));
    queryElement->setNumericValue(numericQuery->getValue() / this->multiplier_);
}

QueryFilter* NumericPropertyQueryType::GetQuery(QueryElement* queryElement) const
{
    QString matchType = queryElement->getMatchTypeSelection();
    ComparisonType comparisonType = comparisonTypeFromString(matchType);
    qint64 value = queryElement->getNumericValue() * this->multiplier_;
    
    return new NumericQuery(this->property_, value, comparisonType);
}

QString NumericPropertyQueryType::toString() const
{
    return this->name_;
}

QStringList NumericPropertyQueryType::getMatchTypeComboButtonEntries() const
{
    return QStringList()
        << comparisonTypeToString(ComparisonType::LessThan)
        << comparisonTypeToString(ComparisonType::GreaterThan)
        << comparisonTypeToString(ComparisonType::Equal)
        << comparisonTypeToString(ComparisonType::NotEqual);
}

NumericPropertyQueryType::ComparisonType NumericPropertyQueryType::comparisonTypeFromString(const QString& str)
{
    if (str == "less than" || str == "Less Than")
        return ComparisonType::LessThan;
    if (str == "greater than" || str == "Greater Than")
        return ComparisonType::GreaterThan;
    if (str == "equal to" || str == "Equal To")
        return ComparisonType::Equal;
    if (str == "not equal to" || str == "Not Equal To")
        return ComparisonType::NotEqual;
    
    return ComparisonType::Equal;
}

QString NumericPropertyQueryType::comparisonTypeToString(ComparisonType type)
{
    switch (type)
    {
        case ComparisonType::LessThan:
            return "Less Than";
        case ComparisonType::GreaterThan:
            return "Greater Than";
        case ComparisonType::Equal:
            return "Equal To";
        case ComparisonType::NotEqual:
            return "Not Equal To";
    }
    return "Equal To";
}

// ============================================================================
// DatePropertyQueryType - start time, last boot, etc.
// ============================================================================

DatePropertyQueryType::DatePropertyQueryType(int group, ObjectTypes appliesTo, PropertyNames property)
    : QueryType(group, appliesTo)
    , property_(property)
{
}

bool DatePropertyQueryType::ForQuery(QueryFilter* query) const
{
    DateQuery* dateQuery = dynamic_cast<DateQuery*>(query);
    if (!dateQuery)
        return false;
    
    return dateQuery->getProperty() == this->property_;
}

void DatePropertyQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    DateQuery* dateQuery = dynamic_cast<DateQuery*>(query);
    if (!dateQuery)
        return;
    
    // Convert DateQuery::ComparisonType to DatePropertyQueryType::ComparisonType
    ComparisonType uiComparisonType;
    DateQuery::ComparisonType queryComparisonType = dateQuery->getComparisonType();
    
    if (queryComparisonType == DateQuery::ComparisonType::Before)
        uiComparisonType = ComparisonType::Before;
    else if (queryComparisonType == DateQuery::ComparisonType::After)
        uiComparisonType = ComparisonType::After;
    else // Exact
        uiComparisonType = ComparisonType::Exact;
    
    queryElement->setMatchTypeSelection(comparisonTypeToString(uiComparisonType));
    queryElement->setDateTimeValue(dateQuery->getValue());
}

QueryFilter* DatePropertyQueryType::GetQuery(QueryElement* queryElement) const
{
    QString matchType = queryElement->getMatchTypeSelection();
    ComparisonType comparisonType = comparisonTypeFromString(matchType);
    QDateTime dateTime;
    DateQuery::ComparisonType queryComparisonType;
    
    // For relative time queries, calculate absolute time
    if (comparisonType == ComparisonType::Last24Hours)
    {
        dateTime = QDateTime::currentDateTime().addDays(-1);
        queryComparisonType = DateQuery::ComparisonType::After;
    } else if (comparisonType == ComparisonType::Last7Days)
    {
        dateTime = QDateTime::currentDateTime().addDays(-7);
        queryComparisonType = DateQuery::ComparisonType::After;
    } else if (comparisonType == ComparisonType::LastMonth)
    {
        dateTime = QDateTime::currentDateTime().addMonths(-1);
        queryComparisonType = DateQuery::ComparisonType::After;
    } else
    {
        dateTime = queryElement->getDateTimeValue();
        
        // Convert to DateQuery::ComparisonType
        if (comparisonType == ComparisonType::Before)
            queryComparisonType = DateQuery::ComparisonType::Before;
        else if (comparisonType == ComparisonType::After)
            queryComparisonType = DateQuery::ComparisonType::After;
        else // Exact
            queryComparisonType = DateQuery::ComparisonType::Exact;
    }
    
    return new DateQuery(this->property_, dateTime, queryComparisonType);
}

QString DatePropertyQueryType::toString() const
{
    return getPropertyName(this->property_);
}

bool DatePropertyQueryType::showDateTimePicker(QueryElement* queryElement) const
{
    QString matchType = queryElement->getMatchTypeSelection();
    ComparisonType type = comparisonTypeFromString(matchType);
    
    // Only show date picker for absolute date queries
    return (type == ComparisonType::Before || 
            type == ComparisonType::After || 
            type == ComparisonType::Exact);
}

QStringList DatePropertyQueryType::getMatchTypeComboButtonEntries() const
{
    return QStringList()
        << comparisonTypeToString(ComparisonType::Before)
        << comparisonTypeToString(ComparisonType::After)
        << comparisonTypeToString(ComparisonType::Exact)
        << comparisonTypeToString(ComparisonType::Last24Hours)
        << comparisonTypeToString(ComparisonType::Last7Days)
        << comparisonTypeToString(ComparisonType::LastMonth);
}

DatePropertyQueryType::ComparisonType DatePropertyQueryType::comparisonTypeFromString(const QString& str)
{
    if (str == "before" || str == "Before")
        return ComparisonType::Before;
    if (str == "after" || str == "After")
        return ComparisonType::After;
    if (str == "exact" || str == "Exact")
        return ComparisonType::Exact;
    if (str == "in the last 24 hours" || str == "In the last 24 hours")
        return ComparisonType::Last24Hours;
    if (str == "in the last 7 days" || str == "In the last 7 days")
        return ComparisonType::Last7Days;
    if (str == "in the last month" || str == "In the last month")
        return ComparisonType::LastMonth;
    
    return ComparisonType::After;
}

QString DatePropertyQueryType::comparisonTypeToString(ComparisonType type)
{
    switch (type)
    {
        case ComparisonType::Before:
            return "Before";
        case ComparisonType::After:
            return "After";
        case ComparisonType::Exact:
            return "Exact";
        case ComparisonType::Last24Hours:
            return "In the last 24 hours";
        case ComparisonType::Last7Days:
            return "In the last 7 days";
        case ComparisonType::LastMonth:
            return "In the last month";
    }
    return "After";
}

// ============================================================================
// BooleanQueryType - HA enabled, shared, etc.
// ============================================================================

BooleanQueryType::BooleanQueryType(int group, ObjectTypes appliesTo, PropertyNames property)
    : QueryType(group, appliesTo)
    , property_(property)
{
}

bool BooleanQueryType::ForQuery(QueryFilter* query) const
{
    BoolQuery* boolQuery = dynamic_cast<BoolQuery*>(query);
    if (!boolQuery)
        return false;
    
    return boolQuery->getProperty() == this->property_;
}

void BooleanQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    BoolQuery* boolQuery = dynamic_cast<BoolQuery*>(query);
    if (!boolQuery)
        return;
    
    queryElement->setMatchTypeSelection(boolQuery->getValue() ? "Yes" : "No");
}

QueryFilter* BooleanQueryType::GetQuery(QueryElement* queryElement) const
{
    QString matchType = queryElement->getMatchTypeSelection();
    bool value = (matchType == "Yes");
    
    return new BoolQuery(this->property_, value);
}

QString BooleanQueryType::toString() const
{
    return getPropertyName(this->property_);
}

QStringList BooleanQueryType::getMatchTypeComboButtonEntries() const
{
    return QStringList() << "Yes" << "No";
}

// ============================================================================
// TagQueryType
// ============================================================================

TagQueryType::TagQueryType(int group, ObjectTypes appliesTo)
    : QueryType(group, appliesTo)
{
}

bool TagQueryType::ForQuery(QueryFilter* query) const
{
    return dynamic_cast<TagQuery*>(query) != nullptr;
}

void TagQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    TagQuery* tagQuery = dynamic_cast<TagQuery*>(query);
    if (!tagQuery)
        return;
    
    queryElement->setMatchTypeSelection(tagQuery->isNegated() ? "Does not contain" : "Contains");
    queryElement->setTextBoxValue(tagQuery->getTag());
}

QueryFilter* TagQueryType::GetQuery(QueryElement* queryElement) const
{
    QString matchType = queryElement->getMatchTypeSelection();
    bool negated = (matchType == "Does not contain");
    QString tag = queryElement->getTextBoxValue();
    
    return new TagQuery(tag, negated);
}

QString TagQueryType::toString() const
{
    return "Tags";
}

QStringList TagQueryType::getMatchTypeComboButtonEntries() const
{
    return QStringList() << "Contains" << "Does not contain";
}

// ============================================================================
// QueryTypeRegistry - Singleton registry of all query types
// ============================================================================

QueryTypeRegistry* QueryTypeRegistry::instance_ = nullptr;

QueryTypeRegistry::QueryTypeRegistry()
    : defaultQueryType_(nullptr)
{
}

QueryTypeRegistry::~QueryTypeRegistry()
{
    qDeleteAll(this->queryTypes_);
}

QueryTypeRegistry* QueryTypeRegistry::instance()
{
    if (!instance_)
    {
        instance_ = new QueryTypeRegistry();
        instance_->initialize();
    }
    return instance_;
}

void QueryTypeRegistry::initialize()
{
    // Clear existing
    qDeleteAll(this->queryTypes_);
    this->queryTypes_.clear();
    
    // Default query type (Group 0)
    this->defaultQueryType_ = new DummyQueryType(0, ObjectTypes::None);
    this->queryTypes_.append(this->defaultQueryType_);
    
    // Group queries (Group 0)
    this->queryTypes_.append(new GroupQueryType(0, ObjectTypes::AllIncFolders, GroupQueryType::GroupType::And));
    this->queryTypes_.append(new GroupQueryType(0, ObjectTypes::AllIncFolders, GroupQueryType::GroupType::Or));
    this->queryTypes_.append(new GroupQueryType(0, ObjectTypes::AllIncFolders, GroupQueryType::GroupType::Nor));
    
    // Basic property queries (Group 1)
    this->queryTypes_.append(new StringPropertyQueryType(1, ObjectTypes::AllIncFolders, PropertyNames::label));
    this->queryTypes_.append(new StringPropertyQueryType(1, ObjectTypes::AllExcFolders, PropertyNames::description));
    this->queryTypes_.append(new TagQueryType(1, ObjectTypes::AllExcFolders));
    
    // Object type query (Group 1)
    this->queryTypes_.append(new EnumPropertyQueryType(1, ObjectTypes::None, PropertyNames::type));
    
    // VM-specific queries (Group 3)
    this->queryTypes_.append(new NumericPropertyQueryType(3, ObjectTypes::VM, PropertyNames::memory, 
                                                          "Memory", BINARY_MEGA, "MB"));
    this->queryTypes_.append(new EnumPropertyQueryType(3, ObjectTypes::VM, PropertyNames::power_state));
    this->queryTypes_.append(new EnumPropertyQueryType(3, ObjectTypes::VM, PropertyNames::virtualisation_status));
    this->queryTypes_.append(new StringPropertyQueryType(3, ObjectTypes::VM, PropertyNames::os_name, "OS Name"));
    this->queryTypes_.append(new EnumPropertyQueryType(3, ObjectTypes::VM, PropertyNames::ha_restart_priority));
    this->queryTypes_.append(new DatePropertyQueryType(3, ObjectTypes::VM, PropertyNames::start_time));
    this->queryTypes_.append(new BooleanQueryType(3, ObjectTypes::VM, PropertyNames::read_caching_enabled));
    this->queryTypes_.append(new BooleanQueryType(3, ObjectTypes::VM, PropertyNames::vendor_device_state));
    this->queryTypes_.append(new BooleanQueryType(3, ObjectTypes::VM, PropertyNames::in_any_appliance));
    
    // Storage queries (Group 4)
    this->queryTypes_.append(new NumericPropertyQueryType(4, ObjectTypes::VDI, PropertyNames::size,
                                                          "Size", BINARY_GIGA, "GB"));
    this->queryTypes_.append(new BooleanQueryType(4, ObjectTypes::LocalSR | ObjectTypes::RemoteSR | ObjectTypes::VDI, 
                                                  PropertyNames::shared));
    this->queryTypes_.append(new EnumPropertyQueryType(4, ObjectTypes::LocalSR | ObjectTypes::RemoteSR, PropertyNames::sr_type));
    
    // Pool queries (Group 4)
    this->queryTypes_.append(new BooleanQueryType(4, ObjectTypes::Pool, PropertyNames::ha_enabled));
    this->queryTypes_.append(new BooleanQueryType(4, ObjectTypes::Pool, PropertyNames::isNotFullyUpgraded));
}

QueryType* QueryTypeRegistry::findQueryTypeForFilter(QueryFilter* filter) const
{
    if (!filter)
        return this->defaultQueryType_;
    
    for (QueryType* queryType : this->queryTypes_)
    {
        if (queryType->ForQuery(filter))
            return queryType;
    }
    
    return this->defaultQueryType_;
}
