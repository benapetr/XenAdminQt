/*
 * Copyright (c) 2026, Petr Bena <petr@bena.rocks>
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

#ifndef XENAPI_AUTH_H
#define XENAPI_AUTH_H

#include <QString>
#include <QStringList>
#include <QVariantMap>
#include "xenlib_global.h"

namespace XenAPI
{
    class Session;

    //! Static-only class providing XenServer external directory lookups (C# XenAPI.Auth)
    class XENLIB_EXPORT Auth
    {
        public:
            Auth() = delete;

            //! The auth type for Active Directory (C# Auth.AUTH_TYPE_AD)
            static const char* const AUTH_TYPE_AD;

            //! auth.get_subject_identifier: the identifier (SID) of a user or group name
            static QString get_subject_identifier(Session* session, const QString& subjectName);
            //! auth.get_subject_information_from_identifier: the directory's details for a SID
            static QVariantMap get_subject_information_from_identifier(Session* session, const QString& subjectIdentifier);
            //! auth.get_group_membership: the SIDs of the groups a subject belongs to
            static QStringList get_group_membership(Session* session, const QString& subjectIdentifier);
    };
}

#endif // XENAPI_AUTH_H
