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

// optionsdialog.cpp - Options dialog with vertical tabs
#include "optionsdialog.h"
#include "optionspages/securityoptionspage.h"
#include "optionspages/displayoptionspage.h"
#include "optionspages/confirmationoptionspage.h"
#include "optionspages/consolesoptionspage.h"
#include "optionspages/saveandrestoreoptionspage.h"
#include "optionspages/connectionoptionspage.h"
#include "../settingsmanager.h"
#include <QMessageBox>
#include <QTimer>

OptionsDialog::OptionsDialog(QWidget* parent)
    : QDialog(parent), ui(new Ui::OptionsDialog)
{
    ui->setupUi(this);

    // Create options pages (matches C# InitializeComponent order)
    SecurityOptionsPage* securityPage = new SecurityOptionsPage(this);
    ConnectionOptionsPage* connectionPage = new ConnectionOptionsPage(this);
    DisplayOptionsPage* displayPage = new DisplayOptionsPage(this);
    ConsolesOptionsPage* consolesPage = new ConsolesOptionsPage(this);
    SaveAndRestoreOptionsPage* saveRestorePage = new SaveAndRestoreOptionsPage(this);
    ConfirmationOptionsPage* confirmationPage = new ConfirmationOptionsPage(this);

    // Add pages to list (matches C# order)
    m_pages.append(securityPage);
    m_pages.append(connectionPage);
    m_pages.append(displayPage);
    m_pages.append(consolesPage);
    m_pages.append(saveRestorePage);
    m_pages.append(confirmationPage);

    // Add pages to vertical tabs and stacked widget
    for (IOptionsPage* page : m_pages)
    {
        ui->verticalTabs->addTab(page->image(), page->text(), page->subText(), page);
        ui->ContentPanel->addWidget(page);
    }

    // Connect vertical tabs selection changed
    connect(ui->verticalTabs, &QListWidget::currentRowChanged,
            this, &OptionsDialog::onVerticalTabsCurrentChanged);

    // Select first page (Security - matches C# default)
    if (!m_pages.isEmpty())
    {
        ui->verticalTabs->setCurrentRow(0);
        ui->ContentPanel->setCurrentWidget(m_pages.first());
        ui->TabImage->setPixmap(m_pages.first()->image().pixmap(32, 32));
        ui->TabTitle->setText(m_pages.first()->text());
        m_pages.first()->show(); // Explicitly show the first page
    }

    // Force update of vertical tabs to trigger paint
    ui->verticalTabs->update();

    // Build pages after showing (matches C# OnLoad)
    QTimer::singleShot(0, this, &OptionsDialog::buildPages);
}

OptionsDialog::~OptionsDialog()
{
    delete ui;
}

void OptionsDialog::buildPages()
{
    // Matches C# OptionsDialog.OnLoad()
    for (IOptionsPage* page : m_pages)
    {
        page->build();
    }
}

void OptionsDialog::selectSecurityPage()
{
    // Matches C# SelectConnectionOptionsPage() pattern
    for (int i = 0; i < m_pages.size(); ++i)
    {
        if (qobject_cast<SecurityOptionsPage*>(m_pages[i]))
        {
            selectPage(m_pages[i]);
            break;
        }
    }
}

void OptionsDialog::selectPage(IOptionsPage* page)
{
    // Matches C# SelectPage(page)
    if (!page || !m_pages.contains(page))
    {
        return;
    }

    int index = m_pages.indexOf(page);
    ui->verticalTabs->setCurrentRow(index);
}

void OptionsDialog::accept()
{
    // Matches C# okButton_Click()

    // Validate all pages
    for (IOptionsPage* page : m_pages)
    {
        QWidget* control = nullptr;
        QString invalidReason;

        if (!page->isValidToSave(&control, invalidReason))
        {
            selectPage(page);
            page->showValidationMessages(control, invalidReason);
            return; // Don't close dialog
        }
    }

    // Save all pages
    for (IOptionsPage* page : m_pages)
    {
        page->save();
    }

    // Save settings (matches C# Settings.TrySaveSettings())
    SettingsManager::instance().sync();

    // Close dialog
    QDialog::accept();
}

void OptionsDialog::reject()
{
    // Matches C# cancelButton_Click()
    QDialog::reject();
}

void OptionsDialog::onVerticalTabsCurrentChanged(int index)
{
    // Matches C# verticalTabs_SelectedIndexChanged
    if (index < 0 || index >= m_pages.size())
    {
        return;
    }

    IOptionsPage* page = m_pages[index];

    // Update header
    ui->TabImage->setPixmap(page->image().pixmap(32, 32));
    ui->TabTitle->setText(page->text());

    // Show selected page
    ui->ContentPanel->setCurrentWidget(page);

    // Hide validation messages
    hideValidationToolTips();
}

void OptionsDialog::hideValidationToolTips()
{
    // Matches C# HideValidationToolTips()
    for (IOptionsPage* page : m_pages)
    {
        page->hideValidationMessages();
    }
}
