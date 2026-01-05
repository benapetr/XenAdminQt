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

#ifndef XENAPI_SESSION_H
#define XENAPI_SESSION_H

#include "../../xenlib_global.h"
#include <QString>
#include <QStringList>

namespace XenAPI
{
    class Session;

    /**
     * @brief XenAPI Session bindings
     *
     * Static-only class providing XenServer Session API bindings.
     * Matches C# XenModel/XenAPI/Session.cs structure.
     */
    class XENLIB_EXPORT SessionAPI
    {
        private:
            SessionAPI() = delete; // Static-only class

        public:
            /**
             * @brief Check if session belongs to local superuser
             * @param session Active XenSession
             * @param sessionRef Session opaque reference (typically session->getSessionId())
             * @return True if local superuser
             *
             * Matches C# Session.get_is_local_superuser()
             */
            static bool get_is_local_superuser(Session* session, const QString& sessionRef);

            /**
             * @brief Get subject (user) reference for session
             * @param session Active XenSession
             * @param sessionRef Session opaque reference
             * @return Subject opaque reference
             *
             * Matches C# Session.get_subject()
             */
            static QString get_subject(Session* session, const QString& sessionRef);

            /**
             * @brief Get Active Directory SID for authenticated user
             * @param session Active XenSession
             * @param sessionRef Session opaque reference
             * @return User SID string
             *
             * Matches C# Session.get_auth_user_sid()
             */
            static QString get_auth_user_sid(Session* session, const QString& sessionRef);

            /**
             * @brief Get RBAC permissions for session
             * @param session Active XenSession
             * @param sessionRef Session opaque reference
             * @return List of permission strings
             *
             * Matches C# Session.get_rbac_permissions()
             */
            static QStringList get_rbac_permissions(Session* session, const QString& sessionRef);
    };

} // namespace XenAPI

#endif // XENAPI_SESSION_H
