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

#include "customsearchdialog.h"
#include "ui_customsearchdialog.h"

using namespace XenSearch;

CustomSearchDialog::CustomSearchDialog(QWidget* parent) : QDialog(parent), ui(new Ui::CustomSearchDialog)
{
    this->ui->setupUi(this);
    
    // Connect buttons
    connect(this->ui->selectAllButton, &QPushButton::clicked, this, &CustomSearchDialog::onSelectAllClicked);
    connect(this->ui->clearAllButton, &QPushButton::clicked, this, &CustomSearchDialog::onClearAllClicked);
    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(this->ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

CustomSearchDialog::~CustomSearchDialog()
{
    delete this->ui;
}

QueryScope* CustomSearchDialog::getQueryScope() const
{
    ObjectTypes types = ObjectTypes::None;
    
    if (this->ui->poolCheckBox->isChecked())
        types = types | ObjectTypes::Pool;
    if (this->ui->serverCheckBox->isChecked())
        types = types | ObjectTypes::Server;
    if (this->ui->vmCheckBox->isChecked())
        types = types | ObjectTypes::VM;
    if (this->ui->localSRCheckBox->isChecked())
        types = types | ObjectTypes::LocalSR;
    if (this->ui->remoteSRCheckBox->isChecked())
        types = types | ObjectTypes::RemoteSR;
    if (this->ui->vdiCheckBox->isChecked())
        types = types | ObjectTypes::VDI;
    if (this->ui->networkCheckBox->isChecked())
        types = types | ObjectTypes::Network;
    if (this->ui->folderCheckBox->isChecked())
        types = types | ObjectTypes::Folder;
    
    return new QueryScope(types);
}

void CustomSearchDialog::setObjectTypes(ObjectTypes types)
{
    this->ui->poolCheckBox->setChecked((types & ObjectTypes::Pool) != ObjectTypes::None);
    this->ui->serverCheckBox->setChecked((types & ObjectTypes::Server) != ObjectTypes::None);
    this->ui->vmCheckBox->setChecked((types & ObjectTypes::VM) != ObjectTypes::None);
    this->ui->localSRCheckBox->setChecked((types & ObjectTypes::LocalSR) != ObjectTypes::None);
    this->ui->remoteSRCheckBox->setChecked((types & ObjectTypes::RemoteSR) != ObjectTypes::None);
    this->ui->vdiCheckBox->setChecked((types & ObjectTypes::VDI) != ObjectTypes::None);
    this->ui->networkCheckBox->setChecked((types & ObjectTypes::Network) != ObjectTypes::None);
    this->ui->folderCheckBox->setChecked((types & ObjectTypes::Folder) != ObjectTypes::None);
}

void CustomSearchDialog::onSelectAllClicked()
{
    this->ui->poolCheckBox->setChecked(true);
    this->ui->serverCheckBox->setChecked(true);
    this->ui->vmCheckBox->setChecked(true);
    this->ui->localSRCheckBox->setChecked(true);
    this->ui->remoteSRCheckBox->setChecked(true);
    this->ui->vdiCheckBox->setChecked(true);
    this->ui->networkCheckBox->setChecked(true);
    this->ui->folderCheckBox->setChecked(true);
}

void CustomSearchDialog::onClearAllClicked()
{
    this->ui->poolCheckBox->setChecked(false);
    this->ui->serverCheckBox->setChecked(false);
    this->ui->vmCheckBox->setChecked(false);
    this->ui->localSRCheckBox->setChecked(false);
    this->ui->remoteSRCheckBox->setChecked(false);
    this->ui->vdiCheckBox->setChecked(false);
    this->ui->networkCheckBox->setChecked(false);
    this->ui->folderCheckBox->setChecked(false);
}
