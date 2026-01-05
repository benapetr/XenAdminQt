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

#include "xenapi_UserDetails.h"
#include "../session.h"
#include "../api.h"
#include <stdexcept>
#include <QMutexLocker>

namespace XenAPI
{
    // Static member initialization
    QMutex UserDetails::cacheMutex_;
    QHash<QString, UserDetails*> UserDetails::sidToUserDetails_;

    UserDetails::UserDetails(Session* session, const QString& sid)
        : userSid_(sid)
    {
        this->fetchUserInfo(session);
    }

    void UserDetails::UpdateDetails(const QString& userSid, Session* session)
    {
        QMutexLocker locker(&cacheMutex_);

        // Remove old entry if exists
        if (sidToUserDetails_.contains(userSid))
        {
            delete sidToUserDetails_[userSid];
            sidToUserDetails_.remove(userSid);
        }

        // Add new entry
        UserDetails* details = new UserDetails(session, userSid);
        sidToUserDetails_[userSid] = details;
    }

    UserDetails* UserDetails::GetUserDetails(const QString& userSid)
    {
        QMutexLocker locker(&cacheMutex_);
        return sidToUserDetails_.value(userSid, nullptr);
    }

    QHash<QString, UserDetails*> UserDetails::GetAllUserDetails()
    {
        QMutexLocker locker(&cacheMutex_);
        return sidToUserDetails_;
    }

    void UserDetails::ClearCache()
    {
        QMutexLocker locker(&cacheMutex_);
        qDeleteAll(sidToUserDetails_);
        sidToUserDetails_.clear();
    }

    void UserDetails::fetchUserInfo(Session* session)
    {
        if (!session || !session->IsLoggedIn())
            return;

        try
        {
            // Call Auth.get_subject_information_from_identifier
            QVariantList params;
            params << session->getSessionId() << this->userSid_;

            XenRpcAPI api(session);
            QByteArray request = api.buildJsonRpcCall("auth.get_subject_information_from_identifier", params);
            QByteArray response = session->sendApiRequest(request);

            QVariant result = api.parseJsonRpcResponse(response);
            if (result.canConvert<QVariantMap>())
            {
                QVariantMap info = result.toMap();
                this->userDisplayName_ = info.value("subject-displayname", "").toString();
                this->userName_ = info.value("subject-name", "").toString();
            }

            // Get group membership
            request = api.buildJsonRpcCall("auth.get_group_membership", params);
            response = session->sendApiRequest(request);

            result = api.parseJsonRpcResponse(response);
            if (result.canConvert<QVariantList>())
            {
                QVariantList groups = result.toList();
                for (const QVariant& group : groups)
                {
                    this->groupMembershipSids_ << group.toString();
                }

                // Fetch group names (lazy - done on demand in C# but we do it here)
                this->groupMembershipNames_ = this->fetchGroupMembershipNames(session);
            }
        }
        catch (const std::exception&)
        {
            // Silently ignore failures (matches C# behavior)
        }
    }

    QStringList UserDetails::fetchGroupMembershipNames(Session* session)
    {
        QStringList names;

        if (!session || !session->IsLoggedIn() || this->groupMembershipSids_.isEmpty())
            return names;

        try
        {
            XenRpcAPI api(session);

            for (const QString& sid : this->groupMembershipSids_)
            {
                QVariantList params;
                params << session->getSessionId() << sid;

                QByteArray request = api.buildJsonRpcCall("auth.get_subject_information_from_identifier", params);
                QByteArray response = session->sendApiRequest(request);

                QVariant result = api.parseJsonRpcResponse(response);
                if (result.canConvert<QVariantMap>())
                {
                    QVariantMap info = result.toMap();
                    QString name = info.value("subject-displayname", "").toString();
                    if (name.isEmpty())
                        name = info.value("subject-name", "").toString();
                    if (name.isEmpty())
                        name = sid; // Fallback to SID

                    names << name;
                } else
                {
                    names << sid;
                }
            }
        }
        catch (const std::exception&)
        {
            // Return partial results on failure
        }

        return names;
    }

} // namespace XenAPI
