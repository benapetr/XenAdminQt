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

#include "xencacheexplorer.h"
#include "ui_xencacheexplorer.h"
#include "xen/network/connectionsmanager.h"
#include "xen/network/connection.h"
#include "xen/session.h"
#include "xencache.h"
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QDebug>
#include <QMenu>
#include <QClipboard>
#include <QApplication>

XenCacheExplorer::XenCacheExplorer(QWidget* parent) : QDialog(parent), ui(new Ui::XenCacheExplorer)
{
    this->ui->setupUi(this);
    this->setWindowTitle(tr("XenCache Explorer - Debug Tool"));
    this->resize(1000, 700);

    // Setup tree widget
    this->ui->cacheTree->setHeaderLabels({tr("Cache Structure")});
    if (this->ui->cacheTree->header())
        this->ui->cacheTree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    // Setup properties tree
    this->ui->propertiesTree->setColumnCount(3);
    this->ui->propertiesTree->setHeaderLabels({tr("Property"), tr("Value"), tr("Type")});
    if (this->ui->propertiesTree->header())
    {
        this->ui->propertiesTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        this->ui->propertiesTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        this->ui->propertiesTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    }
    
    // Enable context menu for properties tree
    this->ui->propertiesTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this->ui->propertiesTree, &QTreeWidget::customContextMenuRequested,
            this, &XenCacheExplorer::onPropertiesTreeContextMenu);

    // Set splitter proportions (30% tree, 70% properties)
    this->ui->splitter->setStretchFactor(0, 3);
    this->ui->splitter->setStretchFactor(1, 7);

    connect(this->ui->cacheTree, &QTreeWidget::itemSelectionChanged, this, &XenCacheExplorer::onTreeItemSelectionChanged);

    this->populateTree();
}

XenCacheExplorer::~XenCacheExplorer()
{
    delete this->ui;
}

void XenCacheExplorer::populateTree()
{
    this->ui->cacheTree->clear();
    this->m_itemToConnection.clear();
    this->m_itemToType.clear();
    this->m_itemToRef.clear();

    QList<XenConnection*> connections = Xen::ConnectionsManager::instance()->GetAllConnections();
    
    if (connections.isEmpty())
    {
        QTreeWidgetItem* emptyItem = new QTreeWidgetItem(this->ui->cacheTree);
        emptyItem->setText(0, tr("No active connections"));
        emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsSelectable);
        return;
    }

    for (XenConnection* connection : connections)
    {
        if (!connection)
            continue;

        QString connectionName = connection->GetHostname();
        if (connectionName.isEmpty())
            connectionName = tr("Unknown Connection");

        QTreeWidgetItem* connectionItem = new QTreeWidgetItem(this->ui->cacheTree);
        connectionItem->setText(0, QString("%1 (%2)")
                                .arg(connectionName)
                                .arg(connection->IsConnected() ? tr("Connected") : tr("Disconnected")));
        connectionItem->setIcon(0, QIcon(":/icons/server-16.png"));
        connectionItem->setData(0, Qt::UserRole, Type_Connection);
        
        this->m_itemToConnection[connectionItem] = connection;

        if (connection->IsConnected())
            this->populateConnectionNode(connectionItem, connection);
    }

    this->ui->cacheTree->collapseAll();
}

