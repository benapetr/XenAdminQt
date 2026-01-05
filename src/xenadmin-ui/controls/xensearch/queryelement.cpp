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
#include "searcher.h"
#include "resourceselectbutton.h"
#include "xenlib/xensearch/query.h"
#include "xenlib/xensearch/sort.h"
#include "xenlib/xensearch/grouping.h"
#include <QHBoxLayout>
#include <QLabel>

using namespace XenSearch;

QueryElement::QueryElement(QWidget* parent)
    : QWidget(parent)
    , queryTypeCombo_(nullptr)
    , matchTypeCombo_(nullptr)
    , comboBox_(nullptr)
    , textBox_(nullptr)
    , numericUpDown_(nullptr)
    , doubleSpinBox_(nullptr)
    , unitsLabel_(nullptr)
    , dateTimePicker_(nullptr)
    , resourceSelectButton_(nullptr)
    , removeButton_(nullptr)
    , subQueryLayout_(nullptr)
    , searcher_(nullptr)
    , queryScope_(nullptr)
    , parentQueryElement_(nullptr)
    , currentQueryType_(nullptr)
    , lastQueryFilter_(nullptr)
{
    this->setupUi();
    this->SelectDefaultQueryType();
}

QueryElement::QueryElement(Searcher* searcher, QWidget* parent)
    : QWidget(parent)
    , queryTypeCombo_(nullptr)
    , matchTypeCombo_(nullptr)  
    , comboBox_(nullptr)
    , textBox_(nullptr)
    , numericUpDown_(nullptr)
    , unitsLabel_(nullptr)
    , dateTimePicker_(nullptr)
    , removeButton_(nullptr)
    , subQueryLayout_(nullptr)
    , searcher_(searcher)
    , queryScope_(nullptr)
    , parentQueryElement_(nullptr)
    , currentQueryType_(nullptr)
    , lastQueryFilter_(nullptr)
{
    this->setupUi();
    this->SelectDefaultQueryType();
}

QueryElement::QueryElement(Searcher* searcher, QueryScope* queryScope, 
                         QueryElement* parentQueryElement, QWidget* parent)
    : QWidget(parent)
    , queryTypeCombo_(nullptr)
    , matchTypeCombo_(nullptr)
    , comboBox_(nullptr)
    , textBox_(nullptr)
    , numericUpDown_(nullptr)
    , unitsLabel_(nullptr)
    , dateTimePicker_(nullptr)
    , removeButton_(nullptr)
    , subQueryLayout_(nullptr)
    , searcher_(searcher)
    , queryScope_(queryScope)
    , parentQueryElement_(parentQueryElement)
    , currentQueryType_(nullptr)
    , lastQueryFilter_(nullptr)
{
    this->setupUi();
    this->SelectDefaultQueryType();
}

QueryElement::~QueryElement()
{
    this->clearSubQueryElements();
    delete this->lastQueryFilter_;
}

