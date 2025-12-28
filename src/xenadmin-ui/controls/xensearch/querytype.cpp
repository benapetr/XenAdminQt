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
#include "xenlib/xensearch/queries.h"
#include "xenlib/xen/network/connectionsmanager.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xencache.h"
#include "xenlib/otherconfig/otherconfigandtagswatcher.h"
#include <algorithm>

// Constants for binary sizes
static const qint64 BINARY_MEGA = 1024 * 1024;
static const qint64 BINARY_GIGA = 1024 * 1024 * 1024;

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

TagQueryType::TagQueryType(int group, ObjectTypes appliesTo, QObject* parent)
    : QueryType(group, appliesTo)
{
    Q_UNUSED(parent);
    // C# equivalent: Subscribes to OtherConfigAndTagsWatcher.TagsChanged
    QObject::connect(OtherConfigAndTagsWatcher::instance(), &OtherConfigAndTagsWatcher::TagsChanged,
            this, &TagQueryType::onTagsChanged);
    
    this->populateCollectedTags();
}

TagQueryType::~TagQueryType()
{
    QObject::disconnect(OtherConfigAndTagsWatcher::instance(), &OtherConfigAndTagsWatcher::TagsChanged,
               this, &TagQueryType::onTagsChanged);
}

void TagQueryType::populateCollectedTags()
{
    // C# equivalent: Tags.GetAllTags()
    // Collects all unique tags from all connections
    
    this->collectedTags_.clear();
    QSet<QString> uniqueTags;
    
    // TODO: When ConnectionsManager is implemented, iterate through all connections
    // For now, this is a placeholder that will be populated by cache updates
    
    // C# logic:
    // foreach (IXenConnection connection in ConnectionsManager.XenConnectionsCopy)
    // {
    //     foreach (IXenObject o in connection.Cache.XenSearchableObjects)
    //     {
    //         String[] tags = Tags.GetTags(o);
    //         if (tags != null)
    //             foreach (string tag in tags)
    //                 uniqueTags.insert(tag);
    //     }
    // }
    
    this->collectedTags_ = uniqueTags.values();
    std::sort(this->collectedTags_.begin(), this->collectedTags_.end());
}

void TagQueryType::onTagsChanged()
{
    // C# equivalent: OtherConfigAndTagsWatcher_TagsChanged
    this->populateCollectedTags();
    emit SomeThingChanged();  // Notify QueryElement to refresh dropdown
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
    
    // C# uses "contains" / "not contains" / "are empty" / "are not empty"
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

bool TagQueryType::showTextBox(QueryElement* queryElement) const
{
    Q_UNUSED(queryElement);
    // Always show text box for tag entry
    return true;
}

bool TagQueryType::showComboButton(QueryElement* queryElement) const
{
    QString matchType = queryElement->getMatchTypeSelection();
    // Show combo box when match type is "Contains" or "Does not contain"
    return matchType == "Contains" || matchType == "Does not contain";
}

QStringList TagQueryType::getMatchTypeComboButtonEntries() const
{
    return QStringList() << "Contains" << "Does not contain";
}

QVariantList TagQueryType::getComboButtonEntries(QueryElement* queryElement) const
{
    Q_UNUSED(queryElement);
    
    QVariantList entries;
    for (const QString& tag : this->collectedTags_)
    {
        entries.append(tag);
    }
    
    return entries;
}

// ============================================================================
// IPAddressQueryType
// ============================================================================

IPAddressQueryType::IPAddressQueryType(int group, ObjectTypes appliesTo, PropertyNames property)
    : QueryType(group, appliesTo)
    , property_(property)
{
}

bool IPAddressQueryType::ForQuery(QueryFilter* query) const
{
    IPAddressQuery* ipQuery = dynamic_cast<IPAddressQuery*>(query);
    if (!ipQuery)
        return false;
    
    return ipQuery->getProperty() == this->property_;
}

void IPAddressQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    IPAddressQuery* ipQuery = dynamic_cast<IPAddressQuery*>(query);
    if (!ipQuery)
        return;
    
    queryElement->setTextBoxValue(ipQuery->getAddress());
}

QueryFilter* IPAddressQueryType::GetQuery(QueryElement* queryElement) const
{
    QString address = queryElement->getTextBoxValue();
    
    if (address.isEmpty())
        return nullptr;
    
    return new IPAddressQuery(this->property_, address);
}

QString IPAddressQueryType::toString() const
{
    return getPropertyName(this->property_);
}

QStringList IPAddressQueryType::getMatchTypeComboButtonEntries() const
{
    return QStringList() << "Is";
}

// ============================================================================
// NullPropertyQueryType
// ============================================================================

NullPropertyQueryType::NullPropertyQueryType(int group, ObjectTypes appliesTo, PropertyNames property, bool isNull, const QString& displayName)
    : QueryType(group, appliesTo)
    , property_(property)
    , isNull_(isNull)
    , displayName_(displayName)
{
}

bool NullPropertyQueryType::ForQuery(QueryFilter* query) const
{
    NullPropertyQuery* nullQuery = dynamic_cast<NullPropertyQuery*>(query);
    if (!nullQuery)
        return false;
    
    return nullQuery->getProperty() == this->property_ &&
           nullQuery->getIsNull() == this->isNull_;
}

void NullPropertyQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    Q_UNUSED(query);
    Q_UNUSED(queryElement);
    // Nothing to populate - no UI controls needed (C# also does nothing here)
}

QueryFilter* NullPropertyQueryType::GetQuery(QueryElement* queryElement) const
{
    Q_UNUSED(queryElement);
    return new NullPropertyQuery(this->property_, this->isNull_);
}

QString NullPropertyQueryType::toString() const
{
    return this->displayName_;
}

// ============================================================================
// ValuePropertyQueryType - Dynamic value collection from cache
// ============================================================================

