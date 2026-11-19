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

#include "searchmarshalling.h"
#include "search.h"
#include "query.h"
#include "queryscope.h"
#include "queries.h"
#include "grouping.h"
#include "sort.h"
#include "common.h"
#include <QDebug>
#include <QUuid>

using namespace XenSearch;

namespace
{
    const char* const XMLNS = "http://www.citrix.com/XenCenter/XenSearch/schema";
    const char* const EXTENSION = "xensearch"; // C# BrandManager.ExtensionSearch

    // C# PropertyNames members (the names are the same in the port)
    const QList<QPair<PropertyNames, QString>>& propertyNames()
    {
        static const QList<QPair<PropertyNames, QString>> names = {
            {PropertyNames::type, "type"}, {PropertyNames::label, "label"}, {PropertyNames::uuid, "uuid"},
            {PropertyNames::description, "description"}, {PropertyNames::tags, "tags"}, {PropertyNames::host, "host"},
            {PropertyNames::pool, "pool"}, {PropertyNames::networks, "networks"}, {PropertyNames::storage, "storage"},
            {PropertyNames::disks, "disks"}, {PropertyNames::memory, "memory"}, {PropertyNames::os_name, "os_name"},
            {PropertyNames::power_state, "power_state"}, {PropertyNames::virtualisation_status, "virtualisation_status"},
            {PropertyNames::start_time, "start_time"}, {PropertyNames::ha_restart_priority, "ha_restart_priority"},
            {PropertyNames::size, "size"}, {PropertyNames::ip_address, "ip_address"}, {PropertyNames::uptime, "uptime"},
            {PropertyNames::ha_enabled, "ha_enabled"}, {PropertyNames::isNotFullyUpgraded, "isNotFullyUpgraded"},
            {PropertyNames::appliance, "appliance"}, {PropertyNames::shared, "shared"}, {PropertyNames::vm, "vm"},
            {PropertyNames::dockervm, "dockervm"}, {PropertyNames::read_caching_enabled, "read_caching_enabled"},
            {PropertyNames::folder, "folder"}, {PropertyNames::folders, "folders"},
            {PropertyNames::memoryText, "memoryText"}, {PropertyNames::memoryValue, "memoryValue"},
            {PropertyNames::memoryRank, "memoryRank"}, {PropertyNames::cpuText, "cpuText"},
            {PropertyNames::cpuValue, "cpuValue"}, {PropertyNames::diskText, "diskText"},
            {PropertyNames::networkText, "networkText"}, {PropertyNames::haText, "haText"},
            {PropertyNames::connection_hostname, "connection_hostname"}, {PropertyNames::sr_type, "sr_type"},
            {PropertyNames::license, "license"}, {PropertyNames::has_custom_fields, "has_custom_fields"},
            {PropertyNames::in_any_appliance, "in_any_appliance"},
            {PropertyNames::vendor_device_state, "vendor_device_state"}};
        return names;
    }

    QString propertyName(PropertyNames property)
    {
        for (const auto& entry : propertyNames())
            if (entry.first == property)
                return entry.second;
        return QString();
    }

    bool propertyFromName(const QString& name, PropertyNames& property)
    {
        for (const auto& entry : propertyNames())
        {
            if (entry.second == name)
            {
                property = entry.first;
                return true;
            }
        }
        return false;
    }

    // C# SearchMarshalling.AddAttribute(bool) / ParseBool ("True" is for backwards compatibility)
    QString boolText(bool value) { return value ? "yes" : "no"; }
    bool parseBool(const QString& value) { return value == "yes" || value == "True"; }

    // C# StringPropertyQuery.PropertyQueryType
    QString stringTypeName(StringPropertyQuery::MatchType type)
    {
        switch (type)
        {
            case StringPropertyQuery::MatchType::ExactMatch: return "exactmatch";
            case StringPropertyQuery::MatchType::Contains: return "contains";
            case StringPropertyQuery::MatchType::StartsWith: return "startswith";
            case StringPropertyQuery::MatchType::EndsWith: return "endswith";
            case StringPropertyQuery::MatchType::NotContains: return "notcontain";
        }
        return "contains";
    }

