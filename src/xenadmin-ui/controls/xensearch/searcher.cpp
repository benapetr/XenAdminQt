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

// SearchFor static members
const ObjectTypes SearchFor::CUSTOM;

// SearchFor implementation
SearchFor::SearchFor(QWidget* parent)
    : QWidget(parent)
    , comboBox_(nullptr)
    , customValue_(ObjectTypes::AllExcFolders)
    , savedTypes_(ObjectTypes::AllExcFolders)
    , autoSelecting_(false)
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    
    QLabel* label = new QLabel("Search for:", this);
    layout->addWidget(label);
    
    this->comboBox_ = new QComboBox(this);
    layout->addWidget(this->comboBox_);
    layout->addStretch();

    this->initializeDictionaries();
    this->populateComboBox();

    connect(this->comboBox_, QOverload<int>::of(&QComboBox::activated),
            this, &SearchFor::onComboActivated);
}

void SearchFor::initializeDictionaries()
{
    // Single types with names
    this->typeNames_[ObjectTypes::Pool] = "Pool";
    this->typeNames_[ObjectTypes::Server] = "Server";
    this->typeNames_[ObjectTypes::DisconnectedServer] = "Disconnected Server";
    this->typeNames_[ObjectTypes::VM] = "VM";
    this->typeNames_[ObjectTypes::Snapshot] = "Snapshot";
    this->typeNames_[ObjectTypes::UserTemplate] = "Custom Template";
    this->typeNames_[ObjectTypes::DefaultTemplate] = "Default Template";
    this->typeNames_[ObjectTypes::RemoteSR] = "Remote Storage";
    this->typeNames_[ObjectTypes::LocalSR] = "Local Storage";
    this->typeNames_[ObjectTypes::VDI] = "Virtual Disk";
    this->typeNames_[ObjectTypes::Network] = "Network";
    this->typeNames_[ObjectTypes::Folder] = "Folder";
    this->typeNames_[ObjectTypes::Appliance] = "vApp";

    // Combination types
    ObjectTypes allSR = ObjectTypes::LocalSR | ObjectTypes::RemoteSR;
    this->typeNames_[allSR] = "All Storage";

    ObjectTypes serversAndVMs = ObjectTypes::Server | ObjectTypes::DisconnectedServer | ObjectTypes::VM;
    this->typeNames_[serversAndVMs] = "Servers and VMs";

    ObjectTypes serversVMsTemplatesRemoteSR = ObjectTypes::Server | ObjectTypes::DisconnectedServer | 
                                              ObjectTypes::VM | ObjectTypes::UserTemplate | ObjectTypes::RemoteSR;
    this->typeNames_[serversVMsTemplatesRemoteSR] = "Servers, VMs, Custom Templates, and Remote Storage";

    ObjectTypes serversVMsTemplatesAllSR = ObjectTypes::Server | ObjectTypes::DisconnectedServer | 
                                           ObjectTypes::VM | ObjectTypes::UserTemplate | 
                                           ObjectTypes::RemoteSR | ObjectTypes::LocalSR;
    this->typeNames_[serversVMsTemplatesAllSR] = "Servers, VMs, Custom Templates, and All Storage";

    this->typeNames_[ObjectTypes::AllExcFolders] = "All types";
    this->typeNames_[ObjectTypes::AllIncFolders] = "All types and folders";

    // Custom placeholder
    this->typeNames_[CUSTOM] = "Custom...";

    // TODO: Load icons for each type
}

