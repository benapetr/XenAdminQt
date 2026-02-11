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

#include "renamefoldercommand.h"

#include <QInputDialog>
#include <QTreeWidget>

#include "../../mainwindow.h"
#include "xenlib/xensearch/grouping.h"
#include "xenlib/xensearch/groupingtag.h"
#include "xenlib/folders/foldersmanager.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/network/connectionsmanager.h"
#include "xenlib/xen/actions/general/generaleditpageaction.h"
#include "xenlib/xen/actions/delegatedasyncoperation.h"

namespace
{
    QString folderNameFromPath(const QString& path)
    {
        const QStringList parts = FoldersManager::PointToPath(path);
        return parts.isEmpty() ? QString() : parts.last();
    }

    QString replaceFolderPrefix(const QString& originalPath, const QString& oldPrefix, const QString& newPrefix)
    {
        if (originalPath == oldPrefix)
            return newPrefix;
        if (!originalPath.startsWith(oldPrefix + FoldersManager::PATH_SEPARATOR))
            return originalPath;

        return newPrefix + originalPath.mid(oldPrefix.length());
    }
}

RenameFolderCommand::RenameFolderCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

bool RenameFolderCommand::CanRun() const
{
    QString path;
    XenConnection* connection = nullptr;
    return this->selectedFolder(path, connection);
}

void RenameFolderCommand::Run()
{
    QString oldPath;
    XenConnection* connection = nullptr;
    if (!this->selectedFolder(oldPath, connection))
        return;

    bool ok = false;
    const QString oldName = folderNameFromPath(oldPath);
    QString newName = QInputDialog::getText(this->mainWindow(),
                                            QObject::tr("Rename Folder"),
                                            QObject::tr("New folder name:"),
                                            QLineEdit::Normal,
                                            oldName,
                                            &ok).trimmed();
    if (!ok)
        return;

    newName = FoldersManager::FixupRelativePath(newName);
    if (newName.isEmpty() || newName == oldName || newName.contains(';') || newName.contains('/'))
        return;

    const QString parentPath = FoldersManager::GetParent(oldPath);
    const QString newPath = parentPath.isEmpty()
        ? FoldersManager::PATH_SEPARATOR + newName
        : FoldersManager::AppendPath(parentPath, newName);

    if (!connection || !connection->GetCache())
        return;

    QList<AsyncOperation*> actions;
    const QList<QPair<XenObjectType, QString>> searchable = connection->GetCache()->GetXenSearchableObjects();
    for (const auto& pair : searchable)
    {
        if (pair.first == XenObjectType::Folder)
            continue;

        QSharedPointer<XenObject> obj = connection->GetCache()->ResolveObject(pair.first, pair.second);
        if (!obj)
            continue;

        const QString currentPath = obj->GetFolderPath();
        const QString updatedPath = replaceFolderPrefix(currentPath, oldPath, newPath);
        if (updatedPath == currentPath)
            continue;

        actions.append(new GeneralEditPageAction(obj,
                                                 currentPath,
                                                 updatedPath,
                                                 obj->GetTags(),
                                                 obj->GetTags(),
                                                 false,
                                                 this->mainWindow()));
    }

    actions.append(new DelegatedAsyncOperation(connection,
                                               QObject::tr("Rename Folder"),
                                               QObject::tr("Renaming folder..."),
                                               [connection, oldPath, newPath](DelegatedAsyncOperation*)
                                               {
                                                   FoldersManager::instance()->CreateFolder(connection, newPath);
                                                   FoldersManager::instance()->DeleteFolder(connection, oldPath);
                                               },
                                               this->mainWindow()));

    this->RunMultipleActions(actions,
                             QObject::tr("Rename Folder"),
                             QObject::tr("Renaming folder..."),
                             QObject::tr("Folder renamed"),
                             true);
}

QString RenameFolderCommand::MenuText() const
{
    return QObject::tr("Rename Folder...");
}

bool RenameFolderCommand::selectedFolder(QString& path, XenConnection*& connection) const
{
    path.clear();
    connection = nullptr;

    QTreeWidget* tree = this->mainWindow() ? this->mainWindow()->GetServerTreeWidget() : nullptr;
    if (!tree)
        return false;

    const QList<QTreeWidgetItem*> items = tree->selectedItems();
    if (items.size() != 1)
        return false;

    QTreeWidgetItem* item = items.first();
    QVariant data = item->data(0, Qt::UserRole);
    if (data.canConvert<QSharedPointer<XenObject>>())
    {
        QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
        if (obj && obj->GetObjectType() == XenObjectType::Folder)
        {
            path = obj->OpaqueRef();
            connection = obj->GetConnection();
            return !path.isEmpty() && path != FoldersManager::PATH_SEPARATOR;
        }
    }

    const QVariant groupTagVar = item->data(0, Qt::UserRole + 3);
    if (!groupTagVar.canConvert<GroupingTag*>())
        return false;

    GroupingTag* gt = groupTagVar.value<GroupingTag*>();
    if (!gt || !dynamic_cast<FolderGrouping*>(gt->getGrouping()))
        return false;

    path = gt->getGroup().toString();
    if (path.isEmpty() || path == FoldersManager::PATH_SEPARATOR)
        return false;

    // Resolve owning connection for this folder path.
    const QList<XenConnection*> connections = Xen::ConnectionsManager::instance()->GetAllConnections();
    for (XenConnection* conn : connections)
    {
        if (!conn || !conn->GetCache())
            continue;
        if (!conn->GetCache()->ResolveObjectData(XenObjectType::Folder, path).isEmpty())
        {
            connection = conn;
            break;
        }
    }

    return connection != nullptr;
}
