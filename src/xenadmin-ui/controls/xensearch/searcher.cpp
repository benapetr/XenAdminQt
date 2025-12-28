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

#include "searcher.h"
#include "queryelement.h"
#include "groupingcontrol.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>

// SearchFor stub implementation
SearchFor::SearchFor(QWidget* parent)
    : QWidget(parent)
    , currentScope_(nullptr)
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    
    QLabel* label = new QLabel("Search for:", this);
    layout->addWidget(label);
    
    // TODO: Add object type checkboxes (VMs, Templates, Snapshots, Servers, Pools, etc.)
    QLabel* stubLabel = new QLabel("SearchFor stub - TODO: Implement", this);
    stubLabel->setStyleSheet("color: #888; font-style: italic;");
    layout->addWidget(stubLabel);
    
    layout->addStretch();
}

QueryScope* SearchFor::GetQueryScope() const
{
    if (this->currentScope_)
        return this->currentScope_;
    
    // Return default scope (all object types except folders)
    return new QueryScope(ObjectTypes::AllExcFolders);
}

void SearchFor::SetQueryScope(QueryScope* scope)
{
    this->currentScope_ = scope;
    // TODO: Update checkboxes to match scope
}

void SearchFor::BlankSearch()
{
    // TODO: Set default checkboxes
    emit QueryChanged();
}

// Searcher implementation
Searcher::Searcher(QWidget* parent)
    : QWidget(parent)
    , queryElement_(nullptr)
    , groupingControl_(nullptr)
    , searchFor_(nullptr)
    , saveButton_(nullptr)
    , closeButton_(nullptr)
    , groupsLabel_(nullptr)
    , maxHeight_(400)
{
    this->setupUi();
}

Searcher::~Searcher()
{
}

void Searcher::setupUi()
{
    this->setAutoFillBackground(true);
    QPalette pal = this->palette();
    pal.setColor(QPalette::Window, Qt::white);
    this->setPalette(pal);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // Top row: SearchFor widget
    this->searchFor_ = new SearchFor(this);
    mainLayout->addWidget(this->searchFor_);

    // QueryElement
    this->queryElement_ = new QueryElement(this, this);
    mainLayout->addWidget(this->queryElement_);

    // Groups label
    this->groupsLabel_ = new QLabel("Group by:", this);
    mainLayout->addWidget(this->groupsLabel_);

    // GroupingControl
    this->groupingControl_ = new GroupingControl(this);
    this->groupingControl_->SetSearcher(this);
    mainLayout->addWidget(this->groupingControl_);

    // Bottom row: Save and Close buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    this->saveButton_ = new QPushButton("Save", this);
    this->saveButton_->setFixedHeight(23);
    buttonLayout->addWidget(this->saveButton_);

    this->closeButton_ = new QPushButton("Close", this);
    this->closeButton_->setFixedHeight(23);
    buttonLayout->addWidget(this->closeButton_);

    mainLayout->addLayout(buttonLayout);
    mainLayout->addStretch();

    // Connect signals
    connect(this->queryElement_, &QueryElement::QueryChanged,
            this, &Searcher::onQueryElementQueryChanged);
    connect(this->groupingControl_, &GroupingControl::GroupingChanged,
            this, &Searcher::onGroupingControlGroupingChanged);
    connect(this->searchFor_, &SearchFor::QueryChanged,
            this, &Searcher::onSearchForQueryChanged);
    connect(this->saveButton_, &QPushButton::clicked,
            this, &Searcher::onSaveButtonClicked);
    connect(this->closeButton_, &QPushButton::clicked,
            this, &Searcher::onCloseButtonClicked);

    this->setVisible(false);  // Start collapsed
}

void Searcher::onQueryElementQueryChanged()
{
    emit SearchChanged();
}

void Searcher::onGroupingControlGroupingChanged()
{
    emit SearchChanged();
}

void Searcher::onSearchForQueryChanged()
{
    emit SearchForChanged();
    emit SearchChanged();
}

void Searcher::onSaveButtonClicked()
{
    emit SaveRequested();
}

void Searcher::onCloseButtonClicked()
{
    this->ToggleExpandedState(false);
}

void Searcher::onQueryElementResize()
{
    this->updateHeight();
}

void Searcher::updateHeight()
{
    // Calculate content height
    int contentsHeight = this->queryElement_->height() + 
                         this->searchFor_->height() +
                         this->groupingControl_->height() + 100;

    // Constrain to maxHeight
    int newHeight = qMin(this->maxHeight_, contentsHeight);
    this->setFixedHeight(newHeight);
}

Search* Searcher::GetSearch() const
{
    QueryScope* scope = this->GetQueryScope();
    QueryFilter* filter = this->GetQueryFilter();
    Grouping* grouping = this->GetGrouping();

    // Create Query from scope and filter
    Query* query = new Query(scope, filter);

    // Create Search with default name
    QList<QPair<QString, int>> columns;  // Empty columns list
    QList<Sort> sorting;                  // Empty sorting list
    
    return new Search(query, grouping, "New Search", "", false, columns, sorting);
}

void Searcher::SetSearch(Search* search)
{
    if (!search)
        return;

    // Set search-for first (important for filtering applicable options)
    if (this->searchFor_ && search->GetQuery())
    {
        this->searchFor_->SetQueryScope(search->GetQuery()->getQueryScope());
    }

    // Set query filter
    if (this->queryElement_ && search->GetQuery())
    {
        this->queryElement_->SetQueryFilter(search->GetQuery()->getQueryFilter());
    }

    // Set grouping
    if (this->groupingControl_)
    {
        this->groupingControl_->SetGrouping(search->GetGrouping());
    }
}

QueryScope* Searcher::GetQueryScope() const
{
    if (this->searchFor_)
        return this->searchFor_->GetQueryScope();
    
    return new QueryScope(ObjectTypes::AllExcFolders);
}

QueryFilter* Searcher::GetQueryFilter() const
{
    if (this->queryElement_)
        return this->queryElement_->GetQueryFilter();
    
    return nullptr;
}

Grouping* Searcher::GetGrouping() const
{
    if (this->groupingControl_)
        return this->groupingControl_->GetGrouping();
    
    return nullptr;
}

void Searcher::ToggleExpandedState(bool expand)
{
    this->setVisible(expand);

    if (this->isVisible())
    {
        // TODO: Enable save button only if there are active connections
        this->saveButton_->setEnabled(true);
    }

    emit SearchPanelExpandChanged();
}

void Searcher::BlankSearch()
{
    if (this->searchFor_)
        this->searchFor_->BlankSearch();
    
    if (this->queryElement_)
        this->queryElement_->SelectDefaultQueryType();
}