ValuePropertyQueryType::ValuePropertyQueryType(int group, ObjectTypes appliesTo, PropertyNames property, QObject* parent)
    : QueryType(group, appliesTo)
    , property_(property)
{
    Q_UNUSED(parent);
    // Monitor ConnectionsManager for connection changes
    Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
    QObject::connect(connMgr, &Xen::ConnectionsManager::connectionAdded, this, &ValuePropertyQueryType::onConnectionsChanged);
    QObject::connect(connMgr, &Xen::ConnectionsManager::connectionRemoved, this, &ValuePropertyQueryType::onConnectionsChanged);
    QObject::connect(connMgr, &Xen::ConnectionsManager::connectionsChanged, this, &ValuePropertyQueryType::onConnectionsChanged);
    
    // Initial population
    this->populateCollectedValues();
}

ValuePropertyQueryType::~ValuePropertyQueryType()
{
    // Qt auto-disconnects on deletion
}

bool ValuePropertyQueryType::ForQuery(QueryFilter* query) const
{
    ValuePropertyQuery* vpq = dynamic_cast<ValuePropertyQuery*>(query);
    return vpq && vpq->getProperty() == this->property_;
}

QString ValuePropertyQueryType::toString() const
{
    return getPropertyName(this->property_);
}

void ValuePropertyQueryType::populateCollectedValues()
{
    this->collectedValues_.clear();
    
    Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
    QList<XenConnection*> connections = connMgr->getAllConnections();
    
    // C# only iterates VMs - see comment in C# code at line 1455
    for (XenConnection* conn : connections)
    {
        if (!conn || !conn->GetCache())
            continue;
        
        // Monitor cache changes for this connection
        connect(conn->GetCache(), &XenCache::objectChanged, this, &ValuePropertyQueryType::onCacheChanged,
                Qt::UniqueConnection);  // Prevent duplicate connections
        
        // Get all VMs from this connection
        QList<QVariantMap> vms = conn->GetCache()->GetAllData("vm");
        
        for (const QVariantMap& vmData : vms)
        {
            // Get property value using accessor
            QVariant value = getPropertyValue(vmData, this->property_);
            
            if (!value.isValid())
                continue;
            
            QString strValue = value.toString();
            if (strValue.isEmpty())
                continue;
            
            // Add to collected values (C# uses Dictionary<String, Object> with null values)
            this->collectedValues_[strValue] = true;
        }
    }
    
    // Note: C# version fires PropertyChanged event here, but Qt QueryTypes don't need this
    // The UI will repopulate when queried
}

void ValuePropertyQueryType::onConnectionsChanged()
{
    this->populateCollectedValues();
    emit SomeThingChanged(); // Notify QueryElement to refresh dropdowns
}

void ValuePropertyQueryType::onCacheChanged(XenConnection* connection, const QString& type, const QString& ref)
{
    Q_UNUSED(connection);
    Q_UNUSED(type);
    Q_UNUSED(ref);
    this->populateCollectedValues();
    emit SomeThingChanged(); // Notify QueryElement to refresh dropdowns
}

bool ValuePropertyQueryType::showComboButton(QueryElement* queryElement) const
{
    Q_UNUSED(queryElement);
    return true;
}

QStringList ValuePropertyQueryType::getMatchTypeComboButtonEntries() const
{
    return QStringList() << "is" << "is not";
}

QVariantList ValuePropertyQueryType::getComboButtonEntries(QueryElement* queryElement) const
{
    Q_UNUSED(queryElement);
    
    QStringList entries = this->collectedValues_.keys();
    
    // Sort alphabetically, but put "Unknown" at the bottom (C# logic)
    std::sort(entries.begin(), entries.end(), [](const QString& a, const QString& b) {
        if (a == "Unknown")
            return false;  // "Unknown" goes to end
        if (b == "Unknown")
            return true;   // "Unknown" goes to end
        return a < b;      // Alphabetical otherwise
    });
    
    // Convert to QVariantList
    QVariantList variantEntries;
    for (const QString& entry : entries)
        variantEntries.append(entry);
    
    return variantEntries;
}

QueryFilter* ValuePropertyQueryType::GetQuery(QueryElement* queryElement) const
{
    QString selectedValue = queryElement->getComboBoxSelection();
    if (selectedValue.isEmpty())
        return nullptr;
    
    // matchTypeSelection selects "is" or "is not"
    QString matchType = queryElement->getMatchTypeSelection();
    bool equals = (matchType == "is");
    
    return new ValuePropertyQuery(this->property_, selectedValue, equals);
}

void ValuePropertyQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    ValuePropertyQuery* valueQuery = dynamic_cast<ValuePropertyQuery*>(query);
    if (!valueQuery)
        return;
    
    // Set match type selection ("is" vs "is not")
    queryElement->setMatchTypeSelection(valueQuery->getEquals() ? "is" : "is not");
    
    // Set combo box value
    queryElement->setComboBoxSelection(valueQuery->getQuery());
}

// ============================================================================
// UuidQueryType - Resource selection with dropdown
// ============================================================================

UuidQueryType::UuidQueryType(int group, ObjectTypes appliesTo, PropertyNames property, QObject* parent)
    : QueryType(group, appliesTo)
    , property_(property)
{
    Q_UNUSED(parent);
}

bool UuidQueryType::ForQuery(QueryFilter* query) const
{
    // TODO: Implement when we have proper query type for UUID selection
    Q_UNUSED(query);
    return false;
}

QString UuidQueryType::toString() const
{
    return getPropertyName(this->property_);
}

QStringList UuidQueryType::getMatchTypeComboButtonEntries() const
{
    return QStringList() << "is" << "is not";
}

bool UuidQueryType::showResourceSelectButton(QueryElement* queryElement) const
{
    Q_UNUSED(queryElement);
    return true;
}

QueryFilter* UuidQueryType::GetQuery(QueryElement* queryElement) const
{
    // Get selected object ref from ResourceSelectButton
    QString ref = queryElement->getResourceSelection();
    if (ref.isEmpty())
        return nullptr;
    
    // Create XenModelObjectPropertyQuery matching the selected object
    return new XenModelObjectPropertyQuery(this->property_, ref, true);
}

void UuidQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    // Restore selected object from XenModelObjectPropertyQuery
    XenModelObjectPropertyQuery* propQuery = dynamic_cast<XenModelObjectPropertyQuery*>(query);
    if (propQuery && propQuery->getProperty() == this->property_)
    {
        QString ref = propQuery->getUuid();
        queryElement->setResourceSelection(ref);
    }
}

// ============================================================================
// UuidStringQueryType - UUID search with limited match types
// ============================================================================

UuidStringQueryType::UuidStringQueryType(int group, ObjectTypes appliesTo)
    : StringPropertyQueryType(group, appliesTo, PropertyNames::uuid)
{
}

QStringList UuidStringQueryType::getMatchTypeComboButtonEntries() const
{
    // Only "starts with" and "exact match" are allowed for UUID searches
    return QStringList()
        << StringPropertyQueryType::matchTypeToString(StringPropertyQuery::MatchType::StartsWith)
        << StringPropertyQueryType::matchTypeToString(StringPropertyQuery::MatchType::ExactMatch);
}

// ============================================================================
// RecursiveQueryTypeBase - Parent/Child filtering base
// ============================================================================

RecursiveQueryTypeBase::RecursiveQueryTypeBase(int group, ObjectTypes appliesTo, PropertyNames property, ObjectTypes subQueryScope, QObject* parent)
    : QueryType(group, appliesTo)
    , property_(property)
    , subQueryScope_(new QueryScope(subQueryScope))
{
    Q_UNUSED(parent);
    // Monitor ConnectionsManager for connection changes (C# XenConnections.CollectionChanged)
    Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
    QObject::connect(connMgr, &Xen::ConnectionsManager::connectionAdded, this, &RecursiveQueryTypeBase::onConnectionsChanged);
    QObject::connect(connMgr, &Xen::ConnectionsManager::connectionRemoved, this, &RecursiveQueryTypeBase::onConnectionsChanged);
    QObject::connect(connMgr, &Xen::ConnectionsManager::connectionsChanged, this, &RecursiveQueryTypeBase::onConnectionsChanged);
    
    // Initial registration
    this->onConnectionsChanged();
}

RecursiveQueryTypeBase::~RecursiveQueryTypeBase()
{
    delete this->subQueryScope_;
}

bool RecursiveQueryTypeBase::ForQuery(QueryFilter* query) const
{
    // Check if query is a RecursivePropertyQuery with matching property
    // C# checks: recursivePropertyQuery != null && recursivePropertyQuery.property == property
    
    // Try RecursiveXMOPropertyQuery
    if (auto* xmoQuery = dynamic_cast<RecursiveXMOPropertyQuery*>(query))
        return xmoQuery->getProperty() == this->property_;
    
    // Try RecursiveXMOListPropertyQuery  
    if (auto* xmoListQuery = dynamic_cast<RecursiveXMOListPropertyQuery*>(query))
        return xmoListQuery->getProperty() == this->property_;
    
    return false;
}

void RecursiveQueryTypeBase::onConnectionsChanged()
{
    // C# registers BatchCollectionChanged for each connection's cache
    // In Qt, we monitor cache changes per connection
    Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
    
    for (XenConnection* conn : connMgr->getAllConnections())
    {
        if (conn && conn->GetCache())
        {
            // Monitor cache object changes (C# Cache.RegisterBatchCollectionChanged<O>)
            QObject::connect(conn->GetCache(), &XenCache::objectChanged, 
                    this, &RecursiveQueryTypeBase::onCacheChanged, Qt::UniqueConnection);
        }
    }
}

void RecursiveQueryTypeBase::onCacheChanged()
{
    // C# fires OnSomeThingChanged() to notify QueryElement
    // In Qt, QueryElement would need to monitor this signal
    // For now, this is a placeholder for future implementation
}

// ============================================================================
// RecursiveXMOQueryType - Single object relationship
// ============================================================================

RecursiveXMOQueryType::RecursiveXMOQueryType(int group, ObjectTypes appliesTo, PropertyNames property, ObjectTypes subQueryScope, QObject* parent)
    : RecursiveQueryTypeBase(group, appliesTo, property, subQueryScope, parent)
{
}

QString RecursiveXMOQueryType::toString() const
{
    // C# returns property display name (e.g., "Pool", "Parent folder", "Appliance")
    return getPropertyName(this->property_);
}

QueryFilter* RecursiveXMOQueryType::GetQuery(QueryElement* queryElement) const
{
    // Get sub-query from QueryElement (C# GetSubQueries())
    // C# code: QueryFilter qf = subQueries[0]
    
    // TODO: Implement when QueryElement::GetSubQueries() exists
    Q_UNUSED(queryElement);
    
    // Placeholder: would extract sub-query from queryElement->subQueryElements[0]
    QueryFilter* subQuery = new DummyQuery();
    
    // Special case for folder="(blank)" → folder="/" (C# CA-30595)
    if (this->property_ == PropertyNames::folder)
    {
        if (auto* strQuery = dynamic_cast<StringPropertyQuery*>(subQuery))
        {
            if (strQuery->getProperty() == PropertyNames::uuid && strQuery->getQuery().isEmpty())
            {
                delete subQuery;
                subQuery = new StringPropertyQuery(PropertyNames::uuid, "/", 
                                                   StringPropertyQuery::MatchType::ExactMatch);
            }
        }
    }
    
    return this->newQuery(this->property_, subQuery);
}

void RecursiveXMOQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    // Extract sub-query from RecursiveXMOPropertyQuery and populate QueryElement
    // C# code: queryElement.ClearSubQueryElements(); queryElement.subQueryElements.Add(new QueryElement(...))
    
    auto* recursiveQuery = dynamic_cast<RecursiveXMOPropertyQuery*>(query);
    if (!recursiveQuery)
        return;
    
    // TODO: Implement when QueryElement sub-query support exists
    Q_UNUSED(queryElement);
    
    // Placeholder: would create sub-QueryElement with recursiveQuery->getSubQuery()
}

