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
#include "xenlib/xensearch/queries.h"
#include "xenlib/xensearch/queryscope.h"
#include "xenlib/xensearch/search.h"
#include "xenlib/xensearch/sort.h"
#include "xenlib/xensearch/grouping.h"
#include "xenlib/customfields/customfieldsmanager.h"
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
#include <climits>
#include <memory>

using namespace XenSearch;

namespace
{
    const char* const COMBO_TEXT = "__text";
    const char* const COMBO_VALUE = "__value";

    QString comboEntryText(const QVariant& entry)
    {
        const QVariantMap map = entry.toMap();
        if (map.contains(COMBO_TEXT))
            return map.value(COMBO_TEXT).toString();
        return entry.toString();
    }

    QVariant comboEntryValue(const QVariant& entry)
    {
        const QVariantMap map = entry.toMap();
        if (map.contains(COMBO_TEXT))
            return map.value(COMBO_VALUE);
        return entry;
    }
}

QVariant QueryElement::ComboEntry(const QString& text, const QVariant& value)
{
    return QVariantMap{{COMBO_TEXT, text}, {COMBO_VALUE, value}};
}

QueryElement::QueryElement(QWidget* parent) : QueryElement(nullptr, nullptr, nullptr, parent)
{
}

QueryElement::QueryElement(Searcher* searcher, QWidget* parent) : QueryElement(searcher, nullptr, nullptr, parent)
{
}

QueryElement::QueryElement(Searcher* searcher, QueryScope* queryScope,
                           QueryElement* parentQueryElement, QWidget* parent)
    : QWidget(parent),
      queryScope_(queryScope),
      parentQueryElement_(parentQueryElement)
{
    this->setupUi();
    this->SetSearcher(searcher);
    this->SelectDefaultQueryType();

    // The registry updates its custom field types first (it connected earlier), then each row
    // refreshes its list
    connect(CustomFieldsManager::instance(), &CustomFieldsManager::customFieldsChanged,
            this, &QueryElement::populateQueryTypeCombo);
}

QueryElement::~QueryElement()
{
    delete this->lastQueryFilter_;
}

