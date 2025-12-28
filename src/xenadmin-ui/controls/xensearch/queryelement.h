/* Copyright (c) Cloud Software Group, Inc.
 * 
 * Redistribution and use in source and binary forms, 
 * with or without modification, are permitted provided 
 * that the following conditions are met: 
 * 
 * *   Redistributions of source code must retain the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer. 
 * *   Redistributions in binary form must reproduce the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer in the documentation and/or other 
 *     materials provided with the distribution. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 */

#ifndef QUERYELEMENT_H
#define QUERYELEMENT_H

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include "../../../xenlib/xensearch/queryfilter.h"

// Forward declaration
class Searcher;

/**
 * QueryElement - Search criterion editor widget
 * 
 * STUB IMPLEMENTATION - Full C# version is ~2561 lines!
 * TODO: Port complete QueryElement implementation from:
 * xenadmin/XenAdmin/Controls/XenSearch/QueryElement.cs
 * 
 * Full implementation includes:
 * - 100+ QueryType classes for different criterion types
 * - Nested QueryElement support for And/Or/Nor groups
 * - Dynamic UI based on selected QueryType
 * - Parent-child queries (Pool→Host→VM)
 * - Custom field queries
 * - Resource selection popups
 * - Query validation and error handling
 * 
 * Based on C# xenadmin/XenAdmin/Controls/XenSearch/QueryElement.cs
 */
class QueryElement : public QWidget
{
    Q_OBJECT

public:
    explicit QueryElement(QWidget* parent = nullptr);
    explicit QueryElement(Searcher* searcher, QWidget* parent = nullptr);
    ~QueryElement();

    /**
     * Get the current query filter
     */
    QueryFilter* GetQueryFilter() const;

    /**
     * Set the query filter
     */
    void SetQueryFilter(QueryFilter* filter);

    /**
     * Set the associated Searcher
     */
    void SetSearcher(Searcher* searcher);

    /**
     * Select the default query type (empty filter)
     */
    void SelectDefaultQueryType();

signals:
    /**
     * Emitted when the query changes
     */
    void QueryChanged();

private slots:
    void onQueryTypeChanged(int index);

private:
    void setupUi();

    QComboBox* queryTypeCombo_;
    Searcher* searcher_;
    QueryFilter* currentFilter_;

    // TODO: Add UI controls for:
    // - Query type selector (dropdown with 100+ types)
    // - Value editors (text, number, date, enum, resource selector)
    // - Nested QueryElement container for And/Or/Nor groups
    // - Add/remove query buttons
    // - Resource selection popups
};

#endif // QUERYELEMENT_H
