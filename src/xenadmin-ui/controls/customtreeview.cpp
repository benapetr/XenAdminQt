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

#include "customtreeview.h"
#include <QHeaderView>
#include <QKeyEvent>

CustomTreeView::CustomTreeView(QWidget* parent) : QTreeWidget(parent)
{
    setColumnCount(2);
    setHeaderHidden(true);
    setIndentation(m_nodeIndent);
    setRootIsDecorated(true);
    setSelectionMode(QAbstractItemView::SingleSelection);

    connect(this, &QTreeWidget::itemChanged, this, &CustomTreeView::onItemChanged);
}

CustomTreeView::~CustomTreeView()
{
    this->m_nodes.clear();
    this->m_itemMap.clear();
}

void CustomTreeView::SetNodeIndent(int value)
{
    this->m_nodeIndent = value;
    setIndentation(this->m_nodeIndent);
}

void CustomTreeView::SetShowCheckboxes(bool value)
{
    this->m_showCheckboxes = value;
    this->rebuildItems();
}

void CustomTreeView::SetShowDescription(bool value)
{
    this->m_showDescription = value;
    setColumnCount(this->m_showDescription ? 2 : 1);
    this->rebuildItems();
}

void CustomTreeView::SetShowImages(bool value)
{
    this->m_showImages = value;
    this->rebuildItems();
}

void CustomTreeView::SetShowRootLines(bool value)
{
    this->m_showRootLines = value;
    setRootIsDecorated(value);
    this->rebuildItems();
}

void CustomTreeView::SetRootAlwaysExpanded(bool value)
{
    this->m_rootAlwaysExpanded = value;
    this->rebuildItems();
}

void CustomTreeView::BeginUpdate()
{
    this->m_inUpdate = true;
    setUpdatesEnabled(false);
}

void CustomTreeView::EndUpdate()
{
    this->m_inUpdate = false;
    this->rebuildItems();
    setUpdatesEnabled(true);
}

void CustomTreeView::AddNode(CustomTreeNode* node)
{
    if (!node)
        return;

    if (this->m_nodes.isEmpty())
        this->m_nodes.append(&this->m_secretNode);

    this->m_secretNode.AddChild(node);
    this->m_nodes.append(node);

    if (!this->m_inUpdate)
        this->rebuildItems();
}

void CustomTreeView::RemoveNode(CustomTreeNode* node)
{
    if (!node)
        return;

    this->m_nodes.removeAll(node);

    if (!this->m_inUpdate)
        this->rebuildItems();
}

void CustomTreeView::AddChildNode(CustomTreeNode* parent, CustomTreeNode* child)
{
    if (!parent || !child)
        return;

    parent->AddChild(child);
    this->m_nodes.append(child);

    if (!this->m_inUpdate)
        this->rebuildItems();
}

void CustomTreeView::ClearAllNodes()
{
    this->m_secretNode.ChildNodes.clear();
    this->m_nodes.clear();
    this->m_itemMap.clear();
    clear();
}

void CustomTreeView::Resort()
{
    this->rebuildItems();
}

QList<CustomTreeNode*> CustomTreeView::CheckedItems() const
{
    QList<CustomTreeNode*> nodes;
    for (CustomTreeNode* node : this->m_nodes)
    {
        if (!node)
            continue;
        if (node->Level >= 0 && node->State() == Qt::Checked && node->Enabled)
            nodes.append(node);
    }
    return nodes;
}

QList<CustomTreeNode*> CustomTreeView::CheckableItems() const
{
    QList<CustomTreeNode*> nodes;
    for (CustomTreeNode* node : this->m_nodes)
    {
        if (!node)
            continue;
        if (node->Level >= 0 && node->State() != Qt::Checked && node->Enabled)
            nodes.append(node);
    }
    return nodes;
}

void CustomTreeView::mouseDoubleClickEvent(QMouseEvent* event)
{
    QTreeWidget::mouseDoubleClickEvent(event);
    emit DoubleClickOnRow();
}

