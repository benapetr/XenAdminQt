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
#include "globals.h"
#include "xenlib/xensearch/queries.h"
#include "xenlib/xen/network/connectionsmanager.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xencache.h"
#include "xenlib/otherconfig/otherconfigandtagswatcher.h"
#include "xenlib/customfields/customfieldsmanager.h"
#include <algorithm>
#include <QSet>

using namespace XenSearch;

namespace
{
    // C# PropertyAccessors.PropertyNames_i18n: the label shown in the query type drop-down
    QString propertyDisplayName(PropertyNames property)
    {
        const QString name = PropertyAccessors::GetPropertyDisplayName(property);
        return name.isEmpty() ? getPropertyName(property) : name;
    }
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
    return QObject::tr("Select a filter...");
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
    
    queryElement->setGroupSubQueries(groupQuery->getSubQueries());
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
            return QObject::tr("All of the following:");
        case GroupType::Or:
            return QObject::tr("Any of the following:");
        case GroupType::Nor:
            return QObject::tr("None of the following:");
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
    
    return propertyDisplayName(this->property_);
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
    // Match the stored name to an offered one regardless of case or flag spacing
    for (const auto& entry : PropertyAccessors::EnumValues(this->property_))
    {
        if (QString(entry.second).remove(' ').compare(QString(enumQuery->getValue()).remove(' '), Qt::CaseInsensitive) == 0)
            queryElement->setComboBoxValue(entry.second);
    }
}

QueryFilter* EnumPropertyQueryType::GetQuery(QueryElement* queryElement) const
{
    const bool negated = (queryElement->getMatchTypeSelection() == "Is not");
    const QString value = queryElement->getComboBoxValue().toString();
    if (value.isEmpty())
        return nullptr;
    return new EnumQuery(this->property_, value, negated);
}

QString EnumPropertyQueryType::toString() const
{
    return propertyDisplayName(this->property_);
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
    // C# PropertyAccessors.Geti18nFor: labels for the C# enum names the query stores
    QVariantList values;
    for (const auto& entry : PropertyAccessors::EnumValues(this->property_))
        values.append(QueryElement::ComboEntry(entry.first, entry.second));
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
    return dateQuery && dateQuery->getProperty() == this->property_;
}

QueryFilter* DatePropertyQueryType::GetQuery(QueryElement* queryElement) const
{
    QDateTime dateTime;
    DateQuery::ComparisonType type;
    dateQueryFrom(queryElement, dateTime, type);
    return new DateQuery(this->property_, dateTime, type);
}

// C# DatePropertyQueryType.GetQuery: the chosen type with the picker's date
void DatePropertyQueryType::dateQueryFrom(QueryElement* queryElement, QDateTime& dateTime,
                                          DateQuery::ComparisonType& type)
{
    type = comparisonTypeFromString(queryElement->getMatchTypeSelection());
    dateTime = queryElement->getDateTimeValue();
}

void DatePropertyQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    DateQuery* dateQuery = dynamic_cast<DateQuery*>(query);
    if (dateQuery)
        showDateQuery(queryElement, dateQuery->getValue(), dateQuery->getComparisonType());
}

void DatePropertyQueryType::showDateQuery(QueryElement* queryElement, const QDateTime& dateTime,
                                          DateQuery::ComparisonType type)
{
    queryElement->setMatchTypeSelection(comparisonTypeToString(type));
    queryElement->setDateTimeValue(dateTime);
}

QString DatePropertyQueryType::toString() const
{
    return propertyDisplayName(this->property_);
}

bool DatePropertyQueryType::showDateTimePicker(QueryElement* queryElement) const
{
    return showsDateTimePicker(queryElement);
}

// C# ShouldShowDateTimePicker: only before, after and exact use a date
bool DatePropertyQueryType::showsDateTimePicker(QueryElement* queryElement)
{
    const ComparisonType type = comparisonTypeFromString(queryElement->getMatchTypeSelection());
    return type == ComparisonType::Before || type == ComparisonType::After || type == ComparisonType::Exact;
}

QStringList DatePropertyQueryType::getMatchTypeComboButtonEntries() const
{
    return matchTypeEntries();
}

