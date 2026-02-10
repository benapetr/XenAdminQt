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

// displayoptionspage.cpp - Display settings options page
#include "displayoptionspage.h"
#include "../../settingsmanager.h"

DisplayOptionsPage::DisplayOptionsPage(QWidget* parent)
    : IOptionsPage(parent), ui(new Ui::DisplayOptionsPage)
{
    this->ui->setupUi(this);
}

DisplayOptionsPage::~DisplayOptionsPage()
{
    delete this->ui;
}

QString DisplayOptionsPage::GetText() const
{
    // Matches C# Messages.DISPLAY
    return tr("Display");
}

QString DisplayOptionsPage::GetSubText() const
{
    // Matches C# Messages.DISPLAY_DETAILS
    return tr("Configure display options");
}

QIcon DisplayOptionsPage::GetImage() const
{
    // Matches C# Images.StaticImages._001_PerformanceGraph_h32bit_16
    return QIcon(":/icons/performance_graph_16.png");
}

void DisplayOptionsPage::Build()
{
    // Matches C# DisplayOptionsPage.Build()
    SettingsManager& settings = SettingsManager::instance();

    // Graph display type
    const bool fillAreas = settings.GetFillAreaUnderGraphs();
    this->ui->GraphAreasRadioButton->setChecked(fillAreas);
    this->ui->GraphLinesRadioButton->setChecked(!fillAreas);

    // Remember last selected tab
    this->ui->checkBoxStoreTab->setChecked(settings.GetRememberLastSelectedTab());

    // Show timestamps in updates log
    this->ui->showTimestampsCheckBox->setChecked(
        settings.GetValue("Display/ShowTimestampsInUpdatesLog", true).toBool());
}

bool DisplayOptionsPage::IsValidToSave(QWidget** control, QString& invalidReason)
{
    // Matches C# DisplayOptionsPage.IsValidToSave()
    // No validation needed
    Q_UNUSED(control);
    Q_UNUSED(invalidReason);
    return true;
}

void DisplayOptionsPage::ShowValidationMessages(QWidget* control, const QString& message)
{
    // Matches C# DisplayOptionsPage.ShowValidationMessages()
    Q_UNUSED(control);
    Q_UNUSED(message);
}

void DisplayOptionsPage::HideValidationMessages()
{
    // Matches C# DisplayOptionsPage.HideValidationMessages()
}

void DisplayOptionsPage::Save()
{
    // Matches C# DisplayOptionsPage.Save()
    SettingsManager& settings = SettingsManager::instance();

    // Graph display type
    settings.SetFillAreaUnderGraphs(this->ui->GraphAreasRadioButton->isChecked());

    // Remember last selected tab
    settings.SetRememberLastSelectedTab(this->ui->checkBoxStoreTab->isChecked());

    // Show timestamps in updates log
    settings.SetValue("Display/ShowTimestampsInUpdatesLog", this->ui->showTimestampsCheckBox->isChecked());
}