void SearchFor::populateComboBox()
{
    this->comboBox_->clear();

    // Single types
    this->comboBox_->addItem(this->getTypeName(ObjectTypes::Pool), 
                             QVariant::fromValue(static_cast<int>(ObjectTypes::Pool)));
    this->comboBox_->addItem(this->getTypeName(ObjectTypes::Server), 
                             QVariant::fromValue(static_cast<int>(ObjectTypes::Server)));
    this->comboBox_->addItem(this->getTypeName(ObjectTypes::DisconnectedServer), 
                             QVariant::fromValue(static_cast<int>(ObjectTypes::DisconnectedServer)));
    this->comboBox_->addItem(this->getTypeName(ObjectTypes::VM), 
                             QVariant::fromValue(static_cast<int>(ObjectTypes::VM)));
    this->comboBox_->addItem(this->getTypeName(ObjectTypes::Snapshot), 
                             QVariant::fromValue(static_cast<int>(ObjectTypes::Snapshot)));
    this->comboBox_->addItem(this->getTypeName(ObjectTypes::UserTemplate), 
                             QVariant::fromValue(static_cast<int>(ObjectTypes::UserTemplate)));
    this->comboBox_->addItem(this->getTypeName(ObjectTypes::DefaultTemplate), 
                             QVariant::fromValue(static_cast<int>(ObjectTypes::DefaultTemplate)));
    this->comboBox_->addItem(this->getTypeName(ObjectTypes::RemoteSR), 
                             QVariant::fromValue(static_cast<int>(ObjectTypes::RemoteSR)));
    
    ObjectTypes allSR = ObjectTypes::LocalSR | ObjectTypes::RemoteSR;
    this->comboBox_->addItem(this->getTypeName(allSR), 
                             QVariant::fromValue(static_cast<int>(allSR)));
    
    this->comboBox_->addItem(this->getTypeName(ObjectTypes::VDI), 
                             QVariant::fromValue(static_cast<int>(ObjectTypes::VDI)));
    this->comboBox_->addItem(this->getTypeName(ObjectTypes::Network), 
                             QVariant::fromValue(static_cast<int>(ObjectTypes::Network)));
    this->comboBox_->addItem(this->getTypeName(ObjectTypes::Folder), 
                             QVariant::fromValue(static_cast<int>(ObjectTypes::Folder)));

    // Separator
    this->comboBox_->insertSeparator(this->comboBox_->count());

    // Combination types
    ObjectTypes serversAndVMs = ObjectTypes::Server | ObjectTypes::DisconnectedServer | ObjectTypes::VM;
    this->comboBox_->addItem(this->getTypeName(serversAndVMs), 
                             QVariant::fromValue(static_cast<int>(serversAndVMs)));
    
    ObjectTypes serversVMsTemplatesRemoteSR = ObjectTypes::Server | ObjectTypes::DisconnectedServer | 
                                              ObjectTypes::VM | ObjectTypes::UserTemplate | ObjectTypes::RemoteSR;
    this->comboBox_->addItem(this->getTypeName(serversVMsTemplatesRemoteSR), 
                             QVariant::fromValue(static_cast<int>(serversVMsTemplatesRemoteSR)));
    
    ObjectTypes serversVMsTemplatesAllSR = ObjectTypes::Server | ObjectTypes::DisconnectedServer | 
                                           ObjectTypes::VM | ObjectTypes::UserTemplate | 
                                           ObjectTypes::RemoteSR | ObjectTypes::LocalSR;
    this->comboBox_->addItem(this->getTypeName(serversVMsTemplatesAllSR), 
                             QVariant::fromValue(static_cast<int>(serversVMsTemplatesAllSR)));

    // Separator
    this->comboBox_->insertSeparator(this->comboBox_->count());

    // All types
    this->comboBox_->addItem(this->getTypeName(ObjectTypes::AllExcFolders), 
                             QVariant::fromValue(static_cast<int>(ObjectTypes::AllExcFolders)));
    this->comboBox_->addItem(this->getTypeName(ObjectTypes::AllIncFolders), 
                             QVariant::fromValue(static_cast<int>(ObjectTypes::AllIncFolders)));

    // Separator
    this->comboBox_->insertSeparator(this->comboBox_->count());

    // Custom option
    this->comboBox_->addItem(this->getTypeName(CUSTOM), 
                             QVariant::fromValue(static_cast<int>(CUSTOM)));

    // Default selection
    this->comboBox_->setCurrentIndex(this->comboBox_->findData(
        QVariant::fromValue(static_cast<int>(ObjectTypes::AllExcFolders))));
}

QString SearchFor::getTypeName(ObjectTypes type) const
{
    if (this->typeNames_.contains(type))
        return this->typeNames_[type];
    return "Unknown";
}

ObjectTypes SearchFor::getSelectedItemTag() const
{
    int index = this->comboBox_->currentIndex();
    if (index < 0)
        return ObjectTypes::None;
    
    QVariant data = this->comboBox_->itemData(index);
    return static_cast<ObjectTypes>(data.toInt());
}

QueryScope* SearchFor::getAsScope() const
{
    ObjectTypes types = this->getSelectedItemTag();
    if (types == CUSTOM)
        return new QueryScope(this->customValue_);
    else
        return new QueryScope(types);
}

void SearchFor::setFromScope(QueryScope* scope)
{
    if (!scope)
        return;

    this->setFromScope(scope->getObjectTypes());
}

void SearchFor::setFromScope(ObjectTypes types)
{
    // Try to find exact match
    for (int i = 0; i < this->comboBox_->count(); ++i)
    {
        QVariant data = this->comboBox_->itemData(i);
        if (data.isValid() && static_cast<ObjectTypes>(data.toInt()) == types)
        {
            this->comboBox_->setCurrentIndex(i);
            return;
        }
    }

    // If no exact match, use Custom
    this->customValue_ = types;
    for (int i = 0; i < this->comboBox_->count(); ++i)
    {
        QVariant data = this->comboBox_->itemData(i);
        if (data.isValid() && static_cast<ObjectTypes>(data.toInt()) == CUSTOM)
        {
            this->comboBox_->setCurrentIndex(i);
            return;
        }
    }
}

void SearchFor::onComboActivated(int index)
{
    Q_UNUSED(index);
    
    ObjectTypes types = this->getSelectedItemTag();
    
    if (types == CUSTOM && !this->autoSelecting_)
    {
        this->onCustomDialogRequested();
        return;
    }

    if (types != CUSTOM)
        this->savedTypes_ = types;

    emit QueryChanged();
}

void SearchFor::onCustomDialogRequested()
{
    // TODO: Launch custom dialog
    // For now, just revert to saved selection
    this->autoSelecting_ = true;
    this->setFromScope(this->savedTypes_);
    this->autoSelecting_ = false;
}

QueryScope* SearchFor::GetQueryScope() const
{
    return this->getAsScope();
}

void SearchFor::SetQueryScope(QueryScope* scope)
{
    this->autoSelecting_ = true;
    if (scope)
        this->customValue_ = scope->getObjectTypes();
    this->setFromScope(scope);
    this->autoSelecting_ = false;
}

void SearchFor::BlankSearch()
{
    this->SetQueryScope(new QueryScope(ObjectTypes::None));
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
