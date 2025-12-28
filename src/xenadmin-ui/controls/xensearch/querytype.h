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

#ifndef QUERYTYPE_H
#define QUERYTYPE_H

#include <QString>
#include <QVariant>
#include <QVariantList>
#include "searcher.h"
#include "../../../xenlib/xensearch/queryfilter.h"
#include "../../../xenlib/xensearch/queries.h"

// Forward declarations
class QueryElement;

/**
 * Base class for all query types
 * Matches C# QueryType abstract class
 */
class QueryType
{
public:
    enum class Category
    {
        Single,      // Match the object itself
        ParentChild, // Match ancestor or descendant object
        Group        // A group query (And/Or/Nor)
    };

    QueryType(int group, ObjectTypes appliesTo);
    virtual ~QueryType();

    // Check if this query type is appropriate for the given query filter
    virtual bool ForQuery(QueryFilter* query) const = 0;
    
    // Populate UI from query filter
    virtual void FromQuery(QueryFilter* query, QueryElement* queryElement) = 0;
    
    // Build query filter from UI
    virtual QueryFilter* GetQuery(QueryElement* queryElement) const = 0;
    
    // Display name for this query type
    virtual QString toString() const = 0;
    
    // Category of this query type
    virtual Category getCategory() const { return Category::Single; }
    
    // For ParentChild queries, the scope of the subquery
    virtual QueryScope* getSubQueryScope() const { return nullptr; }
    
    // UI element visibility
    virtual bool showMatchTypeComboButton() const { return false; }
    virtual bool showTextBox(QueryElement* queryElement) const { Q_UNUSED(queryElement); return false; }
    virtual bool showComboButton(QueryElement* queryElement) const { Q_UNUSED(queryElement); return false; }
    virtual bool showNumericUpDown(QueryElement* queryElement) const { Q_UNUSED(queryElement); return false; }
    virtual bool showDateTimePicker(QueryElement* queryElement) const { Q_UNUSED(queryElement); return false; }
    virtual bool showResourceSelectButton(QueryElement* queryElement) const { Q_UNUSED(queryElement); return false; }
    
    // UI element data
    virtual QStringList getMatchTypeComboButtonEntries() const { return QStringList(); }
    virtual QVariantList getComboButtonEntries(QueryElement* queryElement) const { Q_UNUSED(queryElement); return QVariantList(); }
    virtual QString getUnits(QueryElement* queryElement) const { Q_UNUSED(queryElement); return QString(); }
    
    int getGroup() const { return this->group_; }
    ObjectTypes getAppliesTo() const { return this->appliesTo_; }

protected:
    int group_;
    ObjectTypes appliesTo_;
};

/**
 * Dummy query type - "Select a filter..."
 */
class DummyQueryType : public QueryType
{
public:
    DummyQueryType(int group, ObjectTypes appliesTo);
    
    bool ForQuery(QueryFilter* query) const override;
    void FromQuery(QueryFilter* query, QueryElement* queryElement) override;
    QueryFilter* GetQuery(QueryElement* queryElement) const override;
    QString toString() const override;
};

/**
 * Group query type - And/Or/Nor
 */
class GroupQueryType : public QueryType
{
public:
    enum class GroupType
    {
        And,
        Or,
        Nor
    };

    GroupQueryType(int group, ObjectTypes appliesTo, GroupType type);
    
    Category getCategory() const override { return Category::Group; }
    QueryScope* getSubQueryScope() const override;
    
    bool ForQuery(QueryFilter* query) const override;
    void FromQuery(QueryFilter* query, QueryElement* queryElement) override;
    QueryFilter* GetQuery(QueryElement* queryElement) const override;
    QString toString() const override;
    
    GroupType getType() const { return this->type_; }

private:
    GroupType type_;
};

/**
 * String property query type (name, description, etc.)
 */
class StringPropertyQueryType : public QueryType
{
public:
    // Use MatchType from StringPropertyQuery
    using MatchType = StringPropertyQuery::MatchType;

