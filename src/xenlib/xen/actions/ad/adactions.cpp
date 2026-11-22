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

#include "adactions.h"
#include "../../host.h"
#include "../../pool.h"
#include "../../role.h"
#include "../../session.h"
#include "../../subject.h"
#include "../../network/connection.h"
#include "../../xenapi/xenapi_Auth.h"
#include "../../xenapi/xenapi_Pool.h"
#include "../../xenapi/xenapi_Session.h"
#include "../../xenapi/xenapi_Subject.h"
#include "../../../xencache.h"
#include <QDebug>
#include <QRegularExpression>

namespace
{
    QString nameOf(XenConnection* connection)
    {
        QString name = connection ? connection->GetName() : QString();
        return name.length() > 50 ? name.left(49) + QChar(0x2026) : name;
    }

    //! C#: Pool = Helpers.GetPool(connection), else Host = the coordinator
    void setPoolOrHost(AsyncOperation* op, XenConnection* connection)
    {
        XenCache* cache = connection ? connection->GetCache() : nullptr;
        if (!cache)
            return;
        if (QSharedPointer<Pool> pool = cache->GetPool())
            op->SetPool(pool);
        else if (QSharedPointer<Pool> poolOfOne = cache->GetPoolOfOne())
            op->SetHost(poolOfOne->GetMasterHost());
    }

    QString subjectLabel(const QString& displayName, const QString& name, const QString& sid)
    {
        if (!displayName.isEmpty())
            return displayName;
        return !name.isEmpty() ? name : sid;
    }
}

// EnableAdAction

EnableAdAction::EnableAdAction(XenConnection* connection, const QString& domain, const QString& user, const QString& password, QObject* parent)
    : AsyncOperation(connection, tr("Enabling Active Directory Authentication on pool '%1'").arg(nameOf(connection)),
                     tr("Enabling Active Directory Authentication"), parent),
      m_domain(domain), m_user(user), m_password(password)
{
    setPoolOrHost(this, connection);
    this->AddApiMethodToRoleCheck("pool.enable_external_auth");
    this->AddApiMethodToRoleCheck("pool.disable_external_auth");
}

void EnableAdAction::Enable(XenAPI::Session* session, XenConnection* connection,
                            const QString& domain, const QString& user, const QString& password)
{
    if (domain.isEmpty() || user.isEmpty())
        throw std::runtime_error("A domain and a user name are needed to join a domain");

    XenCache* cache = connection ? connection->GetCache() : nullptr;
    QSharedPointer<Pool> pool = cache ? cache->GetPoolOfOne() : QSharedPointer<Pool>();
    if (!pool)
        throw std::runtime_error("The pool could not be found");

    QVariantMap config;
    config["domain"] = domain;  // C#: redundant now, kept for the old way of passing the domain
    config["user"] = user;
    config["pass"] = password;

    try
    {
        try
        {
            // CA-48122: disable first in case it wasn't disabled properly
            XenAPI::Pool::disable_external_auth(session, pool->OpaqueRef(), QVariantMap());
        } catch (const std::exception& e)
        {
            qDebug() << "EnableAdAction: disabling AD before enabling it failed; ignored, it's best effort only:" << e.what();
        }

        XenAPI::Pool::enable_external_auth(session, pool->OpaqueRef(), config, domain, XenAPI::Auth::AUTH_TYPE_AD);
    } catch (const Failure& f)
    {
        const QStringList description = f.errorDescription();
        const QString code = f.errorCode();

        // CA-37255 CA-38369 CA-39485: likewise returns WinError.h codes that mean nothing to
        // users; C# logs them and shows the failure with the server's name
        if (code == Failure::AUTH_ENABLE_FAILED && description.size() > 2)
        {
            const QRegularExpressionMatch m = QRegularExpression("^([1-9]+) \\(0x.*\\).*").match(description.at(2));
            if (!m.hasMatch())
                throw;
            qWarning() << "EnableAdAction: received error" << m.captured(1) << "from likewise when joining the domain:" << description;
        }

        QSharedPointer<Host> host = description.size() > 1 && cache
            ? cache->ResolveObject<Host>(XenObjectType::Host, description.at(1)) : QSharedPointer<Host>();
        if (!host)
            throw;

        if (code == Failure::POOL_AUTH_ENABLE_FAILED_WRONG_CREDENTIALS)
            throw CredentialsFailure(description);

        // C# AD_FAILURE_WITH_HOST
        throw std::runtime_error(tr("%1\nHost: %2").arg(f.message(), host->GetName()).toStdString());
    }
}

