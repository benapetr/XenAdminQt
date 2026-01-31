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

#include "selectionmanager.h"
#include "mainwindow.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/network/connection.h"
#include <QTreeWidget>
#include <QSet>

SelectionManager::SelectionManager(MainWindow* mainWindow, QObject* parent) : QObject(parent), m_mainWindow(mainWindow)
{
    if (QTreeWidget* tree = this->treeWidget())
    {
        connect(tree, &QTreeWidget::itemSelectionChanged, this, &SelectionManager::onSelectionChanged);
        connect(tree, &QTreeWidget::currentItemChanged, this, &SelectionManager::onSelectionChanged);
    }
}

QTreeWidget* SelectionManager::treeWidget() const
{
    return this->m_mainWindow ? this->m_mainWindow->GetServerTreeWidget() : nullptr;
}

void SelectionManager::onSelectionChanged()
{
    emit SelectionChanged();
}

QTreeWidgetItem* SelectionManager::PrimaryItem() const
{
    QTreeWidget* tree = this->treeWidget();
    return tree ? tree->currentItem() : nullptr;
}

QList<QTreeWidgetItem*> SelectionManager::SelectedItems() const
{
    QTreeWidget* tree = this->treeWidget();
    return tree ? tree->selectedItems() : QList<QTreeWidgetItem*>();
}

QSharedPointer<XenObject> SelectionManager::PrimaryObject() const
{
    QTreeWidgetItem* item = this->PrimaryItem();
    if (!item)
        return QSharedPointer<XenObject>();

    QVariant data = item->data(0, Qt::UserRole);
    if (!data.canConvert<QSharedPointer<XenObject>>())
        return QSharedPointer<XenObject>();

    return data.value<QSharedPointer<XenObject>>();
}

XenObjectType SelectionManager::PrimaryType() const
{
    QTreeWidgetItem* item = this->PrimaryItem();
    if (!item)
        return XenObjectType::Null;

    QVariant data = item->data(0, Qt::UserRole);
    if (data.canConvert<QSharedPointer<XenObject>>())
    {
        QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
        return obj ? obj->GetObjectType() : XenObjectType::Null;
    }

    if (data.canConvert<XenConnection*>())
        return XenObjectType::DisconnectedHost;

    return XenObjectType::Null;
}

XenObjectType SelectionManager::SelectionType() const
{
    const QList<XenObjectType> types = this->SelectedTypes();
    if (types.size() == 1)
        return types.first();
    return XenObjectType::Null;
}

QList<QSharedPointer<XenObject>> SelectionManager::SelectedObjects() const
{
    QList<QSharedPointer<XenObject>> objects;
    const QList<QTreeWidgetItem*> items = this->SelectedItems();

    for (QTreeWidgetItem* item : items)
    {
        if (!item)
            continue;

        QVariant data = item->data(0, Qt::UserRole);
        if (!data.canConvert<QSharedPointer<XenObject>>())
            continue;

        QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
        if (obj)
            objects.append(obj);
    }

    return objects;
}

QList<QSharedPointer<XenObject>> SelectionManager::SelectedObjectsByType(XenObjectType objectType) const
{
    QList<QSharedPointer<XenObject>> objects;
    const QList<QSharedPointer<XenObject>> allObjects = this->SelectedObjects();

    for (const QSharedPointer<XenObject>& obj : allObjects)
    {
        if (obj && obj->GetObjectType() == objectType)
            objects.append(obj);
    }

    return objects;
}

QList<XenObjectType> SelectionManager::SelectedTypes() const
{
    QSet<XenObjectType> types;
    const QList<QSharedPointer<XenObject>> objects = this->SelectedObjects();
    for (const QSharedPointer<XenObject>& obj : objects)
    {
        if (obj)
            types.insert(obj->GetObjectType());
    }

    return types.values();
}

SelectionManager::SelectionKind SelectionManager::SelectionKindValue() const
{
    const int count = this->SelectedItems().size();
    if (count <= 0)
        return SelectionKind::None;
    if (count == 1)
        return SelectionKind::Single;
    return SelectionKind::Multiple;
}

bool SelectionManager::HasSelection() const
{
    return !this->SelectedItems().isEmpty();
}

bool SelectionManager::HasMultipleSelection() const
{
    return this->SelectedItems().size() > 1;
}

QList<QSharedPointer<VM>> SelectionManager::SelectedVMs() const
{
    QList<QSharedPointer<VM>> vms;
    const QList<QTreeWidgetItem*> items = this->SelectedItems();

    for (QTreeWidgetItem* item : items)
    {
        if (!item)
            continue;

        QVariant data = item->data(0, Qt::UserRole);
        if (!data.canConvert<QSharedPointer<XenObject>>())
            continue;

        QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
        if (!obj || obj->GetObjectType() != XenObjectType::VM)
            continue;

        QSharedPointer<VM> vm = qSharedPointerDynamicCast<VM>(obj);
        if (vm)
            vms.append(vm);
    }

    return vms;
}

QList<QSharedPointer<Host>> SelectionManager::SelectedHosts() const
{
    QList<QSharedPointer<Host>> hosts;
    const QList<QTreeWidgetItem*> items = this->SelectedItems();

    for (QTreeWidgetItem* item : items)
    {
        if (!item)
            continue;

        QVariant data = item->data(0, Qt::UserRole);
        if (!data.canConvert<QSharedPointer<XenObject>>())
            continue;

        QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
        if (!obj || obj->GetObjectType() != XenObjectType::Host)
            continue;

        QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(obj);
        if (host)
            hosts.append(host);
    }

    return hosts;
}

QList<XenConnection*> SelectionManager::SelectedConnections() const
{
    QList<XenConnection*> connections;
    const QList<QTreeWidgetItem*> items = this->SelectedItems();

    for (QTreeWidgetItem* item : items)
    {
        if (!item)
            continue;

        QVariant data = item->data(0, Qt::UserRole);
        if (data.canConvert<XenConnection*>())
        {
            XenConnection* connection = data.value<XenConnection*>();
            if (connection)
                connections.append(connection);
        }
    }

    return connections;
}
