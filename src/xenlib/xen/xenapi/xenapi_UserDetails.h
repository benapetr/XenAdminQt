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

#ifndef XENAPI_USERDETAILS_H
#define XENAPI_USERDETAILS_H

#include "../../xenlib_global.h"
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QMutex>
#include <QHash>

namespace XenAPI
{
    class Session;

    /**
     * @brief XenAPI UserDetails - User information with AD lookup
     *
     * Manages user details cache with Active Directory information.
     * Matches C# XenModel/XenAPI/UserDetails.cs structure.
     */
    class XENLIB_EXPORT UserDetails
    {
        public:
            /**
             * @brief Update user details for given SID
             * @param userSid Active Directory SID
             * @param session Active XenSession
             *
             * Matches C# UserDetails.UpdateDetails()
             * Fetches user information from XenServer and caches it
             */
            static void UpdateDetails(const QString& userSid, Session* session);

            /**
             * @brief Get cached user details by SID
             * @param userSid Active Directory SID
             * @return UserDetails object or null if not cached
             */
            static UserDetails* GetUserDetails(const QString& userSid);

            /**
             * @brief Get all cached user details
             * @return Map of SID to UserDetails
             */
            static QHash<QString, UserDetails*> GetAllUserDetails();

            /**
             * @brief Clear all cached user details
             */
            static void ClearCache();

            // Accessors
            QString UserSid() const { return this->userSid_; }
            QString UserDisplayName() const { return this->userDisplayName_; }
            QString UserName() const { return this->userName_; }
            QStringList GroupMembershipSids() const { return this->groupMembershipSids_; }
            QStringList GroupMembershipNames() const { return this->groupMembershipNames_; }

        private:
            UserDetails(Session* session, const QString& sid);

        public:
            ~UserDetails() = default;

            void fetchUserInfo(Session* session);
            QStringList fetchGroupMembershipNames(Session* session);

            QString userSid_;
            QString userDisplayName_;
            QString userName_;
            QStringList groupMembershipSids_;
            QStringList groupMembershipNames_;

            static QMutex cacheMutex_;
            static QHash<QString, UserDetails*> sidToUserDetails_;
    };

} // namespace XenAPI

#endif // XENAPI_USERDETAILS_H
