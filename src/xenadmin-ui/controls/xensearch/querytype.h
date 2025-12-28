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

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include "searcher.h"
#include "xenlib/xensearch/queryfilter.h"
#include "xenlib/xensearch/queries.h"
#include "xenlib/xensearch/queryscope.h"

// Forward declarations
class QueryElement;
class RecursiveXMOPropertyQuery;
class RecursiveXMOListPropertyQuery;
class XenModelObjectPropertyQuery;
class XenModelObjectListContainsQuery;
class XenModelObjectListContainsNameQuery;
class NullPropertyQuery;

/**
 * Base class for all query types
 * Matches C# QueryType abstract class
 */
class QueryType : public QObject
{
    Q_OBJECT

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

signals:
    /**
     * @brief Emitted when dropdown values change (e.g., new OS names, new VMs added)
     * 
     * C# equivalent: SomeThingChanged event
     * QueryElement listens to this and refreshes combo boxes
     */
    void SomeThingChanged();

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
 * 
 * C# Equivalent: TagQueryType in QueryElement.cs
 * Monitors OtherConfigAndTagsWatcher for tag changes to keep dropdown updated
 */
class TagQueryType : public QueryType
{
    Q_OBJECT

public:
    TagQueryType(int group, ObjectTypes appliesTo, QObject* parent = nullptr);
    ~TagQueryType() override;
    
    bool ForQuery(QueryFilter* query) const override;
    void FromQuery(QueryFilter* query, QueryElement* queryElement) override;
    QueryFilter* GetQuery(QueryElement* queryElement) const override;
    QString toString() const override;
    
    bool showMatchTypeComboButton() const override { return true; }
    bool showTextBox(QueryElement* queryElement) const override;
    bool showComboButton(QueryElement* queryElement) const override;
    QStringList getMatchTypeComboButtonEntries() const override;
    QVariantList getComboButtonEntries(QueryElement* queryElement) const override;

private slots:
    void onTagsChanged();

private:
    QStringList collectedTags_;  // All known tags from cache
    
    void populateCollectedTags();
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
 * Value property query type - dynamic value collection from cache
 * 
 * C# Equivalent: ValuePropertyQueryType in QueryElement.cs (lines 1436-1630)
 * Collects unique values for a property (e.g., OS names) from all VMs in cache.
 * Provides dropdown with collected values and "is/is not" matching.
 * 
 * Currently only used for PropertyNames::os_name (VM operating systems).
 * Monitors ConnectionsManager for VM additions/removals to keep list updated.
 */
class ValuePropertyQueryType : public QueryType
{
    Q_OBJECT

public:
    ValuePropertyQueryType(int group, ObjectTypes appliesTo, PropertyNames property, QObject* parent = nullptr);
    ~ValuePropertyQueryType() override;
    
    bool ForQuery(QueryFilter* query) const override;
    QString toString() const override;
    
    bool showMatchTypeComboButton() const override { return true; }
    QStringList getMatchTypeComboButtonEntries() const override;
    
    bool showComboButton(QueryElement* queryElement) const override;
    QVariantList getComboButtonEntries(QueryElement* queryElement) const override;
    
    QueryFilter* GetQuery(QueryElement* queryElement) const override;
    void FromQuery(QueryFilter* query, QueryElement* queryElement) override;

private slots:
    void onConnectionsChanged();
    void onCacheChanged(XenConnection* connection, const QString& type, const QString& ref);

private:
    PropertyNames property_;
    QMap<QString, bool> collectedValues_;  // Value → true (C# uses Dictionary<String, Object>)
    
    void populateCollectedValues();
};

/**
 * UUID query type - resource selection with dropdown
 * 
 * C# Equivalent: Uuid QueryType pattern (not a single class, but pattern used in XenModelObjectPropertyQueryType)
 * Uses ResourceSelectButton widget to show hierarchical object picker.
 * User selects an object (VM, Host, Pool, etc.), query matches by opaque_ref.
 * 
 * Note: Requires ResourceSelectButton widget which is not yet functional (needs Search::PopulateAdapters).
 * For now, this falls back to UuidStringQueryType behavior.
 */
class UuidQueryType : public QueryType
{
    Q_OBJECT

public:
    UuidQueryType(int group, ObjectTypes appliesTo, PropertyNames property, QObject* parent = nullptr);
    
    bool ForQuery(QueryFilter* query) const override;
    QString toString() const override;
    
