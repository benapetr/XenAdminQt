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

#include "queryelement.h"
#include <QVBoxLayout>
#include <QLabel>

QueryElement::QueryElement(QWidget* parent)
    : QWidget(parent)
    , queryTypeCombo_(nullptr)
    , searcher_(nullptr)
    , currentFilter_(nullptr)
{
    this->setupUi();
}

QueryElement::QueryElement(Searcher* searcher, QWidget* parent)
    : QWidget(parent)
    , queryTypeCombo_(nullptr)
    , searcher_(searcher)
    , currentFilter_(nullptr)
{
    this->setupUi();
}

QueryElement::~QueryElement()
{
    delete this->currentFilter_;
}

void QueryElement::setupUi()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create query type selector
    this->queryTypeCombo_ = new QComboBox(this);
    this->queryTypeCombo_->addItem("All");
    this->queryTypeCombo_->addItem("Name");
    this->queryTypeCombo_->addItem("Description");
    this->queryTypeCombo_->addItem("Tags");
    this->queryTypeCombo_->addItem("Power State");
    this->queryTypeCombo_->addItem("OS Name");
    
    // TODO: Add all 100+ query types from C# implementation
    
    layout->addWidget(this->queryTypeCombo_);
    
    // TODO: Add value editor widgets based on selected query type
    // TODO: Add nested QueryElement container for And/Or/Nor groups
    
    QLabel* stubLabel = new QLabel("QueryElement stub - TODO: Implement full UI", this);
    stubLabel->setStyleSheet("color: #888; font-style: italic;");
    layout->addWidget(stubLabel);
    
    layout->addStretch();

    connect(this->queryTypeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QueryElement::onQueryTypeChanged);
}

void QueryElement::SetSearcher(Searcher* searcher)
{
    this->searcher_ = searcher;
}

QueryFilter* QueryElement::GetQueryFilter() const
{
    // TODO: Build QueryFilter from current UI state
    if (this->currentFilter_)
        return this->currentFilter_;
    
    // Return null filter (matches all)
    return nullptr;
}

void QueryElement::SetQueryFilter(QueryFilter* filter)
{
    delete this->currentFilter_;
    this->currentFilter_ = filter;
    
    // TODO: Update UI to match filter
}

void QueryElement::SelectDefaultQueryType()
{
    this->queryTypeCombo_->setCurrentIndex(0);  // "All"
}

void QueryElement::onQueryTypeChanged(int index)
{
    Q_UNUSED(index);
    
    // TODO: Update value editor widgets based on selected query type
    // TODO: Handle nested QueryElement for And/Or/Nor types
    
    emit QueryChanged();
}
