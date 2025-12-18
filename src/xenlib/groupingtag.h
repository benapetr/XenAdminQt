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

/**
 * @brief Represents a grouping node in the tree (e.g., "Servers", "Templates", "Types")
 *
 * Stores the grouping algorithm, optional parent, and the group value
 *
 * C# equivalent: xenadmin/XenAdmin/GroupingTag.cs
 */
class GroupingTag
{
public:
    /**
     * @brief Constructor
     * @param grouping The grouping algorithm (must not be null)
     * @param parent Parent group (may be null for top-level)
     * @param group The group value (must not be null)
     */
    GroupingTag(Grouping* grouping, const QVariant& parent, const QVariant& group);

    ~GroupingTag();

    /**
     * @brief Get the grouping algorithm
     * @return Pointer to Grouping
     */
    Grouping* getGrouping() const
    {
        return m_grouping;
    }

    /**
     * @brief Get the parent group (may be null)
     * @return Parent group value
     */
    QVariant getParent() const
    {
        return m_parent;
    }

    /**
     * @brief Get the group value
     * @return Group value
     */
    QVariant getGroup() const
    {
        return m_group;
    }

    /**
     * @brief Equality comparison (two GroupingTags are equal if grouping and group are equal)
     */
    bool operator==(const GroupingTag& other) const;

    /**
     * @brief Hash code for use in QHash
     * @return 32-bit hash value
     */
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
