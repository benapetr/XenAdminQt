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

// groupingtag.cpp - Implementation of GroupingTag
#include "groupingtag.h"
#include "grouping.h"
#include <QDebug>

GroupingTag::GroupingTag(Grouping* grouping, const QVariant& parent, const QVariant& group) : m_grouping(grouping), m_parent(parent), m_group(group)
{
    // C# equivalent: xenadmin/XenAdmin/GroupingTag.cs constructor
    Q_ASSERT(grouping != nullptr);
    Q_ASSERT(!group.isNull());
}

GroupingTag::~GroupingTag()
{
    // Note: We don't delete m_grouping because it's owned by the caller
}

bool GroupingTag::operator==(const GroupingTag& other) const
{
    // C# equivalent: GroupingTag.Equals()
    // Two GroupingTags are equal if their grouping and group are equal
    return m_grouping->equals(other.m_grouping) && m_group == other.m_group;
}

uint GroupingTag::hashCode() const
{
    // C# equivalent: GroupingTag.GetHashCode()
    // Use group's hash code
    // For QVariant, we need to convert to a hashable type
    if (m_group.type() == QVariant::String)
        return qHash(m_group.toString());
    else if (m_group.type() == QVariant::Int)
        return qHash(m_group.toInt());
    else
        return qHash(m_group.toString()); // Fallback to string representation
}
