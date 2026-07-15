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

#include "installservercertificateaction.h"
#include "../../certificate.h"
#include "../../failure.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_Certificate.h"
#include "../../xenapi/xenapi_Host.h"
#include "../../../xencache.h"
#include <QDateTime>
#include <QFile>
#include <QLocale>
#include <QThread>
#include <stdexcept>

namespace
{
    QString readTextFile(const QString& path, QString* error)
    {
        QFile file(path);
        if (!file.exists())
        {
            if (error)
                *error = QObject::tr("The path does not exist.");
            return QString();
        }

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            if (error)
                *error = QObject::tr("The file could not be read.");
            return QString();
        }

        return QString::fromUtf8(file.readAll());
    }

    QString certificateUuidFromRecord(const QVariantMap& certificateRecord)
    {
        return certificateRecord.value(QStringLiteral("uuid")).toString();
    }

    QString firstCertificateRefFromHostRecord(const QVariantMap& hostRecord)
    {
        const QVariant certificates = hostRecord.value(QStringLiteral("certificates"));
        const QVariantList list = certificates.toList();
        return list.isEmpty() ? QString() : list.first().toString();
    }

    QString friendlyCertificateDate(const QString& date)
    {
        const QDateTime parsed = QDateTime::fromString(date, Qt::ISODate);
        if (!parsed.isValid())
            return date;
        return QLocale::system().toString(parsed.toLocalTime(), QLocale::ShortFormat);
    }
}

InstallServerCertificateAction::InstallServerCertificateAction(const QSharedPointer<Host>& host,
                                                               const QString& keyFile,
                                                               const QString& certificateFile,
                                                               const QStringList& chainFiles,
                                                               QObject* parent)
    : AsyncOperation(host ? host->GetConnection() : nullptr,
                     tr("Installing server certificate on %1").arg(host ? host->GetName() : tr("server")),
                     tr("Installing server certificate..."),
                     parent),
      m_host(host),
      m_hostRef(host ? host->OpaqueRef() : QString()),
      m_keyFile(keyFile),
      m_certificateFile(certificateFile),
      m_chainFiles(chainFiles)
{
    this->SetHost(host);
    this->SetCanCancel(false);
    this->AddApiMethodToRoleCheck(QStringLiteral("host.install_server_certificate"));

    if (host && host->GetConnection() && host->GetConnection()->GetCache())
    {
        const QStringList certRefs = host->CertificateRefs();
        if (!certRefs.isEmpty())
        {
            QSharedPointer<Certificate> cert = host->GetConnection()->GetCache()->ResolveObject<Certificate>(XenObjectType::Certificate, certRefs.first());
            if (cert)
                this->m_oldCertificateUuid = cert->GetUUID();
        }
    }
}

QString InstallServerCertificateAction::KeyError() const
{
    return this->m_keyError;
}

QString InstallServerCertificateAction::CertificateError() const
{
    return this->m_certificateError;
}

QString InstallServerCertificateAction::ChainError() const
{
    return this->m_chainError;
}

void InstallServerCertificateAction::run()
{
    QString privateKey;
    QString certificate;
    QString chain;
    this->collectFileContents(&privateKey, &certificate, &chain);

    if (!this->m_host || !this->m_host->IsLive())
        throw std::runtime_error(tr("The host is unreachable.").toLocal8Bit().constData());

    XenConnection* connection = this->GetConnection();
    XenAPI::Session* session = this->GetSession();
    if (!connection || !session)
        throw std::runtime_error(tr("No active XenAPI session is available.").toLocal8Bit().constData());

    const bool previousExpectDisruption = connection->GetExpectDisruption();
    connection->SetExpectDisruption(true);
    XenAPI::Session* reconnectSession = nullptr;

    try
    {
        const bool gotResponse = XenAPI::Host::install_server_certificate(session, this->m_hostRef, certificate, privateKey, chain);
        if (!gotResponse)
        {
            this->SetDescription(tr("Waiting for server to reconnect after certificate installation..."));
            this->SetPercentComplete(60);
            connection->Interrupt();
            reconnectSession = this->waitForReconnect();
            session = reconnectSession;
        } else
        {
            this->SetPercentComplete(50);
        }

        this->waitForNewCertificate(session);
    } catch (const Failure& failure)
    {
        const QStringList details = failure.errorDescription();
        this->mapFailure(details);
        this->setError(failure.message(), details);
    } catch (...)
    {
        connection->SetExpectDisruption(previousExpectDisruption);
        if (reconnectSession)
            reconnectSession->Logout();
        delete reconnectSession;
        throw;
    }

    connection->SetExpectDisruption(previousExpectDisruption);
    if (reconnectSession)
        reconnectSession->Logout();
    delete reconnectSession;

    if (this->HasError())
        return;

    this->SetDescription(tr("Server certificate installed."));
    this->SetPercentComplete(100);
}

