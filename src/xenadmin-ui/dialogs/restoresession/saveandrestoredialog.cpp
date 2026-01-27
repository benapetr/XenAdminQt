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

#include "saveandrestoredialog.h"
#include "ui_saveandrestoredialog.h"
#include "../optionspages/saveandrestoreoptionspage.h"
#include "../../settingsmanager.h"
#include <QtWidgets/QVBoxLayout>

SaveAndRestoreDialog::SaveAndRestoreDialog(QWidget* parent)
    : QDialog(parent), ui(new Ui::SaveAndRestoreDialog)
{
    this->ui->setupUi(this);

    // Create and embed the SaveAndRestoreOptionsPage
    this->saveAndRestoreOptionsPage_ = new SaveAndRestoreOptionsPage(this);
    
    // Replace the placeholder widget with the options page
    QVBoxLayout* layout = new QVBoxLayout(this->ui->saveAndRestoreOptionsPage);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(this->saveAndRestoreOptionsPage_);

    // Matches C# SaveAndRestoreDialog: do not call SaveServerlist on OK
    this->saveAndRestoreOptionsPage_->SetSaveAllAfter(false);

    // Build the options page with current settings
    this->saveAndRestoreOptionsPage_->Build();

    // Connect signals
    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &SaveAndRestoreDialog::okButton_Click);
    connect(this->ui->buttonBox, &QDialogButtonBox::rejected, this, &SaveAndRestoreDialog::cancelButton_Click);
}

SaveAndRestoreDialog::~SaveAndRestoreDialog()
{
    delete this->ui;
}

void SaveAndRestoreDialog::okButton_Click()
{
    // Matches C# SaveAndRestoreDialog.okButton_Click()
    this->SaveEverything();
    this->accept();
}

void SaveAndRestoreDialog::cancelButton_Click()
{
    // Matches C# SaveAndRestoreDialog.cancelButton_Click()
    this->reject();
}

void SaveAndRestoreDialog::SaveEverything()
{
    // Matches C# SaveAndRestoreDialog.SaveEverything()
    // All prompts for old password must have been made
    this->saveAndRestoreOptionsPage_->Save();
    SettingsManager::instance().Sync();
}
