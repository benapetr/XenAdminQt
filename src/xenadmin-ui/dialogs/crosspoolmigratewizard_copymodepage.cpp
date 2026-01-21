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

#include "crosspoolmigratewizard_copymodepage.h"
#include "ui_crosspoolmigratewizard_copymodepage.h"
#include "crosspoolmigratewizard.h"

CrossPoolMigrateCopyModePage::CrossPoolMigrateCopyModePage(const QList<QString>& selectedVMs, QWidget *parent)
    : QWizardPage(parent)
    , ui(new Ui::CrossPoolMigrateCopyModePage)
    , vmsFromSelection_(selectedVMs)
    , crossPoolRestricted_(false)
{
    ui->setupUi(this);
    
    this->setTitle(tr("Copy Mode"));
    this->setSubTitle(tr("Choose where to copy the selected virtual machine(s)."));
    
    this->setupConnections();
}

CrossPoolMigrateCopyModePage::~CrossPoolMigrateCopyModePage()
{
    delete ui;
}

void CrossPoolMigrateCopyModePage::setupConnections()
{
    connect(ui->intraPoolRadioButton, &QRadioButton::toggled, 
            this, &CrossPoolMigrateCopyModePage::onRadioButtonToggled);
    connect(ui->crossPoolRadioButton, &QRadioButton::toggled,
            this, &CrossPoolMigrateCopyModePage::onRadioButtonToggled);
}

void CrossPoolMigrateCopyModePage::initializePage()
{
    // Check if cross-pool migration is restricted (matches C# Helpers.FeatureForbidden)
    // For now, assume not restricted - will implement license check later
    this->crossPoolRestricted_ = false;
    
    if (this->crossPoolRestricted_)
    {
        ui->crossPoolRadioButton->setEnabled(false);
        ui->intraPoolRadioButton->setChecked(true);
    }
    
    this->updateWarningVisibility();
}

bool CrossPoolMigrateCopyModePage::validatePage()
{
    // Always valid - user just chooses between two options
    return true;
}

int CrossPoolMigrateCopyModePage::nextId() const
{
    auto* wizard = qobject_cast<CrossPoolMigrateWizard*>(this->wizard());
    if (!wizard)
        return -1;

    if (wizard->isIntraPoolCopySelected())
        return wizard->requiresRbacWarning()
            ? CrossPoolMigrateWizard::Page_RbacWarning
            : CrossPoolMigrateWizard::Page_IntraPoolCopy;

    return CrossPoolMigrateWizard::Page_Destination;
}

bool CrossPoolMigrateCopyModePage::IntraPoolCopySelected() const
{
    return ui->intraPoolRadioButton->isChecked();
}

void CrossPoolMigrateCopyModePage::onRadioButtonToggled()
{
    this->updateWarningVisibility();
    emit completeChanged();
}

void CrossPoolMigrateCopyModePage::updateWarningVisibility()
{
    // Warning is always shown in C# for copy operations
    ui->imgStopVM->setVisible(true);
    ui->labelWarning->setVisible(true);
}