    bool showMatchTypeComboButton() const override { return true; }
    QStringList getMatchTypeComboButtonEntries() const override;
    
    bool showResourceSelectButton(QueryElement* queryElement) const override;
    
    QueryFilter* GetQuery(QueryElement* queryElement) const override;
    void FromQuery(QueryFilter* query, QueryElement* queryElement) override;

private:
    PropertyNames property_;
    // TODO: Implement when ResourceSelectButton + Search::PopulateAdapters ready
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

// ============================================================================
// Recursive QueryTypes - Parent/Child Filtering  
// ============================================================================

/**
 * Recursive query type base - enables filtering by parent/child object properties
 * 
 * C# Equivalent: RecursiveQueryType<T, O, Q> in QueryElement.cs (lines 1538-1617)
 * 
 * Allows queries like "Pool is <sub-query>" or "Host contains VM where <sub-query>".
 * The sub-query operates on the related object (parent or child).
 * 
 * Template parameters (C# generics):
 * - propertyName: The PropertyNames value for this query
 * - subQueryScope: The ObjectTypes that the sub-query can filter
 * 
 * Example: RecursiveXMOQueryType for Pool→parent pool relationship
 *   - User sees "Pool is..." with sub-query "Name contains 'Production'"
 *   - Matches VMs/Hosts whose parent pool's name contains "Production"
 */
class RecursiveQueryTypeBase : public QueryType
{
    Q_OBJECT

public:
    RecursiveQueryTypeBase(int group, ObjectTypes appliesTo, PropertyNames property, ObjectTypes subQueryScope, QObject* parent = nullptr);
    ~RecursiveQueryTypeBase() override;
    
    Category getCategory() const override { return Category::ParentChild; }
    QueryScope* getSubQueryScope() const override { return this->subQueryScope_; }
    
    bool ForQuery(QueryFilter* query) const override;
    
protected:
    PropertyNames property_;
    QueryScope* subQueryScope_;
    
    // Derived classes implement this to create the appropriate RecursivePropertyQuery
    virtual QueryFilter* newQuery(PropertyNames property, QueryFilter* subQuery) const = 0;
    
private slots:
    void onConnectionsChanged();
    void onCacheChanged();
};

/**
 * Recursive XMO query type - single object relationship
 * 
 * C# Equivalent: RecursiveXMOQueryType<T> in QueryElement.cs (lines 1619-1629)
 * Uses RecursiveXMOPropertyQuery<T> for evaluation
 * 
 * Examples:
 * - "Pool is <sub-query>" (VM→Pool relationship)
 * - "Parent folder is <sub-query>" (VM→Folder relationship)  
 * - "Appliance is <sub-query>" (VM→VM_appliance relationship)
 */
class RecursiveXMOQueryType : public RecursiveQueryTypeBase
{
public:
    RecursiveXMOQueryType(int group, ObjectTypes appliesTo, PropertyNames property, ObjectTypes subQueryScope, QObject* parent = nullptr);
    
    QString toString() const override;
    QueryFilter* GetQuery(QueryElement* queryElement) const override;
    void FromQuery(QueryFilter* query, QueryElement* queryElement) override;
    
protected:
    QueryFilter* newQuery(PropertyNames property, QueryFilter* subQuery) const override;
};

/**
 * Recursive XMO list query type - list object relationship
 * 
 * C# Equivalent: RecursiveXMOListQueryType<T> in QueryElement.cs (lines 1631-1643)
 * Uses RecursiveXMOListPropertyQuery<T> for evaluation
 * 
 * Examples:
 * - "Host contains VM where <sub-query>" (Pool→Host list relationship)
 * - "Network uses <sub-query>" (VM→Network list relationship)
 * - "Storage uses <sub-query>" (VM→SR list relationship)
 * - "Disks contain <sub-query>" (VM→VDI list relationship)
 */
class RecursiveXMOListQueryType : public RecursiveQueryTypeBase
{
public:
    RecursiveXMOListQueryType(int group, ObjectTypes appliesTo, PropertyNames property, ObjectTypes subQueryScope, QObject* parent = nullptr);
    
