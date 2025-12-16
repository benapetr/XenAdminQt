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

// Forward declarations
class XenLib;

/// <summary>
/// Base class for all grouping algorithms
/// Defines how objects are grouped in tree/search views
///
/// C# equivalent: xenadmin/XenModel/XenSearch/GroupingTypes.cs - class Grouping
/// </summary>
class Grouping
{
public:
    /// <summary>
    /// Constructor with optional subgrouping
    /// </summary>
    /// <param name="subgrouping">Sub-grouping to apply within this grouping (may be null)</param>
    explicit Grouping(Grouping* subgrouping = nullptr);

    virtual ~Grouping();

    /// <summary>
    /// Get the name of this grouping (e.g., "Type", "Pool", "Folder")
    /// </summary>
    virtual QString getGroupingName() const = 0;

    /// <summary>
    /// Get the display name for a specific group value
    /// </summary>
    /// <param name="group">The group value</param>
    /// <returns>Human-readable name for the group</returns>
    virtual QString getGroupName(const QVariant& group) const;

    /// <summary>
    /// Get the icon for a specific group value
    /// </summary>
    /// <param name="group">The group value</param>
    /// <returns>Icon to display for the group</returns>
    virtual QIcon getGroupIcon(const QVariant& group) const;

    /// <summary>
    /// Get the group value for a given object
    /// </summary>
    /// <param name="objectData">The object data (VM, host, etc.)</param>
    /// <param name="objectType">The object type ("vm", "host", etc.)</param>
    /// <returns>The group value, or null if object doesn't belong to any group</returns>
    virtual QVariant getGroup(const QVariantMap& objectData, const QString& objectType) const = 0;

    /// <summary>
    /// Check if object should appear as a group node itself (not just a member)
    /// For example, folders appear as group nodes
    /// </summary>
    virtual bool belongsAsGroupNotMember(const QVariantMap& objectData, const QString& objectType) const;

    /// <summary>
    /// Get the subgrouping for a specific group value
    /// Used when drilling down in the tree (e.g., Type → Pool → Host)
    /// </summary>
    /// <param name="group">The group value</param>
    /// <returns>Subgrouping to apply, or null if no further grouping</returns>
    virtual Grouping* getSubgrouping(const QVariant& group) const;

    /// <summary>
    /// Get a QueryFilter that matches objects in this group
    /// Used when clicking a group node to show only objects in that group
    ///
    /// C# equivalent: Grouping.GetSubquery(object parent, object val)
    /// </summary>
    /// <param name="parent">Parent group value (may be null)</param>
    /// <param name="group">The group value to filter by</param>
    /// <returns>QueryFilter that matches objects in this group, or null if no filtering needed</returns>
    virtual class QueryFilter* getSubquery(const QVariant& parent, const QVariant& group) const;

    /// <summary>
    /// Equality comparison for groupings
    /// </summary>
    virtual bool equals(const Grouping* other) const = 0;

    /// <summary>
    /// Get subgrouping pointer
    /// </summary>
    Grouping* subgrouping() const
    {
        return m_subgrouping;
    }

protected:
    Grouping* m_subgrouping; // Optional nested grouping
};

/// <summary>
/// Grouping by object type (VM, Host, SR, Network, etc.)
///
/// C# equivalent: PropertyGrouping&lt;ObjectTypes&gt; in GroupingTypes.cs
/// Maps to C# property PropertyNames.type
/// </summary>
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

/// <summary>
/// Grouping by pool membership
///
/// C# equivalent: PropertyGrouping&lt;Pool&gt; in GroupingTypes.cs
/// Maps to C# property PropertyNames.pool
/// </summary>
class PoolGrouping : public Grouping
{
public:
    explicit PoolGrouping(Grouping* subgrouping = nullptr);

    QString getGroupingName() const override;
    QString getGroupName(const QVariant& group) const override;
    QIcon getGroupIcon(const QVariant& group) const override;
    QVariant getGroup(const QVariantMap& objectData, const QString& objectType) const override;
    bool equals(const Grouping* other) const override;

    void setXenLib(XenLib* xenLib)
    {
        m_xenLib = xenLib;
    }

private:
    XenLib* m_xenLib = nullptr;
};

/// <summary>
/// Grouping by host membership
///
/// C# equivalent: PropertyGrouping&lt;Host&gt; in GroupingTypes.cs
/// Maps to C# property PropertyNames.host
/// </summary>
class HostGrouping : public Grouping
{
public:
    explicit HostGrouping(Grouping* subgrouping = nullptr);

    QString getGroupingName() const override;
    QString getGroupName(const QVariant& group) const override;
    QIcon getGroupIcon(const QVariant& group) const override;
    QVariant getGroup(const QVariantMap& objectData, const QString& objectType) const override;
    bool equals(const Grouping* other) const override;

    void setXenLib(XenLib* xenLib)
    {
        m_xenLib = xenLib;
    }

private:
    XenLib* m_xenLib = nullptr;
};

#endif // GROUPING_H
