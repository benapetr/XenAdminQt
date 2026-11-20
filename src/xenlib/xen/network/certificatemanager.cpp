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

#include "certificatemanager.h"
#include "connection.h"
#include "connectionsmanager.h"
#include "../xenobject.h"
#include "../../xencache.h"
#include <QtCore/QCryptographicHash>
#include <QtNetwork/QSslSocket>
#include <QtCore/QThread>
#include <QtCore/QMutexLocker>
#include <QtCore/QMutex>
#include <QtCore/QCoreApplication>
#include <QtCore/QEventLoop>
#include <QtCore/QScopeGuard>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QDebug>

XenCertificateManager *XenCertificateManager::s_instance = nullptr;

class XenCertificateManager::Private
{
    public:
        // Trust on first use (C# Settings.KnownServers and warn settings)
        mutable QMutex trustMutex;
        QHash<QString, QString> knownServers;
        bool warnUnrecognized = true;
        bool warnChanged = true;
        PromptHandler promptHandler;
        QMutex promptMutex; // C# CertificateValidationLock: one decision at a time
};

XenCertificateManager *XenCertificateManager::instance()
{
    static QMutex instanceMutex;
    QMutexLocker locker(&instanceMutex);

    if (!XenCertificateManager::s_instance)
        XenCertificateManager::s_instance = new XenCertificateManager();

    return XenCertificateManager::s_instance;
}

XenCertificateManager::XenCertificateManager(QObject* parent) : QObject(parent), d(new Private)
{
}

XenCertificateManager::~XenCertificateManager()
{
    delete this->d;
}

void XenCertificateManager::SetPromptHandler(const PromptHandler& handler)
{
    QMutexLocker locker(&this->d->trustMutex);
    this->d->promptHandler = handler;
}

QHash<QString, QString> XenCertificateManager::GetKnownServers() const
{
    QMutexLocker locker(&this->d->trustMutex);
    return this->d->knownServers;
}

void XenCertificateManager::SetKnownServers(const QHash<QString, QString>& knownServers)
{
    QMutexLocker locker(&this->d->trustMutex);
    this->d->knownServers = knownServers;
}

void XenCertificateManager::SetWarnUnrecognizedCertificate(bool warn)
{
    QMutexLocker locker(&this->d->trustMutex);
    this->d->warnUnrecognized = warn;
}

bool XenCertificateManager::GetWarnUnrecognizedCertificate() const
{
    QMutexLocker locker(&this->d->trustMutex);
    return this->d->warnUnrecognized;
}

void XenCertificateManager::SetWarnChangedCertificate(bool warn)
{
    QMutexLocker locker(&this->d->trustMutex);
    this->d->warnChanged = warn;
}

bool XenCertificateManager::GetWarnChangedCertificate() const
{
    QMutexLocker locker(&this->d->trustMutex);
    return this->d->warnChanged;
}

QString XenCertificateManager::CertificateHashString(const QSslCertificate& certificate)
{
    return QString::fromLatin1(certificate.digest(QCryptographicHash::Sha1).toHex()).toUpper();
}

bool XenCertificateManager::IsKnownPoolCertificate(const QSslCertificate& certificate)
{
    if (certificate.isNull())
        return false;

    const QString fingerprint = QString::fromLatin1(certificate.digest(QCryptographicHash::Sha256).toHex()).toUpper();
    auto normalize = [](QString value) { return value.remove(QLatin1Char(':')).trimmed().toUpper(); };

    Xen::ConnectionsManager* manager = Xen::ConnectionsManager::instance();
    if (!manager)
        return false;

    // Also disconnected ones: their cache still only holds what a trusted pool reported
    for (XenConnection* connection : manager->GetAllConnections())
    {
        XenCache* cache = connection ? connection->GetCache() : nullptr;
        if (!cache)
            continue;

        for (const QSharedPointer<XenObject>& object : cache->GetAll(XenObjectType::Certificate))
        {
            if (!object)
                continue;
            const QVariantMap data = object->GetData();
            QString known = data.value(QStringLiteral("fingerprint_sha256")).toString();
            if (known.isEmpty())
                known = data.value(QStringLiteral("fingerprint")).toString(); // older XAPI: also SHA-256
            if (!known.isEmpty() && normalize(known) == fingerprint)
                return true;
        }
    }
    return false;
}

bool XenCertificateManager::IsUnrecordedPoolMember(const QString& hostname)
{
    if (hostname.isEmpty())
        return false;

    Xen::ConnectionsManager* manager = Xen::ConnectionsManager::instance();
    if (!manager)
        return false;

    for (XenConnection* connection : manager->GetAllConnections())
    {
        XenCache* cache = connection ? connection->GetCache() : nullptr;
        if (!cache)
            continue;

        QString hostRef;
        for (const QSharedPointer<XenObject>& host : cache->GetAll(XenObjectType::Host))
        {
            if (host && host->GetData().value(QStringLiteral("address")).toString().compare(hostname, Qt::CaseInsensitive) == 0)
            {
                hostRef = host->OpaqueRef();
                break;
            }
        }
        if (hostRef.isEmpty())
            continue;

        // A host the pool has a certificate record for must match it (IsKnownPoolCertificate)
        for (const QSharedPointer<XenObject>& object : cache->GetAll(XenObjectType::Certificate))
        {
            if (!object)
                continue;
            const QVariantMap data = object->GetData();
            const QString type = data.value(QStringLiteral("type")).toString();
            if (data.value(QStringLiteral("host")).toString() == hostRef && (type.isEmpty() || type == QLatin1String("host")))
                return false;
        }
        return true;
    }
    return false;
}

