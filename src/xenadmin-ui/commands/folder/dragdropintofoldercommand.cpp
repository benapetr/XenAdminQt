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

#include "dragdropintofoldercommand.h"

#include "../../mainwindow.h"
#include "xenlib/folders/foldersmanager.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/actions/delegatedasyncoperation.h"
#include "xenlib/xen/actions/general/generaleditpageaction.h"
#include <QSet>
#include <algorithm>

namespace
{
    QString folderPathForObject(const QSharedPointer<XenObject>& obj)
    {
        if (!obj)
            return QString();

        if (obj->GetObjectType() == XenObjectType::Folder)
            return obj->OpaqueRef();

        return obj->GetFolderPath();
    }

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

DragDropIntoFolderCommand::DragDropIntoFolderCommand(MainWindow* mainWindow, const QString& targetFolderPath, QObject* parent)
    : Command(mainWindow, parent),
      m_targetFolderPath(targetFolderPath)
{
}

bool DragDropIntoFolderCommand::CanRun() const
{
    const QList<QSharedPointer<XenObject>> selected = this->getSelectedObjects();
    if (selected.isEmpty())
        return false;

    return this->draggedObjectsAreValid(selected);
}

void DragDropIntoFolderCommand::Run()
{
    if (!this->CanRun())
        return;

    QList<AsyncOperation*> actions;
    const QList<QSharedPointer<XenObject>> selected = this->getSelectedObjects();

    QSet<QString> handledObjects;

    for (const QSharedPointer<XenObject>& obj : selected)
    {
        if (!obj)
            continue;

        if (obj->GetObjectType() == XenObjectType::Folder)
        {
            XenConnection* connection = obj->GetConnection();
            if (!connection || !connection->GetCache())
                continue;

            const QString oldFolderPath = obj->OpaqueRef();
            const QString folderName = folderNameFromPath(oldFolderPath);
            if (folderName.isEmpty())
                continue;

            const QString newFolderPath = FoldersManager::AppendPath(m_targetFolderPath, folderName);
            if (newFolderPath == oldFolderPath)
                continue;

            const QList<QPair<XenObjectType, QString>> searchable = connection->GetCache()->GetXenSearchableObjects();
            for (const auto& pair : searchable)
            {
                if (pair.first == XenObjectType::Folder)
                    continue;

                QSharedPointer<XenObject> candidate = connection->GetCache()->ResolveObject(pair.first, pair.second);
                if (!candidate)
                    continue;

                const QString currentPath = candidate->GetFolderPath();
                const QString updatedPath = replaceFolderPrefix(currentPath, oldFolderPath, newFolderPath);
                if (updatedPath == currentPath)
                    continue;

                const QString key = QStringLiteral("%1:%2:%3")
                                        .arg(reinterpret_cast<quintptr>(connection))
                                        .arg(static_cast<int>(candidate->GetObjectType()))
                                        .arg(candidate->OpaqueRef());
                if (handledObjects.contains(key))
                    continue;
                handledObjects.insert(key);

                actions.append(new GeneralEditPageAction(candidate, currentPath, updatedPath, candidate->GetTags(), candidate->GetTags(), false, this->mainWindow(), true));
            }

            actions.append(new DelegatedAsyncOperation(connection,
                                                       QObject::tr("Move Folder"),
                                                       QObject::tr("Moving folder '%1'...").arg(folderName),
                                                       [connection, oldFolderPath, newFolderPath](DelegatedAsyncOperation*)
                                                       {
                                                           QStringList descendants = FoldersManager::instance()->Descendants(connection, oldFolderPath);
                                                           std::sort(descendants.begin(), descendants.end(),
                                                                     [](const QString& a, const QString& b)
                                                                     {
                                                                         return a.size() < b.size();
                                                                     });

                                                           FoldersManager::instance()->CreateFolder(connection, newFolderPath);
                                                           for (const QString& oldDescendant : descendants)
                                                           {
                                                               const QString newDescendant = replaceFolderPrefix(oldDescendant, oldFolderPath, newFolderPath);
                                                               FoldersManager::instance()->CreateFolder(connection, newDescendant);
                                                           }
                                                           FoldersManager::instance()->DeleteFolder(connection, oldFolderPath);
                                                       },
                                                       this->mainWindow()));
            continue;
        }

        const QString currentPath = obj->GetFolderPath();
        if (currentPath == m_targetFolderPath)
            continue;

        const QString key = QStringLiteral("%1:%2:%3")
                                .arg(reinterpret_cast<quintptr>(obj->GetConnection()))
                                .arg(static_cast<int>(obj->GetObjectType()))
                                .arg(obj->OpaqueRef());
        if (handledObjects.contains(key))
            continue;
        handledObjects.insert(key);

        actions.append(new GeneralEditPageAction(obj, currentPath, this->m_targetFolderPath, obj->GetTags(), obj->GetTags(), false, this->mainWindow(), true));
    }

    if (!actions.isEmpty())
    {
        this->RunMultipleActions(actions, QObject::tr("Move To Folder"), QObject::tr("Moving objects to folder..."), QObject::tr("Objects moved to folder"), true, true);
    }
}

QString DragDropIntoFolderCommand::MenuText() const
{
    return QObject::tr("Move To Folder");
}

bool DragDropIntoFolderCommand::draggedObjectsAreValid(const QList<QSharedPointer<XenObject>>& selected) const
{
    if (m_targetFolderPath.isEmpty())
        return false;

    if (m_targetFolderPath == "/")
    {
        for (const QSharedPointer<XenObject>& obj : selected)
        {
            if (obj && obj->GetObjectType() != XenObjectType::Folder)
                return false;
        }
    }

    QString commonParent;
    bool allAlreadyInTarget = true;

    for (const QSharedPointer<XenObject>& obj : selected)
    {
        if (!obj)
            return false;

        const QString currentPath = folderPathForObject(obj);
        if (currentPath != m_targetFolderPath)
            allAlreadyInTarget = false;

        if (obj->GetObjectType() != XenObjectType::Folder)
            continue;

        const QString folderPath = obj->OpaqueRef();
        if (folderPath.isEmpty() || folderPath == "/")
            return false;

        if (folderPath == m_targetFolderPath)
            return false;

        const QString parent = FoldersManager::GetParent(folderPath);
        if (m_targetFolderPath == parent)
            return false;

        if (m_targetFolderPath.startsWith(folderPath + "/"))
            return false;

        if (!commonParent.isEmpty() && commonParent != parent)
            return false;

        commonParent = parent;
    }

    return !allAlreadyInTarget;
}