void QueryElement::setupUi()
{
    QHBoxLayout* mainLayout = new QHBoxLayout();
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(5);
    
    // Query type combo
    this->queryTypeCombo_ = new QComboBox(this);
    mainLayout->addWidget(this->queryTypeCombo_);
    
    // Match type combo (Is/Is not/Contains/etc.)
    this->matchTypeCombo_ = new QComboBox(this);
    this->matchTypeCombo_->setVisible(false);
    mainLayout->addWidget(this->matchTypeCombo_);
    
    // Text box for string queries
    this->textBox_ = new QLineEdit(this);
    this->textBox_->setVisible(false);
    mainLayout->addWidget(this->textBox_);
    
    // Combo box for enum queries
    this->comboBox_ = new QComboBox(this);
    this->comboBox_->setVisible(false);
    mainLayout->addWidget(this->comboBox_);
    
    // Numeric input for numeric queries (integer)
    this->numericUpDown_ = new QSpinBox(this);
    this->numericUpDown_->setRange(0, 999999);
    this->numericUpDown_->setVisible(false);
    mainLayout->addWidget(this->numericUpDown_);
    
    // Double numeric input (for decimal values)
    this->doubleSpinBox_ = new QDoubleSpinBox(this);
    this->doubleSpinBox_->setRange(0.0, 999999.99);
    this->doubleSpinBox_->setDecimals(2);
    this->doubleSpinBox_->setVisible(false);
    mainLayout->addWidget(this->doubleSpinBox_);
    
    this->unitsLabel_ = new QLabel(this);
    this->unitsLabel_->setVisible(false);
    mainLayout->addWidget(this->unitsLabel_);
    
    // Date/time picker for date queries
    this->dateTimePicker_ = new QDateTimeEdit(this);
    this->dateTimePicker_->setVisible(false);
    mainLayout->addWidget(this->dateTimePicker_);
    
    // Resource select button for UUID queries
    this->resourceSelectButton_ = new ResourceSelectButton(this);
    this->resourceSelectButton_->setVisible(false);
    mainLayout->addWidget(this->resourceSelectButton_);
    
    mainLayout->addStretch();
    
    // Remove button
    this->removeButton_ = new QPushButton("−", this);
    this->removeButton_->setMaximumWidth(30);
    this->removeButton_->setVisible(false);
    mainLayout->addWidget(this->removeButton_);
    
    // Sub-query layout for group queries
    this->subQueryLayout_ = new QVBoxLayout();
    this->subQueryLayout_->setContentsMargins(30, 0, 0, 0);
    this->subQueryLayout_->setSpacing(2);
    
    QVBoxLayout* verticalLayout = new QVBoxLayout();
    verticalLayout->setContentsMargins(0, 0, 0, 0);
    verticalLayout->setSpacing(0);
    verticalLayout->addLayout(mainLayout);
    verticalLayout->addLayout(this->subQueryLayout_);
    
    this->setLayout(verticalLayout);
    
    // Connect signals
    connect(this->queryTypeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QueryElement::onQueryTypeChanged);
    connect(this->matchTypeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QueryElement::onMatchTypeChanged);
    connect(this->textBox_, &QLineEdit::textChanged,
            this, &QueryElement::onTextChanged);
    connect(this->comboBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QueryElement::onComboChanged);
    connect(this->numericUpDown_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &QueryElement::onNumericChanged);
    connect(this->doubleSpinBox_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &QueryElement::onDoubleChanged);
    connect(this->dateTimePicker_, &QDateTimeEdit::dateTimeChanged,
            this, &QueryElement::onDateTimeChanged);
    connect(this->resourceSelectButton_, &ResourceSelectButton::itemSelected,
            this, &QueryElement::onResourceSelected);
    connect(this->removeButton_, &QPushButton::clicked,
            this, &QueryElement::onRemoveClicked);
    
    this->populateQueryTypeCombo();
}

void QueryElement::SetSearcher(Searcher* searcher)
{
    this->searcher_ = searcher;
}

void QueryElement::SelectDefaultQueryType()
{
    this->currentQueryType_ = QueryTypeRegistry::instance()->getDefaultQueryType();
    this->setupControls();
}

void QueryElement::populateQueryTypeCombo(bool showAll)
{
    this->queryTypeCombo_->clear();
    
    int lastGroup = -1;
    const QList<QueryType*>& queryTypes = QueryTypeRegistry::instance()->getAllQueryTypes();
    
    for (QueryType* queryType : queryTypes)
    {
        if (!showAll && !this->wantQueryType(queryType))
            continue;
        
        // Add separator between groups
        if (lastGroup != -1 && queryType->getGroup() != lastGroup)
        {
            this->queryTypeCombo_->insertSeparator(this->queryTypeCombo_->count());
        }
        lastGroup = queryType->getGroup();
        
        this->queryTypeCombo_->addItem(queryType->toString(), QVariant::fromValue(queryType));
    }
}

bool QueryElement::wantQueryType(QueryType* queryType) const
{
    QueryScope* scope = this->queryScope_;
    if (!scope && this->searcher_)
        scope = this->searcher_->GetQueryScope();
    
    if (!scope)
        return true;
    
    // Check if this query type applies to the current scope
    ObjectTypes queryAppliesTo = queryType->getAppliesTo();
    ObjectTypes scopeTypes = scope->getObjectTypes();
    
    // If the query type doesn't apply to any of our scope types, hide it
    if ((static_cast<int>(queryAppliesTo) & static_cast<int>(scopeTypes)) == 0 &&
        queryAppliesTo != ObjectTypes::None)
        return false;
    
    return true;
}

