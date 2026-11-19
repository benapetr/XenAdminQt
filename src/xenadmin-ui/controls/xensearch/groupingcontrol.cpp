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

#include "groupingcontrol.h"
#include "searcher.h"
#include "xenlib/customfields/customfieldsmanager.h"
#include "xenlib/xensearch/queryscope.h"
#include <QHBoxLayout>
#include <QMenu>
#include <QMouseEvent>
#include <memory>
#include <typeinfo>

using namespace XenSearch;

// ============================================================================
// Grouping types (C# GroupingControl nested classes)
// ============================================================================

class GroupingControl::FolderGroupingType : public GroupingControl::GroupingType
{
    public:
        FolderGroupingType() : GroupingType(ObjectTypes::AllIncFolders) {}
        QString ToString() const override { return QObject::tr("Folder"); }
        Grouping* GetGroup(Grouping* subgrouping) const override { return new FolderGrouping(subgrouping); }
        bool ForGrouping(const Grouping* grouping) const override { return dynamic_cast<const FolderGrouping*>(grouping); }
};

class GroupingControl::PropertyGroupingType : public GroupingControl::GroupingType
{
    public:
        PropertyGroupingType(ObjectTypes appliesTo, PropertyNames property)
            : GroupingType(appliesTo), property_(property) {}

        QString ToString() const override { return PropertyAccessors::GetPropertyDisplayName(this->property_); }

        Grouping* GetGroup(Grouping* subgrouping) const override
        {
            // The port's dedicated groupings for type and tags show the tree the same way
            if (this->property_ == PropertyNames::type)
                return new TypeGrouping(subgrouping);
            if (this->property_ == PropertyNames::tags)
                return new TagsGrouping(subgrouping);
            return new PropertyGrouping(this->property_, subgrouping);
        }

        bool ForGrouping(const Grouping* grouping) const override
        {
            if (this->property_ == PropertyNames::type)
                return dynamic_cast<const TypeGrouping*>(grouping);
            if (this->property_ == PropertyNames::tags)
                return dynamic_cast<const TagsGrouping*>(grouping);
            const PropertyGrouping* propertyGrouping = dynamic_cast<const PropertyGrouping*>(grouping);
            return propertyGrouping && typeid(*propertyGrouping) == typeid(PropertyGrouping) &&
                   propertyGrouping->property() == this->property_;
        }

    protected:
        PropertyNames property_;
};

class GroupingControl::BoolGroupingType : public GroupingControl::PropertyGroupingType
{
    public:
        BoolGroupingType(ObjectTypes appliesTo, PropertyNames property) : PropertyGroupingType(appliesTo, property) {}

        Grouping* GetGroup(Grouping* subgrouping) const override { return new BoolGrouping(this->property_, subgrouping); }

        bool ForGrouping(const Grouping* grouping) const override
        {
            const BoolGrouping* boolGrouping = dynamic_cast<const BoolGrouping*>(grouping);
            return boolGrouping && boolGrouping->property() == this->property_;
        }
};

class GroupingControl::XenModelObjectPropertyGroupingType : public GroupingControl::PropertyGroupingType
{
    public:
        XenModelObjectPropertyGroupingType(ObjectTypes appliesTo, PropertyNames property, const GroupingType* parent)
            : PropertyGroupingType(appliesTo, property), parent_(parent) {}

        Grouping* GetGroup(Grouping* subgrouping) const override
        {
            if (this->property_ == PropertyNames::pool)
                return new PoolGrouping(subgrouping);
            if (this->property_ == PropertyNames::host)
                return new HostGrouping(subgrouping);
            if (this->property_ == PropertyNames::appliance)
                return new VAppGrouping(subgrouping);
            return new XenModelObjectPropertyGrouping(this->property_, subgrouping);
        }

        bool ForGrouping(const Grouping* grouping) const override
        {
            if (this->property_ == PropertyNames::pool)
                return dynamic_cast<const PoolGrouping*>(grouping);
            if (this->property_ == PropertyNames::host)
                return dynamic_cast<const HostGrouping*>(grouping);
            if (this->property_ == PropertyNames::appliance)
                return dynamic_cast<const VAppGrouping*>(grouping);
            const XenModelObjectPropertyGrouping* xmo = dynamic_cast<const XenModelObjectPropertyGrouping*>(grouping);
            return xmo && xmo->property() == this->property_;
        }

        // The grouping type next up in the tree (pool above server above vApp ...)
        bool IsDescendantOf(const GroupingType* gt) const override
        {
            return gt == this->parent_ || (this->parent_ && this->parent_->IsDescendantOf(gt));
        }