    QString toString() const override;
    QueryFilter* GetQuery(QueryElement* queryElement) const override;
    void FromQuery(QueryFilter* query, QueryElement* queryElement) override;
    
protected:
    QueryFilter* newQuery(PropertyNames property, QueryFilter* subQuery) const override;
};

// ============================================================================
// Legacy Resource QueryTypes (NOT USED FOR NEW QUERIES)
// ============================================================================

/**
 * XenModelObject property query type - direct object selection with dropdown
 * 
 * C# Equivalent: XenModelObjectPropertyQueryType<T> in QueryElement.cs (lines 1645-1729)
 * 
 * Legacy query type for selecting specific objects from dropdown (e.g., "Pool is Production-Pool").
 * New queries should use RecursiveXMOQueryType + sub-query instead.
 * 
 * Kept for backward compatibility with saved searches.
 * Uses XenModelObjectPropertyQuery<T> for evaluation.
 */
class XenModelObjectPropertyQueryType : public QueryType
{
    Q_OBJECT

public:
    XenModelObjectPropertyQueryType(int group, ObjectTypes appliesTo, PropertyNames property, QObject* parent = nullptr);
    ~XenModelObjectPropertyQueryType() override;
    
    bool ForQuery(QueryFilter* query) const override;
    QString toString() const override;
    
    bool showMatchTypeComboButton() const override { return true; }
    QStringList getMatchTypeComboButtonEntries() const override;
    
    bool showComboButton(QueryElement* queryElement) const override;
    QVariantList getComboButtonEntries(QueryElement* queryElement) const override;
    
    QueryFilter* GetQuery(QueryElement* queryElement) const override;
    void FromQuery(QueryFilter* query, QueryElement* queryElement) override;

private slots:
    void onConnectionsChanged();
    void onCacheChanged();

private:
    PropertyNames property_;
};

/**
 * XenModelObject list contains query type - object list filtering with dropdown
 * 
 * C# Equivalent: XenModelObjectListContainsQueryType<T> in QueryElement.cs (lines 1731-1884)
 * 
 * Legacy query type for "Network uses Production-Net", "Storage uses iSCSI-SR", etc.
 * New queries should use RecursiveXMOListQueryType + sub-query instead.
 * 
 * Supports both dropdown selection and text matching.
 * Uses XenModelObjectListContainsQuery<T> for evaluation.
 */
class XenModelObjectListContainsQueryType : public QueryType
{
    Q_OBJECT

public:
    XenModelObjectListContainsQueryType(int group, ObjectTypes appliesTo, PropertyNames property, QObject* parent = nullptr);
    ~XenModelObjectListContainsQueryType() override;
    
    bool ForQuery(QueryFilter* query) const override;
    QString toString() const override;
    
    bool showMatchTypeComboButton() const override { return true; }
    QStringList getMatchTypeComboButtonEntries() const override;
    
    bool showComboButton(QueryElement* queryElement) const override;
    QVariantList getComboButtonEntries(QueryElement* queryElement) const override;
    
    bool showTextBox(QueryElement* queryElement) const override;
    
    QueryFilter* GetQuery(QueryElement* queryElement) const override;
    void FromQuery(QueryFilter* query, QueryElement* queryElement) override;

private slots:
    void onConnectionsChanged();
    void onCacheChanged(XenConnection* connection, const QString& type, const QString& ref);

protected:
    PropertyNames property_;
};

/**
 * Host query type - special handling for standalone/in-pool
 * 
 * C# Equivalent: HostQueryType in QueryElement.cs (lines 1946-2006)
 * 
 * Extends XenModelObjectListContainsQueryType with additional match types:
 * - "Is standalone" - Host not in a pool (uses NullQuery<Pool>)
 * - "Is in a pool" - Host in a pool (uses !NullQuery<Pool>)
 * 
 * Also supports standard "Uses Host-A", text matching, etc.
 */
class HostQueryType : public XenModelObjectListContainsQueryType
{
public:
    HostQueryType(int group, ObjectTypes appliesTo, QObject* parent = nullptr);
    
    QStringList getMatchTypeComboButtonEntries() const override;
    bool showTextBox(QueryElement* queryElement) const override;
    
    QueryFilter* GetQuery(QueryElement* queryElement) const override;
    void FromQuery(QueryFilter* query, QueryElement* queryElement) override;
};

/**
 * NullQueryType - Check if reference property is null or not null
 * 
 * C# Equivalent: NullQueryType<T> in QueryElement.cs (lines 2202-2260)
 * 
 * Simple QueryType that creates NullPropertyQuery for checking null references.
 * Used for queries like "Is standalone" (pool == null), "Not in a folder" (folder == null)
 * 
 * Note: C# uses generics NullQueryType<T> where T : XenObject<T>
 *       Qt uses PropertyNames for type safety instead
 */
class NullQueryType : public QueryType
{
public:
    NullQueryType(int group, ObjectTypes appliesTo, PropertyNames property, 
                  bool isNull, const QString& i18n);
    
