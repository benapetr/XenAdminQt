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

#include "deletetagcommand.h"

#include <QMessageBox>
#include <QTreeWidget>

#include "../../mainwindow.h"
#include "xenlib/xen/network/connectionsmanager.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xensearch/groupingtag.h"
#include "xenlib/xensearch/grouping.h"
#include "xenlib/xen/actions/delegatedasyncoperation.h"
#include "xenlib/xen/api.h"
#include "xenlib/xen/session.h"

namespace
{
    bool isTagGrouping(GroupingTag* tag)
    {
        if (!tag || !tag->getGrouping())
            return false;

        return tag->getGrouping()->getGroupingName().compare("Tags", Qt::CaseInsensitive) == 0;
    }
}

DeleteTagCommand::DeleteTagCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

bool DeleteTagCommand::CanRun() const
{
    return !this->selectedTags().isEmpty();
}

void DeleteTagCommand::Run()
{
    const QStringList tags = this->selectedTags();
    if (tags.isEmpty())
        return;

    const QString prompt = tags.size() == 1
        ? QObject::tr("Delete tag '%1' from all objects?").arg(tags.first())
        : QObject::tr("Delete %1 selected tags from all objects?").arg(tags.size());

    if (QMessageBox::question(this->mainWindow(), QObject::tr("Delete Tags"), prompt,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }

    QList<AsyncOperation*> actions;
    const QList<XenConnection*> connections = Xen::ConnectionsManager::instance()->GetAllConnections();
    for (XenConnection* connection : connections)
    {
        if (!connection || !connection->IsConnected() || !connection->GetCache())
            continue;

        for (const QString& tag : tags)
        {
            actions.append(new DelegatedAsyncOperation(connection,
                                                       QObject::tr("Delete Tag '%1'").arg(tag),
                                                       QObject::tr("Deleting tag '%1'...").arg(tag),
                                                       [connection, tag](DelegatedAsyncOperation* self)
                                                       {
                                                           XenRpcAPI api(self->GetSession());
                                                           const QList<QPair<XenObjectType, QString>> searchable = connection->GetCache()->GetXenSearchableObjects();
                                                           for (const auto& pair : searchable)
                                                           {
                                                               if (pair.first == XenObjectType::Folder)
                                                                   continue;

                                                               QSharedPointer<XenObject> obj = connection->GetCache()->ResolveObject(pair.first, pair.second);
                                                               if (!obj || !obj->GetTags().contains(tag))
                                                                   continue;

                                                               QVariantList params;
                                                               params << self->GetSession()->GetSessionID() << obj->OpaqueRef() << tag;
                                                               QByteArray req = api.BuildJsonRpcCall(QString("%1.remove_tags").arg(obj->GetObjectTypeName()), params);
                                                               api.ParseJsonRpcResponse(connection->SendRequest(req));
                                                           }
                                                       },
                                                       this->mainWindow()));
        }
    }

    if (actions.isEmpty())
        return;

    this->RunMultipleActions(actions,
                             QObject::tr("Delete Tags"),
                             QObject::tr("Deleting tags..."),
                             QObject::tr("Tags deleted"),
                             true);
}

QString DeleteTagCommand::MenuText() const
{
    return QObject::tr("Delete Tag");
}

QStringList DeleteTagCommand::selectedTags() const
{
    QStringList tags;
    QTreeWidget* tree = this->mainWindow() ? this->mainWindow()->GetServerTreeWidget() : nullptr;
    if (!tree)
        return tags;

    for (QTreeWidgetItem* item : tree->selectedItems())
    {
        if (!item)
            continue;

        QVariant tagVar = item->data(0, Qt::UserRole + 3);
        if (!tagVar.canConvert<GroupingTag*>())
            continue;

        GroupingTag* gt = tagVar.value<GroupingTag*>();
        if (!isTagGrouping(gt))
            continue;

        const QString tag = gt->getGroup().toString().trimmed();
        if (!tag.isEmpty() && tag.compare("Tags", Qt::CaseInsensitive) != 0)
            tags.append(tag);
    }

    tags.removeDuplicates();
    return tags;
}
