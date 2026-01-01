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

#ifndef SEARCHER_H
#define SEARCHER_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include "../../../xenlib/xensearch/search.h"
#include "../../../xenlib/xensearch/queryscope.h"
#include "../../../xenlib/xensearch/queryfilter.h"
#include "../../../xenlib/xensearch/grouping.h"

// Forward declarations
class QueryElement;
class GroupingControl;
class SearchFor;

/**
 * Searcher - Query builder UI combining filters, grouping, and search scope
 * 
 * This is the main search configuration panel that combines:
 * - SearchFor widget (what to search for: VMs, Hosts, etc.)
 * - QueryElement (search criteria/filters)
 * - GroupingControl (how to group results)
 * 
 * Based on C# xenadmin/XenAdmin/Controls/XenSearch/Searcher.cs
 */
class Searcher : public QWidget
{
    Q_OBJECT

    public:
        explicit Searcher(QWidget* parent = nullptr);
        ~Searcher();

        /**
         * Get the complete search configuration
         */
        Search* GetSearch() const;

        /**
         * Set the search configuration
         */
        void SetSearch(Search* search);

        /**
         * Get the current query scope (what to search for)
         */
        QueryScope* GetQueryScope() const;

        /**
         * Get the current query filter (search criteria)
         */
        QueryFilter* GetQueryFilter() const;

        /**
         * Get the current grouping
         */
        Grouping* GetGrouping() const;

        /**
         * Get the maximum height for this control
         */
        int GetMaxHeight() const { return this->maxHeight_; }

        /**
         * Set the maximum height for this control
         */
        void SetMaxHeight(int maxHeight) { this->maxHeight_ = maxHeight; }

        /**
         * Toggle the expanded state of the search panel
         */
        void ToggleExpandedState(bool expand);

        /**
         * Reset to blank search
         */
        void BlankSearch();

    signals:
        /**
         * Emitted when the search configuration changes
         */
        void SearchChanged();

        /**
         * Emitted when the search-for (QueryScope) changes
         */
        void SearchForChanged();

        /**
         * Emitted when the user wants to save the current search
         */
        void SaveRequested();

        /**
         * Emitted when the search panel expand state changes
         */
        void SearchPanelExpandChanged();

    private slots:
        void onQueryElementQueryChanged();
        void onGroupingControlGroupingChanged();
        void onSearchForQueryChanged();
        void onSaveButtonClicked();
        void onCloseButtonClicked();
        void onQueryElementResize();

    private:
        void setupUi();
        void updateHeight();

        QueryElement* queryElement_;
        GroupingControl* groupingControl_;
        SearchFor* searchFor_;
        QPushButton* saveButton_;
        QPushButton* closeButton_;
        QLabel* groupsLabel_;
        int maxHeight_;
};

/**
 * SearchFor - Widget for selecting search scope (VMs, Hosts, etc.)
 * 
 * Port of C# xenadmin/XenAdmin/Controls/XenSearch/SearchFor.cs
 * 
 * Provides a dropdown to select which object types to search for.
 * Supports individual types, common combinations, and custom selections.
 */
class SearchFor : public QWidget
{
    Q_OBJECT

    public:
        explicit SearchFor(QWidget* parent = nullptr);

        QueryScope* GetQueryScope() const;
        void SetQueryScope(QueryScope* scope);
        void BlankSearch();

    signals:
        void QueryChanged();

    private slots:
        void onComboActivated(int index);
        void onCustomDialogRequested();

    private:
        void initializeDictionaries();
        void populateComboBox();
        void setFromScope(QueryScope* scope);
        void setFromScope(ObjectTypes types);
        ObjectTypes getSelectedItemTag() const;
        QueryScope* getAsScope() const;
        QString getTypeName(ObjectTypes type) const;

        QComboBox* comboBox_;
        QMap<ObjectTypes, QString> typeNames_;
        QMap<ObjectTypes, QIcon> typeIcons_;
        ObjectTypes customValue_;
        ObjectTypes savedTypes_;
        bool autoSelecting_;

        static const ObjectTypes CUSTOM = ObjectTypes::None;  // Special value for "Custom..."
};

#endif // SEARCHER_H
