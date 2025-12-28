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

#include "treewidgetgroupacceptor.h"
#include "querypanel.h"
#include "xenlib/xensearch/grouping.h"
#include "xenlib/xenlib.h"

TreeWidgetGroupAcceptor::TreeWidgetGroupAcceptor(QTreeWidget* treeWidget, QueryPanel* queryPanel)
    : treeWidget_(treeWidget)
    , parentItem_(nullptr)
    , queryPanel_(queryPanel)
{
}

TreeWidgetGroupAcceptor::TreeWidgetGroupAcceptor(QTreeWidgetItem* parentItem, QueryPanel* queryPanel)
    : treeWidget_(nullptr)
    , parentItem_(parentItem)
    , queryPanel_(queryPanel)
{
}

IAcceptGroups* TreeWidgetGroupAcceptor::Add(Grouping* grouping, const QVariant& group,
                                            const QString& objectType, const QVariantMap& objectData,
                                            int indent, XenConnection *conn)
{
    Q_UNUSED(indent);

    QTreeWidgetItem* item = nullptr;

    if (objectType.isEmpty())
    {
        // Group header (not a leaf object)
        QString groupName = grouping ? grouping->getGroupName(group) : group.toString();
        QIcon groupIcon = grouping ? grouping->getGroupIcon(group) : QIcon();

        item = new QTreeWidgetItem();
        item->setText(0, groupName);
        if (!groupIcon.isNull())
            item->setIcon(0, groupIcon);
        
        // Store group value in UserRole for later reference
        item->setData(0, Qt::UserRole, group);
        item->setData(0, Qt::UserRole + 1, QVariant::fromValue<void*>(nullptr)); // No XenObject for group headers
        
        // Make group headers bold
        QFont font = item->font(0);
        font.setBold(true);
        item->setFont(0, font);
    }
    else
    {
        // Leaf object - delegate to QueryPanel to create proper row
        item = this->queryPanel_->CreateRow(grouping, objectType, objectData, conn);
    }

    if (!item)
        return nullptr;

    // Add item to tree
    if (this->treeWidget_)
    {
        this->treeWidget_->addTopLevelItem(item);
    }
    else if (this->parentItem_)
    {
        this->parentItem_->addChild(item);
    }

    // Return new adapter for adding children to this item
    return new TreeWidgetGroupAcceptor(item, this->queryPanel_);
}

void TreeWidgetGroupAcceptor::FinishedInThisGroup(bool defaultExpand)
{
    // Expand/collapse based on default setting
    if (this->parentItem_)
    {
        this->parentItem_->setExpanded(defaultExpand);
    }
}
