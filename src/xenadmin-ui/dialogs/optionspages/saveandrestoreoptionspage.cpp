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
#include "../restoresession/changemainpassworddialog.h"
#include "../restoresession/entermainpassworddialog.h"
#include "../restoresession/setmainpassworddialog.h"

SaveAndRestoreOptionsPage::SaveAndRestoreOptionsPage(QWidget* parent)
    : IOptionsPage(parent), ui(new Ui::SaveAndRestoreOptionsPage), saveAllAfter_(true)
{
    this->ui->setupUi(this);

    // Connect signals - use manual clicked() handling to prevent auto-toggling
    connect(this->ui->changeMainPasswordButton, &QPushButton::clicked, this, &SaveAndRestoreOptionsPage::changeMainPasswordButton_Click);
    connect(this->ui->requireMainPasswordCheckBox, &QCheckBox::clicked, this, &SaveAndRestoreOptionsPage::requireMainPasswordCheckBox_Click);
    connect(this->ui->saveStateCheckBox, &QCheckBox::clicked, this, &SaveAndRestoreOptionsPage::saveStateCheckBox_Click);

    // Disable auto-check behavior (we handle it manually in click handlers)
    this->ui->requireMainPasswordCheckBox->setAutoExclusive(false);
    this->ui->saveStateCheckBox->setAutoExclusive(false);
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
    // Matches C# Images.StaticImages._000_BackupMetadata_h32bit_16
    return QIcon(":/icons/save_16.png");
}

void SaveAndRestoreOptionsPage::Build()
{
    // Matches C# SaveAndRestoreOptionsPage.Build()
    SettingsManager& settings = SettingsManager::instance();

    bool allowCredSave = SettingsManager::AllowCredentialSave();
    bool saveSession = settings.GetSaveSession();
    bool reqPass = settings.GetRequirePass();

    this->ui->saveStateLabel->setEnabled(allowCredSave);
    this->ui->saveStateCheckBox->setEnabled(allowCredSave);

    this->ui->saveStateCheckBox->setChecked(saveSession && allowCredSave);
    this->ui->mainPasswordGroupBox->setEnabled(saveSession && allowCredSave);

    this->mainPassword_ = SettingsManager::GetMainPassword();

    this->ui->requireMainPasswordCheckBox->setChecked(reqPass && !this->mainPassword_.isEmpty() && allowCredSave);
    this->ui->changeMainPasswordButton->setEnabled(reqPass && !this->mainPassword_.isEmpty() && allowCredSave);
}

bool SaveAndRestoreOptionsPage::IsValidToSave(QWidget** control, QString& invalidReason)
{
    // Matches C# SaveAndRestoreOptionsPage.IsValidToSave()
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
    SaveEverything();
}

void SaveAndRestoreOptionsPage::SetSaveAllAfter(bool saveAllAfter)
{
    this->saveAllAfter_ = saveAllAfter;
}

void SaveAndRestoreOptionsPage::SaveEverything()
{
    // Matches C# SaveAndRestoreOptionsPage.SaveEverything()
    if (!SettingsManager::AllowCredentialSave())
    {
        return;
    }

    SettingsManager& settings = SettingsManager::instance();

    if (!this->ui->saveStateCheckBox->isChecked())
    {
        settings.SetSaveSession(false);
        settings.SetRequirePass(false);
        SettingsManager::SetMainPassword(QByteArray());
    } else if (!this->ui->requireMainPasswordCheckBox->isChecked())
    {
        settings.SetSaveSession(true);
        settings.SetRequirePass(false);
        SettingsManager::SetMainPassword(QByteArray());
    } else
    {
        settings.SetSaveSession(true);
        settings.SetRequirePass(true);
        SettingsManager::SetMainPassword(this->mainPassword_);
    }

    if (this->saveAllAfter_)
    {
        SettingsManager::SaveServerList();
    }

    settings.Sync();
}

void SaveAndRestoreOptionsPage::changeMainPasswordButton_Click()
{
    // Matches C# SaveAndRestoreOptionsPage.changeMainPasswordButton_Click()
    ChangeMainPasswordDialog dialog(this->mainPassword_, this);
    if (dialog.exec() == QDialog::Accepted)
    {
        this->mainPassword_ = dialog.GetNewPassword();
    }
}

void SaveAndRestoreOptionsPage::requireMainPasswordCheckBox_Click()
{
    // Matches C# SaveAndRestoreOptionsPage.requireMainPasswordCheckBox_Click()
    
    // requireMainPasswordCheckBox.Checked was the state before the click
    // if previously checked, the user is trying to clear it => request authorization
    // if previously unchecked, the user is trying to set a password

    if (this->ui->requireMainPasswordCheckBox->isChecked())
    {
        // User is trying to disable password protection
        EnterMainPasswordDialog enterPassword(this->mainPassword_, this);
        if (enterPassword.exec() == QDialog::Accepted)
        {
            this->mainPassword_.clear();
            this->ui->requireMainPasswordCheckBox->setChecked(false);
            this->ui->changeMainPasswordButton->setEnabled(false);
        }
    } else
    {
        // User is trying to enable password protection
        Q_ASSERT(this->mainPassword_.isEmpty() && "Main password is set, but not reflected on GUI");

        if (this->mainPassword_.isEmpty())
        {
            // No previous password existed => set a new one
            SetMainPasswordDialog setPassword(this);
            if (setPassword.exec() == QDialog::Accepted)
            {
                this->mainPassword_ = setPassword.GetNewPassword();
                this->ui->requireMainPasswordCheckBox->setChecked(true);
                this->ui->changeMainPasswordButton->setEnabled(true);
            }
        } else
        {
            // A previous password existed (should never get here but just in case)
            this->ui->requireMainPasswordCheckBox->setChecked(true);
            this->ui->changeMainPasswordButton->setEnabled(true);
        }
    }
}

void SaveAndRestoreOptionsPage::saveStateCheckBox_Click()
{
    // Matches C# SaveAndRestoreOptionsPage.saveStateCheckBox_Click()
    
    // saveStateCheckBox.Checked was the state before the click
    // if previously checked, the user is trying to clear it => authorization maybe required
    // (depending on the state of the requireMainPasswordCheckBox; this should be cleared too if checked)

    if (this->ui->saveStateCheckBox->isChecked() && this->ui->requireMainPasswordCheckBox->isChecked())
    {
        // Require password authorization to disable
        EnterMainPasswordDialog enterPassword(this->mainPassword_, this);
        if (enterPassword.exec() == QDialog::Accepted)
        {
            this->mainPassword_.clear();
            this->ui->saveStateCheckBox->setChecked(false);
            this->ui->requireMainPasswordCheckBox->setChecked(false);
            this->ui->mainPasswordGroupBox->setEnabled(false);
        }
    } else
    {
        // Toggle the checkbox state and enable/disable group box
        this->ui->saveStateCheckBox->setChecked(!this->ui->saveStateCheckBox->isChecked());
        this->ui->mainPasswordGroupBox->setEnabled(this->ui->saveStateCheckBox->isChecked());
        this->ui->changeMainPasswordButton->setEnabled(this->ui->requireMainPasswordCheckBox->isChecked());
    }
}
