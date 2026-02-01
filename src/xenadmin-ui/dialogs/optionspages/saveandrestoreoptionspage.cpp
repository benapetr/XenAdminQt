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
#include "xenlib/utils/encryption.h"
#include "../restoresession/changemainpassworddialog.h"
#include "../restoresession/entermainpassworddialog.h"
#include "../restoresession/setmainpassworddialog.h"

SaveAndRestoreOptionsPage::SaveAndRestoreOptionsPage(QWidget* parent) : IOptionsPage(parent), ui(new Ui::SaveAndRestoreOptionsPage)
{
    this->ui->setupUi(this);

    // Connect signals - use manual clicked() handling to prevent auto-toggling
    connect(this->ui->changeMainPasswordButton, &QPushButton::clicked, this, &SaveAndRestoreOptionsPage::changeMainPasswordButton_Click);
    connect(this->ui->requireMainPasswordCheckBox, &QCheckBox::clicked, this, &SaveAndRestoreOptionsPage::requireMainPasswordCheckBox_Click);
    connect(this->ui->saveStateCheckBox, &QCheckBox::clicked, this, &SaveAndRestoreOptionsPage::saveStateCheckBox_Click);
    connect(this->ui->savePasswordsCheckBox, &QCheckBox::clicked, this, &SaveAndRestoreOptionsPage::savePasswordsCheckBox_Click);

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

    bool saveSession = settings.GetSaveSession();
    bool savePasswords = settings.GetSavePasswords();
    bool autoReconnect = settings.GetAutoReconnect();
    bool reqPass = settings.GetRequirePass();

    if (!saveSession)
    {
        savePasswords = false;
        autoReconnect = false;
        reqPass = false;
    }

    if (!savePasswords)
    {
        autoReconnect = false;
        reqPass = false;
    }

    this->ui->saveStateCheckBox->setChecked(saveSession);
    this->ui->savePasswordsCheckBox->setChecked(savePasswords);
    this->ui->autoReconnectCheckBox->setChecked(autoReconnect);

    this->m_mainKey.clear();
    this->m_mainPasswordHash = settings.GetMainPasswordHash();
    this->m_mainPasswordHashSalt = settings.GetMainPasswordHashSalt();
    this->m_mainKeySalt = settings.GetMainKeySalt();
    this->m_mainKdfIterations = settings.GetMainKdfIterations();
    if (this->m_mainKdfIterations <= 0)
        this->m_mainKdfIterations = 150000;

    bool cryptoAvailable = EncryptionUtils::EncryptionAvailable();
    this->ui->requireMainPasswordCheckBox->setChecked(cryptoAvailable && reqPass && !this->m_mainPasswordHash.isEmpty());
    if (!cryptoAvailable)
        this->ui->requireMainPasswordCheckBox->setChecked(false);
    this->UpdateControlStates();
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
    this->SaveEverything();
}

void SaveAndRestoreOptionsPage::SaveEverything()
{
    SettingsManager& settings = SettingsManager::instance();

    bool saveSession = this->ui->saveStateCheckBox->isChecked();
    bool savePasswords = this->ui->savePasswordsCheckBox->isChecked();
    bool autoReconnect = this->ui->autoReconnectCheckBox->isChecked();

    if (!saveSession)
    {
        savePasswords = false;
        autoReconnect = false;
    }
    else if (!savePasswords)
    {
        autoReconnect = false;
    }

    settings.SetSaveSession(saveSession);
    settings.SetSavePasswords(savePasswords);
    settings.SetAutoReconnect(autoReconnect);

    bool cryptoAvailable = EncryptionUtils::EncryptionAvailable();
    if (!cryptoAvailable)
        this->ui->requireMainPasswordCheckBox->setChecked(false);

    if (!saveSession || !savePasswords)
    {
        settings.SetRequirePass(false);
        settings.SetMainKey(QByteArray());
        settings.SetMainPasswordHash(QByteArray());
        settings.SetMainPasswordHashSalt(QByteArray());
        settings.SetMainKeySalt(QByteArray());
    } else
    {
        if (!this->ui->requireMainPasswordCheckBox->isChecked())
        {
            settings.SetRequirePass(false);
            settings.SetMainKey(QByteArray());
            if (cryptoAvailable)
            {
                settings.SetMainPasswordHash(QByteArray());
                settings.SetMainPasswordHashSalt(QByteArray());
                settings.SetMainKeySalt(QByteArray());
            }
        } else
        {
            settings.SetRequirePass(true);
            if (!this->m_mainKey.isEmpty())
                settings.SetMainKey(this->m_mainKey);
            if (!this->m_mainPasswordHash.isEmpty())
                settings.SetMainPasswordHash(this->m_mainPasswordHash);
            if (!this->m_mainPasswordHashSalt.isEmpty())
                settings.SetMainPasswordHashSalt(this->m_mainPasswordHashSalt);
            if (!this->m_mainKeySalt.isEmpty())
                settings.SetMainKeySalt(this->m_mainKeySalt);
            if (this->m_mainKdfIterations > 0)
                settings.SetMainKdfIterations(this->m_mainKdfIterations);
        }
    }

    settings.Sync();
}

void SaveAndRestoreOptionsPage::changeMainPasswordButton_Click()
{
    // Matches C# SaveAndRestoreOptionsPage.changeMainPasswordButton_Click()
    ChangeMainPasswordDialog dialog(this->m_mainPasswordHash, this->m_mainPasswordHashSalt,
                                   this->m_mainKdfIterations, this);
    if (dialog.exec() == QDialog::Accepted)
    {
        this->m_mainKey = dialog.GetDerivedKey();
        this->m_mainKeySalt = dialog.GetKeySalt();
        this->m_mainPasswordHash = dialog.GetVerifyHash();
        this->m_mainPasswordHashSalt = dialog.GetVerifySalt();
        this->m_mainKdfIterations = dialog.GetIterations();
    }
}

