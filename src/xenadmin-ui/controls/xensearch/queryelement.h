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
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDateTimeEdit>
#include <QVBoxLayout>
#include <QLabel>
#include "../../../xenlib/xensearch/queryfilter.h"
#include "querytype.h"

// Forward declarations
class Searcher;
class QueryScope;
class ResourceSelectButton;
class Search;

/**
 * QueryElement - Search criterion editor widget
 * 
 * Full implementation matching C# xenadmin/XenAdmin/Controls/XenSearch/QueryElement.cs
 * 
 * Features:
 * - Multiple QueryType classes for different criterion types
 * - Nested QueryElement support for And/Or/Nor groups
 * - Dynamic UI based on selected QueryType
 * - Query validation and building
 * 
 * Based on C# xenadmin/XenAdmin/Controls/XenSearch/QueryElement.cs
 */
class QueryElement : public QWidget
{
    Q_OBJECT

    public:
        explicit QueryElement(QWidget* parent = nullptr);
        explicit QueryElement(Searcher* searcher, QWidget* parent = nullptr);
        explicit QueryElement(Searcher* searcher, QueryScope* queryScope,
                             QueryElement* parentQueryElement, QWidget* parent = nullptr);
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

        // UI state access methods (used by QueryType classes)
        QString getMatchTypeSelection() const;
        void setMatchTypeSelection(const QString& value);

        QString getTextBoxValue() const;
        void setTextBoxValue(const QString& value);

        QString getComboBoxSelection() const;
        void setComboBoxSelection(const QString& value);

        qint64 getNumericValue() const;
        void setNumericValue(qint64 value);

        double getDoubleValue() const;
        void setDoubleValue(double value);

        QDateTime getDateTimeValue() const;
        void setDateTimeValue(const QDateTime& value);

        QString getResourceSelection() const;
        void setResourceSelection(const QString& ref);

        QList<QueryFilter*> getSubQueries() const;
        void setSubQueries(const QList<QueryFilter*>& queries);

        /**
         * Get the Search instance for resource picker population
         * C# QueryElement.cs line 462: GetSearchForResourceSelectButton()
         */
        Search* getSearchForResourceSelectButton();

    signals:
        /**
         * Emitted when the query changes
         */
        void QueryChanged();

    private slots:
        void onQueryTypeChanged(int index);
        void onMatchTypeChanged(int index);
        void onTextChanged();
        void onComboChanged(int index);
        void onNumericChanged(int value);
        void onDoubleChanged(double value);
        void onDateTimeChanged(const QDateTime& dateTime);
        void onResourceSelected(const QString& ref);
        void onRemoveClicked();
        void onSubQueryChanged();
        void onSomeThingChanged(); // Refreshes combo boxes when QueryType cache changes

    private:
        void setupUi();
        void setupControls();
        void populateQueryTypeCombo(bool showAll = false);
        void refreshSubQueryElements();
        void clearSubQueryElements();
        void addSubQueryElement(QueryElement* element);
        void removeSubQueryElement(QueryElement* element);
        bool wantQueryType(XenSearch::QueryType* queryType) const;

        // UI widgets
        QComboBox* queryTypeCombo_;
        QComboBox* matchTypeCombo_;
        QComboBox* comboBox_;           // General combo button (showComboButton)
        QLineEdit* textBox_;
        QSpinBox* numericUpDown_;
        QDoubleSpinBox* doubleSpinBox_;
        QLabel* unitsLabel_;
        QDateTimeEdit* dateTimePicker_;
        ResourceSelectButton* resourceSelectButton_;
        QPushButton* removeButton_;
        QVBoxLayout* subQueryLayout_;

        // State
        Searcher* searcher_;
        QueryScope* queryScope_;
        QueryElement* parentQueryElement_;
        XenSearch::QueryType* currentQueryType_;
        QList<QueryElement*> subQueryElements_;
        QueryFilter* lastQueryFilter_;

        // TODO: Add UI controls for:
        // - Query type selector (dropdown with 100+ types)
        // - Value editors (text, number, date, enum, resource selector)
        // - Nested QueryElement container for And/Or/Nor groups
        // - Add/remove query buttons
        // - Resource selection popups
};

#endif // QUERYELEMENT_H