    StringPropertyQuery::MatchType stringTypeFromName(const QString& name)
    {
        if (name == "exactmatch") return StringPropertyQuery::MatchType::ExactMatch;
        if (name == "startswith") return StringPropertyQuery::MatchType::StartsWith;
        if (name == "endswith") return StringPropertyQuery::MatchType::EndsWith;
        if (name == "notcontain") return StringPropertyQuery::MatchType::NotContains;
        return StringPropertyQuery::MatchType::Contains;
    }

    // C# DatePropertyQuery.PropertyQueryType
    const QList<QPair<DateQuery::ComparisonType, QString>>& dateTypeNames()
    {
        static const QList<QPair<DateQuery::ComparisonType, QString>> names = {
            {DateQuery::ComparisonType::Today, "today"}, {DateQuery::ComparisonType::Yesterday, "yesterday"},
            {DateQuery::ComparisonType::ThisWeek, "thisweek"}, {DateQuery::ComparisonType::LastWeek, "lastweek"},
            {DateQuery::ComparisonType::Before, "before"}, {DateQuery::ComparisonType::After, "after"},
            {DateQuery::ComparisonType::Exact, "exact"}};
        return names;
    }

    QString dateTypeName(DateQuery::ComparisonType type)
    {
        for (const auto& entry : dateTypeNames())
            if (entry.first == type)
                return entry.second;
        return "exact";
    }

    DateQuery::ComparisonType dateTypeFromName(const QString& name)
    {
        for (const auto& entry : dateTypeNames())
            if (entry.second == name)
                return entry.first;
        return DateQuery::ComparisonType::Exact;
    }

    // C# dates in searches: "yyyyMMdd" (InvariantCulture)
    QString dateText(const QDateTime& date) { return date.date().toString("yyyyMMdd"); }
    QDateTime parseDate(const QString& text)
    {
        const QDate date = QDate::fromString(text, "yyyyMMdd");
        return date.isValid() ? QDateTime(date, QTime(0, 0)) : QDateTime::currentDateTime();
    }

    QDomElement element(QDomDocument& document, const QString& name)
    {
        return document.createElement(name);
    }

    QDomElement firstChildElement(const QDomElement& parent)
    {
        return parent.firstChildElement();
    }

    QDomElement customFieldDefinitionElement(QDomDocument& document, const QString& name, bool date)
    {
        // C# CustomFieldDefinition.ToXmlNode
        QDomElement definition = document.createElement("CustomFieldDefinition");
        definition.setAttribute("name", name);
        definition.setAttribute("type", date ? "Date" : "String");
        definition.setAttribute("defaultValue", "");
        return definition;
    }

    CustomFieldDefinition customFieldDefinitionFrom(const QDomElement& parent)
    {
        const QDomElement definition = parent.firstChildElement("CustomFieldDefinition");
        return CustomFieldDefinition(definition.attribute("name"),
                                     definition.attribute("type") == "Date" ? CustomFieldDefinition::Type::Date
                                                                            : CustomFieldDefinition::Type::String);
    }

    // C# QueryScope.ToXmlNode: one child element per object type
    QDomElement scopeToElement(QDomDocument& document, const QueryScope* scope)
    {
        QDomElement node = document.createElement("QueryScope");
        const int types = scope ? static_cast<int>(scope->getObjectTypes()) : 0;
        for (int bit = 0; bit < 31; ++bit)
        {
            const ObjectTypes type = static_cast<ObjectTypes>(1 << bit);
            const QString name = PropertyAccessors::ObjectTypeName(type);
            if ((types & (1 << bit)) && !name.isEmpty())
                node.appendChild(document.createElement(name));
        }
        return node;
    }

    QueryScope* scopeFromElement(const QDomElement& node)
    {
        int types = 0;
        for (QDomElement child = node.firstChildElement(); !child.isNull(); child = child.nextSiblingElement())
        {
            if (child.tagName() == "NONE" || child.tagName().startsWith("ALL"))
                continue;
            types |= static_cast<int>(PropertyAccessors::ObjectTypeFromName(child.tagName()));
        }
        return new QueryScope(static_cast<ObjectTypes>(types));
    }

    QDomElement propertyElement(QDomDocument& document, const QString& className, PropertyNames property)
    {
        QDomElement node = document.createElement(className);
        node.setAttribute("property", propertyName(property));
        return node;
    }
}

// ============================================================================
// Query filters
// ============================================================================

