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

#include "connectdialog.h"
#include "ui_connectdialog.h"
#include "../settingsmanager.h"
#include <QPushButton>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QDateTime>

ConnectDialog::ConnectDialog(QWidget* parent)
    : QDialog(parent), ui(new Ui::ConnectDialog)
{
    ui->setupUi(this);

    // Set up Connect button text and validation
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText("Connect");

    // Connect validation signals
    connect(ui->hostnameEdit, &QLineEdit::textChanged, this, &ConnectDialog::validateInput);
    connect(ui->usernameEdit, &QLineEdit::textChanged, this, &ConnectDialog::validateInput);
    connect(ui->passwordEdit, &QLineEdit::textChanged, this, &ConnectDialog::validateInput);

    // Connect profile controls
    connect(ui->profileComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ConnectDialog::onProfileSelected);
    connect(ui->saveProfileCheckBox, &QCheckBox::stateChanged, this, &ConnectDialog::onSaveProfileChanged);
    connect(ui->deleteProfileButton, &QPushButton::clicked, this, &ConnectDialog::onDeleteProfile);

    // Load saved profiles
    loadProfiles();

    validateInput();
}

ConnectDialog::ConnectDialog(const QString& hostname, int port, const QString& username,
                             const QString& errorMessage, QWidget* parent)
    : QDialog(parent), ui(new Ui::ConnectDialog)
{
    ui->setupUi(this);

    // Set up for retry mode
    setWindowTitle(tr("Authentication Failed - Reconnect"));
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText("Retry");

    // Connect validation signals
    connect(ui->hostnameEdit, &QLineEdit::textChanged, this, &ConnectDialog::validateInput);
    connect(ui->usernameEdit, &QLineEdit::textChanged, this, &ConnectDialog::validateInput);
    connect(ui->passwordEdit, &QLineEdit::textChanged, this, &ConnectDialog::validateInput);

    // Connect profile controls
    connect(ui->profileComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ConnectDialog::onProfileSelected);
    connect(ui->saveProfileCheckBox, &QCheckBox::stateChanged, this, &ConnectDialog::onSaveProfileChanged);
    connect(ui->deleteProfileButton, &QPushButton::clicked, this, &ConnectDialog::onDeleteProfile);

    // Load saved profiles
    loadProfiles();

    // Pre-fill with the failed connection details
    ui->hostnameEdit->setText(hostname);
    ui->hostnameEdit->setEnabled(false); // Don't allow changing hostname during retry
    ui->portSpinBox->setValue(port);
    ui->portSpinBox->setEnabled(false); // Don't allow changing port during retry
    ui->usernameEdit->setText(username);
    ui->passwordEdit->clear();    // Clear the failed password
    ui->passwordEdit->setFocus(); // Focus on password field for user convenience

    // Show error message if provided
    if (!errorMessage.isEmpty())
    {
        // Add error label at the top of the dialog if it exists in the UI
        // For now, we'll show it in the status bar or as a message box before showing dialog
        // This would require updating the .ui file to add an error label
        // For simplicity, we can show it via a message box or window title
        setWindowTitle(tr("Authentication Failed - %1").arg(errorMessage));
    }

    validateInput();
}

ConnectDialog::~ConnectDialog()
{
    delete ui;
}

void ConnectDialog::validateInput()
{
    bool valid = !ui->hostnameEdit->text().isEmpty() &&
                 !ui->usernameEdit->text().isEmpty() &&
                 !ui->passwordEdit->text().isEmpty();

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(valid);
}

QString ConnectDialog::getHostname() const
{
    return ui->hostnameEdit->text();
}

int ConnectDialog::getPort() const
{
    return ui->portSpinBox->value();
}

QString ConnectDialog::getUsername() const
{
    return ui->usernameEdit->text();
}

QString ConnectDialog::getPassword() const
{
    return ui->passwordEdit->text();
}

bool ConnectDialog::useSSL() const
{
    return ui->sslCheckBox->isChecked();
}

