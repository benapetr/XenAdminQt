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
#include <QHBoxLayout>
#include <QAction>

GroupingControl::GroupingControl(QWidget* parent)
    : QWidget(parent)
    , folderGroupingType_(nullptr)
    , addGroupButton_(nullptr)
    , searcher_(nullptr)
    , lastButtonClicked_(nullptr)
{
    this->setFixedHeight(29);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 3, 0, 3);
    layout->setSpacing(INNER_GUTTER);

    this->initializePotentialGroups();

    // Create the "Add Group" button
    this->addGroupButton_ = new DropDownButton(this);
    this->addGroupButton_->setText("Add Group");
    this->addGroupButton_->setFixedHeight(23);
    
    QMenu* addGroupMenu = new QMenu(this);
    this->addGroupButton_->SetMenu(addGroupMenu);
    
    connect(this->addGroupButton_, &QPushButton::clicked, 
            this, &GroupingControl::onAddGroupButtonClicked);

    layout->addWidget(this->addGroupButton_);
    layout->addStretch();

    // Start with the first potential group (typically Pool)
    if (!this->potentialGroups_.isEmpty())
    {
        this->addGroup(this->potentialGroups_[0]);
    }

    this->setup();
}

GroupingControl::~GroupingControl()
{
    qDeleteAll(this->potentialGroups_);
    this->potentialGroups_.clear();
    delete this->folderGroupingType_;
}

void GroupingControl::initializePotentialGroups()
{
    // Object type constants (matching C# ObjectTypes)
    const int AllExcFolders = 0x3FFE;  // All except Folder
    const int VMType = 0x0002;         // VM
    const int VDIType = 0x0080;        // VDI

    // Create the folder grouping type
    this->folderGroupingType_ = new FolderGroupingType();

    // Pool grouping
    GroupingType* poolGroup = new PropertyGroupingType(
        "Pool", "pool", AllExcFolders & ~0x0008);  // ~Pool
    this->potentialGroups_.append(poolGroup);

    // Host grouping (with Pool as parent)
    GroupingType* hostGroup = new XenObjectPropertyGroupingType(
        "Server", "host", 
        AllExcFolders & ~0x0008 & ~0x0004, poolGroup);  // ~Pool & ~Server
    this->potentialGroups_.append(hostGroup);

    // OS Name grouping
    this->potentialGroups_.append(new PropertyGroupingType(
        "Operating System", "os_name", VMType));

    // Power State grouping
    this->potentialGroups_.append(new PropertyGroupingType(
        "Power State", "power_state", VMType));

    // Virtualization Status grouping
    this->potentialGroups_.append(new PropertyGroupingType(
        "Virtualization Status", "virtualisation_status", VMType));

    // Object Type grouping
    this->potentialGroups_.append(new PropertyGroupingType(
        "Type", "type", AllExcFolders));

    // Networks grouping (with Pool as parent)
    this->potentialGroups_.append(new XenObjectPropertyGroupingType(
        "Networks", "networks", VMType, poolGroup));

    // Storage grouping (with Pool as parent)
    GroupingType* srGroup = new XenObjectPropertyGroupingType(
        "Storage", "storage", 
        VMType | VDIType, poolGroup);
    this->potentialGroups_.append(srGroup);

    // Disks grouping (with Storage as parent)
    this->potentialGroups_.append(new XenObjectPropertyGroupingType(
        "Virtual Disks", "disks", VMType, srGroup));

    // HA Restart Priority grouping
    this->potentialGroups_.append(new PropertyGroupingType(
        "HA Restart Priority", "ha_restart_priority", VMType));

    // Tags grouping
    this->potentialGroups_.append(new PropertyGroupingType(
        "Tags", "tags", AllExcFolders));

    // TODO: Add custom field grouping types
    // TODO: Add VM appliance grouping
    // TODO: Add read caching enabled grouping
    // TODO: Add vendor device state grouping
}

void GroupingControl::setup()
{
    int offset = 0;
    
    foreach (DropDownButton* button, this->groups_)
    {
        button->move(offset, 3);
        offset += button->width() + INNER_GUTTER;
    }

    this->addGroupButton_->move(offset, 3);

    // Enable/disable Add Group button based on limits
    this->addGroupButton_->setEnabled(
        this->groups_.count() < MAX_GROUPS && 
        this->getRemainingGroupTypes(nullptr).count() > 0);

    emit GroupingChanged();
}

void GroupingControl::addGroup(GroupingType* groupType)
{
    if (!groupType)
        return;

    DropDownButton* button = this->newGroupButton(groupType);
    
    this->groups_.append(button);
    button->show();

    this->setup();
}

