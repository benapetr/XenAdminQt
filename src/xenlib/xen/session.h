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

#ifndef XEN_SESSION_H
#define XEN_SESSION_H

#include "../xenlib_global.h"
#include "apiversion.h"
#include <QtCore/QObject>
#include <QtCore/QString>

class XenConnection;

class XENLIB_EXPORT XenSession : public QObject
{
    Q_OBJECT
    public:
        explicit XenSession(XenConnection* connection, QObject* parent = nullptr);
        ~XenSession();

        bool Login(const QString& username, const QString& password);
        void Logout();
        bool IsLoggedIn() const;

        // Session duplication for separate TCP streams
        static XenSession* DuplicateSession(XenSession* originalSession, QObject* parent = nullptr);

        QString getSessionId() const;
        QString getUsername() const;
        QString getPassword() const;
        QString getLastError() const;

        // API version management
        APIVersion getAPIVersion() const;
        void setAPIVersion(APIVersion version);
        bool apiVersionMeets(APIVersion required) const;

        // API communication - this is always synchronous, despite it's also using the worker thread, it waits for it to finish
        QByteArray sendApiRequest(const QString& jsonRequest);

        bool ownsSessionToken() const;
        void setOwnsSessionToken(bool ownsToken);
        void detachConnection();

    signals:
        void loginSuccessful();
        void loginFailed(const QString& reason);
        void loggedOut();
        void needsRedirectToMaster(const QString& masterAddress);

    private:
        bool parseLoginResponse(const QByteArray& response);
        QString extractAuthenticationError(const QByteArray& response);
        QString buildLoginRequest(const QString& username, const QString& password);
        QString buildLogoutRequest();

        class Private;
        Private* d;
};

#endif // XEN_SESSION_H