QueryFilter* RecursiveXMOQueryType::newQuery(PropertyNames property, QueryFilter* subQuery) const
{
    return new RecursiveXMOPropertyQuery(property, subQuery);
}

// ============================================================================
// RecursiveXMOListQueryType - List object relationship
// ============================================================================

RecursiveXMOListQueryType::RecursiveXMOListQueryType(int group, ObjectTypes appliesTo, PropertyNames property, ObjectTypes subQueryScope, QObject* parent)
    : RecursiveQueryTypeBase(group, appliesTo, property, subQueryScope, parent)
{
}

QString RecursiveXMOListQueryType::toString() const
{
    // C# returns property display name (e.g., "Host", "Networks", "Disks")
    return getPropertyName(this->property_);
}

QueryFilter* RecursiveXMOListQueryType::GetQuery(QueryElement* queryElement) const
{
    // Same pattern as RecursiveXMOQueryType
    Q_UNUSED(queryElement);
    
    QueryFilter* subQuery = new DummyQuery();  // TODO: Extract from queryElement
    return this->newQuery(this->property_, subQuery);
}

void RecursiveXMOListQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    auto* recursiveQuery = dynamic_cast<RecursiveXMOListPropertyQuery*>(query);
    if (!recursiveQuery)
        return;
    
    Q_UNUSED(queryElement);
    // TODO: Populate sub-query element
}

QueryFilter* RecursiveXMOListQueryType::newQuery(PropertyNames property, QueryFilter* subQuery) const
{
    return new RecursiveXMOListPropertyQuery(property, subQuery);
}

// ============================================================================
// XenModelObjectPropertyQueryType - Legacy direct object selection
// ============================================================================

XenModelObjectPropertyQueryType::XenModelObjectPropertyQueryType(int group, ObjectTypes appliesTo, PropertyNames property, QObject* parent)
    : QueryType(group, appliesTo)
    , property_(property)
{
    Q_UNUSED(parent);
    // Monitor cache changes (C# Cache.RegisterBatchCollectionChanged<T>)
    Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
    QObject::connect(connMgr, &Xen::ConnectionsManager::connectionAdded, this, &XenModelObjectPropertyQueryType::onConnectionsChanged);
    QObject::connect(connMgr, &Xen::ConnectionsManager::connectionRemoved, this, &XenModelObjectPropertyQueryType::onConnectionsChanged);
    QObject::connect(connMgr, &Xen::ConnectionsManager::connectionsChanged, this, &XenModelObjectPropertyQueryType::onConnectionsChanged);
    
    this->onConnectionsChanged();
}

XenModelObjectPropertyQueryType::~XenModelObjectPropertyQueryType()
{
}

void XenModelObjectPropertyQueryType::onConnectionsChanged()
{
    Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
    
    for (XenConnection* conn : connMgr->getAllConnections())
    {
        if (conn && conn->GetCache())
        {
            QObject::connect(conn->GetCache(), &XenCache::objectChanged,
                    this, &XenModelObjectPropertyQueryType::onCacheChanged, Qt::UniqueConnection);
        }
    }
}

void XenModelObjectPropertyQueryType::onCacheChanged()
{
    // Notify QueryElement to refresh dropdown
}

bool XenModelObjectPropertyQueryType::ForQuery(QueryFilter* query) const
{
    auto* xmoQuery = dynamic_cast<XenModelObjectPropertyQuery*>(query);
    return xmoQuery && xmoQuery->getProperty() == this->property_;
}

QString XenModelObjectPropertyQueryType::toString() const
{
    return getPropertyName(this->property_);
}

QStringList XenModelObjectPropertyQueryType::getMatchTypeComboButtonEntries() const
{
    return QStringList() << "is" << "is not";
}

bool XenModelObjectPropertyQueryType::showComboButton(QueryElement* queryElement) const
{
    Q_UNUSED(queryElement);
    return true;
}

QVariantList XenModelObjectPropertyQueryType::getComboButtonEntries(QueryElement* queryElement) const
{
    Q_UNUSED(queryElement);
    
    QVariantList entries;
    
    // C# collects all T objects from all connections
    // For now, return empty list until XenObject integration complete
    
    // TODO: Implement when XenObject cache is fully integrated
    // Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
    // for (Xen::XenConnection* conn : connMgr->GetAllConnections()) {
    //     if (conn && conn->GetCache()) {
    //         // Get all objects of appropriate type
    //         // CA-17132: Special case pools (filter out null pools)
    //     }
    // }
    
    return entries;
}

QueryFilter* XenModelObjectPropertyQueryType::GetQuery(QueryElement* queryElement) const
{
    // C# extracts selected object from ComboButton and creates XenModelObjectPropertyQuery
    Q_UNUSED(queryElement);
    
    // TODO: Implement when QueryElement combo button integration exists
    return nullptr;
}

void XenModelObjectPropertyQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    auto* xmoQuery = dynamic_cast<XenModelObjectPropertyQuery*>(query);
    if (!xmoQuery)
        return;
    
    Q_UNUSED(queryElement);
    
    // TODO: Set queryElement match type and combo selection
}

// ============================================================================
// XenModelObjectListContainsQueryType - Legacy list object selection
// ============================================================================

XenModelObjectListContainsQueryType::XenModelObjectListContainsQueryType(int group, ObjectTypes appliesTo, PropertyNames property, QObject* parent)
    : QueryType(group, appliesTo)
    , property_(property)
{
    Q_UNUSED(parent);
    Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
    QObject::connect(connMgr, &Xen::ConnectionsManager::connectionAdded, this, &XenModelObjectListContainsQueryType::onConnectionsChanged);
    QObject::connect(connMgr, &Xen::ConnectionsManager::connectionRemoved, this, &XenModelObjectListContainsQueryType::onConnectionsChanged);
    QObject::connect(connMgr, &Xen::ConnectionsManager::connectionsChanged, this, &XenModelObjectListContainsQueryType::onConnectionsChanged);
    
    this->onConnectionsChanged();
}

XenModelObjectListContainsQueryType::~XenModelObjectListContainsQueryType()
{
}