void GroupingControl::removeGroup(DropDownButton* button)
{
    if (!button)
        return;

    this->groups_.removeOne(button);
    button->deleteLater();
}

void GroupingControl::removeAllGroups()
{
    foreach (DropDownButton* button, this->groups_)
    {
        this->removeGroup(button);
    }
}

void GroupingControl::removeUnwantedGroups()
{
    foreach (DropDownButton* button, this->groups_)
    {
        GroupingType* gt = button->property("groupingType").value<GroupingType*>();
        if (!this->wantGroupingType(gt))
        {
            this->removeGroup(button);
        }
    }
    this->setup();
}

DropDownButton* GroupingControl::newGroupButton(GroupingType* groupType)
{
    DropDownButton* button = new DropDownButton(this);
    button->setText(groupType->GetName());
    button->setFixedHeight(23);
    button->setProperty("groupingType", QVariant::fromValue(groupType));

    QMenu* menu = new QMenu(this);
    button->SetMenu(menu);

    connect(button, &QPushButton::clicked, 
            this, &GroupingControl::onGroupButtonClicked);

    return button;
}

void GroupingControl::onGroupButtonClicked()
{
    DropDownButton* button = qobject_cast<DropDownButton*>(sender());
    if (!button)
        return;

    QMenu* menu = button->GetMenu();
    if (!menu)
        return;

    menu->clear();
    this->buildContextMenu(button, menu);
    this->lastButtonClicked_ = button;
}

void GroupingControl::onAddGroupButtonClicked()
{
    QMenu* menu = this->addGroupButton_->GetMenu();
    if (!menu)
        return;

    menu->clear();
    this->buildAddGroupMenu(menu);
    this->lastButtonClicked_ = this->addGroupButton_;
}

void GroupingControl::buildContextMenu(DropDownButton* button, QMenu* menu)
{
    if (!button || !menu)
        return;

    // Add "Remove Grouping" option
    QAction* removeAction = menu->addAction("Remove Grouping");
    removeAction->setProperty("button", QVariant::fromValue(button));
    connect(removeAction, &QAction::triggered, [this, button]() {
        this->removeGroup(button);
        this->setup();
    });

    // Add separator
    menu->addSeparator();

    // Add folder option if applicable
    if (this->showFolderOption(button))
    {
        QAction* folderAction = menu->addAction(this->folderGroupingType_->GetName());
        folderAction->setProperty("button", QVariant::fromValue(button));
        folderAction->setProperty("groupingType", QVariant::fromValue(this->folderGroupingType_));
        connect(folderAction, &QAction::triggered, [this, button]() {
            button->setText(this->folderGroupingType_->GetName());
            button->setProperty("groupingType", QVariant::fromValue(this->folderGroupingType_));
            this->setup();
        });
        menu->addSeparator();
    }

    // Add remaining grouping types
    QList<GroupingType*> remainingTypes = this->getRemainingGroupTypes(button);
    foreach (GroupingType* gt, remainingTypes)
    {
        QAction* action = menu->addAction(gt->GetName());
        action->setProperty("button", QVariant::fromValue(button));
        action->setProperty("groupingType", QVariant::fromValue(gt));
        connect(action, &QAction::triggered, [this, button, gt]() {
            button->setText(gt->GetName());
            button->setProperty("groupingType", QVariant::fromValue(gt));
            this->setup();
        });
    }
}

void GroupingControl::buildAddGroupMenu(QMenu* menu)
{
    if (!menu)
        return;

    // Add folder option if applicable
    if (this->showFolderOption(nullptr))
    {
        QAction* folderAction = menu->addAction(this->folderGroupingType_->GetName());
        folderAction->setProperty("groupingType", QVariant::fromValue(this->folderGroupingType_));
        connect(folderAction, &QAction::triggered, [this]() {
            this->addGroup(this->folderGroupingType_);
        });
        menu->addSeparator();
    }

    // Add all remaining grouping types
    QList<GroupingType*> remainingTypes = this->getRemainingGroupTypes(nullptr);
    foreach (GroupingType* gt, remainingTypes)
    {
        QAction* action = menu->addAction(gt->GetName());
        action->setProperty("groupingType", QVariant::fromValue(gt));
        connect(action, &QAction::triggered, [this, gt]() {
            this->addGroup(gt);
        });
    }
}

QList<GroupingControl::GroupingType*> GroupingControl::getRemainingGroupTypes(DropDownButton* context) const
{
    QList<GroupingType*> remaining = this->potentialGroups_;

    // TODO: Remove group types which are not relevant to search-for
    // TODO: Implement ancestor/descendant filtering based on hierarchy

    // Remove group types already in use
    foreach (DropDownButton* button, this->groups_)
    {
        if (button == context)
            continue;

        GroupingType* gt = button->property("groupingType").value<GroupingType*>();
        remaining.removeOne(gt);
    }

    return remaining;
}