void QueryElement::setupUi()
{
    QHBoxLayout* mainLayout = new QHBoxLayout();
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(5);

    this->queryTypeCombo_ = new QComboBox(this);
    this->queryTypeCombo_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    mainLayout->addWidget(this->queryTypeCombo_);

    // The sub-row of a parent/child query continues this row (C# indents it to the match type)
    this->parentChildLayout_ = new QHBoxLayout();
    this->parentChildLayout_->setContentsMargins(0, 0, 0, 0);
    mainLayout->addLayout(this->parentChildLayout_, 1);

    this->matchTypeCombo_ = new QComboBox(this);
    this->matchTypeCombo_->setVisible(false);
    mainLayout->addWidget(this->matchTypeCombo_);

    this->textBox_ = new QLineEdit(this);
    this->textBox_->setVisible(false);
    mainLayout->addWidget(this->textBox_);

    this->comboBox_ = new QComboBox(this);
    this->comboBox_->setVisible(false);
    mainLayout->addWidget(this->comboBox_);

    this->numericUpDown_ = new QSpinBox(this);
    this->numericUpDown_->setRange(0, INT_MAX);
    this->numericUpDown_->setVisible(false);
    mainLayout->addWidget(this->numericUpDown_);

    this->doubleSpinBox_ = new QDoubleSpinBox(this);
    this->doubleSpinBox_->setRange(0.0, 999999.99);
    this->doubleSpinBox_->setDecimals(2);
    this->doubleSpinBox_->setVisible(false);
    mainLayout->addWidget(this->doubleSpinBox_);

    this->unitsLabel_ = new QLabel(this);
    this->unitsLabel_->setVisible(false);
    mainLayout->addWidget(this->unitsLabel_);

    this->dateTimePicker_ = new QDateTimeEdit(QDateTime::currentDateTime(), this);
    this->dateTimePicker_->setCalendarPopup(true);
    this->dateTimePicker_->setVisible(false);
    mainLayout->addWidget(this->dateTimePicker_);

    this->resourceSelectButton_ = new ResourceSelectButton(this);
    this->resourceSelectButton_->setVisible(false);
    mainLayout->addWidget(this->resourceSelectButton_);

    mainLayout->addStretch();

    this->removeButton_ = new QPushButton(QStringLiteral("−"), this);
    this->removeButton_->setObjectName("removeButton");
    this->removeButton_->setMaximumWidth(30);
    this->removeButton_->setToolTip(tr("Remove this filter"));
    this->removeButton_->setVisible(false);
    mainLayout->addWidget(this->removeButton_);

    // Sub-rows of a group query, indented below this one
    this->subQueryLayout_ = new QVBoxLayout();
    this->subQueryLayout_->setContentsMargins(30, 0, 0, 0);
    this->subQueryLayout_->setSpacing(2);

    QVBoxLayout* verticalLayout = new QVBoxLayout(this);
    verticalLayout->setContentsMargins(0, 0, 0, 0);
    verticalLayout->setSpacing(2);
    verticalLayout->addLayout(mainLayout);
    verticalLayout->addLayout(this->subQueryLayout_);

    connect(this->queryTypeCombo_, QOverload<int>::of(&QComboBox::activated),
            this, &QueryElement::onQueryTypeActivated);
    connect(this->matchTypeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QueryElement::onMatchTypeChanged);
    connect(this->textBox_, &QLineEdit::textChanged, this, &QueryElement::emitQueryChanged);
    connect(this->comboBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QueryElement::emitQueryChanged);
    connect(this->numericUpDown_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &QueryElement::emitQueryChanged);
    connect(this->doubleSpinBox_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &QueryElement::emitQueryChanged);
    connect(this->dateTimePicker_, &QDateTimeEdit::dateTimeChanged, this, &QueryElement::emitQueryChanged);
    connect(this->resourceSelectButton_, &ResourceSelectButton::itemSelected, this, &QueryElement::emitQueryChanged);
    // C# repopulates the picker before it pops up, so it reflects the current objects
    if (QMenu* menu = this->resourceSelectButton_->GetMenu())
        connect(menu, &QMenu::aboutToShow, this, &QueryElement::populateResourceSelectButton);
    connect(this->removeButton_, &QPushButton::clicked, this, &QueryElement::onRemoveClicked);
}

void QueryElement::SetSearcher(Searcher* searcher)
{
    if (this->searcher_ == searcher)
        return;
    if (this->searcher_)
        disconnect(this->searcher_, &Searcher::SearchForChanged, this, &QueryElement::onSearchForChanged);
    this->searcher_ = searcher;
    if (this->searcher_)
        connect(this->searcher_, &Searcher::SearchForChanged, this, &QueryElement::onSearchForChanged);
}

void QueryElement::SelectDefaultQueryType()
{
    this->setCurrentQueryType(QueryTypeRegistry::instance()->getDefaultQueryType());
}

void QueryElement::setCurrentQueryType(QueryType* queryType)
{
    if (this->currentQueryType_)
        disconnect(this->currentQueryType_, &QueryType::SomeThingChanged, this, &QueryElement::onSomeThingChanged);

    if (!queryType)
        return;

    this->currentQueryType_ = queryType;
    this->populateQueryTypeCombo();
    connect(queryType, &QueryType::SomeThingChanged, this, &QueryElement::onSomeThingChanged, Qt::UniqueConnection);
    this->setup();
}

// C# PopulateGroupTypesComboButton: the types relevant here, grouped with separators. The
// current type stays listed even when not relevant (for example "Select a filter...").
void QueryElement::populateQueryTypeCombo()
{
    const QSignalBlocker blocker(this->queryTypeCombo_);
    this->queryTypeCombo_->clear();

    int lastGroup = -1;
    for (QueryType* queryType : QueryTypeRegistry::instance()->getAllQueryTypes())
    {
        if (queryType != this->currentQueryType_ && !this->wantQueryType(queryType))
            continue;

        if (lastGroup != -1 && queryType->getGroup() != lastGroup)
            this->queryTypeCombo_->insertSeparator(this->queryTypeCombo_->count());
        lastGroup = queryType->getGroup();

        this->queryTypeCombo_->addItem(queryType->toString(), QVariant::fromValue(queryType));
        if (queryType == this->currentQueryType_)
            this->queryTypeCombo_->setCurrentIndex(this->queryTypeCombo_->count() - 1);
    }
}