QDomElement SearchMarshalling::QueryFilterToElement(QDomDocument& document, const QueryFilter* filter)
{
    if (!filter || dynamic_cast<const DummyQuery*>(filter))
        return QDomElement(); // C#: never put into the XML

    if (auto* group = dynamic_cast<const GroupQuery*>(filter))
    {
        QDomElement node = element(document, "GroupQuery");
        for (QueryFilter* subquery : group->getSubQueries())
        {
            QDomElement child = QueryFilterToElement(document, subquery);
            if (!child.isNull())
                node.appendChild(child);
        }
        const char* type = group->getType() == GroupQuery::GroupQueryType::And ? "And"
                         : group->getType() == GroupQuery::GroupQueryType::Or ? "Or" : "Nor";
        node.setAttribute("type", type);
        return node;
    }
    if (auto* recursive = dynamic_cast<const RecursivePropertyQuery*>(filter))
    {
        const bool list = dynamic_cast<const RecursiveXMOListPropertyQuery*>(filter) != nullptr;
        QDomElement node = propertyElement(document, list ? "RecursiveXMOListPropertyQuery" : "RecursiveXMOPropertyQuery",
                                           recursive->getProperty());
        QDomElement child = QueryFilterToElement(document, recursive->getSubQuery());
        if (child.isNull())
            child = element(document, "DummyQuery"); // keeps the element readable; loads as a placeholder
        node.appendChild(child);
        return node;
    }
    if (auto* string = dynamic_cast<const StringPropertyQuery*>(filter))
    {
        QDomElement node = propertyElement(document, "StringPropertyQuery", string->getProperty());
        node.setAttribute("casesensitive", boolText(false));
        node.setAttribute("query", string->getQuery());
        node.setAttribute("type", stringTypeName(string->getMatchType()));
        return node;
    }
    if (auto* enumQuery = dynamic_cast<const EnumQuery*>(filter))
    {
        QDomElement node = propertyElement(document, "EnumPropertyQuery", enumQuery->getProperty());
        node.setAttribute("equals", boolText(!enumQuery->isNegated()));
        node.setAttribute("query", enumQuery->getValue());
        return node;
    }
    if (auto* date = dynamic_cast<const DateQuery*>(filter))
    {
        QDomElement node = propertyElement(document, "DatePropertyQuery", date->getProperty());
        node.setAttribute("query", dateText(date->getValue()));
        node.setAttribute("type", dateTypeName(date->getComparisonType()));
        return node;
    }
    if (auto* numeric = dynamic_cast<const NumericQuery*>(filter))
    {
        QDomElement node = propertyElement(document, "IntPropertyQuery", numeric->getProperty());
        node.setAttribute("query", QString::number(numeric->getValue()));
        const char* type = numeric->getComparisonType() == NumericQuery::ComparisonType::GreaterThan ? "gt"
                         : numeric->getComparisonType() == NumericQuery::ComparisonType::LessThan ? "lt" : "exactmatch";
        node.setAttribute("type", type);
        return node;
    }
    if (auto* boolQuery = dynamic_cast<const BoolQuery*>(filter))
    {
        QDomElement node = propertyElement(document, "BooleanQuery", boolQuery->getProperty());
        node.setAttribute("query", boolText(boolQuery->getValue()));
        return node;
    }
    if (auto* tag = dynamic_cast<const TagQuery*>(filter))
    {
        // C# TagQueryType builds StringListContainsQuery on tags
        QDomElement node = propertyElement(document, "StringListContainsQuery", PropertyNames::tags);
        node.setAttribute("contains", boolText(!tag->isNegated()));
        node.setAttribute("query", tag->getTag());
        return node;
    }
    if (auto* ip = dynamic_cast<const IPAddressQuery*>(filter))
    {
        QDomElement node = propertyElement(document, "IPAddressQuery", ip->getProperty());
        node.setAttribute("contains", boolText(true));
        node.setAttribute("address", ip->getAddress());
        return node;
    }
    if (auto* null = dynamic_cast<const NullPropertyQuery*>(filter))
    {
        QDomElement node = propertyElement(document, "NullQuery", null->getProperty());
        node.setAttribute("query", boolText(null->getIsNull()));
        return node;
    }
    if (auto* value = dynamic_cast<const ValuePropertyQuery*>(filter))
    {
        QDomElement node = propertyElement(document, "ValuePropertyQuery", value->getProperty());
        node.setAttribute("equals", boolText(value->getEquals()));
        node.setAttribute("query", value->getQuery());
        return node;
    }
    if (auto* listEmpty = dynamic_cast<const ListEmptyQuery*>(filter))
    {
        QDomElement node = propertyElement(document, "ListEmptyQuery", listEmpty->getProperty());
        node.setAttribute("empty", boolText(listEmpty->getEmpty()));
        return node;
    }
    if (auto* xmo = dynamic_cast<const XenModelObjectPropertyQuery*>(filter))
    {
        QDomElement node = propertyElement(document, "XenModelObjectPropertyQuery", xmo->getProperty());
        node.setAttribute("equals", boolText(xmo->getEquals()));
        node.setAttribute("uuid", xmo->getUuid());
        return node;
    }
    if (auto* contains = dynamic_cast<const XenModelObjectListContainsQuery*>(filter))
    {
        QDomElement node = propertyElement(document, "XenModelObjectListContainsQuery", contains->getProperty());
        node.setAttribute("contains", boolText(contains->getContains()));
        node.setAttribute("uuid", contains->getUuid());
        return node;
    }
    if (auto* name = dynamic_cast<const XenModelObjectListContainsNameQuery*>(filter))
    {
        QDomElement node = propertyElement(document, "XenModelObjectListContainsNameQuery", name->getProperty());
        node.setAttribute("contains", boolText(true));
        node.setAttribute("query", name->getQuery());
        node.setAttribute("type", stringTypeName(name->getType()));
        return node;
    }
    if (auto* customField = dynamic_cast<const CustomFieldQuery*>(filter))
    {
        QDomElement node = element(document, "CustomFieldQuery");
        node.appendChild(customFieldDefinitionElement(document, customField->getFieldName(), false));
        node.setAttribute("query", customField->getQuery());
        node.setAttribute("type", stringTypeName(customField->getType()));
        return node;
    }
    if (auto* customDate = dynamic_cast<const CustomFieldDateQuery*>(filter))
    {
        QDomElement node = element(document, "CustomFieldDateQuery");
        node.appendChild(customFieldDefinitionElement(document, customDate->getFieldName(), true));
        node.setAttribute("query", dateText(customDate->getQuery()));
        node.setAttribute("type", dateTypeName(customDate->getType()));
        return node;
    }

    qWarning() << "SearchMarshalling: query filter not saved, no C# counterpart";
    return QDomElement();
}

