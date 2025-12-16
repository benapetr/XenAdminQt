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

QString ConfirmationOptionsPage::text() const
{
    // Matches C# Messages.CONFIRMATIONS
    return tr("Confirmations");
}

QString ConfirmationOptionsPage::subText() const
{
    // Matches C# Messages.CONFIRMATIONS_DETAIL
    return tr("Configure confirmation dialogs");
}

QIcon ConfirmationOptionsPage::image() const
{
    // Matches C# Images.StaticImages._075_TickRound_h32bit_16
    return QIcon(":/icons/tick_16.png");
}

void ConfirmationOptionsPage::build()
{
    // Matches C# ConfirmationOptionsPage.Build()
    SettingsManager& settings = SettingsManager::instance();

    // Dismissing notifications
    this->ui->checkBoxDontConfirmDismissAlerts->setChecked(
        settings.getValue("Confirmation/DoNotConfirmDismissAlerts", false).toBool());
    this->ui->checkBoxDontConfirmDismissUpdates->setChecked(
        settings.getValue("Confirmation/DoNotConfirmDismissUpdates", false).toBool());
    this->ui->checkBoxDontConfirmDismissEvents->setChecked(
        settings.getValue("Confirmation/DoNotConfirmDismissEvents", false).toBool());

    // Import/Export warnings
    this->ui->checkBoxIgnoreOvfWarnings->setChecked(
        settings.getValue("Confirmation/IgnoreOvfValidationWarnings", false).toBool());
}

bool ConfirmationOptionsPage::isValidToSave(QWidget** control, QString& invalidReason)
{
    // Matches C# ConfirmationOptionsPage.IsValidToSave()
    // No validation needed
    Q_UNUSED(control);
    Q_UNUSED(invalidReason);
    return true;
}

void ConfirmationOptionsPage::showValidationMessages(QWidget* control, const QString& message)
{
    // Matches C# ConfirmationOptionsPage.ShowValidationMessages()
    Q_UNUSED(control);
    Q_UNUSED(message);
}

void ConfirmationOptionsPage::hideValidationMessages()
{
    // Matches C# ConfirmationOptionsPage.HideValidationMessages()
}

void ConfirmationOptionsPage::save()
{
    // Matches C# ConfirmationOptionsPage.Save()
    SettingsManager& settings = SettingsManager::instance();

    // Dismissing notifications
    settings.setValue("Confirmation/DoNotConfirmDismissAlerts",
                      this->ui->checkBoxDontConfirmDismissAlerts->isChecked());
    settings.setValue("Confirmation/DoNotConfirmDismissUpdates",
                      this->ui->checkBoxDontConfirmDismissUpdates->isChecked());
    settings.setValue("Confirmation/DoNotConfirmDismissEvents",
                      this->ui->checkBoxDontConfirmDismissEvents->isChecked());

    // Import/Export warnings
    settings.setValue("Confirmation/IgnoreOvfValidationWarnings",
                      this->ui->checkBoxIgnoreOvfWarnings->isChecked());
}