// C# WantQueryType: is this type relevant to what is searched for?
bool QueryElement::wantQueryType(QueryType* queryType) const
{
    QueryScope* scope = nullptr;
    std::unique_ptr<QueryScope> searcherScope; // Searcher::GetQueryScope returns a new object
    bool recursive = false; // the sub-row of a parent/child query

    if (this->queryScope_)
    {
        scope = this->queryScope_;
        recursive = true;
    } else if (this->searcher_)
    {
        searcherScope.reset(this->searcher_->GetQueryScope());
        scope = searcherScope.get();
    }

    if (!scope)
        return true;

    // The sub-row of a parent/child query cannot have children itself
    if (recursive && queryType->getCategory() != QueryType::Category::Single)
        return false;

    // Picking one object only makes sense as the sub-row of a parent/child query
    if (!recursive && dynamic_cast<UuidQueryType*>(queryType))
        return false;

    // A parent/child query needs the search to include something besides the related
    // objects (searching for servers with a constraint on servers is pointless); folders are
    // the exception, as the constraint is then on the parent folder
    QueryScope* subScope = queryType->getSubQueryScope();
    if (scope->wantSubsetOf(subScope) && subScope->getObjectTypes() != ObjectTypes::Folder)
        return false;

    return scope->wantSubsetOf(queryType->getAppliesTo());
}

void QueryElement::onQueryTypeActivated(int index)
{
    QueryType* queryType = this->queryTypeCombo_->itemData(index).value<QueryType*>();
    if (!queryType || queryType == this->currentQueryType_)
        return;

    // C# queryTypeComboButton_BeforeItemSelected: remember the old query, so a new group can
    // keep it as its first child
    delete this->lastQueryFilter_;
    this->lastQueryFilter_ = this->GetQueryFilter();

    if (this->currentQueryType_)
        disconnect(this->currentQueryType_, &QueryType::SomeThingChanged, this, &QueryElement::onSomeThingChanged);
    this->currentQueryType_ = queryType;
    connect(queryType, &QueryType::SomeThingChanged, this, &QueryElement::onSomeThingChanged, Qt::UniqueConnection);

    this->setup(true);
}

void QueryElement::setup(bool queryTypeDropDownChanged)
{
    if (this->populating_ || !this->currentQueryType_)
        return;

    QueryType* queryType = this->currentQueryType_;

    // No remove button for "Select a filter...", nor for the sub-row of a parent/child query
    // (the parent row already has one on the same line)
    const bool parentIsParentChild = this->isSubQueryElement() && this->parentQueryElement_->currentQueryType_ &&
        this->parentQueryElement_->currentQueryType_->getCategory() == QueryType::Category::ParentChild;
    this->removeButton_->setVisible(!dynamic_cast<DummyQueryType*>(queryType) && !parentIsParentChild);

    this->matchTypeCombo_->setVisible(queryType->showMatchTypeComboButton());
    if (queryType->showMatchTypeComboButton())
    {
        QVariantList entries;
        for (const QString& entry : queryType->getMatchTypeComboButtonEntries())
            entries.append(entry);
        this->populateComboButton(this->matchTypeCombo_, entries);
    }

    const QueryType::Category category = queryType->getCategory();
    if (category == QueryType::Category::Group)
    {
        // Changing a condition into All/Any/None of keeps it as the first child (CA-25127)
        if (queryTypeDropDownChanged && !dynamic_cast<GroupQuery*>(this->lastQueryFilter_))
        {
            this->clearSubQueryElements();
            QueryElement* first = new QueryElement(this->searcher_, nullptr, this, this);
            first->SetQueryFilter(this->lastQueryFilter_);
            this->addSubQueryElement(first);
        }
        // Always offer a "Select a filter..." row to add another condition
        if (!this->hasDummySubQuery())
            this->addDummySubQuery(nullptr);
    } else if (category == QueryType::Category::ParentChild)
    {
        if (queryTypeDropDownChanged)
            this->clearSubQueryElements();
        // Exactly one sub-row, filtering the related objects
        if (this->subQueryElements_.isEmpty())
            this->addDummySubQuery(queryType->getSubQueryScope());
    } else
    {
        this->clearSubQueryElements();
    }

    this->refreshSubQueryElements();

    // A group gets a new "Select a filter..." row once its last one is used
    if (queryTypeDropDownChanged && dynamic_cast<DummyQuery*>(this->lastQueryFilter_) &&
        this->isSubQueryElement() && dynamic_cast<GroupQueryType*>(this->parentQueryElement_->currentQueryType_) &&
        this->parentQueryElement_->subQueryElements_.contains(this) &&
        !this->parentQueryElement_->hasDummySubQuery())
    {
        this->parentQueryElement_->addDummySubQuery(nullptr);
        this->parentQueryElement_->refreshSubQueryElements();
    }

    this->setup2();
}

