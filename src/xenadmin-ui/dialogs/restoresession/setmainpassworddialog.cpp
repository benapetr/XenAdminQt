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

#include "setmainpassworddialog.h"
#include "ui_setmainpassworddialog.h"
#include "utils/encryption.h"

SetMainPasswordDialog::SetMainPasswordDialog(int kdfIterations, QWidget* parent)
    : QDialog(parent), ui(new Ui::SetMainPasswordDialog), m_iterations(kdfIterations)
{
    this->ui->setupUi(this);
    this->ui->newPasswordError->setVisible(false);

    // Connect signals
    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &SetMainPasswordDialog::okButton_Click);
    connect(this->ui->mainTextBox, &QLineEdit::textChanged, this, &SetMainPasswordDialog::mainTextBox_TextChanged);
    connect(this->ui->reEnterMainTextBox, &QLineEdit::textChanged, this, &SetMainPasswordDialog::reEnterMainTextBox_TextChanged);
}

SetMainPasswordDialog::~SetMainPasswordDialog()
{
    delete this->ui;
}

QByteArray SetMainPasswordDialog::GetDerivedKey() const
{
    return this->m_derivedKey;
}

QByteArray SetMainPasswordDialog::GetKeySalt() const
{
    return this->m_keySalt;
}

QByteArray SetMainPasswordDialog::GetVerifyHash() const
{
    return this->m_verifyHash;
}

QByteArray SetMainPasswordDialog::GetVerifySalt() const
{
    return this->m_verifySalt;
}

int SetMainPasswordDialog::GetIterations() const
{
    return this->m_iterations;
}

void SetMainPasswordDialog::okButton_Click()
{
    // Matches C# SetMainPasswordDialog.okButton_Click()
    if (!this->ui->mainTextBox->text().isEmpty() &&
        this->ui->mainTextBox->text() == this->ui->reEnterMainTextBox->text())
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

    if (this->ui->mainTextBox->text() != this->ui->reEnterMainTextBox->text())
    {
        this->ui->newPasswordError->setText(tr("Passwords don't match"));
    } else
    {
        this->ui->newPasswordError->setText(tr("Password cannot be empty"));
    }

    this->ui->newPasswordError->setVisible(true);
    this->ui->mainTextBox->setFocus();
    this->ui->mainTextBox->selectAll();
}

void SetMainPasswordDialog::mainTextBox_TextChanged()
{
    // Matches C# SetMainPasswordDialog.mainTextBox_TextChanged()
    this->ui->newPasswordError->setVisible(false);
}

void SetMainPasswordDialog::reEnterMainTextBox_TextChanged()
{
    // Matches C# SetMainPasswordDialog.reEnterMainTextBox_TextChanged()
    this->ui->newPasswordError->setVisible(false);
}
