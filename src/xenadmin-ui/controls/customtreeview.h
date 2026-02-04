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

#ifndef CUSTOMTREEVIEW_H
#define CUSTOMTREEVIEW_H

#include <QHash>
#include <QTreeWidget>

#include "customtreenode.h"

class CustomTreeView : public QTreeWidget
{
    Q_OBJECT

    Q_PROPERTY(int nodeIndent READ NodeIndent WRITE SetNodeIndent)
    Q_PROPERTY(bool showCheckboxes READ ShowCheckboxes WRITE SetShowCheckboxes)
    Q_PROPERTY(bool showDescription READ ShowDescription WRITE SetShowDescription)
    Q_PROPERTY(bool showImages READ ShowImages WRITE SetShowImages)
    Q_PROPERTY(bool showRootLines READ ShowRootLines WRITE SetShowRootLines)
    Q_PROPERTY(bool rootAlwaysExpanded READ RootAlwaysExpanded WRITE SetRootAlwaysExpanded)

    public:
        explicit CustomTreeView(QWidget* parent = nullptr);
        ~CustomTreeView() override;

        int NodeIndent() const { return m_nodeIndent; }
        void SetNodeIndent(int value);

        bool ShowCheckboxes() const { return m_showCheckboxes; }
        void SetShowCheckboxes(bool value);

        bool ShowDescription() const { return m_showDescription; }
        void SetShowDescription(bool value);

        bool ShowImages() const { return m_showImages; }
        void SetShowImages(bool value);

        bool ShowRootLines() const { return m_showRootLines; }
        void SetShowRootLines(bool value);

        bool RootAlwaysExpanded() const { return m_rootAlwaysExpanded; }
        void SetRootAlwaysExpanded(bool value);

        void BeginUpdate();
        void EndUpdate();

        void AddNode(CustomTreeNode* node);
        void RemoveNode(CustomTreeNode* node);
        void AddChildNode(CustomTreeNode* parent, CustomTreeNode* child);
        void ClearAllNodes();
        void Resort();

        QList<CustomTreeNode*> CheckedItems() const;
        QList<CustomTreeNode*> CheckableItems() const;

        CustomTreeNode* SecretNode() { return &m_secretNode; }

    signals:
        void ItemCheckChanged(CustomTreeNode* node);
        void DoubleClickOnRow();

    protected:
        void mouseDoubleClickEvent(QMouseEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;

    private slots:
        void onItemChanged(QTreeWidgetItem* item, int column);

    private:
        void rebuildItems();
        QTreeWidgetItem* createItemForNode(CustomTreeNode* node, QTreeWidgetItem* parent);
        void syncNodeState(CustomTreeNode* node);
        CustomTreeNode* nodeFromItem(QTreeWidgetItem* item) const;

        QList<CustomTreeNode*> m_nodes;
        CustomTreeNode m_secretNode;
        QHash<CustomTreeNode*, QTreeWidgetItem*> m_itemMap;
        CustomTreeNode* m_lastSelected = nullptr;

        bool m_inUpdate = false;
        bool m_ignoreItemChanges = false;

        int m_nodeIndent = 19;
        bool m_showCheckboxes = true;
        bool m_showDescription = true;
        bool m_showImages = false;
        bool m_showRootLines = true;
        bool m_rootAlwaysExpanded = false;
};

#endif // CUSTOMTREEVIEW_H
