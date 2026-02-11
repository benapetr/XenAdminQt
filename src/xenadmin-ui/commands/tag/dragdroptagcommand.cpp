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

#include "dragdroptagcommand.h"

#include "../../mainwindow.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/actions/general/generaleditpageaction.h"

DragDropTagCommand::DragDropTagCommand(MainWindow* mainWindow, const QString& tag, QObject* parent) : Command(mainWindow, parent), m_tag(tag)
{
}

bool DragDropTagCommand::CanRun() const
{
    if (m_tag.trimmed().isEmpty())
        return false;

    const QList<QSharedPointer<XenObject>> objects = this->getSelectedObjects();
    if (objects.isEmpty())
        return false;

    for (const QSharedPointer<XenObject>& obj : objects)
    {
        if (!obj || obj->GetObjectType() == XenObjectType::Folder)
            return false;
    }

    return true;
}

void DragDropTagCommand::Run()
{
    if (!this->CanRun())
        return;

    QList<AsyncOperation*> actions;
    const QList<QSharedPointer<XenObject>> objects = this->getSelectedObjects();
    for (const QSharedPointer<XenObject>& obj : objects)
    {
        QStringList oldTags = obj->GetTags();
        QStringList newTags = oldTags;
        if (!newTags.contains(m_tag))
            newTags.append(m_tag);

        oldTags.sort();
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
                             QObject::tr("Add Tag '%1'").arg(m_tag),
                             QObject::tr("Adding tag '%1'...").arg(m_tag),
                             QObject::tr("Tag '%1' added").arg(m_tag),
                             true);
}

QString DragDropTagCommand::MenuText() const
{
    return QObject::tr("Add Tag");
}
