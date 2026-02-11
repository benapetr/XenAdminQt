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

#include "newfoldercommand.h"

#include <QInputDialog>
#include <QTreeWidget>
#include <stdexcept>

#include "../../mainwindow.h"
#include "xenlib/folders/foldersmanager.h"
#include "xenlib/xensearch/groupingtag.h"
#include "xenlib/xensearch/grouping.h"
#include "xenlib/xen/network/connectionsmanager.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/folder.h"
#include "xenlib/xen/actions/delegatedasyncoperation.h"

NewFolderCommand::NewFolderCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

bool NewFolderCommand::CanRun() const
{
    return this->firstConnectedConnection() != nullptr;
}

void NewFolderCommand::Run()
{
    QString baseFolderPath = FoldersManager::PATH_SEPARATOR;
    XenConnection* connection = nullptr;
    if (!this->resolveTargetFolder(baseFolderPath, connection) || !connection)
        return;

    bool ok = false;
    const QString input = QInputDialog::getText(this->mainWindow(), QObject::tr("New Folder"),
                                                QObject::tr("Folder name (or ';' separated list):"),
                                                QLineEdit::Normal,
                                                QString(),
                                                &ok);
    if (!ok)
        return;

    const QStringList parts = input.split(';', Qt::SkipEmptyParts);
    QStringList paths;
    for (const QString& part : parts)
    {
        const QString fixed = FoldersManager::FixupRelativePath(part);
        if (!fixed.isEmpty())
            paths.append(FoldersManager::AppendPath(baseFolderPath, fixed));
    }

    if (paths.isEmpty())
        return;

    QList<AsyncOperation*> actions;
    actions.append(new DelegatedAsyncOperation(connection,
                                               QObject::tr("Create Folder"),
                                               QObject::tr("Creating folder(s)..."),
                                               [connection, paths](DelegatedAsyncOperation* self)
                                               {
                                                   for (const QString& path : paths)
                                                   {
                                                       if (self->IsCancelled())
                                                           return;
                                                       const bool created = FoldersManager::instance()->CreateFolder(connection, path);
                                                       if (!created)
                                                           throw std::runtime_error(QString("Failed to create folder '%1'").arg(path).toStdString());
                                                   }
                                               },
                                               this->mainWindow()));

    this->RunMultipleActions(actions,
                             QObject::tr("Create Folder"),
                             QObject::tr("Creating folder(s)..."),
                             QObject::tr("Folder creation complete"),
                             true);
}

QString NewFolderCommand::MenuText() const
{
    return QObject::tr("New Folder...");
}

QIcon NewFolderCommand::GetIcon() const
{
    return QIcon(":/icons/folder_16.png");
}

XenConnection* NewFolderCommand::firstConnectedConnection()
{
    const QList<XenConnection*> connections = Xen::ConnectionsManager::instance()->GetAllConnections();
    for (XenConnection* connection : connections)
    {
        if (connection && connection->IsConnected())
            return connection;
    }

    return nullptr;
}

bool NewFolderCommand::resolveTargetFolder(QString& baseFolderPath, XenConnection*& connection) const
{
    baseFolderPath = FoldersManager::PATH_SEPARATOR;
    connection = nullptr;

    QTreeWidget* tree = this->mainWindow() ? this->mainWindow()->GetServerTreeWidget() : nullptr;
    QTreeWidgetItem* selectedItem = (tree && tree->selectedItems().size() == 1) ? tree->selectedItems().first() : nullptr;

    if (selectedItem)
    {
        QVariant objVar = selectedItem->data(0, Qt::UserRole);
        if (objVar.canConvert<QSharedPointer<XenObject>>())
        {
            const QSharedPointer<XenObject> obj = objVar.value<QSharedPointer<XenObject>>();
            if (obj && obj->GetObjectType() == XenObjectType::Folder)
            {
                baseFolderPath = obj->OpaqueRef();
                connection = obj->GetConnection();
            }
        }

        if (!connection)
        {
            const QVariant groupTagVar = selectedItem->data(0, Qt::UserRole + 3);
            if (groupTagVar.canConvert<GroupingTag*>())
            {
                GroupingTag* gt = groupTagVar.value<GroupingTag*>();
                if (gt && dynamic_cast<FolderGrouping*>(gt->getGrouping()))
                {
                    const QString groupPath = gt->getGroup().toString().trimmed();
                    if (!groupPath.isEmpty() &&
                        groupPath.compare(QStringLiteral("Folders"), Qt::CaseInsensitive) != 0 &&
                        groupPath.startsWith(FoldersManager::PATH_SEPARATOR))
                    {
                        baseFolderPath = groupPath;
                    }
                }
            }
        }
    }

    if (connection && connection->IsConnected())
        return true;

    QList<XenConnection*> candidates;
    const QList<XenConnection*> connections = Xen::ConnectionsManager::instance()->GetAllConnections();
    for (XenConnection* conn : connections)
    {
        if (conn && conn->IsConnected())
            candidates.append(conn);
    }

    if (candidates.isEmpty())
        return false;

    connection = this->chooseConnection(candidates);
    return connection != nullptr;
}

XenConnection* NewFolderCommand::chooseConnection(const QList<XenConnection*>& candidates) const
{
    if (candidates.isEmpty())
        return nullptr;
    if (candidates.size() == 1)
        return candidates.first();

    QStringList choices;
    QHash<QString, XenConnection*> byLabel;
    for (XenConnection* connection : candidates)
    {
        if (!connection)
            continue;

        const QString base = connection->GetHostname().trimmed().isEmpty() ? QObject::tr("Connection") : connection->GetHostname().trimmed();
        QString label = base;
        int suffix = 2;
        while (byLabel.contains(label))
            label = QStringLiteral("%1 (%2)").arg(base).arg(suffix++);
        byLabel.insert(label, connection);
        choices.append(label);
    }

    bool ok = false;
    const QString chosen = QInputDialog::getItem(this->mainWindow(),
                                                 QObject::tr("Select Connection"),
                                                 QObject::tr("Create folder on:"),
                                                 choices,
                                                 0,
                                                 false,
                                                 &ok);
    if (!ok || chosen.isEmpty())
        return nullptr;

    return byLabel.value(chosen, nullptr);
}