bool XenCertificateManager::IsTrustedChain(const QList<QSslError>& errors)
{
    for (const QSslError& error : errors)
    {
        if (error.error() != QSslError::HostNameMismatch)
            return false;
    }
    return true;
}

bool XenCertificateManager::ValidateServerCertificate(const QSslCertificate& certificate,
                                                      const QList<QSslError>& errors,
                                                      const QString& hostname)
{
    // C# Reference: XenAdmin.Network.SSL.ValidateServerCertificate
    // Callers only get here when Qt reported SSL errors (C# SslPolicyErrors != None)
    if (errors.isEmpty())
        return true;

    if (certificate.isNull())
    {
        qWarning() << "XenCertificateManager: no server certificate from" << hostname;
        return false;
    }

    const bool trusted = IsTrustedChain(errors);

    const QString hash = CertificateHashString(certificate);

    // Fast path without serialization: this exact certificate is already remembered
    {
        QMutexLocker locker(&this->d->trustMutex);
        if (this->d->knownServers.value(hostname) == hash)
            return true;
    }

    // A pool member's certificate, as reported by a connected pool: trust it and remember
    // it for this host name too. C# accepts any certificate on these connections
    // (HTTP.ValidateServerCertificate returns true); this keeps the check without a prompt.
    if (IsKnownPoolCertificate(certificate))
    {
        bool added = false;
        {
            QMutexLocker locker(&this->d->trustMutex);
            if (this->d->knownServers.value(hostname) != hash)
            {
                this->d->knownServers[hostname] = hash;
                added = true;
            }
        }
        if (added)
            emit this->trustSettingsChanged();
        return true;
    }

    // XAPI before 1.295 (e.g. XCP-ng 8.2) keeps a certificate record only for a host that
    // had a certificate installed, not for the default self-signed one. A pool member
    // without a record is accepted like C# does, but not remembered.
    if (IsUnrecordedPoolMember(hostname))
        return true;

    // C# CertificateValidationLock: one decision (and dialog) at a time, so a second
    // connection to the same host sees the first answer. A worker thread may hold this
    // while the UI shows its dialog, so the GUI thread must keep processing events
    // while it waits, or that dialog could never run.
    QCoreApplication* app = QCoreApplication::instance();
    if (app && QThread::currentThread() == app->thread())
    {
        while (!this->d->promptMutex.tryLock(50))
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    } else
    {
        this->d->promptMutex.lock();
    }
    const auto unlockPrompt = qScopeGuard([this]() { this->d->promptMutex.unlock(); });

    PromptHandler handler;
    bool warnUnrecognized = true;
    bool warnChanged = true;
    bool known = false;
    QString knownHash;
    {
        QMutexLocker locker(&this->d->trustMutex);
        handler = this->d->promptHandler;
        warnUnrecognized = this->d->warnUnrecognized;
        warnChanged = this->d->warnChanged;
        known = this->d->knownServers.contains(hostname);
        knownHash = this->d->knownServers.value(hostname);
    }

    // C#: "This allows to run tests without MainWindow"
    if (!handler)
        return true;

    auto remember = [this, &hostname, &hash](bool* warnFlag)
    {
        {
            QMutexLocker locker(&this->d->trustMutex);
            this->d->knownServers[hostname] = hash;
            if (warnFlag)
                *warnFlag = false;
        }
        emit this->trustSettingsChanged();
    };

    if (known)
    {
        if (knownHash == hash)
            return true;

        if (!warnChanged)
        {
            qDebug() << "XenCertificateManager: updating changed certificate silently for" << hostname;
            remember(nullptr);
            return true;
        }

        const PromptResult result = handler(CertificatePrompt::Changed, certificate, hostname, trusted);
        if (!result.accept)
        {
            qWarning() << "XenCertificateManager: user rejected changed certificate for" << hostname;
            return false;
        }
        remember(result.stopWarning ? &this->d->warnChanged : nullptr);
        return true;
    }

    if (!warnUnrecognized)
    {
        // User has chosen to accept new certificates
        qDebug() << "XenCertificateManager: adding new certificate silently for" << hostname;
        remember(nullptr);
        return true;
    }

    const PromptResult result = handler(CertificatePrompt::Unknown, certificate, hostname, trusted);
    if (!result.accept)
    {
        qWarning() << "XenCertificateManager: user rejected new certificate for" << hostname;
        return false;
    }
    remember(result.stopWarning ? &this->d->warnUnrecognized : nullptr);
    return true;
}

void XenCertificateManager::GuardReply(QNetworkReply* reply, const QString& hostname)
{
    if (!reply)
        return;

    QObject::connect(reply, &QNetworkReply::sslErrors, reply, [reply, hostname](const QList<QSslError>& errors)
    {
        if (XenCertificateManager::instance()->ValidateServerCertificate(reply->sslConfiguration().peerCertificate(),
                                                                        errors, hostname))
        {
            reply->ignoreSslErrors();
        }
    }, Qt::DirectConnection);
}