void InstallServerCertificateAction::collectFileContents(QString* privateKey, QString* certificate, QString* chain)
{
    const int fileCount = 2 + this->m_chainFiles.count();
    int completed = 0;

    QString error;
    *privateKey = readTextFile(this->m_keyFile, &error);
    if (!error.isEmpty())
    {
        this->m_keyError = error;
        throw std::runtime_error(error.toLocal8Bit().constData());
    }
    this->SetPercentComplete(++completed * 30 / fileCount);

    *certificate = readTextFile(this->m_certificateFile, &error);
    if (!error.isEmpty())
    {
        this->m_certificateError = error;
        throw std::runtime_error(error.toLocal8Bit().constData());
    }
    this->SetPercentComplete(++completed * 30 / fileCount);

    QStringList chainContents;
    for (const QString& chainFile : this->m_chainFiles)
    {
        const QString content = readTextFile(chainFile, &error);
        if (!error.isEmpty())
        {
            this->m_chainError = tr("%1: %2").arg(chainFile, error);
            throw std::runtime_error(this->m_chainError.toLocal8Bit().constData());
        }
        chainContents.append(content);
        this->SetPercentComplete(++completed * 30 / fileCount);
    }

    *chain = chainContents.join(QStringLiteral("\n"));
}

XenAPI::Session* InstallServerCertificateAction::waitForReconnect()
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        throw std::runtime_error(tr("No active XenAPI connection is available.").toLocal8Bit().constData());

    const QDateTime start = QDateTime::currentDateTimeUtc();
    while (start.msecsTo(QDateTime::currentDateTimeUtc()) < 120000)
    {
        if (connection->IsConnected() && connection->GetSession() && connection->GetSession()->IsLoggedIn())
        {
            XenAPI::Session* duplicate = XenAPI::Session::DuplicateSession(connection->GetSession(), nullptr);
            if (duplicate)
                return duplicate;
        }

        QThread::sleep(5);
    }

    throw std::runtime_error(tr("Timed out waiting for the server to reconnect.").toLocal8Bit().constData());
}

void InstallServerCertificateAction::waitForNewCertificate(XenAPI::Session* session)
{
    this->SetDescription(tr("Waiting for server certificate refresh..."));
    const QDateTime start = QDateTime::currentDateTimeUtc();

    while (start.msecsTo(QDateTime::currentDateTimeUtc()) < 120000)
    {
        QVariantMap hostResult;
        try
        {
            hostResult = XenAPI::Host::get_record(session, this->m_hostRef);
        } catch (const std::exception&)
        {
            return;
        }

        const QString certRef = firstCertificateRefFromHostRecord(hostResult);
        if (!certRef.isEmpty())
        {
            QVariantMap certResult;
            try
            {
                certResult = XenAPI::Certificate::get_record(session, certRef);
            } catch (const std::exception&)
            {
                return;
            }

            const QString newUuid = certificateUuidFromRecord(certResult);
            if (!newUuid.isEmpty() && newUuid != this->m_oldCertificateUuid)
                return;
        }

        this->SetPercentComplete(qMin(95, this->GetPercentComplete() + 5));
        QThread::sleep(3);
    }
}

void InstallServerCertificateAction::mapFailure(const QStringList& errorDetails)
{
    if (errorDetails.isEmpty())
        return;

    const QString code = errorDetails.first();
    const QString message = Failure(errorDetails).message();

    if (code == QLatin1String("SERVER_CERTIFICATE_KEY_ALGORITHM_NOT_SUPPORTED") ||
        code == QLatin1String("SERVER_CERTIFICATE_KEY_INVALID") ||
        code == QLatin1String("SERVER_CERTIFICATE_KEY_MISMATCH") ||
        code == QLatin1String("SERVER_CERTIFICATE_KEY_RSA_LENGTH_NOT_SUPPORTED") ||
        code == QLatin1String("SERVER_CERTIFICATE_KEY_RSA_MULTI_NOT_SUPPORTED"))
    {
        this->m_keyError = message;
    } else if (code == QLatin1String("SERVER_CERTIFICATE_CHAIN_INVALID"))
    {
        this->m_chainError = message;
    } else if (code == QLatin1String("SERVER_CERTIFICATE_NOT_VALID_YET"))
    {
        this->m_certificateError = errorDetails.count() > 2
            ? tr("The certificate is not valid until %1.").arg(friendlyCertificateDate(errorDetails[2]))
            : message;
    } else if (code == QLatin1String("SERVER_CERTIFICATE_EXPIRED"))
    {
        this->m_certificateError = errorDetails.count() > 2
            ? tr("The certificate expired on %1.").arg(friendlyCertificateDate(errorDetails[2]))
            : message;
    } else if (code == QLatin1String("SERVER_CERTIFICATE_INVALID") ||
               code == QLatin1String("SERVER_CERTIFICATE_SIGNATURE_NOT_SUPPORTED"))
    {
        this->m_certificateError = message;
    }
}
