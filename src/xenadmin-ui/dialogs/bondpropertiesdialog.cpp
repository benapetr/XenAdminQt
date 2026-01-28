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

#include "bondpropertiesdialog.h"
#include "ui_bondpropertiesdialog.h"
#include "controls/bonddetailswidget.h"
#include "xen/host.h"
#include "xen/network.h"
#include <QPushButton>

BondPropertiesDialog::BondPropertiesDialog(QSharedPointer<Host> host, QSharedPointer<Network> network, QWidget* parent)
    : QDialog(parent), ui(new Ui::BondPropertiesDialog), m_host(host), m_network(network)
{
    this->ui->setupUi(this);

    if (this->ui->bondDetailsWidget)
    {
        if (this->m_host)
            this->ui->bondDetailsWidget->SetHost(this->m_host);
        connect(this->ui->bondDetailsWidget, &BondDetailsWidget::ValidChanged, this, [this](bool) {
            this->updateOkButtonState();
        });
    }

    this->updateOkButtonState();
}

BondPropertiesDialog::~BondPropertiesDialog()
{
    delete this->ui;
}

QString BondPropertiesDialog::getBondName() const
{
    return this->ui->bondDetailsWidget ? this->ui->bondDetailsWidget->BondName() : QString();
}

QString BondPropertiesDialog::getBondMode() const
{
    return this->ui->bondDetailsWidget ? this->ui->bondDetailsWidget->BondMode() : QString();
}

QString BondPropertiesDialog::getHashingAlgorithm() const
{
    return this->ui->bondDetailsWidget ? this->ui->bondDetailsWidget->HashingAlgorithm() : QString();
}

qint64 BondPropertiesDialog::getMTU() const
{
    return this->ui->bondDetailsWidget ? this->ui->bondDetailsWidget->MTU() : 0;
}

bool BondPropertiesDialog::getAutoPlug() const
{
    return this->ui->bondDetailsWidget ? this->ui->bondDetailsWidget->AutoPlug() : true;
}

QStringList BondPropertiesDialog::getSelectedPIFRefs() const
{
    return this->ui->bondDetailsWidget ? this->ui->bondDetailsWidget->SelectedPifRefs() : QStringList();
}

bool BondPropertiesDialog::canCreateBond(QWidget* parent)
{
    return this->ui->bondDetailsWidget ? this->ui->bondDetailsWidget->CanCreateBond(parent) : false;
}

void BondPropertiesDialog::updateOkButtonState()
{
    const bool valid = this->ui->bondDetailsWidget && this->ui->bondDetailsWidget->IsValid();
    this->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(valid);
}

void BondPropertiesDialog::accept()
{
    if (!this->canCreateBond(this))
        return;
    QDialog::accept();
}