bool GroupingControl::showFolderOption(DropDownButton* context) const
{
    // If this is for the Add Group button, allow folder if no existing buttons
    if (!context)
        return this->groups_.isEmpty();

    // For normal buttons, if this button already shows Folder, don't allow it
    GroupingType* gt = context->property("groupingType").value<GroupingType*>();
    if (dynamic_cast<FolderGroupingType*>(gt))
        return false;

    // Otherwise allow it only on the first button
    return !this->groups_.isEmpty() && this->groups_[0] == context;
}

bool GroupingControl::wantGroupingType(const GroupingType* gt) const
{
    if (!gt)
        return false;

    // TODO: Filter based on searcher's QueryScope
    // For now, accept all grouping types
    return true;
}

Grouping* GroupingControl::GetGrouping() const
{
    Grouping* group = nullptr;

    // Build grouping hierarchy from right to left (last button is innermost)
    for (int i = this->groups_.count() - 1; i >= 0; i--)
    {
        DropDownButton* button = this->groups_[i];
        GroupingType* gt = button->property("groupingType").value<GroupingType*>();
        if (gt)
        {
            group = gt->GetGroup(group);
        }
    }

    return group;
}

void GroupingControl::SetGrouping(Grouping* grouping)
{
    Q_UNUSED(grouping);
    this->removeAllGroups();

    // TODO: Build grouping buttons from Grouping hierarchy
    // This requires walking the grouping chain and matching to GroupingTypes

    this->setup();
}

void GroupingControl::SetSearcher(Searcher* searcher)
{
    if (this->searcher_)
    {
        disconnect(this->searcher_, &Searcher::SearchForChanged, 
                   this, &GroupingControl::onSearchForChanged);
    }

    this->searcher_ = searcher;

    if (this->searcher_)
    {
        connect(this->searcher_, &Searcher::SearchForChanged, 
                this, &GroupingControl::onSearchForChanged);
    }
}

void GroupingControl::onSearchForChanged()
{
    this->removeUnwantedGroups();

    // TODO: If searching for folders, add folder grouping
}

void GroupingControl::onGroupButtonMenuTriggered(QAction* action)
{
    Q_UNUSED(action);
    // Handled by lambdas in buildContextMenu
}

void GroupingControl::onAddGroupMenuTriggered(QAction* action)
{
    Q_UNUSED(action);
    // Handled by lambdas in buildAddGroupMenu
}

//
// GroupingType implementations
//

Grouping* GroupingControl::PropertyGroupingType::GetGroup(Grouping* subgrouping) const
{
    // For now, return the appropriate grouping type based on property name
    // TODO: Implement generic PropertyGrouping class
    if (this->property_ == "type")
        return new TypeGrouping(subgrouping);
    else if (this->property_ == "pool")
        return new PoolGrouping(subgrouping);
    else if (this->property_ == "host")
        return new HostGrouping(subgrouping);
    else
        return new TypeGrouping(subgrouping);  // Fallback
}

bool GroupingControl::PropertyGroupingType::ForGrouping(const Grouping* grouping) const
{
    // TODO: Implement proper matching based on property name
    // For now, just return false
    Q_UNUSED(grouping);
    return false;
}

Grouping* GroupingControl::XenObjectPropertyGroupingType::GetGroup(Grouping* subgrouping) const
{
    // For now, return the appropriate grouping type based on property name
    // TODO: Implement generic XenModelObjectPropertyGrouping class
    if (this->property_ == "pool")
        return new PoolGrouping(subgrouping);
    else if (this->property_ == "host")
        return new HostGrouping(subgrouping);
    else
        return new TypeGrouping(subgrouping);  // Fallback
}

bool GroupingControl::XenObjectPropertyGroupingType::IsDescendantOf(const GroupingType* gt) const
{
    if (!gt)
        return false;

    return (gt == this->parent_ || 
            (this->parent_ && this->parent_->IsDescendantOf(gt)));
}

Grouping* GroupingControl::FolderGroupingType::GetGroup(Grouping* subgrouping) const
{
    // TODO: Implement FolderGrouping class
    Q_UNUSED(subgrouping);
    return new TypeGrouping(nullptr);  // Temporary fallback
}

bool GroupingControl::FolderGroupingType::ForGrouping(const Grouping* grouping) const
{
    // TODO: Implement folder grouping detection
    Q_UNUSED(grouping);
    return false;
}
