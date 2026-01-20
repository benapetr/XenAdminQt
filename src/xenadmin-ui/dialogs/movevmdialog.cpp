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

// movevmdialog.cpp - Dialog for moving VM virtual disks to different SR
#include "movevmdialog.h"
#include "ui_movevmdialog.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/vbd.h"
#include "xenlib/xen/vdi.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xencache.h"
#include "xenlib/vmhelpers.h"
#include "xenlib/xen/actions/vm/vmmoveaction.h"
#include "controls/srpicker.h"
#include <QPushButton>
#include <QMessageBox>

MoveVMDialog::MoveVMDialog(QSharedPointer<VM> vm, QWidget* parent)
    : QDialog(parent),
      ui(new Ui::MoveVMDialog),
      m_vm(vm),
      m_connection(nullptr)
{
    this->ui->setupUi(this);

    if (this->m_vm)
    {
        this->m_connection = this->m_vm->GetConnection();
    }

    // Connect signals
    connect(this->ui->srPicker, &SrPicker::selectedIndexChanged, this, &MoveVMDialog::onSrPickerSelectionChanged);
    connect(this->ui->srPicker, &SrPicker::doubleClickOnRow, this, &MoveVMDialog::onSrPickerDoubleClicked);
    connect(this->ui->srPicker, &SrPicker::canBeScannedChanged, this, &MoveVMDialog::onSrPickerCanBeScannedChanged);
    connect(this->ui->buttonRescan, &QPushButton::clicked, this, &MoveVMDialog::onRescanClicked);

    // Button box
    QPushButton* okButton = this->ui->buttonBox->button(QDialogButtonBox::Ok);
    if (okButton)
    {
        okButton->setText(tr("&Move"));
        okButton->setEnabled(false);
    }
}

MoveVMDialog::~MoveVMDialog()
{
    delete this->ui;
}

QString MoveVMDialog::getSelectedSR() const
{
    return this->ui->srPicker->GetSelectedSR();
}

void MoveVMDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    this->initialize();
}

void MoveVMDialog::initialize()
{
    if (!this->m_vm || !this->m_connection)
        return;

    // Get all VDIs that need to be moved (non-CD VDIs where this VM is the owner)
    QList<QSharedPointer<VDI>> vdis;
    QList<QSharedPointer<VBD>> vbds = this->m_vm->GetVBDs();
    
    for (const QSharedPointer<VBD>& vbd : vbds)
    {
        if (vbd && vbd->IsOwner() && vbd->GetType() != "CD")
        {
            QSharedPointer<VDI> vdi = vbd->GetVDI();
            if (vdi)
            {
                vdis.append(vdi);
            }
        }
    }

    // Populate SR picker with Move mode
    this->ui->srPicker->Populate(SrPicker::SRPickerType::Move,
                                 this->m_connection,
                                 VMHelpers::getVMHome(this->m_connection, this->m_vm->GetData()),
                                 QString(),
                                 QStringList()); // VDI refs not needed for Move mode

    this->enableMoveButton();
}

void MoveVMDialog::enableMoveButton()
{
    QPushButton* okButton = this->ui->buttonBox->button(QDialogButtonBox::Ok);
    if (!okButton)
        return;

    // Enable move button only if SR is selected
    okButton->setEnabled(!this->ui->srPicker->GetSelectedSR().isEmpty());
}

void MoveVMDialog::onSrPickerSelectionChanged()
{
    this->enableMoveButton();
}

void MoveVMDialog::onSrPickerDoubleClicked()
{
    QPushButton* okButton = this->ui->buttonBox->button(QDialogButtonBox::Ok);
    if (okButton && okButton->isEnabled())
    {
        okButton->click();
    }
}

void MoveVMDialog::onSrPickerCanBeScannedChanged()
{
    this->ui->buttonRescan->setEnabled(this->ui->srPicker->CanBeScanned());
    this->enableMoveButton();
}

void MoveVMDialog::onRescanClicked()
{
    this->ui->srPicker->ScanSRs();
}

void MoveVMDialog::accept()
{
    if (!this->m_vm || !this->m_connection)
    {
        QDialog::reject();
        return;
    }

    QString srRef = this->getSelectedSR();
    if (srRef.isEmpty())
    {
        QMessageBox::warning(this, tr("No Storage Repository Selected"),
                           tr("Please select a Storage Repository to move the VM's disks to."));
        return;
    }

    // Get storage host and SR objects
    QString hostRef = VMHelpers::getVMStorageHost(this->m_connection, this->m_vm->GetData(), false);
    QSharedPointer<Host> host;
    if (!hostRef.isEmpty())
    {
        host = this->m_connection->GetCache()->ResolveObject<Host>("host", hostRef);
    }

    QSharedPointer<SR> sr = this->m_connection->GetCache()->ResolveObject<SR>("sr", srRef);

    // Create and run the move action
    VMMoveAction* action = new VMMoveAction(this->m_vm, sr, host, this);
    connect(action, &AsyncOperation::completed, this, &QDialog::accept);
    action->RunAsync(true);
}
