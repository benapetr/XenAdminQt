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
 * SUBSTITUTE GROUPS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

// treewidgetgroupacceptor.h - IAcceptGroups adapter for QTreeWidget
// Port of C# xenadmin/XenAdmin/XenSearch/TreeNodeGroupAcceptor.cs
#ifndef TREEWIDGETGROUPACCEPTOR_H
#define TREEWIDGETGROUPACCEPTOR_H

#include "xenlib/xensearch/iacceptgroups.h"
#include <QTreeWidget>
#include <QTreeWidgetItem>

// Forward declarations
class QueryPanel;

/**
 * @brief IAcceptGroups adapter for QTreeWidget
 *
 * C# equivalent: TreeNodeGroupAcceptor in TreeNodeGroupAcceptor.cs
 *
 * This adapter allows Search::PopulateAdapters() to populate a QTreeWidget
 * with grouped objects without knowing QTreeWidget details.
 *
 * Usage:
 *   QTreeWidget* tree = new QTreeWidget();
 *   TreeWidgetGroupAcceptor adapter(tree);
 *   search->PopulateAdapters(xenLib, {&adapter});
 */
class TreeWidgetGroupAcceptor : public IAcceptGroups
{
    public:
        /**
        * @brief Constructor for root-level adapter (populates QTreeWidget top-level)
        * @param treeWidget QTreeWidget to populate
        * @param queryPanel QueryPanel instance for creating rows
        */
        TreeWidgetGroupAcceptor(QTreeWidget* treeWidget, QueryPanel* queryPanel);

        /**
        * @brief Constructor for child adapter (populates QTreeWidgetItem children)
        * @param parentItem QTreeWidgetItem to add children to
        * @param queryPanel QueryPanel instance for creating rows
        */
        TreeWidgetGroupAcceptor(QTreeWidgetItem* parentItem, QueryPanel* queryPanel);

        ~TreeWidgetGroupAcceptor() override = default;

        IAcceptGroups* Add(Grouping* grouping, const QVariant& group,
                        const QString& objectType, const QVariantMap& objectData,
                        int indent, XenConnection* conn) override;

        void FinishedInThisGroup(bool defaultExpand) override;

    private:
        QTreeWidget* treeWidget_;
        QTreeWidgetItem* parentItem_;
        QueryPanel* queryPanel_;
};

#endif // TREEWIDGETGROUPACCEPTOR_H