// C# DatePropertyQueryType.MatchTypeEntries
QStringList DatePropertyQueryType::matchTypeEntries()
{
    QStringList entries;
    for (ComparisonType type : {ComparisonType::Today, ComparisonType::Yesterday, ComparisonType::ThisWeek,
                                ComparisonType::LastWeek, ComparisonType::Before, ComparisonType::After,
                                ComparisonType::Exact})
        entries << comparisonTypeToString(type);
    return entries;
}

DatePropertyQueryType::ComparisonType DatePropertyQueryType::comparisonTypeFromString(const QString& str)
{
    for (ComparisonType type : {ComparisonType::Today, ComparisonType::Yesterday, ComparisonType::ThisWeek,
                                ComparisonType::LastWeek, ComparisonType::Before, ComparisonType::After,
                                ComparisonType::Exact})
    {
        if (comparisonTypeToString(type) == str)
            return type;
    }
    return ComparisonType::Today;
}

QString DatePropertyQueryType::comparisonTypeToString(ComparisonType type)
{
    switch (type)
    {
        case ComparisonType::Today:     return QObject::tr("is today");
        case ComparisonType::Yesterday: return QObject::tr("was yesterday");
        case ComparisonType::ThisWeek:  return QObject::tr("is this week");
        case ComparisonType::LastWeek:  return QObject::tr("was last week");
        case ComparisonType::Before:    return QObject::tr("is before");
        case ComparisonType::After:     return QObject::tr("is after");
        case ComparisonType::Exact:     return QObject::tr("exactly matches");
    }
    return QObject::tr("is today");
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
    return propertyDisplayName(this->property_);
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
    
    Xen::ConnectionsManager* connectionsManager = Xen::ConnectionsManager::instance();
    const QList<XenConnection*> connections = connectionsManager ? connectionsManager->GetAllConnections()
                                                                 : QList<XenConnection*>();
    for (XenConnection* connection : connections)
    {
        XenCache* cache = connection ? connection->GetCache() : nullptr;
        if (!cache)
            continue;
        for (const auto& object : cache->GetXenSearchableObjects())
        {
            const QVariantList tags = cache->ResolveObjectData(object.first, object.second).value("tags").toList();
            for (const QVariant& tag : tags)
            {
                const QString text = tag.toString();
                if (!text.isEmpty())
                    uniqueTags.insert(text);
            }
        }
    }

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
    return propertyDisplayName(this->property_);
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
    return propertyDisplayName(this->property_);
}

