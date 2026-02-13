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

#include "removefromfoldercommand.h"

#include "../../mainwindow.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/actions/general/generaleditpageaction.h"

RemoveFromFolderCommand::RemoveFromFolderCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

bool RemoveFromFolderCommand::CanRun() const
{
    const QList<QSharedPointer<XenObject>> selected = this->getSelectedObjects();
    if (selected.isEmpty())
        return false;

    for (const QSharedPointer<XenObject>& obj : selected)
    {
        if (!obj || obj->GetObjectType() == XenObjectType::Folder)
            return false;

        if (obj->GetFolderPath().isEmpty() || obj->GetFolderPath() == "/")
            return false;
    }

    return true;
}

void RemoveFromFolderCommand::Run()
{
    if (!this->CanRun())
        return;

    QList<AsyncOperation*> actions;
    const QList<QSharedPointer<XenObject>> selected = this->getSelectedObjects();
    for (const QSharedPointer<XenObject>& obj : selected)
    {
        actions.append(new GeneralEditPageAction(obj, obj->GetFolderPath(), QString(), obj->GetTags(), obj->GetTags(), false, this->mainWindow(), true));
    }

    this->RunMultipleActions(actions, QObject::tr("Remove From Folder"), QObject::tr("Removing selected items from folder..."), QObject::tr("Items removed from folder"), true, true);
}

QString RemoveFromFolderCommand::MenuText() const
{
    return QObject::tr("Remove From Folder");
}
