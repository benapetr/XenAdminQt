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

// loggingoptionspage.cpp - Application log file options page
// C# has no options for its log4net log; this page exists so the log can be turned off,
// made more detailed, or kept smaller.
#include "loggingoptionspage.h"
#include "ui_loggingoptionspage.h"
#include "../../settingsmanager.h"
#include "../../mainwindow.h"
#include <QDir>

LoggingOptionsPage::LoggingOptionsPage(QWidget* parent) : IOptionsPage(parent), ui(new Ui::LoggingOptionsPage)
{
    this->ui->setupUi(this);

    this->ui->LevelComboBox->addItem(tr("Information, warnings and errors"));
    this->ui->LevelComboBox->addItem(tr("Everything, including debug messages"));

    connect(this->ui->EnableCheckBox, &QCheckBox::toggled, this, &LoggingOptionsPage::updateEnabledState);
    connect(this->ui->OpenFolderButton, &QPushButton::clicked, this, &LoggingOptionsPage::openLogFolder);
}

LoggingOptionsPage::~LoggingOptionsPage()
{
    delete this->ui;
}

QString LoggingOptionsPage::GetText() const
{
    return tr("Logging");
}

QString LoggingOptionsPage::GetSubText() const
{
    return tr("Configure the application log file");
}

QIcon LoggingOptionsPage::GetImage() const
{
    return QIcon(":/icons/log_destination_16.png");
}

void LoggingOptionsPage::Build()
{
    SettingsManager& settings = SettingsManager::instance();
    this->ui->EnableCheckBox->setChecked(settings.GetFileLoggingEnabled());
    this->ui->LevelComboBox->setCurrentIndex(settings.GetFileLoggingVerbose() ? 1 : 0);
    this->ui->MaxSizeSpinBox->setValue(settings.GetFileLoggingMaxSizeMB());
    this->ui->LocationValueLabel->setText(QDir::toNativeSeparators(settings.GetLogDirectory()));
    this->updateEnabledState();
}

void LoggingOptionsPage::updateEnabledState()
{
    const bool enabled = this->ui->EnableCheckBox->isChecked();
    this->ui->LevelLabel->setEnabled(enabled);
    this->ui->LevelComboBox->setEnabled(enabled);
    this->ui->MaxSizeLabel->setEnabled(enabled);
    this->ui->MaxSizeSpinBox->setEnabled(enabled);
}

void LoggingOptionsPage::openLogFolder()
{
    MainWindow::ViewLogFiles(this);
}

bool LoggingOptionsPage::IsValidToSave(QWidget** control, QString& invalidReason)
{
    Q_UNUSED(control);
    Q_UNUSED(invalidReason);
    return true;
}

void LoggingOptionsPage::ShowValidationMessages(QWidget* control, const QString& message)
{
    Q_UNUSED(control);
    Q_UNUSED(message);
}

void LoggingOptionsPage::HideValidationMessages()
{
}

void LoggingOptionsPage::Save()
{
    SettingsManager& settings = SettingsManager::instance();
    settings.SetFileLoggingEnabled(this->ui->EnableCheckBox->isChecked());
    settings.SetFileLoggingVerbose(this->ui->LevelComboBox->currentIndex() == 1);
    settings.SetFileLoggingMaxSizeMB(this->ui->MaxSizeSpinBox->value());
    settings.ApplyFileLoggingSettings();
}