void ValuePropertyQueryType::populateCollectedValues()
{
    this->collectedValues_.clear();
    
    Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
    QList<XenConnection*> connections = connMgr->GetAllConnections();
    
    // C# only iterates VMs - see comment in C# code at line 1455
    for (XenConnection* conn : connections)
    {
        if (!conn || !conn->GetCache())
            continue;
        
        // Monitor cache changes for this connection
        connect(conn->GetCache(), &XenCache::itemChanged, this, &ValuePropertyQueryType::onCacheChanged,
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

void ValuePropertyQueryType::onCacheChanged(XenConnection* connection, XenObjectType type, const QString& ref)
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
    // C# PropertyQueryType<String>.ForQuery: a string query on the UUID. Only exact matches are
    // object picks; "UUID starts with" belongs to UuidStringQueryType.
    auto* stringQuery = dynamic_cast<StringPropertyQuery*>(query);
    return stringQuery && stringQuery->getProperty() == this->property_ &&
           stringQuery->getMatchType() == StringPropertyQuery::MatchType::ExactMatch;
}

QString UuidQueryType::toString() const
{
    // C# Messages.UUID_SEARCH, shown as "<relation> Is <object>" in a parent/child query
    return QObject::tr("Is");
}

bool UuidQueryType::showResourceSelectButton(QueryElement* queryElement) const
{
    Q_UNUSED(queryElement);
    return true;
}

QueryFilter* UuidQueryType::GetQuery(QueryElement* queryElement) const
{
    return new StringPropertyQuery(this->property_, queryElement->getResourceSelectionUuid(),
                                   StringPropertyQuery::MatchType::ExactMatch);
}

void UuidQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    auto* stringQuery = dynamic_cast<StringPropertyQuery*>(query);
    if (stringQuery)
        queryElement->setResourceSelectionUuid(stringQuery->getQuery());
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
    
    for (XenConnection* conn : connMgr->GetAllConnections())
    {
        if (conn && conn->GetCache())
        {
            // Monitor cache object changes (C# Cache.RegisterBatchCollectionChanged<O>)
            QObject::connect(conn->GetCache(), &XenCache::itemChanged, 
                    this, &RecursiveQueryTypeBase::onCacheChanged, Qt::UniqueConnection);
        }
    }
}

void RecursiveQueryTypeBase::onCacheChanged()
{
    // C# raises SomeThingChanged here so the drop-downs refresh. The only object list of a
    // parent/child row is the resource picker of its sub-row, which refills itself each time it
    // opens, so nothing needs refreshing here.
}

// C# RecursiveQueryType.GetQuery: the query of the (single) sub-row, or a placeholder
QueryFilter* RecursiveQueryTypeBase::subQueryFrom(QueryElement* queryElement) const
{
    QList<QueryFilter*> subQueries = queryElement ? queryElement->getSubQueries() : QList<QueryFilter*>();
    QueryFilter* subQuery = subQueries.isEmpty() ? new DummyQuery() : subQueries.takeFirst();
    qDeleteAll(subQueries);

    // CA-30595: "parent folder is (blank)" means "parent folder is /"
    if (this->property_ == PropertyNames::folder)
    {
        auto* stringQuery = dynamic_cast<StringPropertyQuery*>(subQuery);
        if (stringQuery && stringQuery->getProperty() == PropertyNames::uuid && stringQuery->getQuery().isEmpty())
        {
            delete subQuery;
            subQuery = new StringPropertyQuery(PropertyNames::uuid, "/", StringPropertyQuery::MatchType::ExactMatch);
        }
    }

    return subQuery;
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
    return propertyDisplayName(this->property_);
}

QueryFilter* RecursiveXMOQueryType::GetQuery(QueryElement* queryElement) const
{
    return this->newQuery(this->property_, this->subQueryFrom(queryElement));
}

void RecursiveXMOQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    auto* recursiveQuery = dynamic_cast<RecursiveXMOPropertyQuery*>(query);
    if (recursiveQuery)
        queryElement->setParentChildSubQuery(recursiveQuery->getSubQuery(), this->subQueryScope_);
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
    return propertyDisplayName(this->property_);
}

QueryFilter* RecursiveXMOListQueryType::GetQuery(QueryElement* queryElement) const
{
    return this->newQuery(this->property_, this->subQueryFrom(queryElement));
}

void RecursiveXMOListQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    auto* recursiveQuery = dynamic_cast<RecursiveXMOListPropertyQuery*>(query);
    if (recursiveQuery)
        queryElement->setParentChildSubQuery(recursiveQuery->getSubQuery(), this->subQueryScope_);
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
    
    for (XenConnection* conn : connMgr->GetAllConnections())
    {
        if (conn && conn->GetCache())
        {
            QObject::connect(conn->GetCache(), &XenCache::itemChanged,
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
    return propertyDisplayName(this->property_);
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
    
    for (XenConnection* conn : connMgr->GetAllConnections())
    {
        if (conn && conn->GetCache())
        {
            QObject::connect(conn->GetCache(), &XenCache::itemChanged,
                    this, &XenModelObjectListContainsQueryType::onCacheChanged, Qt::UniqueConnection);
        }
    }
}

void XenModelObjectListContainsQueryType::onCacheChanged(XenConnection* connection, XenObjectType type, const QString& ref)
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
    return propertyDisplayName(this->property_);
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

MatchType::MatchType(const QString& matchText) : matchText_(matchText)
{
}

bool MatchType::showComboButton(QueryElement *queryElement) const
{
    (void) queryElement;
    return false;
}

bool MatchType::showTextBox(QueryElement *queryElement) const
{
    (void) queryElement;
    return false;
}

bool MatchType::showNumericUpDown(QueryElement *queryElement) const
{
    (void) queryElement;
    return false;
}

QString MatchType::units(QueryElement *queryElement) const
{
    (void) queryElement;
    return QString();
}

// ============================================================================
// XMOListContains - "Contained in" / "Not contained in" match type
// ============================================================================

XMOListContains::XMOListContains(PropertyNames property, bool contains, const QString& matchText, const QString& objectType)
    : MatchType(matchText), property_(property), contains_(contains), objectType_(objectType)
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

IntMatch::IntMatch(PropertyNames property, const QString& matchText, const QString& units, qint64 multiplier, NumericQuery::ComparisonType type)
    : MatchType(matchText), property_(property), type_(type), units_(units), multiplier_(multiplier)
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
    : QueryType(group, appliesTo), i18n_(i18n)
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

DiskQueryType::DiskQueryType(int group, ObjectTypes appliesTo, const QString& i18n) : MatchQueryType(group, appliesTo, i18n)
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

LongQueryType::LongQueryType(int group, ObjectTypes appliesTo, const QString& i18n, PropertyNames property, qint64 multiplier, const QString& unit)
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
// Custom field query types (C# CustomFieldQueryTypeBase and subclasses)
// ============================================================================

CustomFieldQueryTypeBase::CustomFieldQueryTypeBase(int group, ObjectTypes appliesTo, const QString& fieldName)
    : QueryType(group, appliesTo)
    , fieldName_(fieldName)
{
}

bool CustomFieldQueryTypeBase::ForQuery(QueryFilter* query) const
{
    auto* customFieldQuery = dynamic_cast<CustomFieldQueryBase*>(query);
    return customFieldQuery && customFieldQuery->getFieldName() == this->fieldName_;
}

CustomFieldStringQueryType::CustomFieldStringQueryType(int group, ObjectTypes appliesTo, const QString& fieldName)
    : CustomFieldQueryTypeBase(group, appliesTo, fieldName)
{
}

bool CustomFieldStringQueryType::ForQuery(QueryFilter* query) const
{
    // C# compares the definition (name and type)
    return dynamic_cast<CustomFieldQuery*>(query) && CustomFieldQueryTypeBase::ForQuery(query);
}

bool CustomFieldStringQueryType::showTextBox(QueryElement* queryElement) const
{
    Q_UNUSED(queryElement);
    return true;
}

QStringList CustomFieldStringQueryType::getMatchTypeComboButtonEntries() const
{
    // C# StringPropertyQueryType.MatchTypeEntries
    using MatchType = StringPropertyQuery::MatchType;
    return QStringList()
        << StringPropertyQueryType::matchTypeToString(MatchType::Contains)
        << StringPropertyQueryType::matchTypeToString(MatchType::NotContains)
        << StringPropertyQueryType::matchTypeToString(MatchType::StartsWith)
        << StringPropertyQueryType::matchTypeToString(MatchType::EndsWith)
        << StringPropertyQueryType::matchTypeToString(MatchType::ExactMatch);
}

QueryFilter* CustomFieldStringQueryType::GetQuery(QueryElement* queryElement) const
{
    return new CustomFieldQuery(this->fieldName_, queryElement->getTextBoxValue(),
                                StringPropertyQueryType::matchTypeFromString(queryElement->getMatchTypeSelection()));
}

void CustomFieldStringQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    auto* customFieldQuery = dynamic_cast<CustomFieldQuery*>(query);
    if (!customFieldQuery)
        return;
    queryElement->setMatchTypeSelection(StringPropertyQueryType::matchTypeToString(customFieldQuery->getType()));
    queryElement->setTextBoxValue(customFieldQuery->getQuery());
}

CustomFieldDateQueryType::CustomFieldDateQueryType(int group, ObjectTypes appliesTo, const QString& fieldName)
    : CustomFieldQueryTypeBase(group, appliesTo, fieldName)
{
}

bool CustomFieldDateQueryType::ForQuery(QueryFilter* query) const
{
    return dynamic_cast<CustomFieldDateQuery*>(query) && CustomFieldQueryTypeBase::ForQuery(query);
}

bool CustomFieldDateQueryType::showDateTimePicker(QueryElement* queryElement) const
{
    // C# DatePropertyQueryType.ShouldShowDateTimePicker
    return DatePropertyQueryType::showsDateTimePicker(queryElement);
}

QStringList CustomFieldDateQueryType::getMatchTypeComboButtonEntries() const
{
    // C# DatePropertyQueryType.MatchTypeEntries
    return DatePropertyQueryType::matchTypeEntries();
}

QueryFilter* CustomFieldDateQueryType::GetQuery(QueryElement* queryElement) const
{
    QDateTime dateTime;
    DateQuery::ComparisonType type;
    DatePropertyQueryType::dateQueryFrom(queryElement, dateTime, type);
    return new CustomFieldDateQuery(this->fieldName_, dateTime, type);
}

void CustomFieldDateQueryType::FromQuery(QueryFilter* query, QueryElement* queryElement)
{
    auto* dateQuery = dynamic_cast<CustomFieldDateQuery*>(query);
    if (dateQuery)
        DatePropertyQueryType::showDateQuery(queryElement, dateQuery->getQuery(), dateQuery->getType());
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
    qDeleteAll(this->customFieldTypes_);
    qDeleteAll(this->retiredCustomFieldTypes_);
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
    // Picking one object; only offered as the sub-row of a parent/child query (C# WantQueryType)
    this->queryTypes_.append(new UuidQueryType(1, (ObjectTypes::AllIncFolders & ~ObjectTypes::VDI) | ObjectTypes::Appliance,
                                               PropertyNames::uuid));
    this->queryTypes_.append(new StringPropertyQueryType(1, ObjectTypes::AllIncFolders | ObjectTypes::Appliance, PropertyNames::label));
    this->queryTypes_.append(new StringPropertyQueryType(1, ObjectTypes::AllExcFolders | ObjectTypes::Appliance, PropertyNames::description));
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
    
    // One query type per custom field definition, kept up to date (C# QueryElement static
    // constructor and CustomFieldsManager_CustomFieldsChanged)
    QObject::connect(CustomFieldsManager::instance(), &CustomFieldsManager::customFieldsChanged,
                     [this]() { this->updateCustomFieldTypes(); });
    this->updateCustomFieldTypes();
    
    // Legacy query types (Group 5) - NOT USED FOR NEW QUERIES, kept for backward compatibility
    // C# QueryElement.cs lines 92-93
    // this->queryTypes_.append(new XenModelObjectListContainsQueryType(...)); // Replaced by recursive queries
    // this->queryTypes_.append(new DiskQueryType(5, ObjectTypes::None, "Disks"));  // Replaced by recursive disk queries
}

void QueryTypeRegistry::updateCustomFieldTypes()
{
    const QList<CustomFieldDefinition> definitions = CustomFieldsManager::instance()->getCustomFields();

    auto matches = [](CustomFieldQueryTypeBase* type, const CustomFieldDefinition& definition) {
        const bool isDate = dynamic_cast<CustomFieldDateQueryType*>(type) != nullptr;
        return type->getFieldName() == definition.GetName() &&
               isDate == (definition.GetType() == CustomFieldDefinition::Type::Date);
    };

    // Add types for new definitions
    for (const CustomFieldDefinition& definition : definitions)
    {
        const bool exists = std::any_of(this->customFieldTypes_.begin(), this->customFieldTypes_.end(),
                                        [&](CustomFieldQueryTypeBase* t) { return matches(t, definition); });
        if (exists)
            continue;
        if (definition.GetType() == CustomFieldDefinition::Type::Date)
            this->customFieldTypes_.append(new CustomFieldDateQueryType(8, ObjectTypes::AllExcFolders, definition.GetName()));
        else
            this->customFieldTypes_.append(new CustomFieldStringQueryType(8, ObjectTypes::AllExcFolders, definition.GetName()));
    }

    // Stop offering types whose definition is gone. They are kept alive because a search row may
    // still be showing one.
    for (int i = this->customFieldTypes_.size() - 1; i >= 0; --i)
    {
        CustomFieldQueryTypeBase* type = this->customFieldTypes_.at(i);
        const bool defined = std::any_of(definitions.begin(), definitions.end(),
                                         [&](const CustomFieldDefinition& d) { return matches(type, d); });
        if (!defined)
            this->retiredCustomFieldTypes_.append(this->customFieldTypes_.takeAt(i));
    }

    this->allQueryTypes_ = this->queryTypes_;
    for (CustomFieldQueryTypeBase* type : this->customFieldTypes_)
        this->allQueryTypes_.append(type);
}

QueryType* QueryTypeRegistry::findQueryTypeForFilter(QueryFilter* filter) const
{
    if (!filter)
        return this->defaultQueryType_;
    
    for (QueryType* queryType : this->allQueryTypes_)
    {
        if (queryType->ForQuery(filter))
            return queryType;
    }
    
    return this->defaultQueryType_;
}
