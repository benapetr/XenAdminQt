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

#ifndef QUERYELEMENT_H
#define QUERYELEMENT_H

#include <QWidget>
#include <QDateTime>
#include <QPointer>
#include "../../../xenlib/xensearch/queryfilter.h"
#include "querytype.h"

class QComboBox;
class QDateTimeEdit;
class QDoubleSpinBox;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QVBoxLayout;
class Searcher;
class QueryScope;
class ResourceSelectButton;
class Search;

/**
 * QueryElement - one row of the search editor, with its nested rows
 *
 * Qt port of C# XenAdmin.Controls.XenSearch.QueryElement. A row shows a query type
 * ("Name", "Pool", "All of", ...) and the controls that type asks for. Group types (All of /
 * Any of / None of) hold sub-rows below; parent/child types ("Pool", "Server", "Disks", ...)
 * hold one sub-row on the same line that filters the related objects, with its own scope.
 */
class QueryElement : public QWidget
{
    Q_OBJECT

    public:
        explicit QueryElement(QWidget* parent = nullptr);
        explicit QueryElement(Searcher* searcher, QWidget* parent = nullptr);
        //! queryScope is normally null (use the searcher's scope); it is set for the
        //! sub-row of a parent/child query
        QueryElement(Searcher* searcher, QueryScope* queryScope,
                     QueryElement* parentQueryElement, QWidget* parent = nullptr);
        ~QueryElement() override;

        //! The query this row builds; a new object owned by the caller (may be null)
        QueryFilter* GetQueryFilter() const;

        //! Show the query (not taken over; may be null for "Select a filter...")
        void SetQueryFilter(QueryFilter* filter);

        void SetSearcher(Searcher* searcher);
        void SelectDefaultQueryType();

        // Control access for the query types
        QString getMatchTypeSelection() const;
        void setMatchTypeSelection(const QString& value);

        QString getTextBoxValue() const;
        void setTextBoxValue(const QString& value);

        QString getComboBoxSelection() const;
        void setComboBoxSelection(const QString& value);
        //! Value of the selected combo entry (see QueryElement::ComboEntry)
        QVariant getComboBoxValue() const;
        void setComboBoxValue(const QVariant& value);

        qint64 getNumericValue() const;
        void setNumericValue(qint64 value);

        double getDoubleValue() const;
        void setDoubleValue(double value);

        QDateTime getDateTimeValue() const;
        void setDateTimeValue(const QDateTime& value);

        QString getResourceSelection() const;
        QString getResourceSelectionUuid() const;
        void setResourceSelection(const QString& ref);
        void setResourceSelectionUuid(const QString& uuid);

        //! Queries of the sub-rows (new objects owned by the caller)
        QList<QueryFilter*> getSubQueries() const;

        //! C# GroupQueryType.FromQuery: one sub-row per query, reusing existing rows
        void setGroupSubQueries(const QList<QueryFilter*>& queries);
        //! C# RecursiveQueryType.FromQuery: a single sub-row with the given scope
        void setParentChildSubQuery(QueryFilter* query, QueryScope* scope);

        //! Search listing the objects for the resource picker (owned by the caller)
        Search* getSearchForResourceSelectButton() const;

        //! An entry for getComboButtonEntries() with display text different from its value
        static QVariant ComboEntry(const QString& text, const QVariant& value);

    signals:
        void QueryChanged();

    private slots:
        void populateQueryTypeCombo();
        void populateResourceSelectButton();
        void onQueryTypeActivated(int index);
        void onMatchTypeChanged(int index);
        void onRemoveClicked();
        void onSearchForChanged();
        void onSomeThingChanged();
        void emitQueryChanged();

    private:
        void setupUi();
        XenSearch::QueryType* currentQueryType() const { return this->currentQueryType_; }
        void setCurrentQueryType(XenSearch::QueryType* queryType);
        void repopulateAllQueryTypeCombos();
        bool wantQueryType(XenSearch::QueryType* queryType) const;
        void selectQueryTypeFor(QueryFilter* query);

        void setup(bool queryTypeDropDownChanged = false);
        void setup2();
        void populateComboButton(QComboBox* combo, const QVariantList& values);

        bool isSubQueryElement() const { return this->parentQueryElement_ != nullptr; }
        bool hasDummySubQuery() const;
        void addDummySubQuery(QueryScope* queryScope);
        void addSubQueryElement(QueryElement* element);
        void removeSubQueryElement(QueryElement* element);
        void clearSubQueryElements();
        void refreshSubQueryElements();
        void removeUnwantedFilters();

        // Controls
        QComboBox* queryTypeCombo_ = nullptr;
        QHBoxLayout* parentChildLayout_ = nullptr;  // sub-row of a parent/child query
        QComboBox* matchTypeCombo_ = nullptr;
        QComboBox* comboBox_ = nullptr;
        QLineEdit* textBox_ = nullptr;
        QSpinBox* numericUpDown_ = nullptr;
        QDoubleSpinBox* doubleSpinBox_ = nullptr;
        QLabel* unitsLabel_ = nullptr;
        QDateTimeEdit* dateTimePicker_ = nullptr;
        ResourceSelectButton* resourceSelectButton_ = nullptr;
        QPushButton* removeButton_ = nullptr;
        QVBoxLayout* subQueryLayout_ = nullptr;      // sub-rows of a group query

        // State
        Searcher* searcher_ = nullptr;
        QueryScope* queryScope_ = nullptr;
        QueryElement* parentQueryElement_ = nullptr;
        XenSearch::QueryType* currentQueryType_ = nullptr;
        QList<QueryElement*> subQueryElements_;
        QueryFilter* lastQueryFilter_ = nullptr;
        bool populating_ = false;
};

#endif // QUERYELEMENT_H