void SaveAndRestoreOptionsPage::requireMainPasswordCheckBox_Click()
{
    // Matches C# SaveAndRestoreOptionsPage.requireMainPasswordCheckBox_Click()
    
    // checked() is the state after the click
    if (this->ui->requireMainPasswordCheckBox->isChecked())
    {
        // User is trying to enable password protection
        Q_ASSERT(this->m_mainPasswordHash.isEmpty() && "Main password is set, but not reflected on GUI");

        if (this->m_mainPasswordHash.isEmpty())
        {
            // No previous password existed => set a new one
            SetMainPasswordDialog setPassword(this->m_mainKdfIterations, this);
            if (setPassword.exec() == QDialog::Accepted)
            {
                this->m_mainKey = setPassword.GetDerivedKey();
                this->m_mainKeySalt = setPassword.GetKeySalt();
                this->m_mainPasswordHash = setPassword.GetVerifyHash();
                this->m_mainPasswordHashSalt = setPassword.GetVerifySalt();
                this->m_mainKdfIterations = setPassword.GetIterations();
                this->ui->requireMainPasswordCheckBox->setChecked(true);
                this->ui->changeMainPasswordButton->setEnabled(true);
            }
            else
            {
                this->ui->requireMainPasswordCheckBox->setChecked(false);
            }
        }
        else
        {
            // A previous password existed (should never get here but just in case)
            this->ui->requireMainPasswordCheckBox->setChecked(true);
            this->ui->changeMainPasswordButton->setEnabled(true);
        }
    }
    else
    {
        // User is trying to disable password protection
        EnterMainPasswordDialog enterPassword(this->m_mainPasswordHash, this->m_mainPasswordHashSalt,
                                             this->m_mainKeySalt, this->m_mainKdfIterations, this);
        if (enterPassword.exec() == QDialog::Accepted)
        {
            this->m_mainKey.clear();
            this->m_mainPasswordHash.clear();
            this->m_mainPasswordHashSalt.clear();
            this->m_mainKeySalt.clear();
            this->ui->requireMainPasswordCheckBox->setChecked(false);
            this->ui->changeMainPasswordButton->setEnabled(false);
        }
        else
        {
            this->ui->requireMainPasswordCheckBox->setChecked(true);
        }
    }

    this->UpdateControlStates();
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
        EnterMainPasswordDialog enterPassword(this->m_mainPasswordHash, this->m_mainPasswordHashSalt,
                                             this->m_mainKeySalt, this->m_mainKdfIterations, this);
        if (enterPassword.exec() == QDialog::Accepted)
        {
            this->m_mainKey.clear();
            this->m_mainPasswordHash.clear();
            this->m_mainPasswordHashSalt.clear();
            this->m_mainKeySalt.clear();
            this->ui->requireMainPasswordCheckBox->setChecked(false);
        }
    } else
    {
        // No special handling needed
    }

    this->UpdateControlStates();
}

void SaveAndRestoreOptionsPage::savePasswordsCheckBox_Click()
{
    if (!this->ui->savePasswordsCheckBox->isChecked())
    {
        this->ui->autoReconnectCheckBox->setChecked(false);
        if (this->ui->requireMainPasswordCheckBox->isChecked())
        {
            EnterMainPasswordDialog enterPassword(this->m_mainPasswordHash, this->m_mainPasswordHashSalt,
                                                 this->m_mainKeySalt, this->m_mainKdfIterations, this);
            if (enterPassword.exec() == QDialog::Accepted)
            {
                this->m_mainKey.clear();
                this->m_mainPasswordHash.clear();
                this->m_mainPasswordHashSalt.clear();
                this->m_mainKeySalt.clear();
                this->ui->requireMainPasswordCheckBox->setChecked(false);
            }
            else
            {
                this->ui->savePasswordsCheckBox->setChecked(true);
            }
        }
    }

    this->UpdateControlStates();
}

void SaveAndRestoreOptionsPage::UpdateControlStates()
{
    bool saveSession = this->ui->saveStateCheckBox->isChecked();
    bool savePasswords = this->ui->savePasswordsCheckBox->isChecked();
    bool cryptoAvailable = EncryptionUtils::EncryptionAvailable();

    this->ui->savePasswordsCheckBox->setEnabled(saveSession);
    this->ui->autoReconnectCheckBox->setEnabled(saveSession && savePasswords);
    this->ui->mainPasswordGroupBox->setEnabled(saveSession && savePasswords);
    this->ui->requireMainPasswordCheckBox->setEnabled(saveSession && savePasswords && cryptoAvailable);
    this->ui->changeMainPasswordButton->setEnabled(saveSession && savePasswords && this->ui->requireMainPasswordCheckBox->isChecked() && cryptoAvailable);
    this->ui->cryptoUnavailableLabel->setVisible(saveSession && savePasswords && !cryptoAvailable);

    if (!saveSession)
    {
        this->ui->savePasswordsCheckBox->setChecked(false);
        this->ui->autoReconnectCheckBox->setChecked(false);
        this->ui->requireMainPasswordCheckBox->setChecked(false);
    }
    else if (!savePasswords)
    {
        this->ui->autoReconnectCheckBox->setChecked(false);
        this->ui->requireMainPasswordCheckBox->setChecked(false);
    }
    else if (!cryptoAvailable)
    {
        this->ui->requireMainPasswordCheckBox->setChecked(false);
    }
}