void QueryElement::setup2()
{
    QueryType* queryType = this->currentQueryType_;
    if (!queryType)
        return;

    this->textBox_->setVisible(queryType->showTextBox(this));
    this->dateTimePicker_->setVisible(queryType->showDateTimePicker(this));

    const bool showNumeric = queryType->showNumericUpDown(this);
    this->numericUpDown_->setVisible(showNumeric);
    this->unitsLabel_->setVisible(showNumeric);
    this->unitsLabel_->setText(queryType->getUnits(this));

    const bool showCombo = queryType->showComboButton(this);
    this->comboBox_->setVisible(showCombo);
    if (showCombo)
        this->populateComboButton(this->comboBox_, queryType->getComboButtonEntries(this));

    const bool showResource = queryType->showResourceSelectButton(this);
    this->resourceSelectButton_->setVisible(showResource);
    if (showResource)
        this->populateResourceSelectButton();

    this->emitQueryChanged();
}

// C# PopulateComboButton: refill, keeping the selected value when it is still offered
void QueryElement::populateComboButton(QComboBox* combo, const QVariantList& values)
{
    this->populating_ = true;
    const QSignalBlocker blocker(combo);

    const QVariant selected = combo->currentData();
    combo->clear();

    int selectIndex = 0;
    for (const QVariant& entry : values)
    {
        QString text = comboEntryText(entry);
        if (text.length() > 100)
            text = text.left(99) + QChar(0x2026);
        const QVariant value = comboEntryValue(entry);
        combo->addItem(text, value);
        if (selected.isValid() && value == selected)
            selectIndex = combo->count() - 1;
    }
    if (combo->count() > 0)
        combo->setCurrentIndex(selectIndex);

    this->populating_ = false;
}

void QueryElement::populateResourceSelectButton()
{
    if (!this->resourceSelectButton_->isVisible() && !(this->currentQueryType_ &&
                                                       this->currentQueryType_->showResourceSelectButton(this)))
        return;

    Search* search = this->getSearchForResourceSelectButton();
    this->resourceSelectButton_->Populate(search);
    delete search;
}

Search* QueryElement::getSearchForResourceSelectButton() const
{
    // C# GetSearchForResourceSelectButton: the objects of this row's scope, grouped like the tree
    if (!this->queryScope_)
        return nullptr;

    Query* query = new Query(new QueryScope(this->queryScope_->getObjectTypes()), nullptr);
    QList<Sort> sorts;
    sorts.append(Sort("name", true));

    Grouping* grouping = nullptr;
    const ObjectTypes types = this->queryScope_->getObjectTypes();
    if (types == ObjectTypes::Pool)
        grouping = nullptr;
    else if (types == ObjectTypes::Server || types == ObjectTypes::Appliance)
        grouping = new PoolGrouping(nullptr);
    else if (types == ObjectTypes::VM || types == ObjectTypes::Network ||
             types == (ObjectTypes::LocalSR | ObjectTypes::RemoteSR) || types == ObjectTypes::VDI)
        grouping = new PoolGrouping(new HostGrouping(nullptr)); // C# also groups VDIs by SR first
    else if (types == ObjectTypes::Folder)
        grouping = new FolderGrouping(nullptr);

    return new Search(query, grouping, "", "", false, QList<QPair<QString, int>>(), sorts);
}

