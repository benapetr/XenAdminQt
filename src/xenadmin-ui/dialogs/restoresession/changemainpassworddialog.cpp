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

#include "changemainpassworddialog.h"
#include "ui_changemainpassworddialog.h"
#include "utils/encryption.h"

ChangeMainPasswordDialog::ChangeMainPasswordDialog(const QByteArray& currentPasswordHash, const QByteArray& currentSalt,
                                                   int kdfIterations, QWidget* parent)
    : QDialog(parent),
      ui(new Ui::ChangeMainPasswordDialog),
      m_currentPasswordHash(currentPasswordHash),
      m_currentSalt(currentSalt),
      m_iterations(kdfIterations)
{
    this->ui->setupUi(this);
    this->ui->currentPasswordError->setVisible(false);
    this->ui->newPasswordError->setVisible(false);

    // Connect signals
    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &ChangeMainPasswordDialog::okButton_Click);
    connect(this->ui->currentTextBox, &QLineEdit::textChanged, this, &ChangeMainPasswordDialog::currentTextBox_TextChanged);
    connect(this->ui->mainTextBox, &QLineEdit::textChanged, this, &ChangeMainPasswordDialog::mainTextBox_TextChanged);
    connect(this->ui->reEnterMainTextBox, &QLineEdit::textChanged, this, &ChangeMainPasswordDialog::reEnterMainTextBox_TextChanged);
}

ChangeMainPasswordDialog::~ChangeMainPasswordDialog()
{
    delete this->ui;
}

QByteArray ChangeMainPasswordDialog::GetDerivedKey() const
{
    return this->m_derivedKey;
}

QByteArray ChangeMainPasswordDialog::GetKeySalt() const
{
    return this->m_keySalt;
}

QByteArray ChangeMainPasswordDialog::GetVerifyHash() const
{
    return this->m_verifyHash;
}

QByteArray ChangeMainPasswordDialog::GetVerifySalt() const
{
    return this->m_verifySalt;
}

int ChangeMainPasswordDialog::GetIterations() const
{
    return this->m_iterations;
}

void ChangeMainPasswordDialog::okButton_Click()
{
    // Matches C# ChangeMainPasswordDialog.okButton_Click()
    bool currentPasswordCorrect = !this->ui->currentTextBox->text().isEmpty() &&
        EncryptionUtils::VerifyPasswordPBKDF2(this->ui->currentTextBox->text(), this->m_currentPasswordHash,
                                              this->m_currentSalt, this->m_iterations);

    if (currentPasswordCorrect && !this->ui->mainTextBox->text().isEmpty() && this->ui->mainTextBox->text() == this->ui->reEnterMainTextBox->text())
    {
        if (EncryptionUtils::DerivePasswordSecrets(this->ui->mainTextBox->text(), this->m_iterations,
                                                   this->m_derivedKey, this->m_keySalt,
                                                   this->m_verifyHash, this->m_verifySalt))
        {
            this->accept();
            return;
        }

        this->ui->newPasswordError->setText(tr("Failed to derive key"));
        this->ui->newPasswordError->setVisible(true);
        return;
    }

    if (!currentPasswordCorrect)
    {
        this->ui->currentPasswordError->setText(tr("Incorrect password"));
        this->ui->currentPasswordError->setVisible(true);
    } else if (this->ui->mainTextBox->text() != this->ui->reEnterMainTextBox->text())
    {
        this->ui->newPasswordError->setText(tr("Passwords don't match"));
        this->ui->newPasswordError->setVisible(true);
    } else
    {
        this->ui->newPasswordError->setText(tr("Password cannot be empty"));
        this->ui->newPasswordError->setVisible(true);
    }

    this->ui->currentTextBox->setFocus();
    this->ui->currentTextBox->selectAll();
}

void ChangeMainPasswordDialog::currentTextBox_TextChanged()
{
    // Matches C# ChangeMainPasswordDialog.currentTextBox_TextChanged()
    this->ui->currentPasswordError->setVisible(false);
}

void ChangeMainPasswordDialog::mainTextBox_TextChanged()
{
    // Matches C# ChangeMainPasswordDialog.mainTextBox_TextChanged()
    this->ui->newPasswordError->setVisible(false);
}

void ChangeMainPasswordDialog::reEnterMainTextBox_TextChanged()
{
    // Matches C# ChangeMainPasswordDialog.reEnterMainTextBox_TextChanged()
    this->ui->newPasswordError->setVisible(false);
}
