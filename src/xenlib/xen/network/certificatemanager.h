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
#include <QtCore/QHash>
#include <functional>

class QNetworkReply;

class XENLIB_EXPORT XenCertificateManager : public QObject
{
    Q_OBJECT
    public:
        static XenCertificateManager* instance();

        explicit XenCertificateManager(QObject* parent = nullptr);
        ~XenCertificateManager();

        // ── Server certificate trust (C# XenAdmin.Network.SSL) ──────────────────
        //
        // TLS connections keep Qt's normal peer verification. A certificate that verifies
        // against the system CAs and matches the host raises no SSL errors and is used as
        // is. When Qt reports SSL errors (C# SslPolicyErrors != None), the connection's
        // sslErrors handler calls ValidateServerCertificate() and ignores the errors only
        // if it returns true. That trusts the certificate on first use: its SHA-1 hash is
        // remembered per host, and an unknown or changed certificate is passed to the
        // prompt handler unless the matching "warn" setting is off.
        //
        // QSslCertificate::verify() is not used: the macOS Secure Transport backend does
        // not support it and reports no errors for any certificate.

        enum class CertificatePrompt
        {
            Unknown,  //!< First connection to this host (C# UnknownCertificateDialog)
            Changed   //!< Known host presents a different certificate (C# CertificateChangedDialog)
        };

        struct PromptResult
        {
            bool accept = false;        //!< Connect using this certificate
            bool stopWarning = false;   //!< "Automatically accept new" / "Always ignore" was ticked
        };

        //! Asks the user about a certificate. Called on the validating thread; the UI
        //! implementation must show its dialog on the GUI thread.
        using PromptHandler = std::function<PromptResult(CertificatePrompt prompt, const QSslCertificate& certificate,
                                                         const QString& hostname, bool trusted)>;

        //! With no handler every certificate is accepted (C#: no MainWindow, e.g. tests)
        void SetPromptHandler(const PromptHandler& handler);

        //! C# SSL.ValidateServerCertificate, called from an sslErrors handler with Qt's
        //! verification errors. Returns true if the errors may be ignored.
        bool ValidateServerCertificate(const QSslCertificate& certificate, const QList<QSslError>& errors,
                                       const QString& hostname);

        //! For QNetworkAccessManager users: ignore the reply's SSL errors only if
        //! ValidateServerCertificate() accepts its certificate
        void GuardReply(QNetworkReply* reply, const QString& hostname);

        //! Remembered certificate hashes, host -> SHA-1 hash (C# Settings.KnownServers)
        QHash<QString, QString> GetKnownServers() const;
        void SetKnownServers(const QHash<QString, QString>& knownServers);

        //! C# Properties.Settings WarnUnrecognizedCertificate / WarnChangedCertificate
        void SetWarnUnrecognizedCertificate(bool warn);
        bool GetWarnUnrecognizedCertificate() const;
        void SetWarnChangedCertificate(bool warn);
        bool GetWarnChangedCertificate() const;

        //! C# X509Certificate.GetCertHashString(): SHA-1 of the DER encoding, upper-case hex
        static QString CertificateHashString(const QSslCertificate& certificate);

        //! The certificate is one that a connected pool reports for one of its hosts
        //! (Certificate.fingerprint_sha256). Pool members present their own certificates
        //! to console, metrics and import/export connections; since the pool database was
        //! read over the already trusted API connection, such a match needs no prompt.
        static bool IsKnownPoolCertificate(const QSslCertificate& certificate);

        //! The host name is the address of a host in a connected pool that has no
        //! certificate record. XAPI before 1.295 (XCP-ng 8.2) records only certificates
        //! installed with host.install_server_certificate, not the default self-signed ones.
        static bool IsUnrecordedPoolMember(const QString& hostname);

        //! C# X509Ext.VerifyInAllStores(), shown as "trusted" in the prompt: the only
        //! verification problem is a host name mismatch (e.g. connecting by IP address)
        static bool IsTrustedChain(const QList<QSslError>& errors);

    signals:
        //! Known servers or warn settings changed (emitted from the validating thread)
        void trustSettingsChanged();

    private:
        static XenCertificateManager* s_instance;

        class Private;
        Private* d;
};

#endif // XEN_CERTIFICATEMANAGER_H
