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

#ifndef INSTALLSERVERCERTIFICATEACTION_H
#define INSTALLSERVERCERTIFICATEACTION_H

#include "../../asyncoperation.h"
#include "../../host.h"
#include <QSharedPointer>
#include <QStringList>

class XENLIB_EXPORT InstallServerCertificateAction : public AsyncOperation
{
    Q_OBJECT

    public:
        InstallServerCertificateAction(const QSharedPointer<Host>& host,
                                       const QString& keyFile,
                                       const QString& certificateFile,
                                       const QStringList& chainFiles,
                                       QObject* parent = nullptr);

        QString KeyError() const;
        QString CertificateError() const;
        QString ChainError() const;

    protected:
        void run() override;

    private:
        void collectFileContents(QString* privateKey, QString* certificate, QString* chain);
        XenAPI::Session* waitForReconnect();
        void waitForNewCertificate(XenAPI::Session* session);
        void mapFailure(const QStringList& errorDetails);

        QSharedPointer<Host> m_host;
        QString m_hostRef;
        QString m_keyFile;
        QString m_certificateFile;
        QStringList m_chainFiles;
        QString m_oldCertificateUuid;
        QString m_keyError;
        QString m_certificateError;
        QString m_chainError;
};

#endif // INSTALLSERVERCERTIFICATEACTION_H
