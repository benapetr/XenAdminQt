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

// saveandrestoreoptionspage.cpp - Save and restore settings options page
#include "saveandrestoreoptionspage.h"
#include "../../settingsmanager.h"

SaveAndRestoreOptionsPage::SaveAndRestoreOptionsPage(QWidget* parent)
    : IOptionsPage(parent), ui(new Ui::SaveAndRestoreOptionsPage)
{
    this->ui->setupUi(this);
}

SaveAndRestoreOptionsPage::~SaveAndRestoreOptionsPage()
{
    delete this->ui;
}

QString SaveAndRestoreOptionsPage::GetText() const
{
    // Matches C# Messages.SAVE_AND_RESTORE
    return tr("Save and Restore");
}

QString SaveAndRestoreOptionsPage::GetSubText() const
{
    // Matches C# Messages.SAVE_AND_RESTORE_DESC
    return tr("Configure session save and restore");
}

QIcon SaveAndRestoreOptionsPage::GetImage() const
{
    // Matches C# Images.StaticImages.save_16
    return QIcon(":/icons/save_16.png");
}

void SaveAndRestoreOptionsPage::Build()
{
    // Matches C# SaveAndRestoreOptionsPage.Build()
    // Simplified version - just handle save state checkbox
    SettingsManager& settings = SettingsManager::instance();

    this->ui->saveStateCheckBox->setChecked(
        settings.getValue("Session/SaveSession", false).toBool());
}

bool SaveAndRestoreOptionsPage::IsValidToSave(QWidget** control, QString& invalidReason)
{
    // Matches C# SaveAndRestoreOptionsPage.IsValidToSave()
    // No validation needed
    Q_UNUSED(control);
    Q_UNUSED(invalidReason);
    return true;
}

void SaveAndRestoreOptionsPage::ShowValidationMessages(QWidget* control, const QString& message)
{
    // Matches C# SaveAndRestoreOptionsPage.ShowValidationMessages()
    Q_UNUSED(control);
    Q_UNUSED(message);
}

void SaveAndRestoreOptionsPage::HideValidationMessages()
{
    // Matches C# SaveAndRestoreOptionsPage.HideValidationMessages()
}

void SaveAndRestoreOptionsPage::Save()
{
    // Matches C# SaveAndRestoreOptionsPage.Save()
    // Simplified version
    SettingsManager& settings = SettingsManager::instance();

    settings.setValue("Session/SaveSession", this->ui->saveStateCheckBox->isChecked());
}