void XenCacheExplorer::populateConnectionNode(QTreeWidgetItem* connectionNode, XenConnection* connection)
{
    if (!connection || !connectionNode)
        return;

    XenCache* cache = connection->GetCache();
    if (!cache)
        return;

    // Get all types from cache itself
    QStringList types = cache->GetKnownTypes();
    
    for (const QString& type : types)
    {
        QList<QVariantMap> objects = cache->GetAllData(type);
        if (objects.isEmpty())
            continue;

        QTreeWidgetItem* typeItem = new QTreeWidgetItem(connectionNode);
        typeItem->setText(0, QString("%1 (%2)").arg(type).arg(objects.size()));
        typeItem->setIcon(0, QIcon(":/icons/folder-16.png"));
        typeItem->setData(0, Qt::UserRole, Type_Category);
        
        this->m_itemToConnection[typeItem] = connection;
        this->m_itemToType[typeItem] = type;

        // Add individual objects
        for (const QVariantMap& obj : objects)
        {
            QString opaqueRef = obj.value("ref").toString();
            if (opaqueRef.isEmpty())
                opaqueRef = obj.value("opaque_ref").toString();
            
            QString displayName = opaqueRef;
            
            // Try to get a friendly name
            if (obj.contains("name_label"))
            {
                QString nameLabel = obj.value("name_label").toString();
                if (!nameLabel.isEmpty())
                    displayName = QString("%1 (%2)").arg(nameLabel, opaqueRef);
            } else if (obj.contains("uuid"))
            {
                QString uuid = obj.value("uuid").toString();
                if (!uuid.isEmpty())
                    displayName = QString("%1 (%2)").arg(uuid, opaqueRef);
            }

            QTreeWidgetItem* objectItem = new QTreeWidgetItem(typeItem);
            objectItem->setText(0, displayName);
            objectItem->setIcon(0, QIcon(":/icons/object-16.png"));
            objectItem->setData(0, Qt::UserRole, Type_Object);
            
            this->m_itemToConnection[objectItem] = connection;
            this->m_itemToType[objectItem] = type;
            this->m_itemToRef[objectItem] = opaqueRef;
        }
    }
}

void XenCacheExplorer::onTreeItemSelectionChanged()
{
    QList<QTreeWidgetItem*> selectedItems = this->ui->cacheTree->selectedItems();
    if (selectedItems.isEmpty())
    {
        this->ui->propertiesTree->clear();
        this->ui->selectionLabel->setText(tr("Select an item to view properties"));
        this->ui->selectionLabel->setVisible(true);
        return;
    }

    QTreeWidgetItem* item = selectedItems.first();
    ItemType itemType = static_cast<ItemType>(item->data(0, Qt::UserRole).toInt());
    XenConnection* connection = this->m_itemToConnection.value(item);

    this->ui->selectionLabel->setVisible(false);

    switch (itemType)
    {
        case Type_Connection:
            this->displayConnectionInfo(connection);
            break;
        case Type_Category:
        {
            QString type = this->m_itemToType.value(item);
            this->displayCategoryInfo(connection, type);
            break;
        }
        case Type_Object:
        {
            QString type = this->m_itemToType.value(item);
            QString opaqueRef = this->m_itemToRef.value(item);
            this->displayObjectProperties(connection, type, opaqueRef);
            break;
        }
    }
}

void XenCacheExplorer::displayConnectionInfo(XenConnection* connection)
{
    if (!connection)
    {
        this->ui->propertiesTree->clear();
        return;
    }

    this->ui->propertiesTree->clear();

    QList<QPair<QString, QString>> info;
    info << qMakePair(tr("Hostname"), connection->GetHostname());
    info << qMakePair(tr("Port"), QString::number(connection->GetPort()));
    info << qMakePair(tr("Username"), connection->GetUsername());
    info << qMakePair(tr("Connected"), connection->IsConnected() ? tr("Yes") : tr("No"));
    
    if (connection->GetSession())
    {
        info << qMakePair(tr("Session ID"), connection->GetSession()->getSessionId());
        info << qMakePair(tr("Logged In"), connection->GetSession()->IsLoggedIn() ? tr("Yes") : tr("No"));
    }

    XenCache* cache = connection->GetCache();
    if (cache)
    {
        QStringList types = cache->GetKnownTypes();
        int nonEmptyTypes = 0;
        for (const QString& type : types)
        {
            if (!cache->GetAllData(type).isEmpty())
                nonEmptyTypes++;
        }
        info << qMakePair(tr("Cached Types"), QString::number(nonEmptyTypes));
        
        int totalObjects = 0;
        for (const QString& type : types)
            totalObjects += cache->GetAllData(type).size();
        
        info << qMakePair(tr("Total Cached Objects"), QString::number(totalObjects));
    }

    for (const auto& entry : info)
        this->addPropertyItem(nullptr, entry.first, entry.second, tr("Connection Info"));
}

