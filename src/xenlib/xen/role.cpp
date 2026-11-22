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

#include "role.h"
#include "network/connection.h"
#include "session.h"
#include "friendlynames.h"
#include "../xencache.h"

Role::Role(XenConnection* connection, const QString& opaqueRef, QObject* parent)  : XenObject(connection, opaqueRef, parent)
{
}

Role::~Role()
{
}

QStringList Role::SubroleRefs() const
{
    return this->property("subroles").toStringList();
}

bool Role::IsInternal() const
{
    return this->boolProperty("is_internal", false);
}

QString Role::FriendlyName() const
{
    const QString friendly = FriendlyNames::getString(QStringLiteral("Role.%1.NameLabel").arg(this->GetName()));
    return friendly.isEmpty() ? this->GetName() : friendly;
}

QString Role::FriendlyDescription() const
{
    return FriendlyNames::getString(QStringLiteral("Role.%1.Description").arg(this->GetName().toLower()));
}

int Role::Rank() const
{
    static const QStringList ranks = {"read-only", "vm-operator", "vm-admin", "vm-power-admin", "pool-operator", "pool-admin"};
    return ranks.indexOf(this->GetName().toLower());
}

bool Role::IsAssignable() const
{
    return !this->IsInternal() && !this->SubroleRefs().isEmpty();
}

QList<QSharedPointer<Role>> Role::ValidRoles(XenConnection* connection, const QString& method)
{
    QList<QSharedPointer<Role>> roles;
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    if (!cache)
        return roles;

    for (const QSharedPointer<XenObject>& object : cache->GetAll(XenObjectType::Role))
    {
        QSharedPointer<Role> role = qSharedPointerDynamicCast<Role>(object);
        if (!role)
            continue;
        for (const QString& subroleRef : role->SubroleRefs())
        {
            // C# Role.CanPerform: the subrole's name is the call
            const QString subrole = cache->ResolveObjectData(XenObjectType::Role, subroleRef).value("name_label").toString();
            if (subrole.compare(method, Qt::CaseInsensitive) == 0)
            {
                roles.append(role);
                break;
            }
        }
    }
    return roles;
}

QList<QSharedPointer<Role>> Role::SessionRoles(XenConnection* connection)
{
    QList<QSharedPointer<Role>> roles;
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    XenAPI::Session* session = connection ? connection->GetSession() : nullptr;
    if (!cache || !session)
        return roles;

    const QStringList permissions = session->GetPermissions();
    for (const QSharedPointer<XenObject>& object : cache->GetAll(XenObjectType::Role))
    {
        QSharedPointer<Role> role = qSharedPointerDynamicCast<Role>(object);
        if (role && !role->SubroleRefs().isEmpty() && permissions.contains(role->GetName(), Qt::CaseInsensitive))
            roles.append(role);
    }
    return roles;
}

bool Role::SessionCanPerform(XenConnection* connection, const QString& method)
{
    XenAPI::Session* session = connection ? connection->GetSession() : nullptr;
    if (!session || session->IsLocalSuperuser())
        return true;

    const QList<QSharedPointer<Role>> valid = ValidRoles(connection, method);
    if (valid.isEmpty())
        return true;

    for (const QSharedPointer<Role>& role : SessionRoles(connection))
    {
        for (const QSharedPointer<Role>& allowed : valid)
        {
            if (allowed->OpaqueRef() == role->OpaqueRef())
                return true;
        }
    }
    return false;
}
