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
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtCore/QCryptographicHash>
#include <QtNetwork/QSslSocket>

class XenCertificateManager::Private
{
    public:
        QHash<QString, QSslCertificate> storedCertificates;
        QStringList trustedHosts;
        bool allowSelfSigned = true; // XenServer/XCP-ng typically use self-signed certs
        bool allowExpired = false;
        QString certificateStorePath;
};

XenCertificateManager::XenCertificateManager(QObject* parent) : QObject(parent), d(new Private)
{
    // Set up certificate storage path
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(appDataPath);
    this->d->certificateStorePath = appDataPath + "/certificates";
    QDir().mkpath(this->d->certificateStorePath);

    this->loadStoredCertificates();
}

XenCertificateManager::~XenCertificateManager()
{
    this->saveStoredCertificates();
    delete this->d;
}

bool XenCertificateManager::validateCertificate(const QSslCertificate& certificate, const QString& hostname)
{
    if (certificate.isNull())
    {
        return false;
    }

    // Check expiry
    if (certificate.expiryDate() < QDateTime::currentDateTime())
    {
        if (!this->d->allowExpired)
        {
            return false;
        }
    }

    // Check if self-signed
    bool isSelfSigned = (certificate.issuerInfo(QSslCertificate::CommonName).join("") ==
                         certificate.subjectInfo(QSslCertificate::CommonName).join(""));

    if (isSelfSigned && this->d->allowSelfSigned)
    {
        // For self-signed certificates in XenServer/XCP-ng environments,
        // we accept them even if hostname doesn't match (common when using IP addresses)
        emit this->certificateValidated(hostname, true);
        return true;
    }
    if (isSelfSigned && !this->d->allowSelfSigned)
    {
        // Check if explicitly trusted
        if (!this->isCertificateTrusted(certificate, hostname))
        {
            return false;
        }
    }

    // Check hostname match (including wildcard support)
    bool hostnameMatches = this->matchesHostname(certificate, hostname);

    emit this->certificateValidated(hostname, hostnameMatches);
    return hostnameMatches;
}

QList<QSslError> XenCertificateManager::getCertificateErrors(const QSslCertificate& certificate, const QString& hostname)
{
    Q_UNUSED(hostname) // TODO: Use hostname for validation
    QList<QSslError> errors;

    if (certificate.isNull())
    {
        errors.append(QSslError(QSslError::CertificateUntrusted, certificate));
    }

    // Note: isBlacklisted() is deprecated in Qt6, skip for now
    // if (certificate.isBlacklisted())
    // {
    //     errors.append(QSslError(QSslError::CertificateBlacklisted, certificate));
    // }

    if (certificate.expiryDate() < QDateTime::currentDateTime())
    {
        errors.append(QSslError(QSslError::CertificateExpired, certificate));
    }

    if (certificate.effectiveDate() > QDateTime::currentDateTime())
    {
        errors.append(QSslError(QSslError::CertificateNotYetValid, certificate));
    }

    // Check self-signed
    bool isSelfSigned = (certificate.issuerInfo(QSslCertificate::CommonName).join("") ==
                         certificate.subjectInfo(QSslCertificate::CommonName).join(""));

    if (isSelfSigned)
    {
        errors.append(QSslError(QSslError::SelfSignedCertificate, certificate));
    }

    return errors;
}

bool XenCertificateManager::storeCertificate(const QSslCertificate& certificate, const QString& hostname)
{
    this->d->storedCertificates[hostname] = certificate;

    // Save to file
    QString certPath = this->d->certificateStorePath + "/" + hostname + ".crt";
    QFile certFile(certPath);
    if (certFile.open(QIODevice::WriteOnly))
    {
        certFile.write(certificate.toPem());
        certFile.close();

        emit this->certificateStored(hostname);
        return true;
    }

    return false;
}

QSslCertificate XenCertificateManager::getCertificate(const QString& hostname)
{
    return this->d->storedCertificates.value(hostname);
}

bool XenCertificateManager::removeCertificate(const QString& hostname)
{
    this->d->storedCertificates.remove(hostname);
    this->d->trustedHosts.removeAll(hostname);

    // Remove file
    QString certPath = this->d->certificateStorePath + "/" + hostname + ".crt";
    QFile::remove(certPath);

    emit this->certificateRemoved(hostname);
    return true;
}

QStringList XenCertificateManager::getStoredHosts()
{
    return this->d->storedCertificates.keys();
}

bool XenCertificateManager::isCertificateTrusted(const QSslCertificate& certificate, const QString& hostname)
{
    Q_UNUSED(certificate)
    return this->d->trustedHosts.contains(hostname);
}

void XenCertificateManager::trustCertificate(const QSslCertificate& certificate, const QString& hostname)
{
    this->storeCertificate(certificate, hostname);

    if (!this->d->trustedHosts.contains(hostname))
    {
        this->d->trustedHosts.append(hostname);
    }
}