    StringPropertyQueryType(int group, ObjectTypes appliesTo, PropertyNames property);
    StringPropertyQueryType(int group, ObjectTypes appliesTo, PropertyNames property, const QString& customName);
    
    bool ForQuery(QueryFilter* query) const override;
    void FromQuery(QueryFilter* query, QueryElement* queryElement) override;
    QueryFilter* GetQuery(QueryElement* queryElement) const override;
    QString toString() const override;
    
    bool showMatchTypeComboButton() const override { return true; }
    bool showTextBox(QueryElement* queryElement) const override { Q_UNUSED(queryElement); return true; }
    QStringList getMatchTypeComboButtonEntries() const override;
    
    static MatchType matchTypeFromString(const QString& str);
    static QString matchTypeToString(MatchType type);

protected:
    PropertyNames property_;
    QString customName_;
};

/**
 * Enum property query type (power state, OS, type, etc.)
 */
class EnumPropertyQueryType : public QueryType
{
public:
    EnumPropertyQueryType(int group, ObjectTypes appliesTo, PropertyNames property);
    
    bool ForQuery(QueryFilter* query) const override;
    void FromQuery(QueryFilter* query, QueryElement* queryElement) override;
    QueryFilter* GetQuery(QueryElement* queryElement) const override;
    QString toString() const override;
    
    bool showMatchTypeComboButton() const override { return true; }
    bool showComboButton(QueryElement* queryElement) const override;
    QStringList getMatchTypeComboButtonEntries() const override;
    QVariantList getComboButtonEntries(QueryElement* queryElement) const override;

protected:
    PropertyNames property_;
    QVariantList getEnumValues() const;
};

/**
 * Numeric property query type (memory, disk size, CPU count, etc.)
 */
class NumericPropertyQueryType : public QueryType
{
public:
    // Use ComparisonType from NumericQuery
    using ComparisonType = NumericQuery::ComparisonType;

    NumericPropertyQueryType(int group, ObjectTypes appliesTo, PropertyNames property, 
                            const QString& name, qint64 multiplier, const QString& units);
    
    bool ForQuery(QueryFilter* query) const override;
    void FromQuery(QueryFilter* query, QueryElement* queryElement) override;
    QueryFilter* GetQuery(QueryElement* queryElement) const override;
    QString toString() const override;
    
    bool showMatchTypeComboButton() const override { return true; }
    bool showNumericUpDown(QueryElement* queryElement) const override { Q_UNUSED(queryElement); return true; }
    QStringList getMatchTypeComboButtonEntries() const override;
    QString getUnits(QueryElement* queryElement) const override { Q_UNUSED(queryElement); return this->units_; }
    
    static ComparisonType comparisonTypeFromString(const QString& str);
    static QString comparisonTypeToString(ComparisonType type);

protected:
    PropertyNames property_;
    QString name_;
    qint64 multiplier_;
    QString units_;
};

/**
 * Date property query type (start time, last boot time, etc.)
 */
class DatePropertyQueryType : public QueryType
{
public:
    // Extended enum for UI - includes relative time ranges
    enum class ComparisonType
    {
        Before,
        After,
        Exact,
        Last24Hours,
        Last7Days,
        LastMonth
    };

    DatePropertyQueryType(int group, ObjectTypes appliesTo, PropertyNames property);
    
    bool ForQuery(QueryFilter* query) const override;
    void FromQuery(QueryFilter* query, QueryElement* queryElement) override;
    QueryFilter* GetQuery(QueryElement* queryElement) const override;
    QString toString() const override;
    
    bool showMatchTypeComboButton() const override { return true; }
    bool showDateTimePicker(QueryElement* queryElement) const override;
    QStringList getMatchTypeComboButtonEntries() const override;
    
    static ComparisonType comparisonTypeFromString(const QString& str);
    static QString comparisonTypeToString(ComparisonType type);

protected:
    PropertyNames property_;
};

/**
 * Boolean property query type (HA enabled, shared, etc.)
 */
