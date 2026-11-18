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

#ifndef CERTIFICATEWARNINGDIALOG_H
#define CERTIFICATEWARNINGDIALOG_H

#include <QDialog>
#include <QSslCertificate>
#include "xenlib/xen/network/certificatemanager.h"

namespace Ui
{
    class CertificateWarningDialog;
}

/**
 * @brief Asks whether to trust a server's TLS certificate
 *
 * C# Reference: XenAdmin/Dialogs/Network/UnknownCertificateDialog.cs (first connection)
 * and CertificateChangedDialog.cs (certificate differs from the remembered one).
 */
class CertificateWarningDialog : public QDialog
{
    Q_OBJECT

    public:
        CertificateWarningDialog(XenCertificateManager::CertificatePrompt prompt, const QSslCertificate& certificate,
                                 const QString& hostname, bool trusted, QWidget* parent = nullptr);
        ~CertificateWarningDialog() override;

        //! "Automatically accept new certificates" / "Always ignore certificate warnings"
        bool GetStopWarning() const;

        //! Register the dialog as XenCertificateManager's prompt handler and load the
        //! trust settings; changes made by the manager are saved back to the settings.
        static void InstallPromptHandler();

        //! Push the warn settings from SettingsManager to XenCertificateManager
        static void ApplyWarnSettings();

    private slots:
        void onViewCertificate();

    private:
        Ui::CertificateWarningDialog* ui;
        QSslCertificate m_certificate;
};

#endif // CERTIFICATEWARNINGDIALOG_H
