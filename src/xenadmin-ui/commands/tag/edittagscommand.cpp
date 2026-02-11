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

#include "edittagscommand.h"

#include <QSet>

#include "../../dialogs/newtagdialog.h"
#include "../../mainwindow.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/network/connectionsmanager.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/actions/general/generaleditpageaction.h"

namespace
{
    void addTagCandidates(QStringList& allTags, const QStringList& tags)
    {
        for (const QString& tag : tags)
        {
            const QString cleaned = tag.trimmed();
            if (!cleaned.isEmpty())
                allTags.append(cleaned);
        }
    }

    QStringList collectAllTags(const QList<QSharedPointer<XenObject>>& objects)
    {
        QStringList allTags;

        for (const QSharedPointer<XenObject>& obj : objects)
        {
            if (obj)
                addTagCandidates(allTags, obj->GetTags());
        }

        const QList<XenConnection*> connections = Xen::ConnectionsManager::instance()->GetAllConnections();
        for (XenConnection* connection : connections)
        {
            if (!connection || !connection->IsConnected() || !connection->GetCache())
                continue;

            const QList<QPair<XenObjectType, QString>> searchable = connection->GetCache()->GetXenSearchableObjects();
            for (const auto& pair : searchable)
            {
                if (pair.first == XenObjectType::Folder)
                    continue;

                const QSharedPointer<XenObject> candidate = connection->GetCache()->ResolveObject(pair.first, pair.second);
                if (!candidate)
                    continue;

                addTagCandidates(allTags, candidate->GetTags());
            }
        }

        allTags.removeDuplicates();
        allTags.sort();
        return allTags;
    }

    void collectTagState(const QList<QSharedPointer<XenObject>>& objects, QSet<QString>& selectedTags, QSet<QString>& indeterminateTags)
    {
        selectedTags.clear();
        indeterminateTags.clear();

        if (objects.isEmpty())
            return;

        const QStringList allTags = collectAllTags(objects);
        for (const QString& tag : allTags)
        {
            bool contained = false;
            bool notContained = false;
            for (const QSharedPointer<XenObject>& obj : objects)
            {
                const bool hasTag = obj && obj->GetTags().contains(tag);
                contained |= hasTag;
                notContained |= !hasTag;
                if (contained && notContained)
                    break;
            }

            if (contained && notContained)
                indeterminateTags.insert(tag);
            else if (contained)
                selectedTags.insert(tag);
        }
    }
}

EditTagsCommand::EditTagsCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

bool EditTagsCommand::CanRun() const
{
    const QList<QSharedPointer<XenObject>> objects = this->getSelectedObjects();
    if (objects.isEmpty())
        return false;

    for (const QSharedPointer<XenObject>& obj : objects)
    {
        if (!obj || obj->GetObjectType() == XenObjectType::Folder || !obj->GetConnection())
            return false;
    }

    return true;
}

void EditTagsCommand::Run()
{
    if (!this->CanRun())
        return;

    const QList<QSharedPointer<XenObject>> objects = this->getSelectedObjects();

    QSet<QString> selectedTags;
    QSet<QString> indeterminateTags;
    collectTagState(objects, selectedTags, indeterminateTags);

    NewTagDialog dialog(this->mainWindow());
    dialog.SetTags(collectAllTags(objects), selectedTags.values(), indeterminateTags.values());

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QStringList selectedAfterDialog = dialog.GetSelectedTags();
    const QStringList indeterminateAfterDialog = dialog.GetIndeterminateTags();

    QList<AsyncOperation*> actions;
    for (const QSharedPointer<XenObject>& obj : objects)
    {
        QStringList newTags = obj->GetTags();
        for (int i = newTags.size() - 1; i >= 0; --i)
        {
            const QString tag = newTags.at(i);
            if (!selectedAfterDialog.contains(tag) && !indeterminateAfterDialog.contains(tag))
                newTags.removeAt(i);
        }

        for (const QString& selectedTag : selectedAfterDialog)
        {
            if (!newTags.contains(selectedTag))
                newTags.append(selectedTag);
        }

        QStringList oldTags = obj->GetTags();
        oldTags.removeDuplicates();
        oldTags.sort();
        newTags.removeDuplicates();
        newTags.sort();

        if (oldTags == newTags)
            continue;

        actions.append(new GeneralEditPageAction(obj,
                                                 obj->GetFolderPath(),
                                                 obj->GetFolderPath(),
                                                 oldTags,
                                                 newTags,
                                                 false,
                                                 this->mainWindow()));
    }

    if (actions.isEmpty())
        return;

    this->RunMultipleActions(actions,
                             QObject::tr("Save Tags"),
                             QObject::tr("Saving tag changes..."),
                             QObject::tr("Tag changes saved"),
                             true);
}

QString EditTagsCommand::MenuText() const
{
    return QObject::tr("Edit Tags...");
}

QIcon EditTagsCommand::GetIcon() const
{
    return QIcon(":/icons/tag_16.png");
}