bool ConnectDialog::saveProfile() const
{
    return ui->saveProfileCheckBox && ui->saveProfileCheckBox->isChecked();
}

QString ConnectDialog::getProfileName() const
{
    if (ui->profileComboBox && ui->profileComboBox->currentIndex() > 0)
    {
        // Return selected profile name (index > 0 because 0 is "New Connection")
        return ui->profileComboBox->currentText();
    }
    return QString();
}

ConnectionProfile ConnectDialog::getConnectionProfile() const
{
    ConnectionProfile profile;

    // Generate profile name from hostname if not from existing profile
    QString profileName = getProfileName();
    if (profileName.isEmpty() || profileName == tr("New Connection"))
    {
        profileName = getHostname();
    }

    profile.setName(profileName);
    profile.setHostname(getHostname());
    profile.setPort(getPort());
    profile.setUsername(getUsername());
    profile.setPassword(getPassword());
    profile.setRememberPassword(saveProfile());
    profile.setUseSSL(useSSL());
    profile.setFriendlyName(getHostname());
    profile.setAutoConnect(false); // Can be changed in future versions
    profile.setLastConnected(QDateTime::currentSecsSinceEpoch());

    return profile;
}

void ConnectDialog::onProfileSelected(int index)
{
    if (index <= 0 || index > m_profiles.size())
    {
        // "New Connection" selected or invalid index - clear fields
        ui->deleteProfileButton->setEnabled(false);
        return;
    }

    // Load the selected profile (index - 1 because first item is "New Connection")
    const ConnectionProfile& profile = m_profiles[index - 1];
    fillFromProfile(profile);

    // Enable delete button for saved profiles
    ui->deleteProfileButton->setEnabled(true);
}

void ConnectDialog::onSaveProfileChanged(int state)
{
    // This slot handles the save profile checkbox state change
    Q_UNUSED(state);
}

void ConnectDialog::onDeleteProfile()
{
    int index = ui->profileComboBox->currentIndex();
    if (index <= 0 || index > m_profiles.size())
    {
        return; // Nothing to delete
    }

    const ConnectionProfile& profile = m_profiles[index - 1];

    // Show confirmation dialog
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Delete Profile"),
        tr("Are you sure you want to delete the profile '%1'?").arg(profile.displayName()),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        // Remove from settings
        SettingsManager::instance().removeConnectionProfile(profile.name());

        // Reload profiles
        loadProfiles();

        // Select "New Connection"
        ui->profileComboBox->setCurrentIndex(0);
    }
}

void ConnectDialog::loadProfiles()
{
    // Clear existing items
    ui->profileComboBox->clear();
    m_profiles.clear();

    // Add "New Connection" as first item
    ui->profileComboBox->addItem(tr("New Connection"));

    // Load saved profiles
    m_profiles = SettingsManager::instance().loadConnectionProfiles();

    // Add profiles to combo box
    for (const ConnectionProfile& profile : m_profiles)
    {
        ui->profileComboBox->addItem(profile.displayName());
    }

    // Try to select the last used profile
    ConnectionProfile lastProfile = SettingsManager::instance().getLastConnectionProfile();
    if (lastProfile.isValid())
    {
        for (int i = 0; i < m_profiles.size(); ++i)
        {
            if (m_profiles[i].name() == lastProfile.name())
            {
                ui->profileComboBox->setCurrentIndex(i + 1);
                fillFromProfile(m_profiles[i]);
                break;
            }
        }
    }
}

void ConnectDialog::fillFromProfile(const ConnectionProfile& profile)
{
    if (!profile.isValid())
    {
        return;
    }

    // Fill form fields from profile
    ui->hostnameEdit->setText(profile.hostname());
    ui->portSpinBox->setValue(profile.port());
    ui->usernameEdit->setText(profile.username());
    ui->sslCheckBox->setChecked(profile.useSSL());

    // Only fill password if it was remembered
    if (profile.rememberPassword() && !profile.password().isEmpty())
    {
        ui->passwordEdit->setText(profile.password());
    }
}
