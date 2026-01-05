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

// connectionoptionspage.cpp - Connection and proxy settings options page
#include "connectionoptionspage.h"
#include "../../settingsmanager.h"
#include <QRegularExpression>
#include <QToolTip>

ConnectionOptionsPage::ConnectionOptionsPage(QWidget* parent)
    : IOptionsPage(parent), ui(new Ui::ConnectionOptionsPage), invalidControl(nullptr), eventsDisabled(false)
{
    this->ui->setupUi(this);

    // Connect signals (matches C# event handlers)
    connect(this->ui->DirectConnectionRadioButton, &QRadioButton::toggled, this, &ConnectionOptionsPage::onProxySettingChanged);
    connect(this->ui->UseIERadioButton, &QRadioButton::toggled, this, &ConnectionOptionsPage::onProxySettingChanged);
    connect(this->ui->UseProxyRadioButton, &QRadioButton::toggled, this, &ConnectionOptionsPage::onProxySettingChanged);

    connect(this->ui->AuthenticationCheckBox, &QCheckBox::toggled, this, &ConnectionOptionsPage::onAuthenticationCheckBoxChanged);

    connect(this->ui->ProxyAddressTextBox, &QLineEdit::textChanged, this, &ConnectionOptionsPage::onProxyFieldChanged);
    connect(this->ui->ProxyPortTextBox, &QLineEdit::textChanged, this, &ConnectionOptionsPage::onProxyFieldChanged);
    connect(this->ui->ProxyUsernameTextBox, &QLineEdit::textChanged, this, &ConnectionOptionsPage::onProxyFieldChanged);
    connect(this->ui->ProxyPasswordTextBox, &QLineEdit::textChanged, this, &ConnectionOptionsPage::onProxyFieldChanged);

    connect(this->ui->BasicRadioButton, &QRadioButton::toggled, this, &ConnectionOptionsPage::onProxyFieldChanged);
    connect(this->ui->DigestRadioButton, &QRadioButton::toggled, this, &ConnectionOptionsPage::onProxyFieldChanged);

    this->updateControlStates();
}

ConnectionOptionsPage::~ConnectionOptionsPage()
{
    delete this->ui;
}

QString ConnectionOptionsPage::GetText() const
{
    // Matches C# Messages.CONNECTION
    return tr("Connection");
}

QString ConnectionOptionsPage::GetSubText() const
{
    // Matches C# Messages.CONNECTION_DESC
    return tr("Configure connection and proxy settings");
}

QIcon ConnectionOptionsPage::GetImage() const
{
    // Matches C# Images.StaticImages._000_Network_h32bit_16
    return QIcon(":/icons/network_16.png");
}

void ConnectionOptionsPage::Build()
{
    // Matches C# ConnectionOptionsPage.Build()
    this->eventsDisabled = true;

    SettingsManager& settings = SettingsManager::instance();

    // Proxy server settings
    int proxyStyle = settings.getValue("Connection/ProxySetting", 0).toInt();
    switch (proxyStyle)
    {
    case 0: // DirectConnection
        this->ui->DirectConnectionRadioButton->setChecked(true);
        break;
    case 1: // SystemProxy
        this->ui->UseIERadioButton->setChecked(true);
        break;
    case 2: // SpecifiedProxy
        this->ui->UseProxyRadioButton->setChecked(true);
        break;
    default:
        this->ui->DirectConnectionRadioButton->setChecked(true);
        break;
    }

    this->ui->ProxyAddressTextBox->setText(settings.getValue("Connection/ProxyAddress", "").toString());
    this->ui->ProxyPortTextBox->setText(settings.getValue("Connection/ProxyPort", "80").toString());
    this->ui->BypassForServersCheckbox->setChecked(settings.getValue("Connection/BypassProxyForServers", false).toBool());

    this->ui->AuthenticationCheckBox->setChecked(settings.getValue("Connection/ProvideProxyAuthentication", false).toBool());

    // Authentication method
    int authMethod = settings.getValue("Connection/ProxyAuthenticationMethod", 1).toInt(); // 1 = Digest
    if (authMethod == 0)
    { // Basic
        this->ui->BasicRadioButton->setChecked(true);
    } else
    {
        this->ui->DigestRadioButton->setChecked(true);
    }

    // Credentials (stored encrypted in C#, plain in Qt for now)
    this->ui->ProxyUsernameTextBox->setText(settings.getValue("Connection/ProxyUsername", "").toString());
    this->ui->ProxyPasswordTextBox->setText(settings.getValue("Connection/ProxyPassword", "").toString());

    // Connection timeout (C# stores in milliseconds)
    int timeoutMs = settings.getValue("Connection/ConnectionTimeout", 20000).toInt();
    this->ui->ConnectionTimeoutSpinBox->setValue(timeoutMs / 1000);

    this->eventsDisabled = false;
    this->updateControlStates();
}

