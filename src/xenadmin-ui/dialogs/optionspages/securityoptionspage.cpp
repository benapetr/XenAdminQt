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

// securityoptionspage.cpp - Security settings options page
#include "securityoptionspage.h"
#include "../../settingsmanager.h"

SecurityOptionsPage::SecurityOptionsPage(QWidget* parent)
    : IOptionsPage(parent), ui(new Ui::SecurityOptionsPage)
{
    this->ui->setupUi(this);
}

SecurityOptionsPage::~SecurityOptionsPage()
{
    delete this->ui;
}

QString SecurityOptionsPage::GetText() const
{
    // Matches C# Messages.SECURITY
    return tr("Security");
}

QString SecurityOptionsPage::GetSubText() const
{
    // Matches C# Messages.SECURITY_DESC
    return tr("Configure security settings");
}

QIcon SecurityOptionsPage::GetImage() const
{
    // Matches C# Images.StaticImages.padlock
    return QIcon(":/icons/padlock.png");
}

void SecurityOptionsPage::Build()
{
    // Matches C# SecurityOptionsPage.Build()
    // Load settings from SettingsManager
    SettingsManager& settings = SettingsManager::instance();

    // SSL Certificates
    // In C# this checks Registry.SSLCertificateTypes and Properties.Settings
    // For now, we'll use QSettings through SettingsManager
    this->ui->CertificateFoundCheckBox->setChecked(
        settings.getValue("Security/WarnUnrecognizedCertificate", true).toBool());
    this->ui->CertificateChangedCheckBox->setChecked(
        settings.getValue("Security/WarnChangedCertificate", true).toBool());

    // Password reminder
    this->ui->checkBoxReminder->setChecked(
        settings.getValue("Security/RemindChangePassword", false).toBool());
}

bool SecurityOptionsPage::IsValidToSave(QWidget** control, QString& invalidReason)
{
    // Matches C# SecurityOptionsPage.IsValidToSave()
    // This page has no validation requirements
    Q_UNUSED(control);
    Q_UNUSED(invalidReason);
    return true;
}

void SecurityOptionsPage::ShowValidationMessages(QWidget* control, const QString& message)
{
    // Matches C# SecurityOptionsPage.ShowValidationMessages()
    // This page doesn't show validation messages (no validation needed)
    Q_UNUSED(control);
    Q_UNUSED(message);
}

void SecurityOptionsPage::HideValidationMessages()
{
    // Matches C# SecurityOptionsPage.HideValidationMessages()
    // Nothing to hide for this page
}

void SecurityOptionsPage::Save()
{
    // Matches C# SecurityOptionsPage.Save()
    SettingsManager& settings = SettingsManager::instance();

    // SSL Certificates
    settings.setValue("Security/WarnUnrecognizedCertificate",
                      this->ui->CertificateFoundCheckBox->isChecked());
    settings.setValue("Security/WarnChangedCertificate",
                      this->ui->CertificateChangedCheckBox->isChecked());

    // Password reminder
    settings.setValue("Security/RemindChangePassword",
                      this->ui->checkBoxReminder->isChecked());
}