    private:
        const GroupingType* parent_;
};

class GroupingControl::CustomFieldGroupingType : public GroupingControl::GroupingType
{
    public:
        explicit CustomFieldGroupingType(const CustomFieldDefinition& definition)
            : GroupingType(ObjectTypes::AllExcFolders), definition_(definition) {}

        QString ToString() const override { return this->definition_.GetName(); }
        Grouping* GetGroup(Grouping* subgrouping) const override { return new CustomFieldGrouping(this->definition_, subgrouping); }
        bool ForGrouping(const Grouping* grouping) const override
        {
            const CustomFieldGrouping* customField = dynamic_cast<const CustomFieldGrouping*>(grouping);
            return customField && customField->definition() == this->definition_;
        }
        const CustomFieldDefinition& definition() const { return this->definition_; }

    private:
        CustomFieldDefinition definition_;
};

class GroupingControl::AllCustomFieldsGroupingType : public GroupingControl::GroupingType
{
    public:
        AllCustomFieldsGroupingType() : GroupingType(ObjectTypes::AllExcFolders) {}
        QString ToString() const override { return QObject::tr("Custom Fields"); }
        Grouping* GetGroup(Grouping* subgrouping) const override { return new AllCustomFieldsGrouping(subgrouping); }
        bool ForGrouping(const Grouping* grouping) const override { return dynamic_cast<const AllCustomFieldsGrouping*>(grouping); }
};

// ============================================================================
// GroupingControl
// ============================================================================

GroupingControl::GroupingControl(QWidget* parent) : QWidget(parent)
{
    this->layout_ = new QHBoxLayout(this);
    this->layout_->setContentsMargins(0, 3, 0, 3);
    this->layout_->setSpacing(INNER_GUTTER);

    // C# GroupingControl static constructor
    const ObjectTypes allButPool = ObjectTypes::AllExcFolders & ~ObjectTypes::Pool;
    auto* poolGroup = new XenModelObjectPropertyGroupingType(allButPool, PropertyNames::pool, nullptr);
    auto* applianceGroup = new XenModelObjectPropertyGroupingType(ObjectTypes::VM, PropertyNames::appliance, poolGroup);
    auto* hostGroup = new XenModelObjectPropertyGroupingType(allButPool & ~ObjectTypes::Server, PropertyNames::host, applianceGroup);
    auto* srGroup = new XenModelObjectPropertyGroupingType(ObjectTypes::VM | ObjectTypes::VDI, PropertyNames::storage, poolGroup);

    this->potentialGroups_ << poolGroup
                           << hostGroup
                           << new PropertyGroupingType(ObjectTypes::VM, PropertyNames::os_name)
                           << new PropertyGroupingType(ObjectTypes::VM, PropertyNames::power_state)
                           << new PropertyGroupingType(ObjectTypes::VM, PropertyNames::virtualisation_status)
                           << new PropertyGroupingType(ObjectTypes::AllExcFolders, PropertyNames::type)
                           << new XenModelObjectPropertyGroupingType(ObjectTypes::VM, PropertyNames::networks, poolGroup)
                           << srGroup
                           << new XenModelObjectPropertyGroupingType(ObjectTypes::VM, PropertyNames::disks, srGroup)
                           << new PropertyGroupingType(ObjectTypes::VM, PropertyNames::ha_restart_priority)
                           << new BoolGroupingType(ObjectTypes::VM, PropertyNames::read_caching_enabled)
                           << new BoolGroupingType(ObjectTypes::VM, PropertyNames::vendor_device_state)
                           << applianceGroup
                           << new PropertyGroupingType(ObjectTypes::AllExcFolders, PropertyNames::tags)
                           << new XenModelObjectPropertyGroupingType(allButPool & ~ObjectTypes::Server & ~ObjectTypes::VM,
                                                                     PropertyNames::vm, hostGroup)
                           << new AllCustomFieldsGroupingType();
    this->folderGroupingType_ = new FolderGroupingType();

    this->addGroupButton_ = new DropDownButton(this);
    this->addGroupButton_->setText(tr("Add Group"));
    this->addGroupButton_->SetMenu(new QMenu(this));
    connect(this->addGroupButton_->GetMenu(), &QMenu::aboutToShow, this, &GroupingControl::populateAddGroupMenu);
    this->layout_->addWidget(this->addGroupButton_);
    this->layout_->addStretch();

    connect(CustomFieldsManager::instance(), &CustomFieldsManager::customFieldsChanged,
            this, &GroupingControl::onCustomFieldsChanged);
    this->onCustomFieldsChanged();

    // C#: start grouped by pool
    this->addGroup(this->potentialGroups_.first());
}

