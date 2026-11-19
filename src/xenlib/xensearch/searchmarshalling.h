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

#ifndef SEARCHMARSHALLING_H
#define SEARCHMARSHALLING_H

#include "../xenlib_global.h"
#include <QDomDocument>
#include <QDomElement>
#include <QList>
#include <QString>

class Search;
class QueryFilter;
class Grouping;

/**
 * Saved searches in XenCenter's XML format
 *
 * Qt port of C# XenAdmin.XenSearch.SearchMarshalling and the ToXmlNode/(XmlNode) members of
 * Query, QueryScope, the query filters, the groupings and Sort. Elements are named after the C#
 * classes (generic arguments dropped) and attributes use C# enum names, so searches saved by
 * XenCenter (pool gui_config "xensearch.Search-<uuid>" and .xensearch files) load here and the
 * other way round. Filters with no C# counterpart (the "Select a filter..." placeholder) are left
 * out, as in C#.
 */
class XENLIB_EXPORT SearchMarshalling
{
    public:
        //! C# CURRENT_SEARCH_MAJOR_VERSION / MINOR_VERSION
        static const int MajorVersion = 2;
        static const int MinorVersion = 0;

        //! One <Search> element, as stored in a pool's gui_config (C# SearchToXML)
        static QString SearchToXml(const Search* search);
        //! A .xensearch file: declaration, doctype and <Searches> (C# SearchesToXML)
        static QString SearchesToXml(const QList<const Search*>& searches);

        //! A single search (gui_config value); null if it cannot be read (C# LoadSearch(string))
        static Search* LoadSearch(const QString& xml);
        //! Every search in a file (C# LoadSearches); empty if the file cannot be read
        static QList<Search*> LoadSearches(const QString& xml, bool* ok = nullptr);

        static Search* SearchFromElement(const QDomElement& element);
        static QDomElement SearchToElement(QDomDocument& document, const Search* search);

        //! Null element for filters that are not saved
        static QDomElement QueryFilterToElement(QDomDocument& document, const QueryFilter* filter);
        //! Null for elements that are not a known filter
        static QueryFilter* QueryFilterFromElement(const QDomElement& element);
        //! A deep copy (owned by the caller), by a round trip through the XML form
        static QueryFilter* CloneQueryFilter(const QueryFilter* filter);

        static QDomElement GroupingToElement(QDomDocument& document, const Grouping* grouping);
        static Grouping* GroupingFromElement(const QDomElement& element);
};

#endif // SEARCHMARSHALLING_H
