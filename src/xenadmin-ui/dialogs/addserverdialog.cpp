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

#include "addserverdialog.h"
#include "ui_addserverdialog.h"
#include "../settingsmanager.h"
#include "xenlib/xen/network/connection.h"
#include <QtWidgets/QStyle>

namespace
{
    const char* kTextAddNewConnectTo = "Add New Server";
    const char* kTextAddNewEnterCredentials =
        "Enter the host name or IP address of the server you want to add and your user login credentials for that server.";
    const char* kTextConnectToServer = "Connect to Server";
    const char* kTextConnectToServerBlurb = "Enter your user name and password to connect to this server.";
    const char* kTextErrorConnectingBlurb = "XenAdmin Qt has encountered a problem connecting to this server.";
    const char* kTextAddNewIncorrect = "Incorrect user name and/or password.";
}

AddServerDialog::AddServerDialog(XenConnection* connection, bool changedPass, QWidget* parent)
    : QDialog(parent),
      ui(new Ui::AddServerDialog),
      m_connection(connection),
      m_changedPass(changedPass)
{
    this->ui->setupUi(this);

    this->ui->ServerNameComboBox->setEditable(true);

    QStringList history = SettingsManager::instance().getServerHistory();
    history.removeDuplicates();
    std::sort(history.begin(), history.end(), [](const QString& a, const QString& b) {
        return a.toLower() < b.toLower();
    });

    this->ui->ServerNameComboBox->addItems(history);

    if (this->m_connection)
    {
        this->ui->ServerNameComboBox->setEditText(this->hostnameWithPort());
        this->ui->UsernameTextBox->setText(this->m_connection->GetUsername());
        this->ui->PasswordTextBox->setText(this->m_connection->GetPassword());
    }

    connect(this->ui->ServerNameComboBox->lineEdit(), &QLineEdit::textChanged, this, &AddServerDialog::TextFields_TextChanged);
    connect(this->ui->UsernameTextBox, &QLineEdit::textChanged, this, &AddServerDialog::TextFields_TextChanged);
    connect(this->ui->PasswordTextBox, &QLineEdit::textChanged, this, &AddServerDialog::TextFields_TextChanged);
    connect(this->ui->AddButton, &QPushButton::clicked, this, &AddServerDialog::AddButton_Click);
    connect(this->ui->CancelButton2, &QPushButton::clicked, this, &AddServerDialog::CancelButton2_Click);
    QIcon warnIcon = this->style()->standardIcon(QStyle::SP_MessageBoxWarning);
    this->ui->pictureBoxError->setPixmap(warnIcon.pixmap(16, 16));

    this->AddServerDialog_Load();
    this->labelError_TextChanged();
}

AddServerDialog::~AddServerDialog()
{
    delete this->ui;
}

QString AddServerDialog::serverInput() const
{
    return this->ui->ServerNameComboBox->currentText().trimmed();
}

QString AddServerDialog::username() const
{
    return this->ui->UsernameTextBox->text().trimmed();
}

QString AddServerDialog::password() const
{
    return this->ui->PasswordTextBox->text();
}

void AddServerDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    this->AddServerDialog_Shown();
}

void AddServerDialog::AddServerDialog_Load()
{
    this->UpdateText();
    this->UpdateButtons();
}

void AddServerDialog::AddServerDialog_Shown()
{
    if (!this->ui->ServerNameComboBox->isEnabled() && this->m_connection && !this->m_connection->GetUsername().isEmpty())
        this->ui->PasswordTextBox->setFocus();
}

void AddServerDialog::UpdateText()
{
    if (!this->m_connection)
    {
        this->setWindowTitle(kTextAddNewConnectTo);
        this->ui->labelInstructions->setText(kTextAddNewEnterCredentials);
        this->ui->labelError->setText(QString());
        this->ui->ServerNameComboBox->setEnabled(true);
        this->ui->AddButton->setText(this->tr("&Add"));
        return;
    }

    if (!this->m_changedPass)
        return;

    if (this->m_connection->GetPassword().isEmpty())
    {
        this->setWindowTitle(kTextConnectToServer);
        this->ui->labelInstructions->setText(kTextConnectToServerBlurb);
        this->ui->labelError->setText(QString());
        this->ui->ServerNameComboBox->setEnabled(false);
        this->ui->AddButton->setText(this->tr("Connect"));
    } else
    {
        this->setWindowTitle(kTextConnectToServer);
        this->ui->labelInstructions->setText(kTextErrorConnectingBlurb);
        this->ui->labelError->setText(kTextAddNewIncorrect);
        this->ui->ServerNameComboBox->setEnabled(false);
        this->ui->AddButton->setText(this->tr("Connect"));
    }
}

void AddServerDialog::AddButton_Click()
{
    this->accept();
}

void AddServerDialog::CancelButton2_Click()
{
    this->reject();
}

void AddServerDialog::TextFields_TextChanged()
{
    this->UpdateButtons();
}

void AddServerDialog::UpdateButtons()
{
    this->ui->AddButton->setEnabled(this->OKButtonEnabled());
}

bool AddServerDialog::OKButtonEnabled() const
{
    return !this->ui->ServerNameComboBox->currentText().trimmed().isEmpty() && !this->ui->UsernameTextBox->text().trimmed().isEmpty();
}

void AddServerDialog::labelError_TextChanged()
{
    const bool hasError = !this->ui->labelError->text().isEmpty();
    this->ui->labelError->setVisible(hasError);
    this->ui->pictureBoxError->setVisible(hasError);
}

QString AddServerDialog::hostnameWithPort() const
{
    if (!this->m_connection)
        return QString();

    const QString hostname = this->m_connection->GetHostname();
    const int port = this->m_connection->GetPort();
    if (port == 443 || hostname.isEmpty())
        return hostname;

    return QString("%1:%2").arg(hostname).arg(port);
}