QueryFilter* SearchMarshalling::QueryFilterFromElement(const QDomElement& node)
{
    if (node.isNull())
        return nullptr;

    const QString name = node.tagName();
    PropertyNames property = PropertyNames::label;
    const bool hasProperty = propertyFromName(node.attribute("property"), property);

    if (name == "DummyQuery")
        return new DummyQuery();

    if (name == "GroupQuery")
    {
        const QString type = node.attribute("type");
        const GroupQuery::GroupQueryType groupType = type == "Or" ? GroupQuery::GroupQueryType::Or
                                                   : type == "Nor" ? GroupQuery::GroupQueryType::Nor
                                                                   : GroupQuery::GroupQueryType::And;
        QList<QueryFilter*> subqueries;
        for (QDomElement child = node.firstChildElement(); !child.isNull(); child = child.nextSiblingElement())
        {
            if (QueryFilter* subquery = QueryFilterFromElement(child))
                subqueries.append(subquery);
        }
        return new GroupQuery(groupType, subqueries);
    }

    if (name == "CustomFieldQuery")
        return new CustomFieldQuery(customFieldDefinitionFrom(node).GetName(), node.attribute("query"),
                                    stringTypeFromName(node.attribute("type")));
    if (name == "CustomFieldDateQuery")
        return new CustomFieldDateQuery(customFieldDefinitionFrom(node).GetName(), parseDate(node.attribute("query")),
                                        dateTypeFromName(node.attribute("type")));

    if (!hasProperty)
    {
        qWarning() << "SearchMarshalling: unknown property in" << name << node.attribute("property");
        return nullptr;
    }

    if (name == "RecursiveXMOPropertyQuery" || name == "RecursiveXMOListPropertyQuery")
    {
        QueryFilter* subquery = QueryFilterFromElement(firstChildElement(node));
        if (!subquery)
            subquery = new DummyQuery();
        if (name == "RecursiveXMOListPropertyQuery")
            return new RecursiveXMOListPropertyQuery(property, subquery);
        return new RecursiveXMOPropertyQuery(property, subquery);
    }
    if (name == "StringPropertyQuery")
        return new StringPropertyQuery(property, node.attribute("query"), stringTypeFromName(node.attribute("type")));
    if (name == "EnumPropertyQuery")
    {
        // C#: "SR" means a remote SR in old searches
        QString value = node.attribute("query");
        if (property == PropertyNames::type && value == "SR")
            value = "RemoteSR";
        return new EnumQuery(property, value, !parseBool(node.attribute("equals")));
    }
    if (name == "DatePropertyQuery")
        return new DateQuery(property, parseDate(node.attribute("query")), dateTypeFromName(node.attribute("type")));
    if (name == "IntPropertyQuery")
    {
        const QString type = node.attribute("type");
        const NumericQuery::ComparisonType comparison = type == "gt" ? NumericQuery::ComparisonType::GreaterThan
                                                      : type == "lt" ? NumericQuery::ComparisonType::LessThan
                                                                     : NumericQuery::ComparisonType::Equal;
        return new NumericQuery(property, node.attribute("query").toLongLong(), comparison);
    }
    if (name == "BooleanQuery")
        return new BoolQuery(property, parseBool(node.attribute("query")));
    if (name == "StringListContainsQuery")
        return new TagQuery(node.attribute("query"), !parseBool(node.attribute("contains")));
    if (name == "IPAddressQuery")
        return new IPAddressQuery(property, node.attribute("address"));
    if (name == "NullQuery")
        return new NullPropertyQuery(property, parseBool(node.attribute("query")));
    if (name == "ValuePropertyQuery")
        return new ValuePropertyQuery(property, node.attribute("query"), parseBool(node.attribute("equals")));
    if (name == "ListEmptyQuery")
        return new ListEmptyQuery(property, parseBool(node.attribute("empty")));
    if (name == "XenModelObjectPropertyQuery")
        return new XenModelObjectPropertyQuery(property, node.attribute("uuid"), parseBool(node.attribute("equals")));
    if (name == "XenModelObjectListContainsQuery")
        return new XenModelObjectListContainsQuery(property, node.attribute("uuid"), parseBool(node.attribute("contains")));
    if (name == "XenModelObjectListContainsNameQuery")
        return new XenModelObjectListContainsNameQuery(property, node.attribute("query"), stringTypeFromName(node.attribute("type")));

    qWarning() << "SearchMarshalling: unknown query filter" << name;
    return nullptr;
}

