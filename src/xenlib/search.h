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

// search.h - Search definition combining query, grouping, and metadata
// Port of C# xenadmin/XenModel/XenSearch/Search.cs (simplified for overview panel)
#ifndef SEARCH_H
#define SEARCH_H

#include "query.h"
#include "grouping.h"
#include <QString>
#include <QList>

// Forward declaration
class XenConnection;

/**
 * @brief A search definition combining a query (what to match) with a grouping (how to organize results)
 *
 * C# equivalent: xenadmin/XenModel/XenSearch/Search.cs
 *
 * NOTE: This is a simplified initial implementation for the overview panel.
 * C# has additional features like columns, sorting, folder navigator, persistence, etc.
 * These can be added later as needed.
 */
class Search
{
    public:
        /**
         * @brief Constructor with query, grouping, and name
         *
         * C# equivalent: Search(Query query, Grouping grouping, string name, string uuid, bool defaultSearch)
         *
         * @param query The query (what objects to match). Takes ownership.
         * @param grouping The grouping (how to organize results). Takes ownership.
         * @param name The search name
         * @param uuid Optional UUID for saved searches
         * @param defaultSearch True if this is a built-in default search
         */
        Search(Query* query, Grouping* grouping, const QString& name,
            const QString& uuid = QString(), bool defaultSearch = false);

        ~Search();

        /**
         * @brief Get the query
         *
         * C# equivalent: Query property
         * @return Pointer to the Query instance owned by this Search
         */
        Query* getQuery() const
        {
            return m_query;
        }

        /**
         * @brief Get the grouping
         *
         * C# equivalent: Grouping property
         * @return Pointer to the Grouping instance owned by this Search
         */
        Grouping* getGrouping() const
        {
            return m_grouping;
        }

        /**
         * @brief Get the effective grouping (used internally)
         *
         * Different from Grouping when folder navigator is shown
         * C# equivalent: EffectiveGrouping property
         * @return Pointer to the effective Grouping instance
         */
        Grouping* getEffectiveGrouping() const;

        /**
         * @brief Get the search name
         *
         * C# equivalent: Name property
         * @return Name of the search
         */
        QString getName() const
        {
            return m_name;
        }

        /**
         * @brief Set the search name
         * @param name New search name
         */
        void setName(const QString& name)
        {
            m_name = name;
        }

        /**
         * @brief Get the search UUID (may be empty)
         *
         * C# equivalent: UUID property
         * @return UUID string (may be empty)
         */
        QString getUUID() const
        {
            return m_uuid;
        }

        /**
         * @brief Check if this is a default search
         *
         * C# equivalent: DefaultSearch property
         * @return True if this is a built-in default search
         */
        bool isDefaultSearch() const
        {
            return m_defaultSearch;
        }

        /**
         * @brief Get the connection this search is associated with
         *
         * C# equivalent: Connection property
         * @return Pointer to associated XenConnection or nullptr if none
         */
        XenConnection* getConnection() const
        {
            return m_connection;
        }

        /**
         * @brief Set the connection this search is associated with
         * @param connection Pointer to XenConnection (not owned)
         */
        void setConnection(XenConnection* connection)
        {
            m_connection = connection;
        }

        /**
         * @brief Get the number of items matched by this search
         *
         * C# equivalent: Items property
         * @return Number of matched items
         */
        int getItems() const
        {
            return m_items;
        }

        /**
         * @brief Set the number of items matched by this search
         * @param items Number of matched items
         */
        void setItems(int items)
        {
            m_items = items;
        }

        /**
         * @brief Create a search for a non-vApp grouping tag
         *
         * This is the key method for overview panel - creates searches when clicking
         * grouping nodes like "Servers", "Templates", etc.
         *
         * C# equivalent: SearchForNonVappGroup(Grouping grouping, object parent, object v)
         * Line 647 in Search.cs
         *
         * @param grouping The grouping algorithm
         * @param parent Parent group value (may be null)
         * @param group The group value
         * @return New Search instance configured for the group
         */
        static Search* searchForNonVappGroup(Grouping* grouping, const QVariant& parent, const QVariant& group);

        /**
         * @brief Create a search for a folder grouping tag
         *
         * C# equivalent: SearchForFolderGroup(Grouping grouping, object parent, object v)
         * Line 641 in Search.cs
         *
         * @param grouping The grouping algorithm
         * @param parent Parent group value (may be null)
         * @param group The group value
         * @return New Search instance configured for the group
         */
        static Search* searchForFolderGroup(Grouping* grouping, const QVariant& parent, const QVariant& group);

        /**
         * @brief Create a search for a vApp grouping tag
         *
         * C# equivalent: SearchForVappGroup(Grouping grouping, object parent, object v)
         * Line 652 in Search.cs
         *
         * @param grouping The grouping algorithm
         * @param parent Parent group value (may be null)
         * @param group The group value
         * @return New Search instance configured for the vApp group
         */
        static Search* searchForVappGroup(Grouping* grouping, const QVariant& parent, const QVariant& group);

    private:
        Query* m_query;              // The query (what to match) - owned
        Grouping* m_grouping;        // The grouping (how to organize) - owned
        QString m_name;              // Search name
        QString m_uuid;              // UUID for saved searches
        bool m_defaultSearch;        // True if built-in default search
        XenConnection* m_connection; // Associated connection (not owned)
        int m_items;                 // Number of items matched
};

#endif // SEARCH_H