void XenModelObjectListContainsQueryType::onConnectionsChanged()
{
    Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
    
    for (XenConnection* conn : connMgr->getAllConnections())
    {
        if (conn && conn->GetCache())
        {
            QObject::connect(conn->GetCache(), &XenCache::objectChanged,
                    this, &XenModelObjectListContainsQueryType::onCacheChanged, Qt::UniqueConnection);
        }
    }
}

void XenModelObjectListContainsQueryType::onCacheChanged(XenConnection* connection, const QString& type, const QString& ref)
{
    Q_UNUSED(connection);
    Q_UNUSED(type);
    Q_UNUSED(ref);
    // TODO: Update collected values when cache changes
    // For now, we just ignore the change
}

bool XenModelObjectListContainsQueryType::ForQuery(QueryFilter* query) const
{
    // C# checks XenModelObjectListContainsQuery<T> or XenModelObjectListContainsNameQuery
    
    if (auto* listQuery = dynamic_cast<XenModelObjectListContainsQuery*>(query))
        return listQuery->getProperty() == this->property_;
    
    if (auto* nameQuery = dynamic_cast<XenModelObjectListContainsNameQuery*>(query))
        return nameQuery->getProperty() == this->property_;
    
    return false;
}

QString XenModelObjectListContainsQueryType::toString() const
{
    return getPropertyName(this->property_);
}

QStringList XenModelObjectListContainsQueryType::getMatchTypeComboButtonEntries() const
{
    // C# returns different match types based on property
    if (this->property_ == PropertyNames::networks)
    {
        return QStringList() 
            << "uses" 
            << "does not use"
            << "starts with"
            << "ends with"
            << "contains"
            << "not contains";
    }
    else
    {
        return QStringList() 
            << "uses" 
            << "does not use";
    }
}

bool XenModelObjectListContainsQueryType::showComboButton(QueryElement* queryElement) const
{
    if (!queryElement)
        return false;
    
    // Show combo button for "uses" and "does not use" match types
    QString matchType = queryElement->getMatchTypeSelection();
    return matchType == "uses" || matchType == "does not use";
}

QVariantList XenModelObjectListContainsQueryType::getComboButtonEntries(QueryElement* queryElement) const
{
    Q_UNUSED(queryElement);
    
    // TODO: Collect all objects of appropriate type from cache
    return QVariantList();
}

bool XenModelObjectListContainsQueryType::showTextBox(QueryElement* queryElement) const
{
    if (!queryElement)
        return false;
    
    // Show text box for name-based matching (starts with, ends with, contains, not contains)
    QString matchType = queryElement->getMatchTypeSelection();
    return matchType == "starts with" || matchType == "ends with" || 
           matchType == "contains" || matchType == "not contains";
}

QueryFilter* XenModelObjectListContainsQueryType::GetQuery(QueryElement* queryElement) const
{
    Q_UNUSED(queryElement);
    
    // TODO: Create XenModelObjectListContainsQuery or XenModelObjectListContainsNameQuery
    // based on selected match type
    return nullptr;
}

void XenModelObjectListContainsQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    Q_UNUSED(query);
    Q_UNUSED(queryElement);
    
    // TODO: Populate match type and selection from query
}

// ============================================================================
// HostQueryType - Special standalone/in-pool handling
// ============================================================================

HostQueryType::HostQueryType(int group, ObjectTypes appliesTo, QObject* parent)
    : XenModelObjectListContainsQueryType(group, appliesTo, PropertyNames::host, parent)
{
}

QStringList HostQueryType::getMatchTypeComboButtonEntries() const
{
    // C# adds standalone/in-pool to base match types
    QStringList matchTypes = XenModelObjectListContainsQueryType::getMatchTypeComboButtonEntries();
    matchTypes << "is standalone";
    matchTypes << "is in a pool";
    return matchTypes;
}

bool HostQueryType::showTextBox(QueryElement* queryElement) const
{
    if (this->showComboButton(queryElement))
        return false;
    
    QString matchType = queryElement->getMatchTypeSelection();
    
    // Don't show text box for standalone/in-pool queries
    return matchType != "is standalone" && matchType != "is in a pool";
}

QueryFilter* HostQueryType::GetQuery(QueryElement* queryElement) const
{
    // If showing combo button or text box, use base implementation
    if (this->showComboButton(queryElement) || this->showTextBox(queryElement))
        return XenModelObjectListContainsQueryType::GetQuery(queryElement);
    
    // Otherwise, create NullPropertyQuery for standalone/in-pool
    QString matchType = queryElement->getMatchTypeSelection();
    bool isNull = (matchType == "is standalone");
    
    return new NullPropertyQuery(PropertyNames::pool, isNull);
}

void HostQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    // Check if it's a NullPropertyQuery (standalone/in-pool)
    if (auto* nullQuery = dynamic_cast<NullPropertyQuery*>(query))
    {
        if (nullQuery->getProperty() == PropertyNames::pool)
        {
            // Set match type to "is standalone" or "is in a pool"
            QString matchType = nullQuery->getIsNull() ? "is standalone" : "is in a pool";
            queryElement->setMatchTypeSelection(matchType);
            return;
        }
    }
    
    // Otherwise use base implementation
    XenModelObjectListContainsQueryType::FromQuery(query, queryElement);
}

// ============================================================================
// NullQueryType - Check if reference property is null or not null
// ============================================================================

NullQueryType::NullQueryType(int group, ObjectTypes appliesTo, PropertyNames property,
                             bool isNull, const QString& i18n)
    : QueryType(group, appliesTo)
    , property_(property)
    , isNull_(isNull)
    , i18n_(i18n)
{
}

QueryFilter* NullQueryType::GetQuery(QueryElement* queryElement) const
{
    Q_UNUSED(queryElement);
    return new NullPropertyQuery(this->property_, this->isNull_);
}

bool NullQueryType::ForQuery(QueryFilter* query) const
{
    auto* nullQuery = dynamic_cast<NullPropertyQuery*>(query);
    if (!nullQuery)
        return false;
    
    return nullQuery->getProperty() == this->property_ && 
           nullQuery->getIsNull() == this->isNull_;
}

void NullQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    Q_UNUSED(query);
    Q_UNUSED(queryElement);
    // Nothing to do - no additional UI state to restore
}

// ============================================================================
// MatchType - Abstract base class for match types
// ============================================================================

MatchType::MatchType(const QString& matchText)
    : matchText_(matchText)
{
}

// ============================================================================
// XMOListContains - "Contained in" / "Not contained in" match type
// ============================================================================

XMOListContains::XMOListContains(PropertyNames property, bool contains, 
                                 const QString& matchText, const QString& objectType)
    : MatchType(matchText)
    , property_(property)
    , contains_(contains)
    , objectType_(objectType)
{
}

bool XMOListContains::showComboButton(QueryElement* queryElement) const
{
    Q_UNUSED(queryElement);
    return true;
}

QVariantList XMOListContains::getComboButtonEntries() const
{
    // TODO: Populate from cache - needs XenObject cache API
    // C# code iterates over ConnectionsManager.XenConnectionsCopy
    // and calls connection.Cache.AddAll(entries, filter)
    // For now, return empty list (will be implemented when cache API ready)
    return QVariantList();
}

QueryFilter* XMOListContains::getQuery(QueryElement* queryElement) const
{
    // TODO: Get selected object from combo button
    // C# code: T t = queryElement.ComboButton.SelectedItem.Tag as T;
    // For now return nullptr (stubbed)
    Q_UNUSED(queryElement);
    return nullptr;
    
    // Future implementation:
    // QString uuid = queryElement->getComboButtonSelection().toString();
    // return new XenModelObjectListContainsQuery(property_, uuid, contains_);
}

bool XMOListContains::forQuery(QueryFilter* query) const
{
    // TODO: Check if query matches this match type
    // Need XenModelObjectListContainsQuery class first
    Q_UNUSED(query);
    return false;
}

void XMOListContains::fromQuery(QueryFilter* query, QueryElement* queryElement)
{
    // TODO: Restore query to UI
    Q_UNUSED(query);
    Q_UNUSED(queryElement);
}

// ============================================================================
// IntMatch - Numeric match type with multiplier/units
// ============================================================================

IntMatch::IntMatch(PropertyNames property, const QString& matchText,
                   const QString& units, qint64 multiplier,
                   NumericQuery::ComparisonType type)
    : MatchType(matchText)
    , property_(property)
    , type_(type)
    , units_(units)
    , multiplier_(multiplier)
{
}

bool IntMatch::showNumericUpDown(QueryElement* queryElement) const
{
    Q_UNUSED(queryElement);
    return true;
}

QString IntMatch::units(QueryElement* queryElement) const
{
    Q_UNUSED(queryElement);
    return this->units_;
}

QueryFilter* IntMatch::getQuery(QueryElement* queryElement) const
{
    // TODO: Get value from numericUpDown widget
    // C# code: long value = multiplier * (long)queryElement.numericUpDown.Value;
    Q_UNUSED(queryElement);
    return nullptr;
    
    // Future implementation:
    // qint64 value = this->multiplier_ * queryElement->getNumericValue();
    // return new NumericQuery(property_, value, type_);
}

bool IntMatch::forQuery(QueryFilter* query) const
{
    auto* numQuery = dynamic_cast<NumericQuery*>(query);
    if (!numQuery)
        return false;
    
    return numQuery->getProperty() == this->property_ && numQuery->getComparisonType() == this->type_;
}

void IntMatch::fromQuery(QueryFilter* query, QueryElement* queryElement)
{
    auto* numQuery = dynamic_cast<NumericQuery*>(query);
    if (!numQuery)
        return;
    
    // TODO: Set numericUpDown value
    // C# code: queryElement.numericUpDown.Value = (decimal)numQuery.query / multiplier;
    Q_UNUSED(queryElement);
    
    // Future implementation:
    // qint64 displayValue = numQuery->getValue() / this->multiplier_;
    // queryElement->setNumericValue(displayValue);
}

// ============================================================================
// MatchQueryType - Abstract base for QueryTypes that use MatchTypes
// ============================================================================

MatchQueryType::MatchQueryType(int group, ObjectTypes appliesTo, const QString& i18n)
    : QueryType(group, appliesTo)
    , i18n_(i18n)
{
}

MatchQueryType::~MatchQueryType()
{
    // Note: MatchTypes owned by subclasses, not deleted here
}

QStringList MatchQueryType::getMatchTypeComboButtonEntries() const
{
    QStringList entries;
    for (MatchType* matchType : this->getMatchTypes())
    {
        entries.append(matchType->toString());
    }
    return entries;
}

MatchType* MatchQueryType::getSelectedMatchType(QueryElement* queryElement) const
{
    if (!queryElement)
        return nullptr;
    
    QString selectedText = queryElement->getMatchTypeSelection();
    for (MatchType* matchType : this->getMatchTypes())
    {
        if (matchType->toString() == selectedText)
            return matchType;
    }
    
    return nullptr;
}

bool MatchQueryType::showComboButton(QueryElement* queryElement) const
{
    MatchType* matchType = this->getSelectedMatchType(queryElement);
    if (!matchType)
        return false;
    
    return matchType->showComboButton(queryElement);
}

bool MatchQueryType::showNumericUpDown(QueryElement* queryElement) const
{
    MatchType* matchType = this->getSelectedMatchType(queryElement);
    if (!matchType)
        return false;
    
    return matchType->showNumericUpDown(queryElement);
}

bool MatchQueryType::showTextBox(QueryElement* queryElement) const
{
    MatchType* matchType = this->getSelectedMatchType(queryElement);
    if (!matchType)
        return false;
    
    return matchType->showTextBox(queryElement);
}

QString MatchQueryType::getUnits(QueryElement* queryElement) const
{
    MatchType* matchType = this->getSelectedMatchType(queryElement);
    if (!matchType)
        return QString();
    
    return matchType->units(queryElement);
}

