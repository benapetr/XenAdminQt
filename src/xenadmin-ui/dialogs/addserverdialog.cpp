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
#include "../connectionprofile.h"
#include "../../xenlib/xen/network/connection.h"
#include <QtWidgets/QStyle>

namespace
{
    const char* kTextAddNewConnectTo = "Add New Server";
    const char* kTextAddNewEnterCredentials =
        "Enter the host name or IP address of the server you want to add and your user login credentials for that server.";
    const char* kTextConnectToServer = "Connect to Server";
    const char* kTextConnectToServerBlurb = "Enter your user name and password to connect to this server.";
    const char* kTextAddServerPassNew =
        "XenAdmin Qt can no longer authenticate with the existing credentials for this server. Enter new credentials to proceed.";
    const char* kTextErrorConnectingBlurb = "XenAdmin Qt has encountered a problem connecting to this server.";
    const char* kTextAddNewIncorrect = "Incorrect user name and/or password.";
}

AddServerDialog::AddServerDialog(XenConnection* connection, bool changedPass, QWidget* parent)
    : QDialog(parent),
      ui(new Ui::AddServerDialog),
      m_connection(connection),
      m_changedPass(changedPass)
{
    ui->setupUi(this);

    ui->ServerNameComboBox->setEditable(true);

    const QList<ConnectionProfile> profiles = SettingsManager::instance().loadConnectionProfiles();
    QStringList history;
    history.reserve(profiles.size());
    for (const ConnectionProfile& profile : profiles)
    {
        if (!profile.hostname().isEmpty())
            history.append(profile.hostname());
    }
    history.removeDuplicates();
    std::sort(history.begin(), history.end(), [](const QString& a, const QString& b) {
        return a.toLower() < b.toLower();
    });

    ui->ServerNameComboBox->addItems(history);

    if (m_connection)
    {
        ui->ServerNameComboBox->setEditText(hostnameWithPort());
        ui->UsernameTextBox->setText(m_connection->GetUsername());
        ui->PasswordTextBox->setText(m_connection->GetPassword());
    }

    connect(ui->ServerNameComboBox->lineEdit(), &QLineEdit::textChanged, this, &AddServerDialog::TextFields_TextChanged);
    connect(ui->UsernameTextBox, &QLineEdit::textChanged, this, &AddServerDialog::TextFields_TextChanged);
    connect(ui->PasswordTextBox, &QLineEdit::textChanged, this, &AddServerDialog::TextFields_TextChanged);
    connect(ui->AddButton, &QPushButton::clicked, this, &AddServerDialog::AddButton_Click);
    connect(ui->CancelButton2, &QPushButton::clicked, this, &AddServerDialog::CancelButton2_Click);
    QIcon warnIcon = style()->standardIcon(QStyle::SP_MessageBoxWarning);
    ui->pictureBoxError->setPixmap(warnIcon.pixmap(16, 16));

    AddServerDialog_Load();
    labelError_TextChanged();
}

AddServerDialog::~AddServerDialog()
{
    delete ui;
}

QString AddServerDialog::serverInput() const
{
    return ui->ServerNameComboBox->currentText().trimmed();
}

QString AddServerDialog::username() const
{
    return ui->UsernameTextBox->text().trimmed();
}

QString AddServerDialog::password() const
{
    return ui->PasswordTextBox->text();
}

void AddServerDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    AddServerDialog_Shown();
}

void AddServerDialog::AddServerDialog_Load()
{
    UpdateText();
    UpdateButtons();
}

void AddServerDialog::AddServerDialog_Shown()
{
    if (!ui->ServerNameComboBox->isEnabled() && m_connection && !m_connection->GetUsername().isEmpty())
        ui->PasswordTextBox->setFocus();
}

void AddServerDialog::UpdateText()
{
    if (!m_connection)
    {
        setWindowTitle(kTextAddNewConnectTo);
        ui->labelInstructions->setText(kTextAddNewEnterCredentials);
        ui->labelError->setText(QString());
        ui->ServerNameComboBox->setEnabled(true);
        ui->AddButton->setText(tr("&Add"));
        return;
    }

    if (!m_changedPass)
        return;

    if (m_connection->GetPassword().isEmpty())
    {
        setWindowTitle(kTextConnectToServer);
        ui->labelInstructions->setText(kTextConnectToServerBlurb);
        ui->labelError->setText(QString());
        ui->ServerNameComboBox->setEnabled(false);
        ui->AddButton->setText(tr("Connect"));
    } else
    {
        setWindowTitle(kTextConnectToServer);
        ui->labelInstructions->setText(kTextErrorConnectingBlurb);
        ui->labelError->setText(kTextAddNewIncorrect);
        ui->ServerNameComboBox->setEnabled(false);
        ui->AddButton->setText(tr("Connect"));
    }
}

void AddServerDialog::AddButton_Click()
{
    accept();
}

void AddServerDialog::CancelButton2_Click()
{
    reject();
}

void AddServerDialog::TextFields_TextChanged()
{
    UpdateButtons();
}

void AddServerDialog::UpdateButtons()
{
    ui->AddButton->setEnabled(OKButtonEnabled());
}

bool AddServerDialog::OKButtonEnabled() const
{
    return !ui->ServerNameComboBox->currentText().trimmed().isEmpty() &&
           !ui->UsernameTextBox->text().trimmed().isEmpty();
}

void AddServerDialog::labelError_TextChanged()
{
    const bool hasError = !ui->labelError->text().isEmpty();
    ui->labelError->setVisible(hasError);
    ui->pictureBoxError->setVisible(hasError);
}

QString AddServerDialog::hostnameWithPort() const
{
    if (!m_connection)
        return QString();

    const QString hostname = m_connection->GetHostname();
    const int port = m_connection->GetPort();
    if (port == 443 || hostname.isEmpty())
        return hostname;

    return QString("%1:%2").arg(hostname).arg(port);
}