void XenCacheExplorer::displayCategoryInfo(XenConnection* connection, const QString& type)
{
    if (!connection || type.isEmpty())
    {
        this->ui->propertiesTree->clear();
        return;
    }

    XenCache* cache = connection->GetCache();
    if (!cache)
    {
        this->ui->propertiesTree->clear();
        return;
    }

    QList<QVariantMap> objects = cache->GetAllData(type);
    
    this->addPropertyItem(nullptr, tr("Object Type"), type, tr("Category Info"));
    this->addPropertyItem(nullptr, tr("Object Count"), QString::number(objects.size()), tr("Category Info"));
    this->addPropertyItem(nullptr, tr("Connection"), connection->GetHostname(), tr("Category Info"));
}

void XenCacheExplorer::displayObjectProperties(XenConnection* connection, const QString& type, const QString& opaqueRef)
{
    if (!connection || type.isEmpty() || opaqueRef.isEmpty())
    {
        this->ui->propertiesTree->clear();
        return;
    }

    XenCache* cache = connection->GetCache();
    if (!cache)
    {
        this->ui->propertiesTree->clear();
        return;
    }

    QVariantMap objectData = cache->ResolveObjectData(type, opaqueRef);
    if (objectData.isEmpty())
    {
        this->ui->propertiesTree->clear();
        this->addPropertyItem(nullptr, tr("Object not found in cache"), "", "");
        return;
    }

    // Sort keys for consistent display
    QStringList keys = objectData.keys();
    keys.sort();

    this->ui->propertiesTree->clear();

    for (const QString& key : keys)
    {
        const QVariant& value = objectData.value(key);
        QTreeWidgetItem* item = this->addPropertyItem(nullptr,
                                                      key,
                                                      this->formatVariantValue(value),
                                                      this->getVariantTypeName(value));

        if (key == "ref" || key == "opaque_ref" || key == "uuid")
        {
            item->setForeground(0, Qt::green);
            item->setFont(0, QFont(item->font(0).family(), -1, QFont::Bold));
        }

        this->appendVariantChildren(item, value);
    }
}

