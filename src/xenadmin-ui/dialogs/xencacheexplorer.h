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

#ifndef XENCACHEEXPLORER_H
#define XENCACHEEXPLORER_H

#include <QDialog>
#include <QMap>
#include <QVariant>

class QTreeWidgetItem;
class XenConnection;

namespace Ui
{
    class XenCacheExplorer;
}

class XenCacheExplorer : public QDialog
{
    Q_OBJECT

    public:
        explicit XenCacheExplorer(QWidget* parent = nullptr);
        ~XenCacheExplorer() override;

    private slots:
        void onTreeItemSelectionChanged();
        void onRefreshClicked();
        void onPropertiesTreeContextMenu(const QPoint& pos);
        void onCopySelectedValue();
        void onCopySelectedRows();

    private:
        void populateTree();
        void populateConnectionNode(QTreeWidgetItem* connectionNode, XenConnection* connection);
        void displayObjectProperties(XenConnection* connection, const QString& type, const QString& opaqueRef);
        void displayCategoryInfo(XenConnection* connection, const QString& type);
        void displayConnectionInfo(XenConnection* connection);
        QString formatVariantValue(const QVariant& value) const;
        QString getVariantTypeName(const QVariant& value) const;
        QTreeWidgetItem* addPropertyItem(QTreeWidgetItem* parent, const QString& name, const QString& value, const QString& type);
        void appendVariantChildren(QTreeWidgetItem* parent, const QVariant& value);

        enum ItemType
        {
            Type_Connection,
            Type_Category,
            Type_Object
        };

        Ui::XenCacheExplorer* ui;
        QMap<QTreeWidgetItem*, XenConnection*> m_itemToConnection;
        QMap<QTreeWidgetItem*, QString> m_itemToType;
        QMap<QTreeWidgetItem*, QString> m_itemToRef;
};

#endif // XENCACHEEXPLORER_H
