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

#include "certificatewarningdialog.h"
#include "ui_certificatewarningdialog.h"
#include "../mainwindow.h"
#include "../settingsmanager.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QMessageBox>
#include <QStyle>
#include <QThread>

namespace
{
    const char* const KnownServersKey = "Security/KnownServers";
    const char* const WarnUnrecognizedKey = "Security/WarnUnrecognizedCertificate";
    const char* const WarnChangedKey = "Security/WarnChangedCertificate";

    // C# string.Ellipsise(35)
    QString ellipsise(const QString& text, int maxLength)
    {
        return text.length() <= maxLength ? text : text.left(maxLength - 3) + QStringLiteral("...");
    }

    QString fingerprint(const QSslCertificate& certificate, QCryptographicHash::Algorithm algorithm)
    {
        const QByteArray hex = certificate.digest(algorithm).toHex().toUpper();
        QStringList pairs;
        for (int i = 0; i < hex.size(); i += 2)
            pairs.append(QString::fromLatin1(hex.mid(i, 2)));
        return pairs.join(QLatin1Char(':'));
    }

    void saveTrustSettings()
    {
        XenCertificateManager* manager = XenCertificateManager::instance();
        SettingsManager& settings = SettingsManager::instance();

        QVariantMap known;
        const QHash<QString, QString> servers = manager->GetKnownServers();
        for (auto it = servers.constBegin(); it != servers.constEnd(); ++it)
            known.insert(it.key(), it.value());

        settings.SetValue(KnownServersKey, known);
        settings.SetValue(WarnUnrecognizedKey, manager->GetWarnUnrecognizedCertificate());
        settings.SetValue(WarnChangedKey, manager->GetWarnChangedCertificate());
        settings.Sync();
    }
} // namespace

CertificateWarningDialog::CertificateWarningDialog(XenCertificateManager::CertificatePrompt prompt,
                                                   const QSslCertificate& certificate, const QString& hostname,
                                                   bool trusted, QWidget* parent)
    : QDialog(parent), ui(new Ui::CertificateWarningDialog), m_certificate(certificate)
{
    this->ui->setupUi(this);

    const QString h = ellipsise(hostname, 35);
    const int iconSize = this->style()->pixelMetric(QStyle::PM_MessageBoxIconSize, nullptr, this);
    this->ui->iconLabel->setPixmap(this->style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(iconSize, iconSize));
    this->ui->trustedLabel->setText(tr("The certificate on server '%1' is %2.").arg(h, trusted ? tr("trusted") : tr("not trusted")));

    if (prompt == XenCertificateManager::CertificatePrompt::Unknown)
    {
        this->setWindowTitle(tr("New Security Certificate"));
        this->ui->blurbLabel->setText(tr("Do you want to accept the security certificate on server '%1'?").arg(h));
        this->ui->blurb2Label->setText(tr("You should only accept the certificate if you are sure that it belongs to '%1'.").arg(h));
        this->ui->blurb3Label->setText(tr("Before you accept this certificate and connect to the server, you should check it against the server console."));
        this->ui->stopWarningCheckBox->setText(tr("&Automatically accept new certificates from now on"));
    } else
    {
        this->setWindowTitle(tr("Security Certificate Changed"));
        this->ui->blurbLabel->setText(tr("The security certificate on '%1' has changed since the last time you connected.").arg(h));
        this->ui->blurb2Label->setText(tr("If you are not expecting this change, then you may be connected to a server pretending to be '%1', possibly to obtain your confidential information.").arg(h));
        this->ui->blurb3Label->setText(tr("If the server has been reinstalled, then this is expected.  You should check the new certificate against the server console."));
        this->ui->stopWarningCheckBox->setText(tr("&Always ignore certificate warnings"));
    }

    connect(this->ui->viewCertificateButton, &QPushButton::clicked, this, &CertificateWarningDialog::onViewCertificate);
    connect(this->ui->acceptButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(this->ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    this->ui->cancelButton->setFocus();
}

CertificateWarningDialog::~CertificateWarningDialog()
{
    delete this->ui;
}

bool CertificateWarningDialog::GetStopWarning() const
{
    return this->ui->stopWarningCheckBox->isChecked();
}

void CertificateWarningDialog::onViewCertificate()
{
    // C# X509Certificate2UI.DisplayCertificate - Qt has no native viewer, so list the key fields
    const QSslCertificate& c = this->m_certificate;
    QString details;
    details += tr("Issued to: %1").arg(c.subjectDisplayName()) + "\n";
    details += tr("Issued by: %1").arg(c.issuerDisplayName()) + "\n";
    details += tr("Valid from: %1").arg(QLocale().toString(c.effectiveDate().toLocalTime(), QLocale::LongFormat)) + "\n";
    details += tr("Valid to: %1").arg(QLocale().toString(c.expiryDate().toLocalTime(), QLocale::LongFormat)) + "\n";
    details += tr("Serial number: %1").arg(QString::fromLatin1(c.serialNumber())) + "\n";
    details += tr("SHA-256 fingerprint: %1").arg(fingerprint(c, QCryptographicHash::Sha256)) + "\n";
    details += tr("SHA-1 fingerprint: %1").arg(fingerprint(c, QCryptographicHash::Sha1));

    QMessageBox box(QMessageBox::Information, tr("Certificate"), details, QMessageBox::Close, this);
    box.setDetailedText(QString::fromLatin1(c.toPem()));
    box.setTextInteractionFlags(Qt::TextSelectableByMouse);
    box.exec();
}

void CertificateWarningDialog::ApplyWarnSettings()
{
    SettingsManager& settings = SettingsManager::instance();
    XenCertificateManager* manager = XenCertificateManager::instance();
    manager->SetWarnUnrecognizedCertificate(settings.GetValue(WarnUnrecognizedKey, true).toBool());
    manager->SetWarnChangedCertificate(settings.GetValue(WarnChangedKey, true).toBool());
}

void CertificateWarningDialog::InstallPromptHandler()
{
    XenCertificateManager* manager = XenCertificateManager::instance();

    // Load remembered certificates and warn settings (C# Settings.KnownServers)
    QHash<QString, QString> known;
    const QVariantMap stored = SettingsManager::instance().GetValue(KnownServersKey).toMap();
    for (auto it = stored.constBegin(); it != stored.constEnd(); ++it)
        known.insert(it.key(), it.value().toString());
    manager->SetKnownServers(known);
    ApplyWarnSettings();

    // The manager may change these from a worker thread; save on the GUI thread
    QObject::connect(manager, &XenCertificateManager::trustSettingsChanged, qApp, []() { saveTrustSettings(); },
                     Qt::QueuedConnection);

    manager->SetPromptHandler([](XenCertificateManager::CertificatePrompt prompt, const QSslCertificate& certificate,
                                 const QString& hostname, bool trusted)
    {
        XenCertificateManager::PromptResult result;
        auto ask = [&]()
        {
            CertificateWarningDialog dialog(prompt, certificate, hostname, trusted, MainWindow::instance());
            result.accept = dialog.exec() == QDialog::Accepted;
            result.stopWarning = result.accept && dialog.GetStopWarning();
        };

        // C# Program.Invoke(Program.MainWindow, ...): show the dialog on the GUI thread
        if (QThread::currentThread() == qApp->thread())
            ask();
        else
            QMetaObject::invokeMethod(qApp, ask, Qt::BlockingQueuedConnection);

        return result;
    });
}
