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

// queryfilter.cpp - Implementation of QueryFilter
#include "queryfilter.h"
#include "xenlib.h"

//==============================================================================
// NullQuery - Matches all objects (no filtering)
//==============================================================================

QVariant NullQuery::match(const QVariantMap& objectData, const QString& objectType, XenLib* xenLib) const
{
    Q_UNUSED(objectData);
    Q_UNUSED(objectType);
    Q_UNUSED(xenLib);

    // C# equivalent: No explicit NullQuery in C#, but this represents
    // the behavior when Query.queryFilter is null (no filtering)
    // Always matches all objects
    return true;
}

bool NullQuery::equals(const QueryFilter* other) const
{
    // All NullQuery instances are equivalent
    return dynamic_cast<const NullQuery*>(other) != nullptr;
}

uint NullQuery::hashCode() const
{
    // C# equivalent: DummyQuery doesn't have explicit hash, but we return a constant
    return 0; // All NullQuery instances have same hash
}

//==============================================================================
// TypePropertyQuery - Matches objects by type
//==============================================================================

TypePropertyQuery::TypePropertyQuery(const QString& objectType, bool equals)
    : m_objectType(objectType), m_equals(equals)
{
}

QVariant TypePropertyQuery::match(const QVariantMap& objectData, const QString& objectType, XenLib* xenLib) const
{
    Q_UNUSED(objectData);
    Q_UNUSED(xenLib);

    // C# equivalent: EnumPropertyQuery<ObjectTypes>.MatchProperty()
    // Compare the object's type with the query type

    // For VMs, we need to distinguish between templates and regular VMs
    if (m_objectType == "template")
    {
        // Match templates: objectType must be "vm" AND is_a_template must be true
        bool isTemplate = objectData.value("is_a_template", false).toBool();
        bool isMatch = (objectType == "vm" && isTemplate);
        return isMatch == m_equals;
    } else if (m_objectType == "vm")
    {
        // Match VMs (non-templates): objectType must be "vm" AND is_a_template must be false
        bool isTemplate = objectData.value("is_a_template", false).toBool();
        bool isMatch = (objectType == "vm" && !isTemplate);
        return isMatch == m_equals;
    } else
    {
        // Simple type match
        bool isMatch = (objectType == m_objectType);
        return isMatch == m_equals;
    }
}

bool TypePropertyQuery::equals(const QueryFilter* other) const
{
    const TypePropertyQuery* otherTypeQuery = dynamic_cast<const TypePropertyQuery*>(other);
    if (!otherTypeQuery)
        return false;

    return m_objectType == otherTypeQuery->m_objectType &&
           m_equals == otherTypeQuery->m_equals;
}

uint TypePropertyQuery::hashCode() const
{
    // Combine hash of object type and equals flag
    return qHash(m_objectType) ^ (m_equals ? 1 : 0);
}