void EnableAdAction::run()
{
    try
    {
        Enable(this->GetSession(), this->GetConnection(), this->m_domain, this->m_user, this->m_password);
        this->SetDescription(tr("Completed"));
    } catch (const CredentialsFailure& f)
    {
        this->m_credentialsFailed = true;
        this->setError(f.message(), f.errorDescription());
    } catch (const Failure& f)
    {
        this->setError(f.message(), f.errorDescription());
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}

// DisableAdAction

DisableAdAction::DisableAdAction(XenConnection* connection, const QVariantMap& creds, QObject* parent)
    : AsyncOperation(connection, tr("Disabling Active Directory Authentication on pool '%1'").arg(nameOf(connection)),
                     tr("Disabling Active Directory Authentication"), parent),
      m_creds(creds)
{
    setPoolOrHost(this, connection);
    this->AddApiMethodToRoleCheck("pool.disable_external_auth");
}

void DisableAdAction::run()
{
    try
    {
        QSharedPointer<Pool> pool = this->GetConnection()->GetCache()->GetPoolOfOne();
        if (!pool)
            throw std::runtime_error("The pool could not be found");
        XenAPI::Pool::disable_external_auth(this->GetSession(), pool->OpaqueRef(), this->m_creds);
        this->SetDescription(tr("Completed"));
    } catch (const Failure& f)
    {
        this->setError(f.message(), f.errorDescription());
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}

// AddRemoveSubjectsAction

AddRemoveSubjectsAction::AddRemoveSubjectsAction(XenConnection* connection, const QStringList& namesToAdd,
                                                 const QList<QSharedPointer<Subject>>& subjectsToRemove, QObject* parent)
    : AsyncOperation(connection, tr("Adding/removing users from Active Directory ACL on '%1'").arg(nameOf(connection)),
                     tr("Adding/removing users from Active Directory ACL"), parent)
{
    setPoolOrHost(this, connection);

    // C# clears out duplicate names
    for (const QString& name : namesToAdd)
    {
        if (!this->m_namesToAdd.contains(name))
            this->m_namesToAdd.append(name);
    }
    for (const QSharedPointer<Subject>& subject : subjectsToRemove)
    {
        if (subject)
            this->m_toRemove.append({subject->OpaqueRef(), subject->SubjectIdentifier()});
    }

    // We remove ourselves last and log out by hand; work out who we are now, on this thread
    XenAPI::Session* session = connection ? connection->GetSession() : nullptr;
    if (session && !session->IsLocalSuperuser() && !session->GetSessionSubject().isEmpty())
    {
        QSharedPointer<Subject> self = connection->GetCache()->ResolveObject<Subject>(XenObjectType::Subject, session->GetSessionSubject());
        if (self)
            this->m_selfSid = self->SubjectIdentifier();
    }

    if (!this->m_namesToAdd.isEmpty())
        this->AddApiMethodToRoleCheck("subject.create");
    if (!this->m_toRemove.isEmpty())
    {
        this->AddApiMethodToRoleCheck("session.logout_subject_identifier");
        this->AddApiMethodToRoleCheck("subject.destroy");
    }
}

void AddRemoveSubjectsAction::run()
{
    // C#: resolve each name, add the ones found, then remove
    this->m_steps = qMax(1, int(this->m_namesToAdd.size() * 2 + this->m_toRemove.size()));
    this->m_stepsComplete = 0;
    XenAPI::Session* session = this->GetSession();
    auto step = [this]()
    {
        ++this->m_stepsComplete;
        this->SetPercentComplete(100 * this->m_stepsComplete / this->m_steps);
    };

    // Resolve
    QStringList sidsToAdd;
    for (const QString& name : this->m_namesToAdd)
    {
        QString sid, resolvedName, error;
        try
        {
            sid = XenAPI::Auth::get_subject_identifier(session, name);
            sidsToAdd.append(sid);
            const QVariantMap info = XenAPI::Auth::get_subject_information_from_identifier(session, sid);
            resolvedName = info.contains(Subject::SUBJECT_NAME_KEY) ? info.value(Subject::SUBJECT_NAME_KEY).toString() : tr("<unknown AD user>");
        } catch (const std::exception& e)
        {
            error = QString::fromUtf8(e.what());
            this->m_lastError = error;
            qWarning() << "AddRemoveSubjectsAction: Exception resolving AD user" << name << ":" << error;
        }
        emit this->nameResolveComplete(name, resolvedName, error.isEmpty() ? sid : QString(), error);
        step();
    }
    emit this->allResolveComplete();

    // Add
    for (const QString& sid : sidsToAdd)
    {
        QString name, error;
        try
        {
            const QVariantMap info = XenAPI::Auth::get_subject_information_from_identifier(session, sid);
            name = subjectLabel(info.value(Subject::SUBJECT_DISPLAYNAME_KEY).toString(), info.value(Subject::SUBJECT_NAME_KEY).toString(), sid);
            if (Subject::GetBySID(this->GetConnection(), sid))
            {
                qWarning() << "AddRemoveSubjectsAction: A Subject with sid" << sid << "already exists on the server.";
                throw std::runtime_error(tr("Subject '%1' already has access to the server.")
                                             .arg(name.length() > 50 ? name.left(49) + QChar(0x2026) : name).toStdString());
            }
            XenAPI::Subject::create(session, QVariantMap{{"subject_identifier", sid}, {"other_config", info}});
        } catch (const std::exception& e)
        {
            error = QString::fromUtf8(e.what());
            this->m_lastError = error;
            qWarning() << "AddRemoveSubjectsAction: Exception adding AD user to subject list:" << error;
        }
        emit this->subjectAddComplete(sid, name, error);
        step();
    }

    // Remove
    bool logoutSelf = false;
    for (const Removal& removal : this->m_toRemove)
    {
        QString error;
        try
        {
            if (!this->m_selfSid.isEmpty() && removal.sid == this->m_selfSid)
                logoutSelf = true;  // C#: we log ourselves out at the end
            else
                XenAPI::SessionAPI::logout_subject_identifier(session, removal.sid);
            XenAPI::Subject::destroy(session, removal.ref);
        } catch (const std::exception& e)
        {
            error = QString::fromUtf8(e.what());
            this->m_lastError = error;
            qWarning() << "AddRemoveSubjectsAction: Exception removing AD user from subject list:" << error;
        }
        emit this->subjectRemoveComplete(removal.sid, error);
        step();
    }

    // C# keeps only the latest exception and refers the user to the logs
    this->SetDescription(this->m_lastError.isEmpty() ? tr("Completed") : tr("Completed, with some errors encountered. See the application log files for more information."));
    if (!this->m_lastError.isEmpty())
        this->setError(this->m_lastError);

    if (logoutSelf)
    {
        XenConnection* connection = this->GetConnection();
        QMetaObject::invokeMethod(connection, [connection]() { connection->EndConnect(true, false); }, Qt::QueuedConnection);
    }
}

// AddRemoveRolesAction

AddRemoveRolesAction::AddRemoveRolesAction(XenConnection* connection, QSharedPointer<Subject> subject, const QStringList& newRoleRefs, QObject* parent)
    : AsyncOperation(connection,
                     tr("Assigning new roles to subject '%1'").arg(subjectLabel(subject->DisplayName(), subject->SubjectName(), subject->SubjectIdentifier()).left(50)),
                     tr("Assigning new roles to subject"), parent),
      m_subjectRef(subject->OpaqueRef())
{
    setPoolOrHost(this, connection);

    // C#: the roles a subject can hold have subroles; newer servers also have internal ones to skip
    const QStringList current = subject->RoleRefs();
    for (const QSharedPointer<Role>& role : connection->GetCache()->GetAll<Role>(XenObjectType::Role))
    {
        if (!role || role->IsInternal() || role->SubroleRefs().isEmpty())
            continue;
        const QString ref = role->OpaqueRef();
        if (newRoleRefs.contains(ref) && !current.contains(ref))
            this->m_toAdd.append(ref);
        else if (!newRoleRefs.contains(ref) && current.contains(ref))
            this->m_toRemove.append(ref);
    }

    if (!this->m_toAdd.isEmpty())
        this->AddApiMethodToRoleCheck("subject.add_to_roles");
    if (!this->m_toRemove.isEmpty())
        this->AddApiMethodToRoleCheck("subject.remove_from_roles");
}

void AddRemoveRolesAction::run()
{
    try
    {
        const int count = qMax(1, int(this->m_toAdd.size() + this->m_toRemove.size()));
        int done = 0;
        for (const QString& role : this->m_toAdd)
        {
            XenAPI::Subject::add_to_roles(this->GetSession(), this->m_subjectRef, role);
            this->SetPercentComplete(100 * ++done / count);
        }
        for (const QString& role : this->m_toRemove)
        {
            XenAPI::Subject::remove_from_roles(this->GetSession(), this->m_subjectRef, role);
            this->SetPercentComplete(100 * ++done / count);
        }
        this->SetDescription(tr("Completed"));
    } catch (const Failure& f)
    {
        this->setError(f.message(), f.errorDescription());
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}
