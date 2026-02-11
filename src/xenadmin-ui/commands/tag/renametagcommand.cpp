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

#include "renametagcommand.h"

#include <QInputDialog>
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

RenameTagCommand::RenameTagCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

bool RenameTagCommand::CanRun() const
{
    QString oldTag;
    return this->selectedTag(oldTag);
}

void RenameTagCommand::Run()
{
    QString oldTag;
    if (!this->selectedTag(oldTag))
        return;

    bool ok = false;
    const QString newTag = QInputDialog::getText(this->mainWindow(),
                                                 QObject::tr("Rename Tag"),
                                                 QObject::tr("New tag name:"),
                                                 QLineEdit::Normal,
                                                 oldTag,
                                                 &ok).trimmed();
    if (!ok || newTag.isEmpty() || newTag == oldTag)
        return;

    QList<AsyncOperation*> actions;
    const QList<XenConnection*> connections = Xen::ConnectionsManager::instance()->GetAllConnections();
    for (XenConnection* connection : connections)
    {
        if (!connection || !connection->IsConnected() || !connection->GetCache())
            continue;

        actions.append(new DelegatedAsyncOperation(connection,
                                                   QObject::tr("Rename Tag '%1'").arg(oldTag),
                                                   QObject::tr("Renaming tag '%1'...").arg(oldTag),
                                                   [connection, oldTag, newTag](DelegatedAsyncOperation* self)
                                                   {
                                                       XenRpcAPI api(self->GetSession());
                                                       const QList<QPair<XenObjectType, QString>> searchable = connection->GetCache()->GetXenSearchableObjects();
                                                       for (const auto& pair : searchable)
                                                       {
                                                           if (pair.first == XenObjectType::Folder)
                                                               continue;

                                                           QSharedPointer<XenObject> obj = connection->GetCache()->ResolveObject(pair.first, pair.second);
                                                           if (!obj)
                                                               continue;

                                                           if (!obj->GetTags().contains(oldTag))
                                                               continue;

                                                           QVariantList removeParams;
                                                           removeParams << self->GetSession()->GetSessionID() << obj->OpaqueRef() << oldTag;
                                                           QByteArray removeReq = api.BuildJsonRpcCall(QString("%1.remove_tags").arg(obj->GetObjectTypeName()), removeParams);
                                                           api.ParseJsonRpcResponse(connection->SendRequest(removeReq));

                                                           QVariantList addParams;
                                                           addParams << self->GetSession()->GetSessionID() << obj->OpaqueRef() << newTag;
                                                           QByteArray addReq = api.BuildJsonRpcCall(QString("%1.add_tags").arg(obj->GetObjectTypeName()), addParams);
                                                           api.ParseJsonRpcResponse(connection->SendRequest(addReq));
                                                       }
                                                   },
                                                   this->mainWindow()));
    }

    if (actions.isEmpty())
        return;

    this->RunMultipleActions(actions,
                             QObject::tr("Rename Tag"),
                             QObject::tr("Renaming tag..."),
                             QObject::tr("Tag renamed"),
                             true);
}

QString RenameTagCommand::MenuText() const
{
    return QObject::tr("Rename Tag...");
}

bool RenameTagCommand::selectedTag(QString& tagName) const
{
    tagName.clear();

    QTreeWidget* tree = this->mainWindow() ? this->mainWindow()->GetServerTreeWidget() : nullptr;
    if (!tree)
        return false;

    const QList<QTreeWidgetItem*> items = tree->selectedItems();
    if (items.size() != 1)
        return false;

    QVariant tagVar = items.first()->data(0, Qt::UserRole + 3);
    if (!tagVar.canConvert<GroupingTag*>())
        return false;

    GroupingTag* gt = tagVar.value<GroupingTag*>();
    if (!isTagGrouping(gt))
        return false;

    tagName = gt->getGroup().toString().trimmed();
    return !tagName.isEmpty() && tagName.compare("Tags", Qt::CaseInsensitive) != 0;
}
