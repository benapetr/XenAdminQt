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

// grouping.h - Base class for grouping algorithms
// Port of C# xenadmin/XenModel/XenSearch/GroupingTypes.cs
#ifndef GROUPING_H
#define GROUPING_H

#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QIcon>
#include "common.h"
#include "../customfields/customfielddefinition.h"

// Forward declarations
class XenConnection;

/**
 * @brief Base class for all grouping algorithms
 *
 * Defines how objects are grouped in tree/search views
 *
 * C# equivalent: xenadmin/XenModel/XenSearch/GroupingTypes.cs - class Grouping
 */
class Grouping
{
    public:
        /**
         * @brief Constructor with optional subgrouping
         * @param subgrouping Sub-grouping to apply within this grouping (may be null)
         */
        explicit Grouping(Grouping* subgrouping = nullptr);

        virtual ~Grouping();

        /**
         * @brief Get the name of this grouping (e.g., "Type", "Pool", "Folder")
         * @return Grouping name
         */
        virtual QString getGroupingName() const = 0;

        /**
         * @brief Get the display name for a specific group value
         * @param group The group value
         * @return Human-readable name for the group
         */
        virtual QString getGroupName(const QVariant& group) const;

        /**
         * @brief Get the icon for a specific group value
         * @param group The group value
         * @return Icon to display for the group
         */
        virtual QIcon getGroupIcon(const QVariant& group) const;

        /**
         * @brief Get the group value for a given object
         * @param objectData The object data (VM, host, etc.)
         * @param objectType The object type ("vm", "host", etc.)
         * @return The group value, or an invalid QVariant if object doesn't belong to any group
         */
        virtual QVariant getGroup(const QVariantMap& objectData, const QString& objectType) const = 0;

        /**
         * @brief Check if object should appear as a group node itself (not just a member)
         *
         * For example, folders appear as group nodes
         * @param objectData The object data
         * @param objectType The object type
         * @return true if the object should be shown as a group node
         */
        virtual bool belongsAsGroupNotMember(const QVariantMap& objectData, const QString& objectType) const;

        /**
         * @brief Get the subgrouping for a specific group value
         *
         * Used when drilling down in the tree (e.g., Type → Pool → Host)
         * @param group The group value
         * @return Subgrouping to apply, or nullptr if no further grouping
         */
        virtual Grouping* getSubgrouping(const QVariant& group) const;

        /**
         * @brief Get a QueryFilter that matches objects in this group
         *
         * Used when clicking a group node to show only objects in that group
         * C# equivalent: Grouping.GetSubquery(object parent, object val)
         * @param parent Parent group value (may be null)
         * @param group The group value to filter by
         * @return QueryFilter that matches objects in this group, or nullptr if no filtering needed
         */
        virtual class QueryFilter* getSubquery(const QVariant& parent, const QVariant& group) const;

        /**
         * @brief Equality comparison for groupings
         * @param other Other grouping to compare
         * @return true if equal
         */
        virtual bool equals(const Grouping* other) const = 0;

        /**
         * @brief Get subgrouping pointer
         * @return Pointer to subgrouping or nullptr
         */
        Grouping* subgrouping() const
        {
            return m_subgrouping;
        }

        //! The connection whose objects are being grouped. Set before getGroup() and the name
        //! lookups; applies to the whole chain of subgroupings.
        virtual void SetConnection(XenConnection* conn);
        XenConnection* GetConnection() const { return this->m_connection; }

    protected:
        Grouping* m_subgrouping; // Optional nested grouping
        XenConnection* m_connection = nullptr;
};

/**
 * @brief Grouping by object type (VM, Host, SR, Network, etc.)
 *
 * C# equivalent: PropertyGrouping<ObjectTypes> in GroupingTypes.cs
 * Maps to C# property PropertyNames.type
 */
class TypeGrouping : public Grouping
{
    public:
        explicit TypeGrouping(Grouping* subgrouping = nullptr);

        QString getGroupingName() const override;
        QString getGroupName(const QVariant& group) const override;
        QIcon getGroupIcon(const QVariant& group) const override;
        QVariant getGroup(const QVariantMap& objectData, const QString& objectType) const override;
        QueryFilter* getSubquery(const QVariant& parent, const QVariant& group) const override;
        bool equals(const Grouping* other) const override;
};

/**
 * @brief Grouping by pool membership
 *
 * C# equivalent: PropertyGrouping<Pool> in GroupingTypes.cs
 * Maps to C# property PropertyNames.pool
 */
class PoolGrouping : public Grouping
{
    public:
        explicit PoolGrouping(Grouping* subgrouping = nullptr);

        QString getGroupingName() const override;
        QString getGroupName(const QVariant& group) const override;
        QIcon getGroupIcon(const QVariant& group) const override;
        QVariant getGroup(const QVariantMap& objectData, const QString& objectType) const override;
        bool belongsAsGroupNotMember(const QVariantMap& objectData, const QString& objectType) const override;
        bool equals(const Grouping* other) const override;

};

/**
 * @brief Grouping by host membership
 *
 * C# equivalent: PropertyGrouping<Host> in GroupingTypes.cs
 * Maps to C# property PropertyNames.host
 */
class HostGrouping : public Grouping
{
    public:
        explicit HostGrouping(Grouping* subgrouping = nullptr);