    QString toString() const override { return this->i18n_; }
    
    QueryFilter* GetQuery(QueryElement* queryElement) const override;
    bool ForQuery(QueryFilter* query) const override;
    void FromQuery(QueryFilter* query, QueryElement* queryElement) override;

private:
    PropertyNames property_;
    bool isNull_;
    QString i18n_;
};

/**
 * MatchType - Abstract base class for match types used by MatchQueryType
 * 
 * C# Equivalent: MatchType in QueryElement.cs (lines 2374-2394)
 * 
 * Represents different matching modes (e.g., "Contained in", "Bigger than", etc.)
 * Each MatchType controls which UI elements are shown and how to build queries.
 */
class MatchType
{
public:
    explicit MatchType(const QString& matchText);
    virtual ~MatchType() = default;
    
    QString toString() const { return this->matchText_; }
    
    virtual bool showComboButton(QueryElement* queryElement) const { return false; }
    virtual bool showTextBox(QueryElement* queryElement) const { return false; }
    virtual bool showNumericUpDown(QueryElement* queryElement) const { return false; }
    
    virtual QString units(QueryElement* queryElement) const { return QString(); }
    virtual QVariantList getComboButtonEntries() const { return QVariantList(); }
    
    virtual QueryFilter* getQuery(QueryElement* queryElement) const = 0;
    virtual bool forQuery(QueryFilter* query) const = 0;
    virtual void fromQuery(QueryFilter* query, QueryElement* queryElement) = 0;

protected:
    QString matchText_;
};

/**
 * XMOListContains - MatchType for "Contained in" / "Not contained in" with resource picker
 * 
 * C# Equivalent: XMOListContains<T> in QueryElement.cs (lines 2396-2448)
 * 
 * Template note: C# uses XenObject<T> generics, Qt uses QString type parameter.
 * Filter predicate not implemented yet (needs cache API).
 */
class XMOListContains : public MatchType
{
public:
    XMOListContains(PropertyNames property, bool contains, const QString& matchText,
                    const QString& objectType);
    
    bool showComboButton(QueryElement* queryElement) const override;
    QVariantList getComboButtonEntries() const override;
    
    QueryFilter* getQuery(QueryElement* queryElement) const override;
    bool forQuery(QueryFilter* query) const override;
    void fromQuery(QueryFilter* query, QueryElement* queryElement) override;

private:
    PropertyNames property_;
    bool contains_;
    QString objectType_;  // "vm", "host", "sr", etc.
};

/**
 * IntMatch - MatchType for numeric matching with multiplier/units
 * 
 * C# Equivalent: IntMatch in QueryElement.cs (lines 2450-2495)
 * 
 * Used for size queries (e.g., "Size is 10 GB", "Bigger than 5 MB")
 */
class IntMatch : public MatchType
{
public:
    IntMatch(PropertyNames property, const QString& matchText,
             const QString& units, qint64 multiplier,
             NumericQuery::ComparisonType type);
    
    bool showNumericUpDown(QueryElement* queryElement) const override;
    QString units(QueryElement* queryElement) const override;
    
    QueryFilter* getQuery(QueryElement* queryElement) const override;
    bool forQuery(QueryFilter* query) const override;
    void fromQuery(QueryFilter* query, QueryElement* queryElement) override;

private:
    PropertyNames property_;
    NumericQuery::ComparisonType type_;
    QString units_;
    qint64 multiplier_;
};

/**
 * MatchQueryType - Abstract base class for QueryTypes that use MatchTypes
 * 
 * C# Equivalent: MatchQueryType in QueryElement.cs (lines 2260-2372)
 * 
 * Delegates UI control and query building to selected MatchType.
 * Used by DiskQueryType and LongQueryType.
 */
class MatchQueryType : public QueryType
{
public:
    MatchQueryType(int group, ObjectTypes appliesTo, const QString& i18n);
    virtual ~MatchQueryType();
    
    QString toString() const override { return this->i18n_; }
    
    bool showMatchTypeComboButton() const override { return true; }
    QStringList getMatchTypeComboButtonEntries() const override;
    
    bool showComboButton(QueryElement* queryElement) const override;
    bool showNumericUpDown(QueryElement* queryElement) const override;
    bool showTextBox(QueryElement* queryElement) const override;
    
