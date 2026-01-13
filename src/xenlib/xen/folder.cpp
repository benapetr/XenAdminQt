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

#include "folder.h"
#include "network/connection.h"
#include <QMutexLocker>
#include <QUuid>

Folder::Folder(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
    , parent_(nullptr)
{
}

Folder::~Folder()
{
}

Folder* Folder::Create(XenConnection* connection, const QString& name, Folder* parent)
{
    // Generate a unique opaque ref for client-side folder
    QString opaqueRef = QString("OpaqueRef:folder-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

    Folder* folder = new Folder(connection, opaqueRef);
    folder->nameLabel_ = name;
    folder->parent_ = parent;

    return folder;
}

Folder* Folder::GetParent() const
{
    return this->parent_;
}

QString Folder::GetPath() const
{
    if (this->IsRootFolder())
    {
        return "/";
    }

    if (this->parent_)
    {
        QString parentPath = this->parent_->GetPath();
        if (parentPath == "/")
        {
            return "/" + this->nameLabel_;
        }
        return parentPath + "/" + this->nameLabel_;
    }

    return "/" + this->nameLabel_;
}

bool Folder::IsRootFolder() const
{
    return this->parent_ == nullptr;
}

void Folder::AddObject(XenObject* obj)
{
    if (!obj)
    {
        return;
    }

    QMutexLocker locker(&this->mutex_);
    if (!this->xenObjects_.contains(obj))
    {
        this->xenObjects_.append(obj);
    }
}

bool Folder::RemoveObject(XenObject* obj)
{
    if (!obj)
    {
        return false;
    }

    QMutexLocker locker(&this->mutex_);
    return this->xenObjects_.removeOne(obj);
}

QList<XenObject*> Folder::GetXenObjects() const
{
    QMutexLocker locker(&this->mutex_);
    return this->xenObjects_;
}

QList<XenObject*> Folder::GetRecursiveXenObjects() const
{
    QList<XenObject*> objects;

    QMutexLocker locker(&this->mutex_);
    for (XenObject* obj : this->xenObjects_)
    {
        Folder* folder = qobject_cast<Folder*>(obj);
        if (folder)
        {
            objects.append(folder->GetRecursiveXenObjects());
        }
        else
        {
            objects.append(obj);
        }
    }

    return objects;
}

int Folder::GetXenObjectsCount() const
{
    QMutexLocker locker(&this->mutex_);
    return this->xenObjects_.count();
}

QString Folder::ToString() const
{
    return this->nameLabel_;
}

bool Folder::operator==(const Folder& other) const
{
    return this->getFullPath() == other.getFullPath();
}

QString Folder::getFullPath() const
{
    QString path = this->GetPath();
    if (path != "/")
    {
        return QString("%1/%2").arg(path, this->nameLabel_);
    }
    return QString("/%1").arg(this->nameLabel_);
}