void ConnectionOptionsPage::onProxySettingChanged()
{
    if (this->eventsDisabled)
        return;
    this->updateControlStates();
}

void ConnectionOptionsPage::onAuthenticationCheckBoxChanged(bool checked)
{
    if (this->eventsDisabled)
        return;

    this->eventsDisabled = true;

    if (!checked)
    {
        this->ui->ProxyUsernameTextBox->clear();
        this->ui->ProxyPasswordTextBox->clear();
    }
    this->selectUseThisProxyServer();

    this->eventsDisabled = false;
    this->updateControlStates();
}

void ConnectionOptionsPage::onProxyFieldChanged()
{
    if (this->eventsDisabled)
        return;

    QObject* sender = QObject::sender();

    if (sender == this->ui->ProxyAddressTextBox || sender == this->ui->ProxyPortTextBox)
    {
        this->selectUseThisProxyServer();
    } else if (sender == this->ui->ProxyUsernameTextBox || sender == this->ui->ProxyPasswordTextBox ||
               sender == this->ui->BasicRadioButton || sender == this->ui->DigestRadioButton)
    {
        this->selectProvideCredentials();
    }

    this->updateControlStates();
}

void ConnectionOptionsPage::selectUseThisProxyServer()
{
    if (!this->eventsDisabled)
        this->ui->UseProxyRadioButton->setChecked(true);
}

void ConnectionOptionsPage::selectProvideCredentials()
{
    this->eventsDisabled = true;
    this->ui->AuthenticationCheckBox->setChecked(true);
    this->ui->UseProxyRadioButton->setChecked(true);
    this->eventsDisabled = false;
}

void ConnectionOptionsPage::updateControlStates()
{
    // Enable/disable proxy fields based on radio button selection
    bool useProxy = this->ui->UseProxyRadioButton->isChecked();
    this->ui->ProxyAddressTextBox->setEnabled(useProxy);
    this->ui->ProxyPortTextBox->setEnabled(useProxy);
    this->ui->BypassForServersCheckbox->setEnabled(useProxy);
    this->ui->AuthenticationCheckBox->setEnabled(useProxy);

    // Enable/disable authentication fields
    bool useAuth = useProxy && this->ui->AuthenticationCheckBox->isChecked();
    this->ui->ProxyUsernameTextBox->setEnabled(useAuth);
    this->ui->ProxyPasswordTextBox->setEnabled(useAuth);
    this->ui->AuthMethodLabel->setEnabled(useAuth);
    this->ui->BasicRadioButton->setEnabled(useAuth);
    this->ui->DigestRadioButton->setEnabled(useAuth);
}