// ============================================================================
// Groupings
// ============================================================================

QDomElement SearchMarshalling::GroupingToElement(QDomDocument& document, const Grouping* grouping)
{
    if (!grouping)
        return QDomElement();

    QDomElement node;
    // The port's dedicated groupings are C# property/object groupings
    if (dynamic_cast<const TypeGrouping*>(grouping))
        node = propertyElement(document, "PropertyGrouping", PropertyNames::type);
    else if (dynamic_cast<const TagsGrouping*>(grouping))
        node = propertyElement(document, "PropertyGrouping", PropertyNames::tags);
    else if (dynamic_cast<const PoolGrouping*>(grouping))
        node = propertyElement(document, "XenModelObjectPropertyGrouping", PropertyNames::pool);
    else if (dynamic_cast<const HostGrouping*>(grouping))
        node = propertyElement(document, "XenModelObjectPropertyGrouping", PropertyNames::host);
    else if (dynamic_cast<const VAppGrouping*>(grouping))
        node = propertyElement(document, "XenModelObjectPropertyGrouping", PropertyNames::appliance);
    else if (dynamic_cast<const FolderGrouping*>(grouping))
        node = element(document, "FolderGrouping");
    else if (auto* xmo = dynamic_cast<const XenModelObjectPropertyGrouping*>(grouping))
        node = propertyElement(document, "XenModelObjectPropertyGrouping", xmo->property());
    else if (auto* boolGrouping = dynamic_cast<const BoolGrouping*>(grouping))
        node = propertyElement(document, "BoolGrouping", boolGrouping->property());
    else if (auto* property = dynamic_cast<const PropertyGrouping*>(grouping))
        node = propertyElement(document, "PropertyGrouping", property->property());
    else if (auto* customField = dynamic_cast<const CustomFieldGrouping*>(grouping))
    {
        node = element(document, "CustomFieldGrouping");
        node.appendChild(customFieldDefinitionElement(document, customField->definition().GetName(),
                                                      customField->definition().GetType() == CustomFieldDefinition::Type::Date));
    } else if (dynamic_cast<const AllCustomFieldsGrouping*>(grouping))
        node = element(document, "AllCustomFieldsGrouping");
    else
        return QDomElement();

    if (grouping->subgrouping())
    {
        QDomElement child = GroupingToElement(document, grouping->subgrouping());
        if (!child.isNull())
            node.appendChild(child);
    }
    return node;
}

