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

#include "deletefoldercommand.h"

#include <QMessageBox>
#include <QTreeWidget>
#include <QSet>

#include "../../mainwindow.h"
#include "xenlib/xensearch/grouping.h"
#include "xenlib/xensearch/groupingtag.h"
#include "xenlib/folders/foldersmanager.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/folder.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/network/connectionsmanager.h"
#include "xenlib/xen/actions/delegatedasyncoperation.h"
#include "xenlib/xen/actions/general/generaleditpageaction.h"

namespace
{
    QList<QSharedPointer<XenObject>> collectObjectsInPath(XenConnection* connection, const QString& folderPath)
    {
        QList<QSharedPointer<XenObject>> objects;
        if (!connection || !connection->GetCache())
            return objects;

        const QList<QPair<XenObjectType, QString>> searchable = connection->GetCache()->GetXenSearchableObjects();
        for (const auto& pair : searchable)
        {
            if (pair.first == XenObjectType::Folder)
                continue;

            QSharedPointer<XenObject> obj = connection->GetCache()->ResolveObject(pair.first, pair.second);
            if (!obj)
                continue;

            const QString objectPath = obj->GetFolderPath();
            if (objectPath == folderPath || objectPath.startsWith(folderPath + FoldersManager::PATH_SEPARATOR))
                objects.append(obj);
        }

        return objects;
    }
}

DeleteFolderCommand::DeleteFolderCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool DeleteFolderCommand::CanRun() const
{
    const QStringList paths = this->selectedFolderPaths();
    return !paths.isEmpty();
}

void DeleteFolderCommand::Run()
{
    const QStringList paths = this->selectedFolderPaths();
    if (paths.isEmpty())
        return;

    if (QMessageBox::question(this->mainWindow(), QObject::tr("Delete Folder"),
                              paths.size() == 1
                                  ? QObject::tr("Delete folder '%1' and remove all folder assignments below it?").arg(paths.first())
                                  : QObject::tr("Delete %1 selected folders and remove all folder assignments below them?").arg(paths.size()),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No)
        != QMessageBox::Yes)
    {
        return;
    }

    QList<AsyncOperation*> actions;
    QSet<QString> handledObjects;

    // Move all objects out of deleted folders (C# parity: delete folder clears/moves assignments).
    const QList<XenConnection*> connections = Xen::ConnectionsManager::instance()->GetAllConnections();
    for (XenConnection* connection : connections)
    {
        if (!connection || !connection->GetCache())
            continue;

        for (const QString& path : paths)
        {
            const QList<QSharedPointer<XenObject>> objs = collectObjectsInPath(connection, path);
            for (const QSharedPointer<XenObject>& obj : objs)
            {
                const QString key = QString("%1:%2:%3").arg(reinterpret_cast<quintptr>(connection)).arg(static_cast<int>(obj->GetObjectType())).arg(obj->OpaqueRef());
                if (handledObjects.contains(key))
                    continue;
                handledObjects.insert(key);
                actions.append(new GeneralEditPageAction(obj, obj->GetFolderPath(), QString(), obj->GetTags(), obj->GetTags(), false, this->mainWindow(), true));
            }
        }
    }

    for (const QString& path : paths)
    {
        auto* deleteFolderOp = new DelegatedAsyncOperation(QObject::tr("Delete Folder"),
                                                           QObject::tr("Deleting folder '%1'...").arg(path),
                                                           [path](DelegatedAsyncOperation*)
                                                           {
                                                               XenConnection* conn = nullptr;
                                                               const QList<XenConnection*> connections = Xen::ConnectionsManager::instance()->GetAllConnections();
                                                               for (XenConnection* c : connections)
                                                               {
                                                                   if (!c || !c->GetCache())
                                                                       continue;
                                                                   if (!c->GetCache()->ResolveObjectData(XenObjectType::Folder, path).isEmpty())
                                                                   {
                                                                       conn = c;
                                                                       break;
                                                                   }
                                                               }
                                                               if (conn)
                                                                   FoldersManager::instance()->DeleteFolder(conn, path);
                                                           },
                                                           nullptr);
        actions.append(deleteFolderOp);
    }

    this->RunMultipleActions(actions, QObject::tr("Delete Folders"), QObject::tr("Deleting selected folders..."), QObject::tr("Folder deletion complete"), true, true);
}

QString DeleteFolderCommand::MenuText() const
{
    return QObject::tr("Delete Folder");
}

QStringList DeleteFolderCommand::selectedFolderPaths() const
{
    QStringList result;
    QTreeWidget* tree = this->mainWindow() ? this->mainWindow()->GetServerTreeWidget() : nullptr;
    if (!tree)
        return result;

    const QList<QTreeWidgetItem*> items = tree->selectedItems();
    for (QTreeWidgetItem* item : items)
    {
        if (!item)
            continue;

        QVariant data = item->data(0, Qt::UserRole);
        if (data.canConvert<QSharedPointer<XenObject>>())
        {
            QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
            if (obj && obj->GetObjectType() == XenObjectType::Folder)
            {
                const QString path = obj->OpaqueRef();
                if (!path.isEmpty() && path != FoldersManager::PATH_SEPARATOR)
                    result.append(path);
                continue;
            }
        }

        const QVariant groupTagVar = item->data(0, Qt::UserRole + 3);
        if (groupTagVar.canConvert<GroupingTag*>())
        {
            GroupingTag* gt = groupTagVar.value<GroupingTag*>();
            if (gt && dynamic_cast<FolderGrouping*>(gt->getGrouping()))
            {
                const QString path = gt->getGroup().toString();
                if (path.startsWith(FoldersManager::PATH_SEPARATOR) && path != FoldersManager::PATH_SEPARATOR)
                    result.append(path);
            }
        }
    }

    result.removeDuplicates();
    return result;
}
