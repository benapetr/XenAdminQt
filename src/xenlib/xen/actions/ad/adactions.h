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

#ifndef ADACTIONS_H
#define ADACTIONS_H

#include "../../asyncoperation.h"
#include "../../failure.h"
#include <QStringList>
#include <QVariantMap>

class Role;
class Subject;

/**
 * Joins the pool (or standalone server) on a connection to an Active Directory domain.
 * C# XenModel/Actions/AD/EnableAdAction.cs
 */
class XENLIB_EXPORT EnableAdAction : public AsyncOperation
{
    Q_OBJECT

    public:
        //! Wrong domain credentials (C# EnableAdAction.CredentialsFailure)
        class CredentialsFailure : public Failure
        {
            public:
                explicit CredentialsFailure(const QStringList& errorDescription) : Failure(errorDescription) {}
        };

        EnableAdAction(XenConnection* connection, const QString& domain, const QString& user, const QString& password, QObject* parent = nullptr);

        //! True when the action failed because of the credentials (the caller can ask again)
        bool FailedOnCredentials() const { return this->m_credentialsFailed; }

        /**
         * The work of the action, for callers already on a worker thread with a session for
         * \a connection (C# FixAd runs the action synchronously). Throws CredentialsFailure
         * for wrong credentials, Failure or std::runtime_error otherwise.
         */
        static void Enable(XenAPI::Session* session, XenConnection* connection,
                           const QString& domain, const QString& user, const QString& password);

    protected:
        void run() override;

    private:
        QString m_domain;
        QString m_user;
        QString m_password;
        bool m_credentialsFailed = false;
};

/**
 * Takes the pool out of its Active Directory domain. With "user" and "pass" in \a creds the
 * machine accounts are disabled in AD too (C# DisableAdAction.KEY_USER / KEY_PASS).
 * C# XenModel/Actions/AD/DisableAdAction.cs
 */
class XENLIB_EXPORT DisableAdAction : public AsyncOperation
{
    Q_OBJECT

    public:
        DisableAdAction(XenConnection* connection, const QVariantMap& creds, QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QVariantMap m_creds;
};

/**
 * Resolves names to directory users or groups and gives them access, and removes access
 * from subjects (logging them out first). Reports each step through signals, emitted on the
 * worker thread. C# XenModel/Actions/AD/AddRemoveSubjectsAction.cs
 */
class XENLIB_EXPORT AddRemoveSubjectsAction : public AsyncOperation
{
    Q_OBJECT

    public:
        AddRemoveSubjectsAction(XenConnection* connection, const QStringList& namesToAdd,
                                const QList<QSharedPointer<Subject>>& subjectsToRemove, QObject* parent = nullptr);

    signals:
        //! A name was looked up; \a error is empty on success
        void nameResolveComplete(const QString& enteredName, const QString& resolvedName, const QString& sid, const QString& error);
        void allResolveComplete();
        //! A subject was created for \a sid (named \a name); \a error is empty on success
        void subjectAddComplete(const QString& sid, const QString& name, const QString& error);
        //! The subject \a sid was removed; \a error is empty on success
        void subjectRemoveComplete(const QString& sid, const QString& error);

    protected:
        void run() override;

    private:
        struct Removal
        {
            QString ref;
            QString sid;
        };

        QStringList m_namesToAdd;
        QList<Removal> m_toRemove;
        QString m_selfSid;           //!< the logged-in user's SID, empty for the local superuser
        int m_steps = 0;
        int m_stepsComplete = 0;
        QString m_lastError;
};

/**
 * Gives a subject exactly the given roles.
 * C# XenModel/Actions/AD/AddRemoveRolesAction.cs
 */
class XENLIB_EXPORT AddRemoveRolesAction : public AsyncOperation
{
    Q_OBJECT

    public:
        AddRemoveRolesAction(XenConnection* connection, QSharedPointer<Subject> subject, const QStringList& newRoleRefs, QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QString m_subjectRef;
        QStringList m_toAdd;
        QStringList m_toRemove;
};

#endif // ADACTIONS_H