    QString getUnits(QueryElement* queryElement) const override;
    QVariantList getComboButtonEntries(QueryElement* queryElement) const override;
    
    QueryFilter* GetQuery(QueryElement* queryElement) const override;
    bool ForQuery(QueryFilter* query) const override;
    void FromQuery(QueryFilter* query, QueryElement* queryElement) override;

protected:
    virtual QList<MatchType*> getMatchTypes() const = 0;
    
    MatchType* getSelectedMatchType(QueryElement* queryElement) const;

private:
    QString i18n_;
};

/**
 * DiskQueryType - Complex disk query with 7 match types
 * 
 * C# Equivalent: DiskQueryType in QueryElement.cs (lines 2497-2524)
 * 
 * Match types:
 * - Contained in SR X / Not contained in SR X
 * - Attached to VM X / Not attached to VM X
 * - Size is X GB / Bigger than X GB / Smaller than X GB
 * 
 * NOTE: Marked as "NOT USED FOR NEW QUERIES" in C# - kept for backward compatibility
 */
class DiskQueryType : public MatchQueryType
{
public:
    DiskQueryType(int group, ObjectTypes appliesTo, const QString& i18n);
    ~DiskQueryType() override;

protected:
    QList<MatchType*> getMatchTypes() const override;

private:
    mutable QList<MatchType*> matchTypes_;
};

/**
 * LongQueryType - Numeric range query (bigger/smaller/exact)
 * 
 * C# Equivalent: LongQueryType in QueryElement.cs (lines 2526-2545)
 * 
 * Generic numeric query with 3 match types:
 * - Bigger than X
 * - Smaller than X  
 * - Is exactly X
 */
class LongQueryType : public MatchQueryType
{
public:
    LongQueryType(int group, ObjectTypes appliesTo, const QString& i18n,
                  PropertyNames property, qint64 multiplier, const QString& unit);
    ~LongQueryType() override;

protected:
    QList<MatchType*> getMatchTypes() const override;

private:
    mutable QList<MatchType*> matchTypes_;
};

/**
 * CustomFieldQueryTypeBase - Abstract base for custom field queries
 * 
 * C# Equivalent: CustomFieldQueryTypeBase in QueryElement.cs (lines 2106-2129)
 * 
 * NOTE: Requires CustomFieldDefinition class - stubbed for now
 */
class CustomFieldQueryTypeBase : public QueryType
{
public:
    CustomFieldQueryTypeBase(int group, ObjectTypes appliesTo, 
                            const QString& fieldName);
    
    QString toString() const override { return this->fieldName_; }
    
    bool ForQuery(QueryFilter* query) const override;

protected:
    QString fieldName_;  // Will be CustomFieldDefinition* when implemented
};

/**
 * CustomFieldStringQueryType - String custom field queries
 * 
 * C# Equivalent: CustomFieldStringQueryType in QueryElement.cs (lines 2131-2162)
 * 
 * Uses same match types as StringPropertyQueryType (contains, starts with, etc.)
 */
class CustomFieldStringQueryType : public CustomFieldQueryTypeBase
{
public:
    CustomFieldStringQueryType(int group, ObjectTypes appliesTo,
                               const QString& fieldName);
    
    bool showMatchTypeComboButton() const override { return true; }
    bool showTextBox(QueryElement* queryElement) const override { return true; }
    
    QStringList getMatchTypeComboButtonEntries() const override;
    
    QueryFilter* GetQuery(QueryElement* queryElement) const override;
    void FromQuery(QueryFilter* query, QueryElement* queryElement) override;
};

/**
 * CustomFieldDateQueryType - Date custom field queries
 * 
 * C# Equivalent: CustomFieldDateQueryType in QueryElement.cs (lines 2164-2200)
 * 
 * Uses same match types as DatePropertyQueryType (on, before, after, etc.)
 */
class CustomFieldDateQueryType : public CustomFieldQueryTypeBase
{
public:
    CustomFieldDateQueryType(int group, ObjectTypes appliesTo,
                            const QString& fieldName);
    
    bool showMatchTypeComboButton() const override { return true; }
    bool showDateTimePicker(QueryElement* queryElement) const override;
    
    QStringList getMatchTypeComboButtonEntries() const override;
    
    QueryFilter* GetQuery(QueryElement* queryElement) const override;
    void FromQuery(QueryFilter* query, QueryElement* queryElement) override;
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
