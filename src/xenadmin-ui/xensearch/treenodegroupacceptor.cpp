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

#include "treenodegroupacceptor.h"
#include "treenodefactory.h"
#include "xensearch/grouping.h"

TreeNodeGroupAcceptor::TreeNodeGroupAcceptor(QTreeWidget* treeWidget, const QVariant& needToBeExpanded)
    : treeWidget_(treeWidget),
      parentItem_(nullptr),
      needToBeExpanded_(needToBeExpanded),
      index_(0)
{
}

TreeNodeGroupAcceptor::TreeNodeGroupAcceptor(QTreeWidgetItem* parentItem, const QVariant& needToBeExpanded)
    : treeWidget_(nullptr),
      parentItem_(parentItem),
      needToBeExpanded_(needToBeExpanded),
      index_(0)
{
}

IAcceptGroups* TreeNodeGroupAcceptor::Add(Grouping* grouping,
                                          const QVariant& group,
                                          const QString& objectType,
                                          const QVariantMap& objectData,
                                          int /*indent*/,
                                          XenConnection* /*conn*/)
{
    if (!group.isValid())
        return nullptr;
    
    Q_UNUSED(grouping);
    Q_UNUSED(objectType);
    Q_UNUSED(objectData);

    QTreeWidgetItem* node = TreeNodeFactory::CreateGroupNode(group);
    if (!node)
        return nullptr;

    if (this->treeWidget_)
    {
        this->treeWidget_->insertTopLevelItem(this->index_, node);
        this->index_++;
    }
    else if (this->parentItem_)
    {
        this->parentItem_->insertChild(this->index_, node);
        this->index_++;
    }

    return new TreeNodeGroupAcceptor(node, QVariant());
}

void TreeNodeGroupAcceptor::FinishedInThisGroup(bool defaultExpand)
{
    // Determine if we should expand this group
    bool shouldExpand = defaultExpand;
    
    if (this->needToBeExpanded_.isValid())
    {
        // Use explicit value if provided
        shouldExpand = this->needToBeExpanded_.toBool();
    }
    
    // Expand or collapse the parent node
    if (this->parentItem_)
        this->parentItem_->setExpanded(shouldExpand);
}