void QueryElement::setupControls()
{
    if (!this->currentQueryType_)
        return;
    
    // Update remove button visibility
    bool isSubQuery = (this->parentQueryElement_ != nullptr);
    bool isDummy = dynamic_cast<DummyQueryType*>(this->currentQueryType_) != nullptr;
    this->removeButton_->setVisible(!isDummy && isSubQuery);
    
    // Update control visibility based on query type
    this->matchTypeCombo_->setVisible(this->currentQueryType_->showMatchTypeComboButton());
    this->textBox_->setVisible(this->currentQueryType_->showTextBox(this));
    this->comboBox_->setVisible(this->currentQueryType_->showComboButton(this));
    
    // Show numeric updown OR double spinbox (not both)
    bool showNumeric = this->currentQueryType_->showNumericUpDown(this);
    this->numericUpDown_->setVisible(showNumeric);
    this->doubleSpinBox_->setVisible(false);  // For now, always use integer spin box
    this->unitsLabel_->setVisible(showNumeric);
    
    this->dateTimePicker_->setVisible(this->currentQueryType_->showDateTimePicker(this));
    this->resourceSelectButton_->setVisible(this->currentQueryType_->showResourceSelectButton(this));
    
    // Populate resource button if needed
    if (this->currentQueryType_->showResourceSelectButton(this))
    {
        Search* search = this->getSearchForResourceSelectButton();
        if (search)
            this->resourceSelectButton_->Populate(search);
    }
    
    // Populate match type combo
    if (this->currentQueryType_->showMatchTypeComboButton())
    {
        this->matchTypeCombo_->clear();
        QStringList entries = this->currentQueryType_->getMatchTypeComboButtonEntries();
        for (const QString& entry : entries)
        {
            this->matchTypeCombo_->addItem(entry);
        }
    }
    
    // Populate value combo
    if (this->currentQueryType_->showComboButton(this))
    {
        this->comboBox_->clear();
        QVariantList entries = this->currentQueryType_->getComboButtonEntries(this);
        for (const QVariant& entry : entries)
        {
            this->comboBox_->addItem(entry.toString());
        }
    }
    
    // Set units label
    QString units = this->currentQueryType_->getUnits(this);
    if (!units.isEmpty())
        this->unitsLabel_->setText(units);
    
    // Handle sub-queries for group queries
    if (this->currentQueryType_->getCategory() == QueryType::Category::Group)
    {
        if (this->subQueryElements_.isEmpty())
        {
            // Add default sub-query element
            QueryElement* subElement = new QueryElement(this->searcher_, this->queryScope_, this, this);
            this->addSubQueryElement(subElement);
        }
    } else
    {
        this->clearSubQueryElements();
    }
    
    this->refreshSubQueryElements();
    emit QueryChanged();
}

void QueryElement::refreshSubQueryElements()
{
    for (QueryElement* subElement : this->subQueryElements_)
    {
        if (this->subQueryLayout_->indexOf(subElement) == -1)
            this->subQueryLayout_->addWidget(subElement);
    }
}

void QueryElement::clearSubQueryElements()
{
    for (QueryElement* element : this->subQueryElements_)
    {
        this->subQueryLayout_->removeWidget(element);
        element->deleteLater();
    }
    this->subQueryElements_.clear();
}

void QueryElement::addSubQueryElement(QueryElement* element)
{
    this->subQueryElements_.append(element);
    this->subQueryLayout_->addWidget(element);
    connect(element, &QueryElement::QueryChanged, this, &QueryElement::onSubQueryChanged);
}

void QueryElement::removeSubQueryElement(QueryElement* element)
{
    this->subQueryElements_.removeOne(element);
    this->subQueryLayout_->removeWidget(element);
    element->deleteLater();
    emit QueryChanged();
}

QueryFilter* QueryElement::GetQueryFilter() const
{
    if (!this->currentQueryType_)
        return nullptr;
    
    return this->currentQueryType_->GetQuery(const_cast<QueryElement*>(this));
}