GroupingControl::~GroupingControl()
{
    qDeleteAll(this->potentialGroups_);
    qDeleteAll(this->customFieldTypes_);
    delete this->folderGroupingType_;
}

void GroupingControl::SetSearcher(Searcher* searcher)
{
    if (this->searcher_)
        disconnect(this->searcher_, &Searcher::SearchForChanged, this, &GroupingControl::onSearchForChanged);
    this->searcher_ = searcher;
    if (this->searcher_)
        connect(this->searcher_, &Searcher::SearchForChanged, this, &GroupingControl::onSearchForChanged);
}

QList<const GroupingControl::GroupingType*> GroupingControl::allGroupTypes() const
{
    QList<const GroupingType*> types;
    for (GroupingType* type : this->potentialGroups_)
        types.append(type);
    for (GroupingType* type : this->customFieldTypes_)
        types.append(type);
    return types;
}

// C# OtherConfigWatcher_OtherConfigChanged: one grouping type per custom field definition
void GroupingControl::onCustomFieldsChanged()
{
    const QList<CustomFieldDefinition> definitions = CustomFieldsManager::instance()->getCustomFields();

    for (const CustomFieldDefinition& definition : definitions)
    {
        const bool exists = std::any_of(this->customFieldTypes_.begin(), this->customFieldTypes_.end(), [&](GroupingType* t) {
            return static_cast<CustomFieldGroupingType*>(t)->definition() == definition;
        });
        if (!exists)
            this->customFieldTypes_.append(new CustomFieldGroupingType(definition));
    }

    bool removedButton = false;
    for (int i = this->customFieldTypes_.size() - 1; i >= 0; --i)
    {
        auto* type = static_cast<CustomFieldGroupingType*>(this->customFieldTypes_.at(i));
        if (definitions.contains(type->definition()))
            continue;
        // C# CustomFieldRemoved: drop the buttons using it
        for (DropDownButton* button : QList<DropDownButton*>(this->groups_))
        {
            if (this->buttonType(button) == type)
            {
                this->removeGroup(button);
                removedButton = true;
            }
        }
        delete this->customFieldTypes_.takeAt(i);
    }

    if (removedButton)
        this->setup();
}

bool GroupingControl::searchingForFolders() const
{
    if (!this->searcher_)
        return false;
    std::unique_ptr<QueryScope> scope(this->searcher_->GetQueryScope());
    return scope && scope->wantType(ObjectTypes::Folder);
}

// C# WantGroupingType: relevant to what is searched for
bool GroupingControl::wantGroupingType(const GroupingType* gt) const
{
    if (!gt)
        return false;
    if (!this->searcher_)
        return true;
    std::unique_ptr<QueryScope> scope(this->searcher_->GetQueryScope());
    if (!scope)
        return true;
    if (scope->wantType(ObjectTypes::Folder))
        return false; // searching for folders forbids all groupings (Folder is added back separately)
    return scope->wantAnyOf(gt->GetAppliesTo());
}

// C# GetRemainingGroupTypes: unused, relevant types that keep the parent/child order around
// the given button (null for the Add Group button)
QList<const GroupingControl::GroupingType*> GroupingControl::getRemainingGroupTypes(DropDownButton* context) const
{
    QList<const GroupingType*> remaining;
    for (const GroupingType* gt : this->allGroupTypes())
    {
        if (this->wantGroupingType(gt))
            remaining.append(gt);
    }

    int position = -1; // -1 before the context button, 0 on it, +1 after it
    for (DropDownButton* button : this->groups_)
    {
        if (button == context)
            position = 0;
        else if (position == 0)
            position = 1;

        const GroupingType* groupType = this->buttonType(button);
        if (!groupType)
            continue;

        remaining.removeAll(groupType);
        for (const GroupingType* gt : QList<const GroupingType*>(remaining))
        {
            if ((position == -1 && groupType->IsDescendantOf(gt)) ||
                (position == 1 && gt->IsDescendantOf(groupType)) ||
                (position != 0 && dynamic_cast<const FolderGroupingType*>(groupType)))
            {
                remaining.removeAll(gt);
            }
        }
    }
    return remaining;
}

// C# ShowFolderOption: Folder only as the first (or only) grouping
bool GroupingControl::showFolderOption(DropDownButton* context) const
{
    if (!context)
        return this->groups_.isEmpty();
    if (dynamic_cast<const FolderGroupingType*>(this->buttonType(context)))
        return false;
    return !this->groups_.isEmpty() && this->groups_.first() == context;
}

