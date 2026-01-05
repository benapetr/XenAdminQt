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

// consolesoptionspage.cpp - Console settings options page
#include "consolesoptionspage.h"
#include "../../settingsmanager.h"

ConsolesOptionsPage::ConsolesOptionsPage(QWidget* parent)
    : IOptionsPage(parent), ui(new Ui::ConsolesOptionsPage)
{
    this->ui->setupUi(this);
}

ConsolesOptionsPage::~ConsolesOptionsPage()
{
    delete this->ui;
}

QString ConsolesOptionsPage::GetText() const
{
    // Matches C# Messages.CONSOLE
    return tr("Console");
}

QString ConsolesOptionsPage::GetSubText() const
{
    // Matches C# Messages.CONSOLE_DESC
    return tr("Configure console settings");
}

QIcon ConsolesOptionsPage::GetImage() const
{
    // Matches C# Images.StaticImages.console_16
    return QIcon(":/icons/console_16.png");
}

void ConsolesOptionsPage::Build()
{
    // Matches C# ConsolesOptionsPage.Build()

    // Fullscreen shortcut keys
    this->buildKeyCodeListBox();

    // Dock-undock shortcut keys
    this->buildDockKeyCodeComboBox();

    // Uncapture keyboard and mouse shortcut keys
    this->buildUncaptureKeyCodeComboBox();

    SettingsManager& settings = SettingsManager::instance();

    // Windows Remote Desktop console
    this->ui->WindowsKeyCheckBox->setChecked(
        settings.getValue("Console/WindowsShortcuts", true).toBool());
    this->ui->SoundCheckBox->setChecked(
        settings.getValue("Console/ReceiveSoundFromRDP", false).toBool());
    this->ui->AutoSwitchCheckBox->setChecked(
        settings.getValue("Console/AutoSwitchToRDP", true).toBool());
    this->ui->ClipboardCheckBox->setChecked(
        settings.getValue("Console/ClipboardAndPrinterRedirection", true).toBool());
    this->ui->ConnectToServerConsoleCheckBox->setChecked(
        settings.getValue("Console/ConnectToServerConsole", false).toBool());

    // Console scaling
    this->ui->PreserveUndockedScaleCheckBox->setChecked(
        settings.getValue("Console/PreserveScaleWhenUndocked", false).toBool());
    this->ui->PreserveVNCConsoleScalingCheckBox->setChecked(
        settings.getValue("Console/PreserveScaleWhenSwitchBackToVNC", false).toBool());
}

void ConsolesOptionsPage::buildKeyCodeListBox()
{
    // Matches C# buildKeyCodeListBox
    this->ui->KeyComboListBox->clear();
    this->ui->KeyComboListBox->addItem("Ctrl+Alt");
    this->ui->KeyComboListBox->addItem("Ctrl+Alt+F");
    this->ui->KeyComboListBox->addItem("F12");
    this->ui->KeyComboListBox->addItem("Ctrl+Enter");
    this->selectKeyCombo();
}

void ConsolesOptionsPage::buildDockKeyCodeComboBox()
{
    // Matches C# buildDockKeyCodeComboBox
    this->ui->DockKeyComboBox->clear();
    this->ui->DockKeyComboBox->addItem(tr("None"));
    this->ui->DockKeyComboBox->addItem("Alt+Shift+U");
    this->ui->DockKeyComboBox->addItem("F11");
    this->selectDockKeyCombo();
}

void ConsolesOptionsPage::selectDockKeyCombo()
{
    // Matches C# selectDockKeyCombo
    SettingsManager& settings = SettingsManager::instance();
    int index = settings.getValue("Console/DockShortcutKey", 0).toInt();
    if (index >= 0 && index < this->ui->DockKeyComboBox->count())
        this->ui->DockKeyComboBox->setCurrentIndex(index);
}

void ConsolesOptionsPage::selectKeyCombo()
{
    // Matches C# selectKeyCombo
    SettingsManager& settings = SettingsManager::instance();
    int index = settings.getValue("Console/FullScreenShortcutKey", 0).toInt();
    if (index >= 0 && index < this->ui->KeyComboListBox->count())
        this->ui->KeyComboListBox->setCurrentIndex(index);
}

void ConsolesOptionsPage::buildUncaptureKeyCodeComboBox()
{
    // Matches C# buildUncaptureKeyCodeComboBox
    this->ui->UncaptureKeyComboBox->clear();
    this->ui->UncaptureKeyComboBox->addItem(tr("Right Ctrl"));
    this->ui->UncaptureKeyComboBox->addItem(tr("Left Alt"));
    this->selectUncaptureKeyCombo();
}

void ConsolesOptionsPage::selectUncaptureKeyCombo()
{
    // Matches C# selectUncaptureKeyCombo
    SettingsManager& settings = SettingsManager::instance();
    int index = settings.getValue("Console/UncaptureShortcutKey", 0).toInt();
    if (index >= 0 && index < this->ui->UncaptureKeyComboBox->count())
        this->ui->UncaptureKeyComboBox->setCurrentIndex(index);
}

bool ConsolesOptionsPage::IsValidToSave(QWidget** control, QString& invalidReason)
{
    // Matches C# ConsolesOptionsPage.IsValidToSave()
    // No validation needed
    Q_UNUSED(control);
    Q_UNUSED(invalidReason);
    return true;
}

void ConsolesOptionsPage::ShowValidationMessages(QWidget* control, const QString& message)
{
    // Matches C# ConsolesOptionsPage.ShowValidationMessages()
    Q_UNUSED(control);
    Q_UNUSED(message);
}

void ConsolesOptionsPage::HideValidationMessages()
{
    // Matches C# ConsolesOptionsPage.HideValidationMessages()
}

void ConsolesOptionsPage::Save()
{
    // Matches C# ConsolesOptionsPage.Save()
    SettingsManager& settings = SettingsManager::instance();

    // Keyboard shortcuts
    settings.setValue("Console/FullScreenShortcutKey", this->ui->KeyComboListBox->currentIndex());
    settings.setValue("Console/DockShortcutKey", this->ui->DockKeyComboBox->currentIndex());
    settings.setValue("Console/UncaptureShortcutKey", this->ui->UncaptureKeyComboBox->currentIndex());

    // Windows Remote Desktop
    settings.setValue("Console/WindowsShortcuts", this->ui->WindowsKeyCheckBox->isChecked());
    settings.setValue("Console/ReceiveSoundFromRDP", this->ui->SoundCheckBox->isChecked());
    settings.setValue("Console/AutoSwitchToRDP", this->ui->AutoSwitchCheckBox->isChecked());
    settings.setValue("Console/ClipboardAndPrinterRedirection", this->ui->ClipboardCheckBox->isChecked());
    settings.setValue("Console/ConnectToServerConsole", this->ui->ConnectToServerConsoleCheckBox->isChecked());

    // Console scaling
    settings.setValue("Console/PreserveScaleWhenUndocked", this->ui->PreserveUndockedScaleCheckBox->isChecked());
    settings.setValue("Console/PreserveScaleWhenSwitchBackToVNC", this->ui->PreserveVNCConsoleScalingCheckBox->isChecked());
}