class BooleanQueryType : public QueryType
{
public:
    BooleanQueryType(int group, ObjectTypes appliesTo, PropertyNames property);
    
    bool ForQuery(QueryFilter* query) const override;
    void FromQuery(QueryFilter* query, QueryElement* queryElement) override;
    QueryFilter* GetQuery(QueryElement* queryElement) const override;
    QString toString() const override;
    
    bool showMatchTypeComboButton() const override { return true; }
    QStringList getMatchTypeComboButtonEntries() const override;

protected:
    PropertyNames property_;
};

/**
 * Tag query type
 */
class TagQueryType : public QueryType
{
public:
    TagQueryType(int group, ObjectTypes appliesTo);
    
    bool ForQuery(QueryFilter* query) const override;
    void FromQuery(QueryFilter* query, QueryElement* queryElement) override;
    QueryFilter* GetQuery(QueryElement* queryElement) const override;
    QString toString() const override;
    
    bool showMatchTypeComboButton() const override { return true; }
    bool showTextBox(QueryElement* queryElement) const override { Q_UNUSED(queryElement); return true; }
    QStringList getMatchTypeComboButtonEntries() const override;
};

/**
 * IP Address query type
 * 
 * C# Equivalent: IPAddressQueryType in QueryElement.cs (extends PropertyQueryType<List<ComparableAddress>>)
 * Matches IP addresses (simplified version without full CIDR support)
 */
class IPAddressQueryType : public QueryType
{
public:
    IPAddressQueryType(int group, ObjectTypes appliesTo, PropertyNames property);
    
    bool ForQuery(QueryFilter* query) const override;
    void FromQuery(QueryFilter* query, QueryElement* queryElement) override;
    QueryFilter* GetQuery(QueryElement* queryElement) const override;
    QString toString() const override;
    
    bool showMatchTypeComboButton() const override { return true; }
    bool showTextBox(QueryElement* queryElement) const override { Q_UNUSED(queryElement); return true; }
    QStringList getMatchTypeComboButtonEntries() const override;

protected:
    PropertyNames property_;
};

/**
 * Null property query type - checks if a reference is null or not null
 * 
 * C# Equivalent: NullQueryType<T> in QueryElement.cs (extends PropertyQueryType<T> where T : XenObject<T>)
 * Used for "Is standalone", "Not in a folder", etc.
 */
class NullPropertyQueryType : public QueryType
{
public:
    NullPropertyQueryType(int group, ObjectTypes appliesTo, PropertyNames property, bool isNull, const QString& displayName);
    
    bool ForQuery(QueryFilter* query) const override;
    void FromQuery(QueryFilter* query, QueryElement* queryElement) override;
    QueryFilter* GetQuery(QueryElement* queryElement) const override;
    QString toString() const override;

protected:
    PropertyNames property_;
    bool isNull_;
    QString displayName_;
};

/**
 * UUID string query type - searches for UUID strings
 * 
 * C# Equivalent: UuidStringQueryType in QueryElement.cs (extends StringPropertyQueryType)
 * Limits match types to "startswith" and "exactmatch" only
 */
class UuidStringQueryType : public StringPropertyQueryType
{
public:
    UuidStringQueryType(int group, ObjectTypes appliesTo);
    
    QStringList getMatchTypeComboButtonEntries() const override;
};

/**
 * QueryType Registry - manages all available query types
 */
class QueryTypeRegistry
{
public:
    static QueryTypeRegistry* instance();
    
    void initialize();
    const QList<QueryType*>& getAllQueryTypes() const { return this->queryTypes_; }
    QueryType* getDefaultQueryType() const { return this->defaultQueryType_; }
    
    // Find appropriate query type for a given filter
    QueryType* findQueryTypeForFilter(QueryFilter* filter) const;

private:
    QueryTypeRegistry();
    ~QueryTypeRegistry();
    
    static QueryTypeRegistry* instance_;
    QList<QueryType*> queryTypes_;
    QueryType* defaultQueryType_;
};

#endif // QUERYTYPE_H
