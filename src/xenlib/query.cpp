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

// query.cpp - Implementation of Query
#include "query.h"
#include "xenlib.h"
#include <QDebug>

Query::Query(QueryScope* scope, QueryFilter* filter)
    : m_scope(scope), m_filter(filter)
{
    // C# equivalent: Query constructor

    // If scope is null, default to AllExcFolders
    if (m_scope == nullptr)
    {
        m_scope = new QueryScope(ObjectTypes::AllExcFolders);
    }

    // filter can be null (no filtering)
}

Query::~Query()
{
    // Clean up owned pointers
    delete m_scope;
    delete m_filter;
}

bool Query::match(const QVariantMap& objectData, const QString& objectType, XenLib* xenLib) const
{
    // C# equivalent: Match(IXenObject o)
    //
    // C# code:
    //   return (scope.WantType(o) &&
    //           o.Show(XenAdminConfigManager.Provider.ShowHiddenVMs) &&
    //           (filter == null || filter.Match(o) != false));
    //
    // We skip the "Show(ShowHiddenVMs)" check for now since we don't have that setting yet

    QString objectName = objectData.value("name_label", objectData.value("name_description", "(unnamed)").toString()).toString();

    // Check if scope wants this object type
    if (!m_scope->wantType(objectData, objectType, xenLib))
    {
        // qDebug() << "    Query::match() - scope rejected:" << objectType << objectName;
        return false;
    }

    // Check if filter matches (or there is no filter)
    if (m_filter != nullptr)
    {
        QVariant filterResult = m_filter->match(objectData, objectType, xenLib);

        // If filter returns false, object doesn't match
        // If filter returns null (indeterminate), treat as match (like C# != false)
        if (filterResult.isValid() && filterResult.toBool() == false)
        {
            // qDebug() << "    Query::match() - filter rejected:" << objectType << objectName;
            return false;
        }
    }

    // qDebug() << "    Query::match() - ACCEPTED:" << objectType << objectName;
    return true;
}

bool Query::equals(const Query* other) const
{
    // C# equivalent: Equals(object obj)
    if (other == nullptr)
        return false;

    // Check filter equality
    if ((m_filter == nullptr) != (other->m_filter == nullptr))
        return false;

    if (m_filter != nullptr && !m_filter->equals(other->m_filter))
        return false;

    // Check scope equality
    return m_scope->equals(other->m_scope);
}

uint Query::hashCode() const
{
    // C# equivalent: GetHashCode()
    // return filter == null ? scope.GetHashCode() : (filter.GetHashCode() + 1) * scope.GetHashCode();

    if (m_filter == nullptr)
        return m_scope->hashCode();
    else
        return (qHash(*m_filter) + 1) * m_scope->hashCode();
}
