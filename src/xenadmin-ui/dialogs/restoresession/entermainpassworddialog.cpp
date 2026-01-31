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

#include "entermainpassworddialog.h"
#include "ui_entermainpassworddialog.h"
#include "utils/encryption.h"
#include <QtWidgets/QPushButton>

EnterMainPasswordDialog::EnterMainPasswordDialog(const QByteArray& temporaryMainPassword, QWidget* parent)
    : QDialog(parent), ui(new Ui::EnterMainPasswordDialog), temporaryMainPassword_(temporaryMainPassword)
{
    this->ui->setupUi(this);
    this->ui->passwordError->setVisible(false);

    // Connect signals
    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &EnterMainPasswordDialog::okButton_Click);
    connect(this->ui->mainTextBox, &QLineEdit::textChanged, this, &EnterMainPasswordDialog::mainTextBox_TextChanged);

    // Set initial state
    this->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
}

EnterMainPasswordDialog::~EnterMainPasswordDialog()
{
    delete this->ui;
}

void EnterMainPasswordDialog::okButton_Click()
{
    // Matches C# EnterMainPasswordDialog.okButton_Click()
    if (!this->ui->mainTextBox->text().isEmpty() &&
        EncryptionUtils::ArrayElementsEqual(EncryptionUtils::ComputeHash(this->ui->mainTextBox->text()),
                                            this->temporaryMainPassword_))
    {
        this->accept();
    } else
    {
        this->ui->passwordError->setText(tr("Incorrect password"));
        this->ui->passwordError->setVisible(true);
        this->ui->mainTextBox->setFocus();
        this->ui->mainTextBox->selectAll();
    }
}

void EnterMainPasswordDialog::mainTextBox_TextChanged(const QString& text)
{
    // Matches C# EnterMainPasswordDialog.mainTextBox_TextChanged()
    this->ui->passwordError->setVisible(false);
    this->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!text.isEmpty());
}
