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

#include "resourceselectbutton.h"
#include "../../iconmanager.h"
#include "xenlib/xensearch/search.h"
#include "xenlib/xensearch/query.h"
#include "xenlib/xensearch/queryscope.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xencache.h"
#include <QAction>
#include <QMenu>

const QString ResourceSelectButton::INDENT = "       ";

ResourceSelectButton::ResourceSelectButton(QWidget* parent) : DropDownButton(parent)
{
    QMenu* menu = new QMenu(this);
    this->SetMenu(menu);
    connect(menu, &QMenu::triggered, this, &ResourceSelectButton::onActionTriggered);
}

void ResourceSelectButton::Populate(Search* search)
{
    if (!this->GetMenu())
        return;

    this->GetMenu()->clear();

    QueryScope* scope = (search && search->GetQuery()) ? search->GetQuery()->getQueryScope() : nullptr;
    this->hasScope_ = scope != nullptr;
    this->scopeTypes_ = scope ? scope->getObjectTypes() : XenSearch::ObjectTypes::None;

    // PopulateAdapters walks every connection
    if (search)
        search->PopulateAdapters(search->GetConnection(), QList<IAcceptGroups*>() << this);

    // Keep the selection if the object is still listed
    QAction* selected = this->findAction("ref", this->selectedRef_);
    if (selected)
        this->select(selected);
    else if (!this->selectedRef_.isEmpty())
        this->setText(QString());
}

QString ResourceSelectButton::selectedRef() const
{
    return this->selectedRef_;
}

QString ResourceSelectButton::selectedUuid() const
{
    return this->selectedUuid_;
}

void ResourceSelectButton::setSelectedRef(const QString& ref)
{
    this->selectedRef_ = ref;
    QAction* action = this->findAction("ref", ref);
    if (action)
        this->select(action);
    else
        this->setText(QString());
}

void ResourceSelectButton::setSelectedUuid(const QString& uuid)
{
    QAction* action = this->findAction("uuid", uuid);
    if (action)
    {
        this->select(action);
    } else
    {
        this->selectedRef_.clear();
        this->selectedUuid_ = uuid;
        this->setText(QString());
    }
}

QAction* ResourceSelectButton::findAction(const QString& key, const QString& value) const
{
    if (!this->GetMenu() || value.isEmpty())
        return nullptr;
    for (QAction* action : this->GetMenu()->actions())
    {
        if (action->data().toMap().value(key).toString() == value)
            return action;
    }
    return nullptr;
}

void ResourceSelectButton::select(QAction* action)
{
    const QVariantMap data = action->data().toMap();
    this->selectedRef_ = data.value("ref").toString();
    this->selectedUuid_ = data.value("uuid").toString();
    // C# SetButtonTextFromItem: the object's name (folders show their path)
    this->setText(data.value("name").toString());
}

IAcceptGroups* ResourceSelectButton::Add(Grouping* grouping, const QVariant& group,
                                         const QString& objectType, const QVariantMap& objectData,
                                         int indent, XenConnection* conn)
{
    Q_UNUSED(grouping);

    // C#: only groups that are objects get an item; headings such as "Servers" are skipped,
    // but their contents are still added (return this)
    if (!this->GetMenu() || objectData.isEmpty())
        return this;

    const QString ref = group.toString();
    QString name = objectData.value("name_label").toString();
    const XenObjectType type = XenObject::TypeFromString(objectType);
    if (type == XenObjectType::Folder)
        name = ref; // the full path, as C# shows folders with a FolderListItem
    if (name.isEmpty())
        name = ref;

    QString text = INDENT;
    for (int i = 0; i < indent; ++i)
        text += INDENT;
    text += QString(name).replace("&", "&&");

    QAction* action = new QAction(text, this->GetMenu());
    action->setData(QVariantMap{{"ref", ref},
                                {"uuid", objectData.value("uuid").toString()},
                                {"name", name}});
    if (conn)
        action->setIcon(IconManager::instance().GetIconForObject(conn, type, ref));

    // Objects outside the scope are context only (C# greys them out)
    if (this->hasScope_)
    {
        QueryScope scope(this->scopeTypes_);
        if (!scope.wantType(objectData, objectType, conn))
            action->setEnabled(false);
    }

    this->GetMenu()->addAction(action);
    return this;
}

void ResourceSelectButton::FinishedInThisGroup(bool defaultExpand)
{
    Q_UNUSED(defaultExpand);
}

void ResourceSelectButton::onActionTriggered(QAction* action)
{
    if (!action || !action->isEnabled())
        return;

    this->select(action);
    emit this->itemSelected(this->selectedRef_);
}
