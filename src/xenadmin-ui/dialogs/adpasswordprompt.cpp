/*
 * Copyright (c) 2026, Petr Bena <petr@bena.rocks>
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

#include "adpasswordprompt.h"
#include "ui_adpasswordprompt.h"

AdPasswordPrompt::AdPasswordPrompt(bool join, const QString& currentDomainName, QWidget* parent)
    : QDialog(parent), ui(new Ui::AdPasswordPrompt)
{
    this->ui->setupUi(this);
    this->ui->labelAdditionalInfo->setVisible(false);

    if (join)
    {
        const bool promptForDomain = currentDomainName.isEmpty();
        this->ui->labelBlurb->setText(promptForDomain
            ? tr("Enter your fully qualified AD domain name, and a user name and password with sufficient privileges to add servers to your domain.")
            : tr("Please enter a user name and password with sufficient privileges to add servers to domain '%1'.").arg(currentDomainName));
        this->ui->labelDomain->setVisible(promptForDomain);
        this->ui->textBoxDomain->setVisible(promptForDomain);
        this->ui->textBoxDomain->setText(currentDomainName);
        this->ui->buttonSkip->setVisible(false);
    } else
    {
        this->setWindowTitle(tr("Disabling AD authentication"));
        this->ui->labelBlurb->setText(tr("Enter a user name and password with sufficient privileges to remove your machine account from AD. "
                                         "Authentication will be disabled even if the machine account cannot be removed."));
        this->ui->textBoxDomain->setText(currentDomainName);
        this->ui->textBoxDomain->setEnabled(false);
        this->ui->labelAdditionalInfo->setVisible(true);
        this->ui->labelAdditionalInfo->setText(tr("Alternatively, select ignore to leave the machine account enabled."));
        this->ui->buttonOk->setText(tr("Disa&ble"));
    }

    connect(this->ui->buttonOk, &QPushButton::clicked, this, &QDialog::accept);
    connect(this->ui->buttonCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(this->ui->buttonSkip, &QPushButton::clicked, this, [this]() { this->done(Ignored); });
    connect(this->ui->textBoxDomain, &QLineEdit::textChanged, this, &AdPasswordPrompt::updateOkButton);
    connect(this->ui->textBoxUsername, &QLineEdit::textChanged, this, &AdPasswordPrompt::updateOkButton);
    connect(this->ui->textBoxPassword, &QLineEdit::textChanged, this, &AdPasswordPrompt::updateOkButton);
    this->updateOkButton();
}

AdPasswordPrompt::~AdPasswordPrompt()
{
    delete this->ui;
}

QString AdPasswordPrompt::Domain() const
{
    return this->ui->textBoxDomain->text().trimmed();
}

QString AdPasswordPrompt::Username() const
{
    return this->ui->textBoxUsername->text().trimmed();
}

QString AdPasswordPrompt::Password() const
{
    return this->ui->textBoxPassword->text();
}

void AdPasswordPrompt::SetDomain(const QString& domain)
{
    this->ui->textBoxDomain->setText(domain);
}

void AdPasswordPrompt::SetUsername(const QString& username)
{
    this->ui->textBoxUsername->setText(username);
}

void AdPasswordPrompt::updateOkButton()
{
    // C# EnableOkButton: with the domain box shown all three are needed, otherwise the user name
    const bool domainShown = !this->ui->textBoxDomain->isHidden();
    this->ui->buttonOk->setEnabled(domainShown
        ? !this->Domain().isEmpty() && !this->Username().isEmpty() && !this->ui->textBoxPassword->text().trimmed().isEmpty()
        : !this->Username().isEmpty());
}