void XenCertificateManager::untrustCertificate(const QString& hostname)
{
    this->d->trustedHosts.removeAll(hostname);
    this->removeCertificate(hostname);
}

QString XenCertificateManager::getCertificateInfo(const QSslCertificate& certificate)
{
    QString info;
    info += "Subject: " + certificate.subjectInfo(QSslCertificate::CommonName).join(", ") + "\n";
    info += "Issuer: " + certificate.issuerInfo(QSslCertificate::CommonName).join(", ") + "\n";
    info += "Valid From: " + certificate.effectiveDate().toString() + "\n";
    info += "Valid To: " + certificate.expiryDate().toString() + "\n";
    info += "Serial Number: " + QString::fromUtf8(certificate.serialNumber()) + "\n";
    info += "Fingerprint (SHA1): " + this->getCertificateFingerprint(certificate) + "\n";

    QStringList altNames = certificate.subjectAlternativeNames().values();
    if (!altNames.isEmpty())
    {
        info += "Alternative Names: " + altNames.join(", ") + "\n";
    }

    return info;
}

QString XenCertificateManager::getCertificateFingerprint(const QSslCertificate& certificate)
{
    QByteArray certData = certificate.toDer();
    QByteArray hash = QCryptographicHash::hash(certData, QCryptographicHash::Sha1);
    return hash.toHex(':').toUpper();
}

bool XenCertificateManager::isCertificateExpired(const QSslCertificate& certificate)
{
    return certificate.expiryDate() < QDateTime::currentDateTime();
}

QDateTime XenCertificateManager::getCertificateExpiry(const QSslCertificate& certificate)
{
    return certificate.expiryDate();
}

void XenCertificateManager::setValidationPolicy(bool allowSelfSigned, bool allowExpired)
{
    this->d->allowSelfSigned = allowSelfSigned;
    this->d->allowExpired = allowExpired;
}

bool XenCertificateManager::getAllowSelfSigned() const
{
    return this->d->allowSelfSigned;
}

bool XenCertificateManager::getAllowExpired() const
{
    return this->d->allowExpired;
}

void XenCertificateManager::loadStoredCertificates()
{
    QDir certDir(this->d->certificateStorePath);
    QStringList certFiles = certDir.entryList(QStringList() << "*.crt", QDir::Files);

    for (const QString& certFile : certFiles)
    {
        QString hostname = certFile;
        hostname.chop(4); // Remove .crt extension

        QFile file(certDir.absoluteFilePath(certFile));
        if (file.open(QIODevice::ReadOnly))
        {
            QByteArray certData = file.readAll();
            QSslCertificate certificate(certData, QSsl::Pem);

            if (!certificate.isNull())
            {
                this->d->storedCertificates[hostname] = certificate;
                this->d->trustedHosts.append(hostname);
            }

            file.close();
        }
    }

    this->checkExpiringCertificates();
}

void XenCertificateManager::saveStoredCertificates()
{
    QSettings settings;
    settings.beginGroup("TrustedCertificates");
    settings.setValue("trustedHosts", this->d->trustedHosts);
    settings.endGroup();
}

void XenCertificateManager::checkExpiringCertificates()
{
    QDateTime nowPlus30Days = QDateTime::currentDateTime().addDays(30);

    QHashIterator<QString, QSslCertificate> it(this->d->storedCertificates);
    while (it.hasNext())
    {
        it.next();
        const QSslCertificate& cert = it.value();

        if (cert.expiryDate() <= nowPlus30Days)
        {
            emit this->certificateExpiringSoon(it.key(), cert.expiryDate());
        }
    }
}

bool XenCertificateManager::matchesHostname(const QSslCertificate& certificate, const QString& hostname)
{
    // Check Common Name
    QStringList commonNames = certificate.subjectInfo(QSslCertificate::CommonName);
    for (const QString& name : commonNames)
    {
        if (this->matchesPattern(name, hostname))
        {
            return true;
        }
    }

    // Check Subject Alternative Names
    QStringList altNames = certificate.subjectAlternativeNames().values();
    for (const QString& name : altNames)
    {
        if (this->matchesPattern(name, hostname))
        {
            return true;
        }
    }

    return false;
}

bool XenCertificateManager::matchesPattern(const QString& pattern, const QString& hostname)
{
    // Exact match
    if (pattern.compare(hostname, Qt::CaseInsensitive) == 0)
    {
        return true;
    }

    // Wildcard match (*.example.com matches subdomain.example.com)
    if (pattern.startsWith("*."))
    {
        QString wildcardDomain = pattern.mid(2); // Remove "*."
        if (hostname.endsWith(wildcardDomain, Qt::CaseInsensitive))
        {
            // Make sure there's exactly one more level
            QString prefix = hostname.left(hostname.length() - wildcardDomain.length());
            if (prefix.endsWith("."))
            {
                prefix.chop(1);
                return !prefix.contains('.');
            }
        }
    }

    return false;
}
