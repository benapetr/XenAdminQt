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

#include "xensearch/treesearch.h"

#include "settingsmanager.h"
#include "xen/xenobject.h"

using namespace XenSearch;

Search* TreeSearch::s_defaultTreeSearch = nullptr;

Search* TreeSearch::DefaultTreeSearch()
{
    if (!s_defaultTreeSearch)
    {
        s_defaultTreeSearch = Search::SearchFor(QStringList(), QStringList(), nullptr, GetTreeSearchScope());
    }

    return s_defaultTreeSearch;
}

void TreeSearch::ResetDefaultTreeSearch()
{
    if (s_defaultTreeSearch)
    {
        delete s_defaultTreeSearch;
        s_defaultTreeSearch = nullptr;
    }
}

Search* TreeSearch::SearchFor(XenObject* value)
{
    QueryScope* scope = GetTreeSearchScope();
    if (!value)
    {
        return Search::SearchFor(QStringList(), QStringList(), nullptr, scope);
    }

    QStringList refs;
    refs.append(value->OpaqueRef());

    QStringList types;
    types.append(value->GetObjectType());

    return Search::SearchFor(refs, types, value->GetConnection(), scope);
}

QueryScope* TreeSearch::GetTreeSearchScope()
{
    ObjectTypes types = Search::DefaultObjectTypes();
    types |= ObjectTypes::Pool;

    SettingsManager& settings = SettingsManager::instance();

    if (settings.getDefaultTemplatesVisible())
        types |= ObjectTypes::DefaultTemplate;

    if (settings.getUserTemplatesVisible())
        types |= ObjectTypes::UserTemplate;

    if (settings.getLocalSRsVisible())
        types |= ObjectTypes::LocalSR;

    return new QueryScope(types);
}