Grouping* SearchMarshalling::GroupingFromElement(const QDomElement& node)
{
    if (node.isNull())
        return nullptr;

    // C# Grouping(XmlNode): the first child that is a grouping (not the custom field definition)
    Grouping* subgrouping = nullptr;
    for (QDomElement child = node.firstChildElement(); !child.isNull() && !subgrouping; child = child.nextSiblingElement())
    {
        if (child.tagName() != "CustomFieldDefinition")
            subgrouping = GroupingFromElement(child);
    }

    const QString name = node.tagName();
    PropertyNames property = PropertyNames::label;
    const bool hasProperty = propertyFromName(node.attribute("property"), property);

    if (name == "FolderGrouping")
        return new FolderGrouping(subgrouping);
    if (name == "AllCustomFieldsGrouping")
        return new AllCustomFieldsGrouping(subgrouping);
    if (name == "CustomFieldGrouping")
        return new CustomFieldGrouping(customFieldDefinitionFrom(node), subgrouping);
    if (hasProperty && name == "PropertyGrouping")
    {
        if (property == PropertyNames::type)
            return new TypeGrouping(subgrouping);
        if (property == PropertyNames::tags)
            return new TagsGrouping(subgrouping);
        return new PropertyGrouping(property, subgrouping);
    }
    if (hasProperty && name == "BoolGrouping")
        return new BoolGrouping(property, subgrouping);
    if (hasProperty && name == "XenModelObjectPropertyGrouping")
    {
        if (property == PropertyNames::pool)
            return new PoolGrouping(subgrouping);
        if (property == PropertyNames::host)
            return new HostGrouping(subgrouping);
        if (property == PropertyNames::appliance)
            return new VAppGrouping(subgrouping);
        return new XenModelObjectPropertyGrouping(property, subgrouping);
    }

    qWarning() << "SearchMarshalling: unknown grouping" << name;
    delete subgrouping;
    return nullptr;
}

// ============================================================================
// Searches
// ============================================================================

QDomElement SearchMarshalling::SearchToElement(QDomDocument& document, const Search* search)
{
    QDomElement node = document.createElement("Search");
    node.setAttribute("uuid", search->GetUUID());
    node.setAttribute("name", search->GetName());
    node.setAttribute("major_version", MajorVersion);
    node.setAttribute("minor_version", MinorVersion);

    // C# Query.ToXmlNode
    QDomElement query = document.createElement("Query");
    query.appendChild(scopeToElement(document, search->GetQuery() ? search->GetQuery()->getQueryScope() : nullptr));
    QDomElement filter = QueryFilterToElement(document, search->GetQuery() ? search->GetQuery()->getQueryFilter() : nullptr);
    if (!filter.isNull())
        query.appendChild(filter);
    node.appendChild(query);

    QDomElement grouping = GroupingToElement(document, search->GetGrouping());
    if (!grouping.isNull())
        node.appendChild(grouping);

    if (!search->GetColumns().isEmpty())
    {
        QDomElement columns = document.createElement("columns");
        for (const auto& column : search->GetColumns())
        {
            QDomElement columnNode = document.createElement("column");
            columnNode.setAttribute("name", column.first);
            columnNode.setAttribute("width", QString::number(column.second));
            columns.appendChild(columnNode);
        }
        node.appendChild(columns);
    }

    QDomElement sorting = document.createElement("sorting");
    for (const Sort& sort : search->GetSorting())
    {
        QDomElement sortNode = document.createElement("sort");
        sortNode.setAttribute("column", sort.GetColumn());
        sortNode.setAttribute("ascending", boolText(sort.IsAscending()));
        sorting.appendChild(sortNode);
    }
    node.appendChild(sorting);

    return node;
}

