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

#include "command.h"
#include "../mainwindow.h"
#include "../selectionmanager.h"
#include "operations/multipleactionlauncher.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/network/connection.h"
#include "xen/asyncoperation.h"
#include <QTreeWidget>
#include <QList>

Command::Command(MainWindow* mainWindow, QObject* parent) : QObject(parent), m_mainWindow(mainWindow)
{
}

Command::Command(MainWindow* mainWindow, const QStringList& selection, QObject* parent) : QObject(parent), m_mainWindow(mainWindow), m_selection(selection)
{
}

QIcon Command::GetIcon() const
{
    return QIcon(":/icons/empty_icon.png");
}

QSharedPointer<XenObject> Command::GetObject() const
{
    if (!this->m_selectionOverride.isEmpty())
        return this->m_selectionOverride.first();

    SelectionManager* selection = this->GetSelectionManager();
    if (selection)
        return selection->PrimaryObject();

    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QSharedPointer<XenObject>();

    QVariant data = item->data(0, Qt::UserRole);
    return data.value<QSharedPointer<XenObject>>();
}

SelectionManager* Command::GetSelectionManager() const
{
    return this->m_mainWindow ? this->m_mainWindow->GetSelectionManager() : nullptr;
}

QTreeWidgetItem* Command::getSelectedItem() const
{
    if (!this->m_selectionOverride.isEmpty())
        return nullptr;

    SelectionManager* selection = this->GetSelectionManager();
    if (selection)
        return selection->PrimaryItem();

    if (!this->m_mainWindow)
        return nullptr;

    // Get the server tree widget from main window
    QTreeWidget* treeWidget = MainWindow::instance()->GetServerTreeWidget();
    if (!treeWidget)
        return nullptr;

    QList<QTreeWidgetItem*> selectedItems = treeWidget->selectedItems();
    if (selectedItems.isEmpty())
        return nullptr;

    return selectedItems.first();
}

QString Command::getSelectedObjectRef() const
{
    if (!this->m_selectionOverride.isEmpty())
    {
        QSharedPointer<XenObject> obj = this->m_selectionOverride.first();
        return obj ? obj->OpaqueRef() : QString();
    }

    SelectionManager* selection = this->GetSelectionManager();
    if (selection)
    {
        QSharedPointer<XenObject> obj = selection->PrimaryObject();
        return obj ? obj->OpaqueRef() : QString();
    }

    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    QVariant data = item->data(0, Qt::UserRole);
    QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
    if (obj)
        return obj->OpaqueRef();

    return QString();
}

QString Command::getSelectedObjectName() const
{
    if (!this->m_selectionOverride.isEmpty())
    {
        QSharedPointer<XenObject> obj = this->m_selectionOverride.first();
        return obj ? obj->GetName() : QString();
    }

    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    return item->text(0);
}

XenObjectType Command::getSelectedObjectType() const
{
    if (!this->m_selectionOverride.isEmpty())
    {
        return this->m_selectionOverride.first()->GetObjectType();
    }
    SelectionManager* selection = this->GetSelectionManager();
    if (selection)
        return selection->SelectionType();

    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return XenObjectType::Null;

    QVariant data = item->data(0, Qt::UserRole);
    if (data.canConvert<QSharedPointer<XenObject>>())
    {
        QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
        if (obj)
            return obj->GetObjectType();
    } else if (data.canConvert<XenConnection*>())
    {
        return XenObjectType::DisconnectedHost;
    }

    return XenObjectType::Null;
}

QSharedPointer<XenObject> Command::getSelectedObject() const
{
    if (!this->m_selectionOverride.isEmpty())
        return this->m_selectionOverride.first();

    SelectionManager* selection = this->GetSelectionManager();
    if (selection)
        return selection->PrimaryObject();

    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QSharedPointer<XenObject>();

    QVariant data = item->data(0, Qt::UserRole);
    if (data.canConvert<QSharedPointer<XenObject>>())
        return data.value<QSharedPointer<XenObject>>();

    return QSharedPointer<XenObject>();
}

void Command::SetSelectionOverride(const QList<QSharedPointer<XenObject>>& objects)
{
    this->m_selectionOverride = objects;
}

QList<QSharedPointer<XenObject>> Command::getSelectedObjects() const
{
    if (!this->m_selectionOverride.isEmpty())
        return this->m_selectionOverride;

    SelectionManager* selection = this->GetSelectionManager();
    if (selection)
        return selection->SelectedObjects();

    QSharedPointer<XenObject> obj = this->getSelectedObject();
    if (obj)
        return { obj };

    return {};
}

void Command::RunMultipleActions(const QList<AsyncOperation*>& actions, const QString& title, const QString& startDescription, const QString& endDescription, bool runActionsInParallel)
{
    MultipleActionLauncher launcher(actions, title, startDescription, endDescription, runActionsInParallel, nullptr);
    launcher.Run();
}