bool ConnectionOptionsPage::IsValidToSave(QWidget** control, QString& invalidReason)
{
    // Matches C# ConnectionOptionsPage.IsValidToSave()

    if (!this->ui->UseProxyRadioButton->isChecked())
    {
        *control = nullptr;
        invalidReason.clear();
        return true;
    }

    invalidReason = tr("Invalid parameter");

    // Validate proxy address
    QString address = this->ui->ProxyAddressTextBox->text().trimmed();
    if (address.isEmpty())
    {
        *control = this->ui->ProxyAddressTextBox;
        return false;
    }

    // Check if it's a valid hostname or IPv4 address
    QRegularExpression ipv6Regex("^\\[.*:.*\\]$");
    if (ipv6Regex.match(address).hasMatch())
    {
        // IPv6 not supported in C# version
        *control = this->ui->ProxyAddressTextBox;
        return false;
    }

    // Validate port
    QString portText = this->ui->ProxyPortTextBox->text().trimmed();
    bool ok;
    int port = portText.toInt(&ok);
    if (!ok || port < 1 || port > 65535)
    {
        *control = this->ui->ProxyPortTextBox;
        return false;
    }

    // Validate authentication credentials if enabled
    if (this->ui->AuthenticationCheckBox->isChecked())
    {
        if (this->ui->ProxyUsernameTextBox->text().trimmed().isEmpty())
        {
            *control = this->ui->ProxyUsernameTextBox;
            return false;
        }
    }

    *control = nullptr;
    invalidReason.clear();
    return true;
}

void ConnectionOptionsPage::ShowValidationMessages(QWidget* control, const QString& message)
{
    // Matches C# ConnectionOptionsPage.ShowValidationMessages()
    if (control && !message.isEmpty())
    {
        this->invalidControl = control;
        QToolTip::showText(control->mapToGlobal(QPoint(0, control->height())),
                           message, control);
    }
}

void ConnectionOptionsPage::HideValidationMessages()
{
    // Matches C# ConnectionOptionsPage.HideValidationMessages()
    if (this->invalidControl)
    {
        QToolTip::hideText();
        this->invalidControl = nullptr;
    }
}

void ConnectionOptionsPage::Save()
{
    // Matches C# ConnectionOptionsPage.Save()
    SettingsManager& settings = SettingsManager::instance();

    // Proxy server settings
    int proxyStyle = 0; // DirectConnection
    if (this->ui->UseIERadioButton->isChecked())
    {
        proxyStyle = 1; // SystemProxy
    } else if (this->ui->UseProxyRadioButton->isChecked())
    {
        proxyStyle = 2; // SpecifiedProxy
    }
    settings.setValue("Connection/ProxySetting", proxyStyle);

    QString address = this->ui->ProxyAddressTextBox->text().trimmed();
    if (!address.isEmpty())
    {
        settings.setValue("Connection/ProxyAddress", address);
    }

    QString portText = this->ui->ProxyPortTextBox->text().trimmed();
    bool ok;
    int port = portText.toInt(&ok);
    if (ok && port > 0 && port <= 65535)
    {
        settings.setValue("Connection/ProxyPort", port);
    } else
    {
        settings.setValue("Connection/ProxyPort", 80);
    }

    settings.setValue("Connection/BypassProxyForServers", this->ui->BypassForServersCheckbox->isChecked());

    // Authentication settings
    settings.setValue("Connection/ProvideProxyAuthentication", this->ui->AuthenticationCheckBox->isChecked());

    // TODO: In C# these are encrypted with EncryptionUtils.Protect()
    // For now, store plain text (will implement encryption later)
    settings.setValue("Connection/ProxyUsername", this->ui->ProxyUsernameTextBox->text());
    settings.setValue("Connection/ProxyPassword", this->ui->ProxyPasswordTextBox->text());

    // Authentication method
    int authMethod = this->ui->BasicRadioButton->isChecked() ? 0 : 1; // 0 = Basic, 1 = Digest
    settings.setValue("Connection/ProxyAuthenticationMethod", authMethod);

    // Connection timeout (store in milliseconds like C#)
    int timeoutSeconds = this->ui->ConnectionTimeoutSpinBox->value();
    settings.setValue("Connection/ConnectionTimeout", timeoutSeconds * 1000);
}
