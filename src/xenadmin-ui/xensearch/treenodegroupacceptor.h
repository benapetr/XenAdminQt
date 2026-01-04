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

#ifndef TREENODEGROUPACCEPTOR_H
#define TREENODEGROUPACCEPTOR_H

#include "xensearch/iacceptgroups.h"
#include <QTreeWidget>
#include <QVariant>

class Grouping;
class XenConnection;

/**
 * @brief Accepts groups and builds virtual tree nodes
 * 
 * Qt equivalent of C# TreeNodeGroupAcceptor class.
 * Implements IAcceptGroups interface to populate tree structure from search results.
 * 
 * C# equivalent: xenadmin/XenAdmin/XenSearch/TreeNodeGroupAcceptor.cs
 */
class TreeNodeGroupAcceptor : public IAcceptGroups
{
    public:
        /**
         * @brief Construct a new tree node group acceptor rooted at a tree widget
         *
         * @param treeWidget Tree widget that receives top-level nodes
         * @param needToBeExpanded Optional expansion state (null = use default from grouping)
         */
        TreeNodeGroupAcceptor(QTreeWidget* treeWidget, const QVariant& needToBeExpanded = QVariant());

        /**
         * @brief Construct a new tree node group acceptor rooted at a tree item
         *
         * @param parentItem Parent item that receives child nodes
         * @param needToBeExpanded Optional expansion state (null = use default from grouping)
         */
        TreeNodeGroupAcceptor(QTreeWidgetItem* parentItem, const QVariant& needToBeExpanded = QVariant());
        ~TreeNodeGroupAcceptor() override = default;
        
        // IAcceptGroups interface implementation
        IAcceptGroups* Add(Grouping* grouping,
                          const QVariant& group,
                          const QString& objectType,
                          const QVariantMap& objectData,
                          int indent,
                          XenConnection* conn) override;
        
        void FinishedInThisGroup(bool defaultExpand) override;
        
    private:
        QTreeWidget* treeWidget_;
        QTreeWidgetItem* parentItem_;
        QVariant needToBeExpanded_;  // QVariant::Invalid means null (use default)
        int index_;
};

#endif // TREENODEGROUPACCEPTOR_H