QTreeWidgetItem* XenCacheExplorer::addPropertyItem(QTreeWidgetItem* parent,
                                                   const QString& name,
                                                   const QString& value,
                                                   const QString& type)
{
    QTreeWidgetItem* item = parent
        ? new QTreeWidgetItem(parent)
        : new QTreeWidgetItem(this->ui->propertiesTree);

    item->setText(0, name);
    item->setText(1, value);
    item->setText(2, type);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

void XenCacheExplorer::appendVariantChildren(QTreeWidgetItem* parent, const QVariant& value)
{
    if (!parent)
        return;

    if (value.userType() == QMetaType::QVariantMap)
    {
        QVariantMap map = value.toMap();
        QStringList keys = map.keys();
        keys.sort();
        for (const QString& key : keys)
        {
            const QVariant& childValue = map.value(key);
            QTreeWidgetItem* item = addPropertyItem(parent,
                                                    key,
                                                    formatVariantValue(childValue),
                                                    getVariantTypeName(childValue));
            appendVariantChildren(item, childValue);
        }
        return;
    }

    if (value.userType() == QMetaType::QVariantList)
    {
        QVariantList list = value.toList();
        for (int i = 0; i < list.size(); ++i)
        {
            const QVariant& childValue = list[i];
            QTreeWidgetItem* item = addPropertyItem(parent,
                                                    QString("[%1]").arg(i),
                                                    formatVariantValue(childValue),
                                                    getVariantTypeName(childValue));
            appendVariantChildren(item, childValue);
        }
        return;
    }

    if (value.userType() == QMetaType::QStringList)
    {
        QStringList list = value.toStringList();
        for (int i = 0; i < list.size(); ++i)
        {
            addPropertyItem(parent, QString("[%1]").arg(i), list[i], tr("String"));
        }
    }
}

QString XenCacheExplorer::formatVariantValue(const QVariant& value) const
{
    switch (value.userType())
    {
        case QMetaType::QVariantMap:
        {
            QVariantMap map = value.toMap();
            if (map.isEmpty())
                return tr("{}");
            return tr("{...} (%1 keys)").arg(map.size());
        }
        case QMetaType::QVariantList:
        {
            QVariantList list = value.toList();
            if (list.isEmpty())
                return tr("[]");
            return tr("[...] (%1 items)").arg(list.size());
        }
        case QMetaType::QStringList:
        {
            QStringList list = value.toStringList();
            if (list.isEmpty())
                return tr("[]");
            if (list.size() == 1)
                return QString("[%1]").arg(list.first());
            return tr("[...] (%1 items)").arg(list.size());
        }
        case QMetaType::Bool:
            return value.toBool() ? tr("true") : tr("false");
        case QMetaType::QString:
        {
            QString str = value.toString();
            if (str.length() > 100)
                return str.left(97) + "...";
            return str;
        }
        default:
            return value.toString();
    }
}

QString XenCacheExplorer::getVariantTypeName(const QVariant& value) const
{
    switch (value.userType())
    {
        case QMetaType::QVariantMap:
            return tr("Map");
        case QMetaType::QVariantList:
            return tr("List");
        case QMetaType::QStringList:
            return tr("String List");
        case QMetaType::Bool:
            return tr("Boolean");
        case QMetaType::Int:
        case QMetaType::LongLong:
            return tr("Integer");
        case QMetaType::Double:
            return tr("Double");
        case QMetaType::QString:
            return tr("String");
        default:
            return QString(value.typeName());
    }
}

void XenCacheExplorer::onRefreshClicked()
{
    this->populateTree();
}

void XenCacheExplorer::onPropertiesTreeContextMenu(const QPoint& pos)
{
    QList<QTreeWidgetItem*> selectedItems = this->ui->propertiesTree->selectedItems();
    if (selectedItems.isEmpty())
        return;

    QMenu contextMenu(this);
    QAction* copyValueAction = contextMenu.addAction(tr("Copy Selected Value"));
    QAction* copyRowsAction = contextMenu.addAction(tr("Copy Selected Row(s)"));
    
    connect(copyValueAction, &QAction::triggered, this, &XenCacheExplorer::onCopySelectedValue);
    connect(copyRowsAction, &QAction::triggered, this, &XenCacheExplorer::onCopySelectedRows);
    
    contextMenu.exec(this->ui->propertiesTree->mapToGlobal(pos));
}

void XenCacheExplorer::onCopySelectedValue()
{
    QList<QTreeWidgetItem*> selectedItems = this->ui->propertiesTree->selectedItems();
    if (selectedItems.isEmpty())
        return;

    QStringList values;
    for (QTreeWidgetItem* item : selectedItems)
    {
        // Copy the value column (column 1)
        QString value = item->text(1);
        if (!value.isEmpty())
            values << value;
    }

    if (!values.isEmpty())
    {
        QString text = values.join("\n");
        QApplication::clipboard()->setText(text);
    }
}

void XenCacheExplorer::onCopySelectedRows()
{
    QList<QTreeWidgetItem*> selectedItems = this->ui->propertiesTree->selectedItems();
    if (selectedItems.isEmpty())
        return;

    QStringList rows;
    for (QTreeWidgetItem* item : selectedItems)
    {
        // Copy all three columns: Property, Value, Type
        QString property = item->text(0);
        QString value = item->text(1);
        QString type = item->text(2);
        
        QString row = QString("%1: %2 (%3)").arg(property, value, type);
        rows << row;
    }

    if (!rows.isEmpty())
    {
        QString text = rows.join("\n");
        QApplication::clipboard()->setText(text);
    }
}