QVariantList MatchQueryType::getComboButtonEntries(QueryElement* queryElement) const
{
    MatchType* matchType = this->getSelectedMatchType(queryElement);
    if (!matchType)
        return QVariantList();
    
    return matchType->getComboButtonEntries();
}

QueryFilter* MatchQueryType::GetQuery(QueryElement* queryElement) const
{
    MatchType* matchType = this->getSelectedMatchType(queryElement);
    if (!matchType)
        return nullptr;
    
    return matchType->getQuery(queryElement);
}

bool MatchQueryType::ForQuery(QueryFilter* query) const
{
    for (MatchType* matchType : this->getMatchTypes())
    {
        if (matchType->forQuery(query))
            return true;
    }
    return false;
}

void MatchQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    for (MatchType* matchType : this->getMatchTypes())
    {
        if (matchType->forQuery(query))
        {
            // Set the match type combo button
            queryElement->setMatchTypeSelection(matchType->toString());
            
            // Let the MatchType restore the rest
            matchType->fromQuery(query, queryElement);
            return;
        }
    }
}

// ============================================================================
// DiskQueryType - Complex disk query with 7 match types
// ============================================================================

DiskQueryType::DiskQueryType(int group, ObjectTypes appliesTo, const QString& i18n)
    : MatchQueryType(group, appliesTo, i18n)
{
    // Build match types list (C# QueryElement.cs lines 2497-2514)
    // Note: Using "Contained in" / "Not contained in" for SR
    //       Using "Attached to" / "Not attached to" for VM
    //       Using size queries for disk size
    
    this->matchTypes_.append(new XMOListContains(PropertyNames::storage, true, 
                                                 "Contained in", "sr"));
    this->matchTypes_.append(new XMOListContains(PropertyNames::storage, false, 
                                                 "Not contained in", "sr"));
    this->matchTypes_.append(new XMOListContains(PropertyNames::vm, true, 
                                                 "Attached to", "vm"));
    this->matchTypes_.append(new XMOListContains(PropertyNames::vm, false, 
                                                 "Not attached to", "vm"));
    
    // Size queries: 1 GiB = 1024*1024*1024 bytes
    const qint64 GIGA = Q_INT64_C(1073741824);
    this->matchTypes_.append(new IntMatch(PropertyNames::size, "Size is", 
                                         "GB", GIGA, 
                                         NumericQuery::ComparisonType::Equal));
    this->matchTypes_.append(new IntMatch(PropertyNames::size, "Bigger than", 
                                         "GB", GIGA, 
                                         NumericQuery::ComparisonType::GreaterThan));
    this->matchTypes_.append(new IntMatch(PropertyNames::size, "Smaller than", 
                                         "GB", GIGA, 
                                         NumericQuery::ComparisonType::LessThan));
}

DiskQueryType::~DiskQueryType()
{
    qDeleteAll(this->matchTypes_);
}

QList<MatchType*> DiskQueryType::getMatchTypes() const
{
    return this->matchTypes_;
}

// ============================================================================
// LongQueryType - Numeric range query (bigger/smaller/exact)
// ============================================================================

LongQueryType::LongQueryType(int group, ObjectTypes appliesTo, const QString& i18n,
                             PropertyNames property, qint64 multiplier, const QString& unit)
    : MatchQueryType(group, appliesTo, i18n)
{
    // Build 3 match types: bigger than, smaller than, is exactly
    // C# QueryElement.cs lines 2531-2539
    this->matchTypes_.append(new IntMatch(property, "Bigger than", 
                                         unit, multiplier, 
                                         NumericQuery::ComparisonType::GreaterThan));
    this->matchTypes_.append(new IntMatch(property, "Smaller than", 
                                         unit, multiplier, 
                                         NumericQuery::ComparisonType::LessThan));
    this->matchTypes_.append(new IntMatch(property, "Is exactly", 
                                         unit, multiplier, 
                                         NumericQuery::ComparisonType::Equal));
}

LongQueryType::~LongQueryType()
{
    qDeleteAll(this->matchTypes_);
}

QList<MatchType*> LongQueryType::getMatchTypes() const
{
    return this->matchTypes_;
}

// ============================================================================
// CustomFieldQueryTypeBase - Abstract base for custom field queries
// ============================================================================

CustomFieldQueryTypeBase::CustomFieldQueryTypeBase(int group, ObjectTypes appliesTo,
                                                   const QString& fieldName)
    : QueryType(group, appliesTo)
    , fieldName_(fieldName)
{
}

bool CustomFieldQueryTypeBase::ForQuery(QueryFilter* query) const
{
    // TODO: Check if query matches this custom field
    // Need CustomFieldQuery classes first
    Q_UNUSED(query);
    return false;
}

// ============================================================================
// CustomFieldStringQueryType - String custom field queries
// ============================================================================

CustomFieldStringQueryType::CustomFieldStringQueryType(int group, ObjectTypes appliesTo,
                                                       const QString& fieldName)
    : CustomFieldQueryTypeBase(group, appliesTo, fieldName)
{
}

QStringList CustomFieldStringQueryType::getMatchTypeComboButtonEntries() const
{
    // Use same match types as StringPropertyQueryType
    // C# QueryElement.cs line 2142
    QStringList entries;
    entries << "contains";
    entries << "does not contain";
    entries << "starts with";
    entries << "ends with";
    entries << "is exactly";
    entries << "is not";
    return entries;
}

QueryFilter* CustomFieldStringQueryType::GetQuery(QueryElement* queryElement) const
{
    // TODO: Create CustomFieldQuery
    // Need CustomFieldQuery class first
    Q_UNUSED(queryElement);
    return nullptr;
}

void CustomFieldStringQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    // TODO: Restore custom field query
    Q_UNUSED(query);
    Q_UNUSED(queryElement);
}

// ============================================================================
// CustomFieldDateQueryType - Date custom field queries
// ============================================================================

CustomFieldDateQueryType::CustomFieldDateQueryType(int group, ObjectTypes appliesTo,
                                                   const QString& fieldName)
    : CustomFieldQueryTypeBase(group, appliesTo, fieldName)
{
}