void CustomTreeView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space)
    {
        CustomTreeNode* node = this->nodeFromItem(currentItem());
        if (node && this->m_showCheckboxes && !node->HideCheckbox && node->Enabled)
        {
            const Qt::CheckState nextState = node->State() == Qt::Checked ? Qt::Unchecked : Qt::Checked;
            node->SetState(nextState);
            this->syncNodeState(node);
            emit ItemCheckChanged(node);
            return;
        }
    }

    QTreeWidget::keyPressEvent(event);
}

void CustomTreeView::onItemChanged(QTreeWidgetItem* item, int column)
{
    if (this->m_ignoreItemChanges || column != 0)
        return;

    CustomTreeNode* node = this->nodeFromItem(item);
    if (!node || node->HideCheckbox || !node->Enabled || !this->m_showCheckboxes)
        return;

    const Qt::CheckState newState = item->checkState(0);
    if (newState == node->State())
        return;

    node->SetState(newState);
    this->syncNodeState(node);
    emit ItemCheckChanged(node);
}

void CustomTreeView::rebuildItems()
{
    if (this->m_inUpdate)
        return;

    this->m_lastSelected = this->nodeFromItem(currentItem());

    if (this->m_nodes.size() > 1)
    {
        std::sort(this->m_nodes.begin(), this->m_nodes.end(), [](const CustomTreeNode* a, const CustomTreeNode* b) {
            if (!a)
                return false;
            return a->CompareTo(b) < 0;
        });
    }

    this->m_ignoreItemChanges = true;
    clear();
    this->m_itemMap.clear();

    for (CustomTreeNode* node : this->m_nodes)
    {
        if (!node || node->Level == -1)
            continue;

        QTreeWidgetItem* parentItem = nullptr;
        if (node->ParentNode && node->ParentNode != &this->m_secretNode)
            parentItem = this->m_itemMap.value(node->ParentNode);

        QTreeWidgetItem* item = this->createItemForNode(node, parentItem);
        this->m_itemMap.insert(node, item);
    }

    if (this->m_lastSelected && this->m_itemMap.contains(this->m_lastSelected))
        setCurrentItem(this->m_itemMap.value(this->m_lastSelected));

    this->m_ignoreItemChanges = false;
}

QTreeWidgetItem* CustomTreeView::createItemForNode(CustomTreeNode* node, QTreeWidgetItem* parent)
{
    QTreeWidgetItem* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(this);

    item->setText(0, node->ToString());

    if (this->m_showDescription && columnCount() > 1)
        item->setText(1, node->Description);

    if (this->m_showImages && !node->Image.isNull())
        item->setIcon(0, node->Image);

    Qt::ItemFlags flags = item->flags();
    if (!node->Selectable())
        flags &= ~Qt::ItemIsSelectable;

    if (!node->Enabled)
        flags &= ~Qt::ItemIsEnabled;

    if (this->m_showCheckboxes && !node->HideCheckbox)
        flags |= Qt::ItemIsUserCheckable;
    else
        flags &= ~Qt::ItemIsUserCheckable;

    item->setFlags(flags);

    if (this->m_showCheckboxes && !node->HideCheckbox)
        item->setCheckState(0, node->State());

    const bool forceExpanded = this->m_rootAlwaysExpanded && node->Level == 0;
    item->setExpanded(forceExpanded ? true : node->Expanded());

    return item;
}

void CustomTreeView::syncNodeState(CustomTreeNode* node)
{
    if (!node)
        return;

    this->m_ignoreItemChanges = true;

    QList<CustomTreeNode*> stack;
    stack.append(node);

    while (!stack.isEmpty())
    {
        CustomTreeNode* current = stack.takeLast();
        QTreeWidgetItem* item = this->m_itemMap.value(current);
        if (item && this->m_showCheckboxes && !current->HideCheckbox)
            item->setCheckState(0, current->State());

        if (current->ParentNode)
            stack.append(current->ParentNode);

        for (CustomTreeNode* child : current->ChildNodes)
            if (child)
                stack.append(child);
    }

    this->m_ignoreItemChanges = false;
}

CustomTreeNode* CustomTreeView::nodeFromItem(QTreeWidgetItem* item) const
{
    if (!item)
        return nullptr;

    for (auto it = this->m_itemMap.constBegin(); it != this->m_itemMap.constEnd(); ++it)
    {
        if (it.value() == item)
            return it.key();
    }

    return nullptr;
}