const GroupingControl::GroupingType* GroupingControl::buttonType(DropDownButton* button) const
{
    return button ? reinterpret_cast<const GroupingType*>(button->property("groupingType").value<quintptr>()) : nullptr;
}

void GroupingControl::setButtonType(DropDownButton* button, const GroupingType* groupType)
{
    button->setProperty("groupingType", QVariant::fromValue(reinterpret_cast<quintptr>(groupType)));
    button->setText(groupType->ToString());
}

DropDownButton* GroupingControl::newGroupButton(const GroupingType* groupType)
{
    DropDownButton* button = new DropDownButton(this);
    this->setButtonType(button, groupType);
    button->SetMenu(new QMenu(button));
    connect(button->GetMenu(), &QMenu::aboutToShow, this, [this, button]() { this->populateButtonMenu(button); });
    button->installEventFilter(this);
    return button;
}

void GroupingControl::addGroup(const GroupingType* groupType)
{
    if (!groupType)
        return;
    DropDownButton* button = this->newGroupButton(groupType);
    this->groups_.append(button);
    this->setup();
}

void GroupingControl::removeGroup(DropDownButton* button)
{
    if (!button || !this->groups_.removeOne(button))
        return;
    this->layout_->removeWidget(button);
    button->hide();
    button->deleteLater();
}

void GroupingControl::removeAllGroups()
{
    for (DropDownButton* button : QList<DropDownButton*>(this->groups_))
        this->removeGroup(button);
}

// C# Setup: lay the buttons out; Folder, when present, is first and excludes the others
void GroupingControl::setup()
{
    bool folderSeen = false;
    for (DropDownButton* button : QList<DropDownButton*>(this->groups_))
    {
        if (folderSeen)
        {
            this->removeGroup(button);
            continue;
        }
        if (dynamic_cast<const FolderGroupingType*>(this->buttonType(button)))
        {
            folderSeen = true;
            // Folder cannot be turned off while searching for folders
            button->setEnabled(!this->searchingForFolders());
        }
    }

    for (int i = 0; i < this->groups_.size(); ++i)
    {
        DropDownButton* button = this->groups_.at(i);
        this->layout_->removeWidget(button);
        this->layout_->insertWidget(i, button);
        button->show();
    }

    this->addGroupButton_->setEnabled(this->groups_.size() < MAX_GROUPS &&
                                      !this->getRemainingGroupTypes(nullptr).isEmpty());
    emit this->GroupingChanged();
}

void GroupingControl::populateButtonMenu(DropDownButton* button)
{
    QMenu* menu = button->GetMenu();
    menu->clear();

    QAction* remove = menu->addAction(tr("Remove Grouping"));
    connect(remove, &QAction::triggered, this, [this, button]() {
        this->removeGroup(button);
        this->setup();
    });

    auto change = [this, button](const GroupingType* type) {
        return [this, button, type]() {
            this->setButtonType(button, type);
            this->setup();
        };
    };

    if (this->showFolderOption(button))
    {
        menu->addSeparator();
        connect(menu->addAction(this->folderGroupingType_->ToString()), &QAction::triggered, this,
                change(this->folderGroupingType_));
    }

    const QList<const GroupingType*> remaining = this->getRemainingGroupTypes(button);
    if (!remaining.isEmpty())
    {
        menu->addSeparator();
        for (const GroupingType* type : remaining)
            connect(menu->addAction(type->ToString()), &QAction::triggered, this, change(type));
    }
}

void GroupingControl::populateAddGroupMenu()
{
    QMenu* menu = this->addGroupButton_->GetMenu();
    menu->clear();

    if (this->groups_.size() >= MAX_GROUPS)
        return;

    if (this->showFolderOption(nullptr))
    {
        const GroupingType* folder = this->folderGroupingType_;
        connect(menu->addAction(folder->ToString()), &QAction::triggered, this, [this, folder]() { this->addGroup(folder); });
        menu->addSeparator();
    }

    for (const GroupingType* type : this->getRemainingGroupTypes(nullptr))
        connect(menu->addAction(type->ToString()), &QAction::triggered, this, [this, type]() { this->addGroup(type); });
}

Grouping* GroupingControl::GetGrouping() const
{
    // The last button is the innermost grouping
    Grouping* grouping = nullptr;
    for (int i = this->groups_.size() - 1; i >= 0; --i)
    {
        const GroupingType* type = this->buttonType(this->groups_.at(i));
        if (!type)
            return nullptr;
        grouping = type->GetGroup(grouping);
    }
    return grouping;
}

