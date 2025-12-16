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

// groupingtag.h - Tag for grouping objects in tree view
// Port of C# xenadmin/XenAdmin/GroupingTag.cs
#ifndef GROUPINGTAG_H
#define GROUPINGTAG_H

#include <QVariant>
#include <QString>

// Forward declarations
class Grouping;

/// <summary>
/// Represents a grouping node in the tree (e.g., "Servers", "Templates", "Types")
/// Stores the grouping algorithm, optional parent, and the group value
///
/// C# equivalent: xenadmin/XenAdmin/GroupingTag.cs
/// </summary>
class GroupingTag
{
public:
    /// <summary>
    /// Constructor
    /// </summary>
    /// <param name="grouping">The grouping algorithm (must not be null)</param>
    /// <param name="parent">Parent group (may be null for top-level)</param>
    /// <param name="group">The group value (must not be null)</param>
    GroupingTag(Grouping* grouping, const QVariant& parent, const QVariant& group);

    ~GroupingTag();

    /// <summary>
    /// Get the grouping algorithm
    /// </summary>
    Grouping* getGrouping() const
    {
        return m_grouping;
    }

    /// <summary>
    /// Get the parent group (may be null)
    /// </summary>
    QVariant getParent() const
    {
        return m_parent;
    }

    /// <summary>
    /// Get the group value
    /// </summary>
    QVariant getGroup() const
    {
        return m_group;
    }

    /// <summary>
    /// Equality comparison (two GroupingTags are equal if grouping and group are equal)
    /// </summary>
    bool operator==(const GroupingTag& other) const;

    /// <summary>
    /// Hash code for use in QHash
    /// </summary>
    uint hashCode() const;

private:
    Grouping* m_grouping; // The grouping algorithm
    QVariant m_parent;    // Parent group (optional)
    QVariant m_group;     // The group value
};

// Hash function for QHash support
inline uint qHash(const GroupingTag& tag)
{
    return tag.hashCode();
}

// Register GroupingTag* for QVariant
Q_DECLARE_METATYPE(GroupingTag*)

#endif // GROUPINGTAG_H
