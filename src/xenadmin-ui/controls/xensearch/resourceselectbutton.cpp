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
#include "xenlib/xensearch/search.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xensearch/queryscope.h"
#include "xenlib/xen/network/connection.h"
#include <QAction>
#include <QMenu>
#include <QDebug>

const QString ResourceSelectButton::INDENT = "       ";

ResourceSelectButton::ResourceSelectButton(QWidget* parent)
    : DropDownButton(parent)
    , scope_(nullptr)
{
    // Create menu for dropdown
    QMenu* menu = new QMenu(this);
    this->setMenu(menu);
    
    // Connect menu actions
    connect(menu, &QMenu::triggered, this, &ResourceSelectButton::onActionTriggered);
}

void ResourceSelectButton::Populate(Search* search)
{
    // Clear existing items
    if (this->menu())
        this->menu()->clear();
    
    this->scope_ = (search && search->GetQuery()) ? search->GetQuery()->getQueryScope() : nullptr;
    
    if (search)
    {
        // Get connection from search
        XenConnection* conn = search->GetConnection();
        if (conn)
        {
            // Call PopulateAdapters which will call our Add method for each object
            QList<IAcceptGroups*> adapters;
            adapters.append(this);
            search->PopulateAdapters(conn, adapters);
        }
    }
}

QString ResourceSelectButton::selectedRef() const
{
    return this->selectedRef_;
}

void ResourceSelectButton::setSelectedRef(const QString& ref)
{
    this->selectedRef_ = ref;
    
    // Find action with matching ref
    if (!this->menu())
        return;
    
    for (QAction* action : this->menu()->actions())
    {
        QString actionRef = action->data().toString();
        if (actionRef == ref)
        {
            this->setText(action->text().trimmed());
            return;
        }
    }
    
    // Not found - clear text
    this->setText("");
}

IAcceptGroups* ResourceSelectButton::Add(Grouping* grouping, const QVariant& group,
                                         const QString& objectType, const QVariantMap& objectData,
                                         int indent, XenConnection* conn)
{
    Q_UNUSED(grouping);
    
    if (!this->menu())
        return nullptr;
    
    // Extract object ref from group variant
    QString objectRef = group.toString();
    
    // Build indented text (matches C# padding logic)
    QString text = INDENT;
    for (int i = 0; i < indent; ++i)
        text += INDENT;
    
    QString name = objectData.value("name_label").toString();
    if (name.isEmpty())
        name = objectRef;  // Fallback to ref if no name
    
    // Escape ampersands (C# EscapeAmpersands)
    name.replace("&", "&&");
    text += name;
    
    // Create action
    QAction* action = new QAction(text, this->menu());
    action->setData(objectRef);  // Store ref for selection
    
    // Set icon based on object type
    // TODO
    //QIcon icon = this->getObjectIcon(objectType);
    //if (!icon.isNull())
    //    action->setIcon(icon);
    
    // Check if object is within scope
    if (this->scope_ && !this->scope_->wantType(objectData, objectType, conn))
    {
        action->setEnabled(false);
        // C# sets background to Gainsboro - Qt doesn't support per-action background easily,
        // so we just disable the item
    }
    
    this->menu()->addAction(action);
    
    // Return nullptr - we don't support nested groups
    return nullptr;
}

void ResourceSelectButton::FinishedInThisGroup(bool defaultExpand)
{
    Q_UNUSED(defaultExpand);
    // C# does nothing here
}

void ResourceSelectButton::onActionTriggered()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (!action)
        return;
    
    QSharedPointer<XenObject> object = action->data().value<QSharedPointer<XenObject>>();
    if (!object)
        return;
    
    this->selectedRef_ = object->OpaqueRef();
    QString displayName = object->GetName();
    this->setText(displayName);
    
    emit this->itemSelected(this->selectedRef_);
}

QIcon ResourceSelectButton::getObjectIcon(QSharedPointer<XenObject> object) const
{
    if (!object)
        return QIcon();
    
    // TODO: Implement icon lookup based on object type when XenObject::objectType() is available
    // C# uses Images.GetImage16For(IXenObject)
    // For now, return null icon - icons will be added when we implement Images helper
    
    QString objectType = object->GetObjectType();
    // Placeholder - would map to actual icons:
    // if (objectType == "vm") return QIcon(":/icons/vm_16.png");
    // if (objectType == "host") return QIcon(":/icons/host_16.png");
    // etc.
    
    return QIcon();
}