void QueryElement::SetQueryFilter(QueryFilter* filter)
{
    if (!filter)
    {
        this->SelectDefaultQueryType();
        return;
    }
    
    QueryType* queryType = QueryTypeRegistry::instance()->findQueryTypeForFilter(filter);
    if (queryType)
    {
        this->currentQueryType_ = queryType;
        
        // Find and select this query type in combo
        for (int i = 0; i < this->queryTypeCombo_->count(); ++i)
        {
            QueryType* comboType = this->queryTypeCombo_->itemData(i).value<QueryType*>();
            if (comboType == queryType)
            {
                this->queryTypeCombo_->setCurrentIndex(i);
                break;
            }
        }
        
        // Let query type populate UI from filter
        queryType->FromQuery(filter, this);
        this->setupControls();
    }
}

// UI state access methods

QString QueryElement::getMatchTypeSelection() const
{
    return this->matchTypeCombo_->currentText();
}

void QueryElement::setMatchTypeSelection(const QString& value)
{
    int index = this->matchTypeCombo_->findText(value);
    if (index >= 0)
        this->matchTypeCombo_->setCurrentIndex(index);
}

QString QueryElement::getTextBoxValue() const
{
    return this->textBox_->text();
}

void QueryElement::setTextBoxValue(const QString& value)
{
    this->textBox_->setText(value);
}

QString QueryElement::getComboBoxSelection() const
{
    return this->comboBox_->currentText();
}

void QueryElement::setComboBoxSelection(const QString& value)
{
    int index = this->comboBox_->findText(value);
    if (index >= 0)
        this->comboBox_->setCurrentIndex(index);
}

qint64 QueryElement::getNumericValue() const
{
    return this->numericUpDown_->value();
}

void QueryElement::setNumericValue(qint64 value)
{
    this->numericUpDown_->setValue(static_cast<int>(value));
}

double QueryElement::getDoubleValue() const
{
    return this->doubleSpinBox_->value();
}

void QueryElement::setDoubleValue(double value)
{
    this->doubleSpinBox_->setValue(value);
}

QDateTime QueryElement::getDateTimeValue() const
{
    return this->dateTimePicker_->dateTime();
}

void QueryElement::setDateTimeValue(const QDateTime& value)
{
    this->dateTimePicker_->setDateTime(value);
}

QString QueryElement::getResourceSelection() const
{
    return this->resourceSelectButton_->selectedRef();
}

void QueryElement::setResourceSelection(const QString& ref)
{
    this->resourceSelectButton_->setSelectedRef(ref);
}

Search* QueryElement::getSearchForResourceSelectButton()
{
    // C# QueryElement.cs lines 467-511: GetSearchForResourceSelectButton()
    // Creates Search with appropriate grouping based on ObjectTypes
    
    if (!this->queryScope_)
        return nullptr;
    
    // Create query with scope and no filter
    Query* query = new Query(this->queryScope_, nullptr);
    
    // Create sort by name (ascending)
    Sort nameSort("name", true);
    QList<Sort> sorts;
    sorts.append(nameSort);
    
    // Determine grouping based on object types
    Grouping* grouping = nullptr;
    ObjectTypes types = this->queryScope_->GetObjectTypes();
    
    // Same list of recursive types as defined in static QueryElement (C# line 481)
    if (types == ObjectTypes::Pool)
    {
        grouping = nullptr;  // No grouping for pools
    }
    else if (types == ObjectTypes::Server || types == ObjectTypes::Appliance)
    {
        // Host -> Pool
        grouping = new PoolGrouping(nullptr);
    }
    else if (types == (ObjectTypes::VM | ObjectTypes::Network | (ObjectTypes::LocalSR | ObjectTypes::RemoteSR)))
    {
        // VM/Network/SR -> Host -> Pool
        Grouping* serverGrouping = new HostGrouping(nullptr);
        grouping = new PoolGrouping(serverGrouping);
    }
    else if (types == ObjectTypes::VDI)
    {
        // VDI -> SR -> Host -> Pool
        // TODO: Add SRGrouping when implemented
        Grouping* serverGrouping = new HostGrouping(nullptr);
        grouping = new PoolGrouping(serverGrouping);
    }
    else if (types == ObjectTypes::Folder)
    {
        // TODO: Add FolderGrouping when implemented
        grouping = nullptr;
    }
    else
    {
        // Default grouping for other types
        Grouping* serverGrouping = new HostGrouping(nullptr);
        grouping = new PoolGrouping(serverGrouping);
    }
    
    // Create search with query, grouping, and sorts
    return new Search(query, grouping, "", "", false, QList<QPair<QString, int>>(), sorts);
}