void QueryElement::refreshSubQueryElements()
{
    if (!this->currentQueryType_)
        return;

    const bool parentChild = this->currentQueryType_->getCategory() == QueryType::Category::ParentChild;
    for (QueryElement* subElement : this->subQueryElements_)
    {
        QLayout* wanted = parentChild ? static_cast<QLayout*>(this->parentChildLayout_)
                                      : static_cast<QLayout*>(this->subQueryLayout_);
        QLayout* other = parentChild ? static_cast<QLayout*>(this->subQueryLayout_)
                                     : static_cast<QLayout*>(this->parentChildLayout_);
        if (other->indexOf(subElement) != -1)
            other->removeWidget(subElement);
        if (wanted->indexOf(subElement) == -1)
            wanted->addWidget(subElement);
        subElement->setVisible(true);
        connect(subElement, &QueryElement::QueryChanged, this, &QueryElement::emitQueryChanged, Qt::UniqueConnection);
    }
}

bool QueryElement::hasDummySubQuery() const
{
    for (QueryElement* subElement : this->subQueryElements_)
    {
        if (dynamic_cast<DummyQueryType*>(subElement->currentQueryType_))
            return true;
    }
    return false;
}

void QueryElement::addDummySubQuery(QueryScope* queryScope)
{
    this->addSubQueryElement(new QueryElement(this->searcher_, queryScope, this, this));
}

void QueryElement::addSubQueryElement(QueryElement* element)
{
    this->subQueryElements_.append(element);
}

void QueryElement::removeSubQueryElement(QueryElement* element)
{
    if (!this->subQueryElements_.removeOne(element))
        return;
    this->subQueryLayout_->removeWidget(element);
    this->parentChildLayout_->removeWidget(element);
    disconnect(element, nullptr, this, nullptr);
    element->hide();
    element->deleteLater();
}

void QueryElement::clearSubQueryElements()
{
    const QList<QueryElement*> elements = this->subQueryElements_;
    for (QueryElement* element : elements)
        this->removeSubQueryElement(element);
}

QueryFilter* QueryElement::GetQueryFilter() const
{
    return this->currentQueryType_ ? this->currentQueryType_->GetQuery(const_cast<QueryElement*>(this)) : nullptr;
}

void QueryElement::SetQueryFilter(QueryFilter* filter)
{
    this->selectQueryTypeFor(filter);
}

// C# SelectQueryTypeFor
void QueryElement::selectQueryTypeFor(QueryFilter* query)
{
    if (!query)
    {
        this->SelectDefaultQueryType();
        return;
    }

    if (this->currentQueryType_ && this->currentQueryType_->ForQuery(query))
    {
        this->setup2();
        this->currentQueryType_->FromQuery(query, this);
        return;
    }

    // Prefer a type relevant here: a UUID query is "Is <object>" inside a parent/child query
    // but "UUID is exactly" at the top level
    QueryType* chosen = nullptr;
    const QList<QueryType*> queryTypes = QueryTypeRegistry::instance()->getAllQueryTypes();
    for (QueryType* queryType : queryTypes)
    {
        if (this->wantQueryType(queryType) && queryType->ForQuery(query))
        {
            chosen = queryType;
            break;
        }
    }
    if (!chosen)
    {
        for (QueryType* queryType : queryTypes)
        {
            if (queryType->ForQuery(query))
            {
                chosen = queryType;
                break;
            }
        }
    }

    if (!chosen)
    {
        this->SelectDefaultQueryType();
        return;
    }

    this->setCurrentQueryType(chosen);
    chosen->FromQuery(query, this);
}

void QueryElement::setGroupSubQueries(const QList<QueryFilter*>& queries)
{
    for (int i = 0; i < queries.size(); ++i)
    {
        if (i < this->subQueryElements_.size())
        {
            this->subQueryElements_.at(i)->selectQueryTypeFor(queries.at(i));
        } else
        {
            QueryElement* element = new QueryElement(this->searcher_, nullptr, this, this);
            element->SetQueryFilter(queries.at(i));
            this->addSubQueryElement(element);
        }
    }

    while (this->subQueryElements_.size() > queries.size())
        this->removeSubQueryElement(this->subQueryElements_.last());

    this->setup();
}