bool CustomFieldDateQueryType::showDateTimePicker(QueryElement* queryElement) const
{
    // Use same logic as DatePropertyQueryType
    // Show date picker for most match types except "null" checks
    Q_UNUSED(queryElement);
    return true;  // Simplified - will refine when QueryElement complete
}

QStringList CustomFieldDateQueryType::getMatchTypeComboButtonEntries() const
{
    // Use same match types as DatePropertyQueryType
    // C# QueryElement.cs line 2181
    QStringList entries;
    entries << "on";
    entries << "after";
    entries << "before";
    entries << "between";
    entries << "is empty";
    entries << "is not empty";
    return entries;
}

QueryFilter* CustomFieldDateQueryType::GetQuery(QueryElement* queryElement) const
{
    // TODO: Create CustomFieldDateQuery
    // Need CustomFieldDateQuery class first
    Q_UNUSED(queryElement);
    return nullptr;
}

void CustomFieldDateQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    // TODO: Restore custom field date query
    Q_UNUSED(query);
    Q_UNUSED(queryElement);
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
    this->queryTypes_.append(new UuidStringQueryType(1, ObjectTypes::AllExcFolders));
    this->queryTypes_.append(new TagQueryType(1, ObjectTypes::AllExcFolders));
    
    // Object type query (Group 1)
    this->queryTypes_.append(new EnumPropertyQueryType(1, ObjectTypes::None, PropertyNames::type));
    
    // VM-specific queries (Group 3)
    this->queryTypes_.append(new NumericPropertyQueryType(3, ObjectTypes::VM, PropertyNames::memory, 
                                                          "Memory", BINARY_MEGA, "MB"));
    this->queryTypes_.append(new IPAddressQueryType(3, ObjectTypes::VM | ObjectTypes::Server | ObjectTypes::LocalSR | ObjectTypes::RemoteSR,
                                                    PropertyNames::ip_address));
    this->queryTypes_.append(new EnumPropertyQueryType(3, ObjectTypes::VM, PropertyNames::power_state));
    this->queryTypes_.append(new EnumPropertyQueryType(3, ObjectTypes::VM, PropertyNames::virtualisation_status));
    this->queryTypes_.append(new ValuePropertyQueryType(3, ObjectTypes::VM, PropertyNames::os_name));  // Dynamic OS name collection
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
    this->queryTypes_.append(new NullQueryType(4, ObjectTypes::Server | ObjectTypes::VM, PropertyNames::pool, false, "Is in a pool"));
    this->queryTypes_.append(new NullQueryType(4, ObjectTypes::Server | ObjectTypes::VM, PropertyNames::pool, true, "Is standalone"));
    
    // Recursive queries - Parent/Child filtering (Group 6)
    // C# QueryElement.cs lines 96-102
    this->queryTypes_.append(new RecursiveXMOQueryType(6, ObjectTypes::AllExcFolders, 
                                                       PropertyNames::pool, ObjectTypes::Pool));
    this->queryTypes_.append(new RecursiveXMOListQueryType(6, ObjectTypes::AllExcFolders & ~ObjectTypes::Pool, 
                                                           PropertyNames::host, ObjectTypes::Server));
    this->queryTypes_.append(new RecursiveXMOListQueryType(6, ObjectTypes::AllExcFolders, 
                                                           PropertyNames::vm, ObjectTypes::VM));
    this->queryTypes_.append(new RecursiveXMOListQueryType(6, ObjectTypes::Network | ObjectTypes::VM, 
                                                           PropertyNames::networks, ObjectTypes::Network));
    this->queryTypes_.append(new RecursiveXMOListQueryType(6, ObjectTypes::LocalSR | ObjectTypes::RemoteSR | ObjectTypes::VM | ObjectTypes::VDI,
                                                           PropertyNames::storage, ObjectTypes::LocalSR | ObjectTypes::RemoteSR));
    this->queryTypes_.append(new RecursiveXMOListQueryType(6, ObjectTypes::VM | ObjectTypes::VDI, 
                                                           PropertyNames::disks, ObjectTypes::VDI));
    
    // VM Appliance recursive query (Group 7)
    // C# QueryElement.cs line 104-105
    this->queryTypes_.append(new RecursiveXMOQueryType(7, ObjectTypes::VM, 
                                                       PropertyNames::appliance, ObjectTypes::Appliance));
    this->queryTypes_.append(new BooleanQueryType(7, ObjectTypes::VM, PropertyNames::in_any_appliance));
    
    // Folder recursive queries (Group 8)  
    // C# QueryElement.cs lines 107-109
    this->queryTypes_.append(new RecursiveXMOQueryType(8, ObjectTypes::AllIncFolders, 
                                                       PropertyNames::folder, ObjectTypes::Folder));
    this->queryTypes_.append(new RecursiveXMOListQueryType(8, ObjectTypes::AllIncFolders, 
                                                           PropertyNames::folders, ObjectTypes::Folder));
    this->queryTypes_.append(new NullQueryType(8, ObjectTypes::AllExcFolders, PropertyNames::folder, 
                                               true, "Not in a folder"));
    
    // Custom fields (Group 9)
    // C# QueryElement.cs line 111
    this->queryTypes_.append(new BooleanQueryType(9, ObjectTypes::AllExcFolders, PropertyNames::has_custom_fields));
    
    // NOTE: Dynamic custom field query types are added by CustomFieldsManager
    // C# QueryElement.cs lines 113-116, 119-145
    // TODO: Connect to OtherConfigAndTagsWatcher and CustomFieldsManager when available
    
    // Legacy query types (Group 5) - NOT USED FOR NEW QUERIES, kept for backward compatibility
    // C# QueryElement.cs lines 92-93
    // this->queryTypes_.append(new XenModelObjectListContainsQueryType(...)); // Replaced by recursive queries
    // this->queryTypes_.append(new DiskQueryType(5, ObjectTypes::None, "Disks"));  // Replaced by recursive disk queries
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
