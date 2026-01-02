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

#ifndef XEN_CERTIFICATEMANAGER_H
#define XEN_CERTIFICATEMANAGER_H

#include "../../xenlib_global.h"
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslError>

class XENLIB_EXPORT XenCertificateManager : public QObject
{
    Q_OBJECT
    public:
        static XenCertificateManager* instance();

        explicit XenCertificateManager(QObject* parent = nullptr);
        ~XenCertificateManager();

        // Certificate validation
        bool validateCertificate(const QSslCertificate& certificate, const QString& hostname);
        QList<QSslError> getCertificateErrors(const QSslCertificate& certificate, const QString& hostname);

        // Certificate storage
        bool storeCertificate(const QSslCertificate& certificate, const QString& hostname);
        QSslCertificate getCertificate(const QString& hostname);
        bool removeCertificate(const QString& hostname);
        QStringList getStoredHosts();

        // Trust management
        bool isCertificateTrusted(const QSslCertificate& certificate, const QString& hostname);
        void trustCertificate(const QSslCertificate& certificate, const QString& hostname);
        void untrustCertificate(const QString& hostname);

        // Certificate information
        QString getCertificateInfo(const QSslCertificate& certificate);
        QString getCertificateFingerprint(const QSslCertificate& certificate);
        bool isCertificateExpired(const QSslCertificate& certificate);
        QDateTime getCertificateExpiry(const QSslCertificate& certificate);

        // Policy management
        void setValidationPolicy(bool allowSelfSigned, bool allowExpired);
        bool getAllowSelfSigned() const;
        bool getAllowExpired() const;

    signals:
        void certificateValidated(const QString& hostname, bool isValid);
        void certificateStored(const QString& hostname);
        void certificateRemoved(const QString& hostname);
        void certificateExpiringSoon(const QString& hostname, const QDateTime& expiry);

    private:
        static XenCertificateManager* s_instance;

        class Private;
        Private* d;

        void loadStoredCertificates();
        void saveStoredCertificates();
        void checkExpiringCertificates();
        bool matchesHostname(const QSslCertificate& certificate, const QString& hostname);
        bool matchesPattern(const QString& pattern, const QString& hostname);
};

#endif // XEN_CERTIFICATEMANAGER_H