void QueryElement::setParentChildSubQuery(QueryFilter* query, QueryScope* scope)
{
    this->clearSubQueryElements();
    QueryElement* element = new QueryElement(this->searcher_, scope, this, this);
    element->SetQueryFilter(query);
    this->addSubQueryElement(element);
    this->setup();
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

// C# RemoveUnwantedFilters: after "Search for" changes, drop conditions that no longer apply
void QueryElement::removeUnwantedFilters()
{
    QueryType* queryType = this->currentQueryType_;
    if (!queryType)
        return;

    bool wanted = this->wantQueryType(queryType);
    if (wanted && queryType->getCategory() == QueryType::Category::Group)
    {
        const QList<QueryElement*> elements = this->subQueryElements_;
        for (QueryElement* element : elements)
            element->removeUnwantedFilters();

        if (this->subQueryElements_.isEmpty())
            wanted = false;

        // All of / Any of with a single child left: promote the child
        GroupQueryType* groupType = dynamic_cast<GroupQueryType*>(queryType);
        if (this->subQueryElements_.size() == 1 && groupType &&
            (groupType->getType() == GroupQueryType::GroupType::And ||
             groupType->getType() == GroupQueryType::GroupType::Or))
        {
            QueryFilter* child = this->subQueryElements_.first()->GetQueryFilter();
            this->SetQueryFilter(child);
            delete child;
            this->refreshSubQueryElements();
        }
    }

    if (!wanted)
    {
        if (this->isSubQueryElement())
        {
            QueryElement* parent = this->parentQueryElement_;
            parent->removeSubQueryElement(this);
            parent->refreshSubQueryElements();
        } else
        {
            this->SelectDefaultQueryType();
        }
    }
}

void QueryElement::onSearchForChanged()
{
    // Sub-rows are pruned by their parents (removeUnwantedFilters is recursive)
    if (this->isSubQueryElement())
        return;

    this->removeUnwantedFilters();
    this->setup();
    this->repopulateAllQueryTypeCombos();
    this->emitQueryChanged();
}

void QueryElement::repopulateAllQueryTypeCombos()
{
    this->populateQueryTypeCombo();
    for (QueryElement* element : this->subQueryElements_)
        element->repopulateAllQueryTypeCombos();
}

void QueryElement::onMatchTypeChanged(int index)
{
    Q_UNUSED(index);
    if (this->populating_)
        return;
    this->setup2();
}

void QueryElement::onSomeThingChanged()
{
    // C# queryType_SomeThingChanged: the offered values changed (new objects, tags, ...)
    this->setup();
}

void QueryElement::onRemoveClicked()
{
    if (this->isSubQueryElement())
    {
        QueryElement* parent = this->parentQueryElement_;
        parent->removeSubQueryElement(this);
        parent->refreshSubQueryElements();
        parent->emitQueryChanged();
    } else
    {
        this->SelectDefaultQueryType();
        this->emitQueryChanged();
    }
}

void QueryElement::emitQueryChanged()
{
    if (this->populating_)
        return;
    emit this->QueryChanged();
}

// Control access for the query types

QString QueryElement::getMatchTypeSelection() const
{
    return this->matchTypeCombo_->currentText();
}

void QueryElement::setMatchTypeSelection(const QString& value)
{
    const int index = this->matchTypeCombo_->findText(value);
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
    const int index = this->comboBox_->findText(value);
    if (index >= 0)
        this->comboBox_->setCurrentIndex(index);
}

QVariant QueryElement::getComboBoxValue() const
{
    return this->comboBox_->currentData();
}

void QueryElement::setComboBoxValue(const QVariant& value)
{
    const int index = this->comboBox_->findData(value);
    if (index >= 0)
        this->comboBox_->setCurrentIndex(index);
}

qint64 QueryElement::getNumericValue() const
{
    return this->numericUpDown_->value();
}

void QueryElement::setNumericValue(qint64 value)
{
    this->numericUpDown_->setValue(static_cast<int>(qBound(qint64(0), value, qint64(INT_MAX))));
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

QString QueryElement::getResourceSelectionUuid() const
{
    return this->resourceSelectButton_->selectedUuid();
}

void QueryElement::setResourceSelection(const QString& ref)
{
    this->resourceSelectButton_->setSelectedRef(ref);
}

void QueryElement::setResourceSelectionUuid(const QString& uuid)
{
    this->resourceSelectButton_->setSelectedUuid(uuid);
}