Search* SearchMarshalling::SearchFromElement(const QDomElement& node)
{
    const QString uuid = node.attribute("uuid");
    const QString name = node.attribute("name");
    bool versionOk = false;
    const int majorVersion = node.attribute("major_version").toInt(&versionOk);
    if (!versionOk || majorVersion > MajorVersion)
    {
        qWarning() << "SearchMarshalling: unsupported search version" << node.attribute("major_version");
        return nullptr;
    }

    const QDomElement first = node.firstChildElement();
    Query* query = nullptr;
    if (majorVersion < 2)
    {
        // Versions 0 and 1 held just a filter
        QueryFilter* filter = QueryFilterFromElement(first);
        if (!filter)
            return nullptr;
        query = new Query(nullptr, filter);
    } else
    {
        if (first.tagName() != "Query")
            return nullptr;
        QueryScope* scope = nullptr;
        QueryFilter* filter = nullptr;
        for (QDomElement child = first.firstChildElement(); !child.isNull(); child = child.nextSiblingElement())
        {
            if (child.tagName() == "QueryScope")
                scope = scopeFromElement(child);
            else
                filter = QueryFilterFromElement(child);
        }
        if (!scope)
        {
            delete filter;
            return nullptr;
        }
        query = new Query(scope, filter);
    }

    Grouping* grouping = nullptr;
    QList<Sort> sorting;
    QList<QPair<QString, int>> columns;
    for (QDomElement child = first.nextSiblingElement(); !child.isNull(); child = child.nextSiblingElement())
    {
        if (child.tagName() == "columns")
        {
            for (QDomElement column = child.firstChildElement(); !column.isNull(); column = column.nextSiblingElement())
            {
                const QString columnName = column.attribute("name");
                if (columnName == "state") // CA-25116: column no longer exists
                    continue;
                bool ok = false;
                const int width = column.attribute("width").toInt(&ok);
                if (ok)
                    columns.append(qMakePair(columnName, qAbs(width)));
            }
        } else if (child.tagName() == "sorting")
        {
            for (QDomElement sort = child.firstChildElement(); !sort.isNull(); sort = sort.nextSiblingElement())
                sorting.append(Sort(sort.attribute("column"), parseBool(sort.attribute("ascending"))));
        } else if (!grouping)
        {
            grouping = GroupingFromElement(child);
        }
    }

    return new Search(query, grouping, name, uuid, false, columns, sorting);
}

QString SearchMarshalling::SearchToXml(const Search* search)
{
    QDomDocument document;
    document.appendChild(SearchToElement(document, search));
    return document.toString(-1).trimmed();
}

QString SearchMarshalling::SearchesToXml(const QList<const Search*>& searches)
{
    // C# SearchesToXML: declaration, doctype, <Searches xmlns=...>, indented by 4
    QDomDocument document;
    QDomElement root = document.createElement("Searches");
    root.setAttribute("xmlns", XMLNS);
    document.appendChild(root);
    for (const Search* search : searches)
        root.appendChild(SearchToElement(document, search));

    const QString upper = QString(EXTENSION).toUpper();
    return QStringLiteral("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n")
        + QStringLiteral("<!DOCTYPE %1 PUBLIC \"-//%2//DTD %2 1//EN\" \"%1-1.dtd\">\n").arg(EXTENSION, upper)
        + document.toString(4);
}

Search* SearchMarshalling::LoadSearch(const QString& xml)
{
    // "null" was the default in old versions
    if (xml.isEmpty() || xml == "null")
        return nullptr;

    QDomDocument document;
    if (!static_cast<bool>(document.setContent(xml)))
    {
        qWarning() << "SearchMarshalling: cannot parse saved search";
        return nullptr;
    }
    return SearchFromElement(document.documentElement());
}

QList<Search*> SearchMarshalling::LoadSearches(const QString& xml, bool* ok)
{
    QList<Search*> result;
    QDomDocument document;
    const bool parsed = static_cast<bool>(document.setContent(xml));
    if (ok)
        *ok = parsed;
    if (!parsed)
        return result;

    // "search" is for backwards compatibility
    for (const QString& tag : {QStringLiteral("Search"), QStringLiteral("search")})
    {
        const QDomNodeList nodes = document.elementsByTagName(tag);
        for (int i = 0; i < nodes.count(); ++i)
        {
            if (Search* search = SearchFromElement(nodes.at(i).toElement()))
                result.append(search);
        }
    }
    return result;
}
