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

// confirmationoptionspage.cpp - Confirmation settings options page
#include "confirmationoptionspage.h"
#include "../../settingsmanager.h"

ConfirmationOptionsPage::ConfirmationOptionsPage(QWidget* parent)
    : IOptionsPage(parent), ui(new Ui::ConfirmationOptionsPage)
{
    this->ui->setupUi(this);
}

ConfirmationOptionsPage::~ConfirmationOptionsPage()
{
    delete this->ui;
}

QString ConfirmationOptionsPage::GetText() const
{
    // Matches C# Messages.CONFIRMATIONS
    return tr("Confirmations");
}

QString ConfirmationOptionsPage::GetSubText() const
{
    // Matches C# Messages.CONFIRMATIONS_DETAIL
    return tr("Configure confirmation dialogs");
}

QIcon ConfirmationOptionsPage::GetImage() const
{
    // Matches C# Images.StaticImages._075_TickRound_h32bit_16
    return QIcon(":/icons/tick_16.png");
}

void ConfirmationOptionsPage::Build()
{
    // Matches C# ConfirmationOptionsPage.Build()
    SettingsManager& settings = SettingsManager::instance();

    // Dismissing notifications
    this->ui->checkBoxDontConfirmDismissAlerts->setChecked(
        settings.GetDoNotConfirmDismissAlerts());
    this->ui->checkBoxDontConfirmDismissUpdates->setChecked(
        settings.GetDoNotConfirmDismissUpdates());
    this->ui->checkBoxDontConfirmDismissEvents->setChecked(
        settings.GetDoNotConfirmDismissEvents());

    // Import/Export warnings
    this->ui->checkBoxIgnoreOvfWarnings->setChecked(
        settings.GetIgnoreOvfValidationWarnings());
}

bool ConfirmationOptionsPage::IsValidToSave(QWidget** control, QString& invalidReason)
{
    // Matches C# ConfirmationOptionsPage.IsValidToSave()
    // No validation needed
    Q_UNUSED(control);
    Q_UNUSED(invalidReason);
    return true;
}

void ConfirmationOptionsPage::ShowValidationMessages(QWidget* control, const QString& message)
{
    // Matches C# ConfirmationOptionsPage.ShowValidationMessages()
    Q_UNUSED(control);
    Q_UNUSED(message);
}

void ConfirmationOptionsPage::HideValidationMessages()
{
    // Matches C# ConfirmationOptionsPage.HideValidationMessages()
}

void ConfirmationOptionsPage::Save()
{
    // Matches C# ConfirmationOptionsPage.Save()
    SettingsManager& settings = SettingsManager::instance();

    // Dismissing notifications
    settings.SetDoNotConfirmDismissAlerts(this->ui->checkBoxDontConfirmDismissAlerts->isChecked());
    settings.SetDoNotConfirmDismissUpdates(this->ui->checkBoxDontConfirmDismissUpdates->isChecked());
    settings.SetDoNotConfirmDismissEvents(this->ui->checkBoxDontConfirmDismissEvents->isChecked());

    // Import/Export warnings
    settings.SetIgnoreOvfValidationWarnings(this->ui->checkBoxIgnoreOvfWarnings->isChecked());
}
