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

#ifndef ROLE_H
#define ROLE_H

#include "xenobject.h"
#include <QString>
#include <QStringList>

/**
 * @brief Role - A set of permissions associated with a subject
 *
 * Qt equivalent of C# Role class.
 * First published in XenServer 5.6.
 */
class XENLIB_EXPORT Role : public XenObject
{
    Q_OBJECT

    public:
        explicit Role(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        virtual ~Role();

        XenObjectType GetObjectType() const override { return XenObjectType::Role; }

        // Property accessors
        QStringList SubroleRefs() const;
        bool IsInternal() const;

        //! The friendly name of a role (e.g. "Pool Admin"), else its name
        QString FriendlyName() const;
        //! C# Role.FriendlyDescription: what the role allows, else empty
        QString FriendlyDescription() const;
        //! C# Role.RoleRank: read-only 0 ... pool-admin 5, -1 for others (C# CompareTo orders by it)
        int Rank() const;
        //! A role a subject can hold: a parent role (it has subroles) that isn't internal
        //! (C# RoleSelectionDialog / AddRemoveRolesAction filter)
        bool IsAssignable() const;

        // RBAC helpers. Each of the six parent roles has every permission (an API call or
        // "http/..." name) it grants as a subrole.

        //! C# Role.ValidRoleList: the parent roles that can perform this call
        static QList<QSharedPointer<Role>> ValidRoles(XenConnection* connection, const QString& method);
        //! C# Session.Roles: the parent roles the session holds (its permissions include
        //! their names)
        static QList<QSharedPointer<Role>> SessionRoles(XenConnection* connection);
        //! The session may perform the call: it is the local superuser, holds one of the
        //! roles that can, or no role on the server is known to grant it (roles not loaded)
        static bool SessionCanPerform(XenConnection* connection, const QString& method);
};

#endif // ROLE_H
