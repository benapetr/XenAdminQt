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

#include "vncpassworddialog.h"
#include "ui_vncpassworddialog.h"
#include <QPushButton>

VNCPasswordDialog::VNCPasswordDialog(bool passwordIncorrect, QWidget* parent)
    : QDialog(parent), ui(new Ui::VNCPasswordDialog)
{
    this->ui->setupUi(this);
    this->ui->passwordFailureLabel->setVisible(passwordIncorrect);
    this->ui->passwordFailureLabel->setStyleSheet(QStringLiteral("color: red;"));

    // C# passBox_TextChanged: typing hides the failure, OK needs a password
    connect(this->ui->passBox, &QLineEdit::textChanged, this, [this]()
    {
        this->ui->passwordFailureLabel->setVisible(false);
        this->updateOkButton();
    });
    this->updateOkButton();
}

VNCPasswordDialog::~VNCPasswordDialog()
{
    delete this->ui;
}

QString VNCPasswordDialog::GetPassword() const
{
    return this->ui->passBox->text();
}

void VNCPasswordDialog::updateOkButton()
{
    this->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!this->ui->passBox->text().isEmpty());
}