void GroupingControl::SetGrouping(Grouping* grouping)
{
    this->removeAllGroups();
    for (const Grouping* g = grouping; g; g = g->subgrouping())
        this->fromGrouping(g);
    this->setup();
}

// C# FromGrouping
void GroupingControl::fromGrouping(const Grouping* grouping)
{
    if (this->folderGroupingType_->ForGrouping(grouping))
    {
        this->groups_.append(this->newGroupButton(this->folderGroupingType_));
        return;
    }

    for (const GroupingType* type : this->allGroupTypes())
    {
        if (type->ForGrouping(grouping))
        {
            this->groups_.append(this->newGroupButton(type));
            return;
        }
    }

    // A custom field that is not (yet) defined here
    if (const CustomFieldGrouping* customField = dynamic_cast<const CustomFieldGrouping*>(grouping))
    {
        auto* type = new CustomFieldGroupingType(customField->definition());
        this->customFieldTypes_.append(type);
        this->groups_.append(this->newGroupButton(type));
    }
}

// C# searcher_SearchForChanged
void GroupingControl::onSearchForChanged()
{
    this->removeUnwantedGroups();

    // Searching for folders requires grouping by folder
    if (this->searchingForFolders() && this->groups_.isEmpty())
        this->addGroup(this->folderGroupingType_);
}

void GroupingControl::removeUnwantedGroups()
{
    for (DropDownButton* button : QList<DropDownButton*>(this->groups_))
    {
        if (!this->wantGroupingType(this->buttonType(button)))
            this->removeGroup(button);
    }
    this->setup();
}

// Drag to reorder, within the parent/child order (C# button_MouseMove)
bool GroupingControl::eventFilter(QObject* watched, QEvent* event)
{
    DropDownButton* button = qobject_cast<DropDownButton*>(watched);
    if (!button || !this->groups_.contains(button))
        return QWidget::eventFilter(watched, event);

    switch (event->type())
    {
        case QEvent::MouseButtonPress:
        {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::LeftButton)
            {
                this->draggedButton_ = button;
                this->dragStart_ = button->mapToParent(mouse->pos());
                this->dragging_ = false;
            }
            break;
        }
        case QEvent::MouseMove:
        {
            if (this->draggedButton_ != button)
                break;
            auto* mouse = static_cast<QMouseEvent*>(event);
            const QPoint position = button->mapToParent(mouse->pos());
            if (!this->dragging_ && qAbs(position.x() - this->dragStart_.x()) < 5)
                break;
            this->dragging_ = true;
            this->dragTo(button, position.x());
            return true;
        }
        case QEvent::MouseButtonRelease:
        {
            const bool wasDragging = this->dragging_;
            this->draggedButton_ = nullptr;
            this->dragging_ = false;
            if (wasDragging)
            {
                this->setup();
                return true; // don't open the menu after a drag
            }
            break;
        }
        default:
            break;
    }
    return QWidget::eventFilter(watched, event);
}

void GroupingControl::dragTo(DropDownButton* button, int x)
{
    const int from = this->groups_.indexOf(button);
    const GroupingType* draggedType = this->buttonType(button);

    // Barriers: an ancestor must stay to the left, a descendant to the right
    int leftBarrier = -1;
    int rightBarrier = this->groups_.size();
    for (int i = 0; i < this->groups_.size(); ++i)
    {
        const GroupingType* type = this->buttonType(this->groups_.at(i));
        if (i < from && draggedType && type && draggedType->IsDescendantOf(type))
            leftBarrier = i;
        else if (i > from && draggedType && type && type->IsDescendantOf(draggedType) && rightBarrier == this->groups_.size())
            rightBarrier = i;
    }

    // The position whose button's middle the cursor has passed
    int to = from;
    for (int i = 0; i < this->groups_.size(); ++i)
    {
        DropDownButton* candidate = this->groups_.at(i);
        const int middle = candidate->geometry().center().x();
        if (i < from && x < middle)
        {
            to = i;
            break;
        }
        if (i > from && x > middle)
            to = i;
    }
    to = qBound(leftBarrier + 1, to, rightBarrier - 1);

    if (to != from)
    {
        this->groups_.move(from, to);
        for (int i = 0; i < this->groups_.size(); ++i)
        {
            this->layout_->removeWidget(this->groups_.at(i));
            this->layout_->insertWidget(i, this->groups_.at(i));
        }
    }
}