QList<QueryFilter*> QueryElement::getSubQueries() const
{
    QList<QueryFilter*> queries;
    for (QueryElement* element : this->subQueryElements_)
    {
        QueryFilter* filter = element->GetQueryFilter();
        if (filter)
            queries.append(filter);
    }
    return queries;
}

void QueryElement::setSubQueries(const QList<QueryFilter*>& queries)
{
    this->clearSubQueryElements();
    
    for (QueryFilter* query : queries)
    {
        QueryElement* subElement = new QueryElement(this->searcher_, this->queryScope_, this, this);
        subElement->SetQueryFilter(query);
        this->addSubQueryElement(subElement);
    }
}

// Slot implementations

void QueryElement::onQueryTypeChanged(int index)
{
    QVariant data = this->queryTypeCombo_->itemData(index);
    if (!data.isValid())
        return;
    
    QueryType* newQueryType = data.value<QueryType*>();
    if (!newQueryType || newQueryType == this->currentQueryType_)
        return;
    
    // Disconnect old query type signals
    if (this->currentQueryType_)
    {
        disconnect(this->currentQueryType_, &QueryType::SomeThingChanged,
                   this, &QueryElement::onSomeThingChanged);
    }
    
    // Save last query filter
    delete this->lastQueryFilter_;
    this->lastQueryFilter_ = this->GetQueryFilter();
    
    this->currentQueryType_ = newQueryType;
    
    // Connect new query type signals
    if (this->currentQueryType_)
    {
        connect(this->currentQueryType_, &QueryType::SomeThingChanged,
                this, &QueryElement::onSomeThingChanged);
    }
    
    this->setupControls();
}

void QueryElement::onMatchTypeChanged(int index)
{
    Q_UNUSED(index);
    
    // Match type change may affect other control visibility (e.g., date picker)
    if (this->currentQueryType_)
    {
        this->dateTimePicker_->setVisible(this->currentQueryType_->showDateTimePicker(this));
    }
    
    emit QueryChanged();
}

void QueryElement::onTextChanged()
{
    emit QueryChanged();
}

void QueryElement::onComboChanged(int index)
{
    Q_UNUSED(index);
    emit QueryChanged();
}

void QueryElement::onNumericChanged(int value)
{
    Q_UNUSED(value);
    emit QueryChanged();
}

void QueryElement::onDateTimeChanged(const QDateTime& dateTime)
{
    Q_UNUSED(dateTime);
    emit QueryChanged();
}

void QueryElement::onDoubleChanged(double value)
{
    Q_UNUSED(value);
    emit QueryChanged();
}

void QueryElement::onResourceSelected(const QString& ref)
{
    Q_UNUSED(ref);
    emit QueryChanged();
}

void QueryElement::onRemoveClicked()
{
    // Notify parent to remove this element
    if (this->parentQueryElement_)
    {
        this->parentQueryElement_->removeSubQueryElement(this);
    }
}

void QueryElement::onSubQueryChanged()
{
    emit QueryChanged();
}

void QueryElement::onSomeThingChanged()
{
    // C# equivalent: QueryElement_SomeThingChanged handler
    // Refreshes combo box values when QueryType cache updates
    
    if (!this->currentQueryType_)
        return;
    
    // Save current selection
    QString currentSelection = this->comboBox_->currentText();
    
    // Repopulate combo box
    this->comboBox_->clear();
    QVariantList entries = this->currentQueryType_->getComboButtonEntries(this);
    for (const QVariant& entry : entries)
    {
        this->comboBox_->addItem(entry.toString());
    }
    
    // Restore selection if still valid
    int index = this->comboBox_->findText(currentSelection);
    if (index >= 0)
    {
        this->comboBox_->setCurrentIndex(index);
    } else if (this->comboBox_->count() > 0)
    {
        this->comboBox_->setCurrentIndex(0); // Select first if previous value gone
    }
}