        QString getGroupingName() const override;
        QString getGroupName(const QVariant& group) const override;
        QIcon getGroupIcon(const QVariant& group) const override;
        QVariant getGroup(const QVariantMap& objectData, const QString& objectType) const override;
        bool belongsAsGroupNotMember(const QVariantMap& objectData, const QString& objectType) const override;
        bool equals(const Grouping* other) const override;

};

/**
 * @brief Grouping by folder ancestry
 *
 * C# equivalent: FolderGrouping in GroupingTypes.cs
 */
class FolderGrouping : public Grouping
{
    public:
        explicit FolderGrouping(Grouping* subgrouping = nullptr);

        QString getGroupingName() const override;
        QString getGroupName(const QVariant& group) const override;
        QIcon getGroupIcon(const QVariant& group) const override;
        QVariant getGroup(const QVariantMap& objectData, const QString& objectType) const override;
        bool belongsAsGroupNotMember(const QVariantMap& objectData, const QString& objectType) const override;
        bool equals(const Grouping* other) const override;
};

/**
 * @brief Grouping by individual tag values
 */
class TagsGrouping : public Grouping
{
    public:
        explicit TagsGrouping(Grouping* subgrouping = nullptr);

        QString getGroupingName() const override;
        QString getGroupName(const QVariant& group) const override;
        QIcon getGroupIcon(const QVariant& group) const override;
        QVariant getGroup(const QVariantMap& objectData, const QString& objectType) const override;
        QueryFilter* getSubquery(const QVariant& parent, const QVariant& group) const override;
        bool equals(const Grouping* other) const override;
};

/**
 * @brief Grouping by vApp/appliance reference
 */
class VAppGrouping : public Grouping
{
    public:
        explicit VAppGrouping(Grouping* subgrouping = nullptr);

        QString getGroupingName() const override;
        QString getGroupName(const QVariant& group) const override;
        QIcon getGroupIcon(const QVariant& group) const override;
        QVariant getGroup(const QVariantMap& objectData, const QString& objectType) const override;
        QueryFilter* getSubquery(const QVariant& parent, const QVariant& group) const override;
        bool equals(const Grouping* other) const override;

};

/**
 * Group by the value of a property (C# PropertyGrouping<T>), e.g. operating system, power state,
 * tools status, HA restart priority. List properties put an object in several groups.
 */
class PropertyGrouping : public Grouping
{
    public:
        explicit PropertyGrouping(XenSearch::PropertyNames property, Grouping* subgrouping = nullptr);

        QString getGroupingName() const override;
        QString getGroupName(const QVariant& group) const override;
        QVariant getGroup(const QVariantMap& objectData, const QString& objectType) const override;
        QueryFilter* getSubquery(const QVariant& parent, const QVariant& group) const override;
        bool equals(const Grouping* other) const override;

        XenSearch::PropertyNames property() const { return this->m_property; }

    protected:
        //! The typed property of the object (C# PropertyAccessors.Get(property)(o))
        QVariant propertyValue(const QVariantMap& objectData, const QString& objectType) const;

        XenSearch::PropertyNames m_property;
};

/**
 * Group by a yes/no property with readable group names (C# BoolGrouping, CA-165366)
 */
class BoolGrouping : public PropertyGrouping
{
    public:
        explicit BoolGrouping(XenSearch::PropertyNames property, Grouping* subgrouping = nullptr);

        QString getGroupName(const QVariant& group) const override;
        QueryFilter* getSubquery(const QVariant& parent, const QVariant& group) const override;
        bool equals(const Grouping* other) const override;
};

/**
 * Group by a related object, e.g. network, SR, disk, VM (C# XenModelObjectPropertyGrouping<T>).
 * Objects of the related type appear as the groups themselves.
 */
class XenModelObjectPropertyGrouping : public PropertyGrouping
{
    public:
        explicit XenModelObjectPropertyGrouping(XenSearch::PropertyNames property, Grouping* subgrouping = nullptr);

        QString getGroupName(const QVariant& group) const override;
        QVariant getGroup(const QVariantMap& objectData, const QString& objectType) const override;
        bool belongsAsGroupNotMember(const QVariantMap& objectData, const QString& objectType) const override;
        QueryFilter* getSubquery(const QVariant& parent, const QVariant& group) const override;
        bool equals(const Grouping* other) const override;

        //! The object type of the groups ("network", "sr", "vdi", ...)
        QString groupObjectType() const;
};

/**
 * Group by the value of one custom field (C# CustomFieldGrouping)
 */
class CustomFieldGrouping : public Grouping
{
    public:
        explicit CustomFieldGrouping(const CustomFieldDefinition& definition, Grouping* subgrouping = nullptr);

        QString getGroupingName() const override;
        QVariant getGroup(const QVariantMap& objectData, const QString& objectType) const override;
        QueryFilter* getSubquery(const QVariant& parent, const QVariant& group) const override;
        bool equals(const Grouping* other) const override;

        const CustomFieldDefinition& definition() const { return this->m_definition; }

    private:
        CustomFieldDefinition m_definition;
};

/**
 * Group by each custom field an object has (C# AllCustomFieldsGrouping)
 */
class AllCustomFieldsGrouping : public Grouping
{
    public:
        explicit AllCustomFieldsGrouping(Grouping* subgrouping = nullptr);

        QString getGroupingName() const override;
        QVariant getGroup(const QVariantMap& objectData, const QString& objectType) const override;
        QueryFilter* getSubquery(const QVariant& parent, const QVariant& group) const override;
        bool equals(const Grouping* other) const override;
};

#endif // GROUPING_H
