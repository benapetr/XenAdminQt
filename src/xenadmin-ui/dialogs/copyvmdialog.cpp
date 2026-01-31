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

// copyvmdialog.cpp - Dialog for copying VMs and templates
#include "copyvmdialog.h"
#include "ui_copyvmdialog.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/vdi.h"
#include "xenlib/xen/vbd.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xencache.h"
#include "xenlib/vmhelpers.h"
#include "xenlib/xen/actions/vm/vmcloneaction.h"
#include "xenlib/xen/actions/vm/vmcopyaction.h"
#include "controls/srpicker.h"
#include <QPushButton>
#include <QMessageBox>

CopyVMDialog::CopyVMDialog(QSharedPointer<VM> vm, QWidget* parent)
    : QDialog(parent),
      ui(new Ui::CopyVMDialog),
      m_vm(vm),
      m_connection(nullptr)
{
    this->ui->setupUi(this);

    if (this->m_vm)
    {
        this->m_connection = this->m_vm->GetConnection();
    }

    // Connect signals
    connect(this->ui->nameTextBox, &QLineEdit::textChanged, this, &CopyVMDialog::onNameTextChanged);
    connect(this->ui->descriptionTextBox, &QTextEdit::textChanged, this, &CopyVMDialog::onDescriptionTextChanged);
    connect(this->ui->cloneRadioButton, &QRadioButton::toggled, this, &CopyVMDialog::onCloneRadioToggled);
    connect(this->ui->copyRadioButton, &QRadioButton::toggled, this, &CopyVMDialog::onCopyRadioToggled);
    connect(this->ui->buttonRescan, &QPushButton::clicked, this, &CopyVMDialog::onRescanClicked);
    
    // SrPicker signals
    connect(this->ui->srPicker, &SrPicker::selectedIndexChanged, this, &CopyVMDialog::onSrPickerSelectionChanged);
    connect(this->ui->srPicker, &SrPicker::canBeScannedChanged, this, &CopyVMDialog::onSrPickerCanBeScannedChanged);

    // Button box
    QPushButton* okButton = this->ui->buttonBox->button(QDialogButtonBox::Ok);
    if (okButton)
    {
        okButton->setText(tr("C&opy"));
        okButton->setEnabled(false);
    }
}

CopyVMDialog::~CopyVMDialog()
{
    delete this->ui;
}

QString CopyVMDialog::getName() const
{
    return this->ui->nameTextBox->text().trimmed();
}

QString CopyVMDialog::getDescription() const
{
    return this->ui->descriptionTextBox->toPlainText();
}

bool CopyVMDialog::isFastClone() const
{
    return this->ui->cloneRadioButton->isChecked();
}

QString CopyVMDialog::getSelectedSR() const
{
    return this->ui->srPicker->GetSelectedSR();
}

void CopyVMDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    
    if (!this->m_vm)
        return;

    this->initialize();
}

void CopyVMDialog::initialize()
{
    if (!this->m_vm)
        return;

    bool isTemplate = this->m_vm->IsTemplate();

    // Set window title and hint text
    if (isTemplate)
    {
        this->setWindowTitle(tr("Copy Template"));
        this->ui->labelSrHint->setText(tr("Select a Storage Repository for the template disks:"));
    } else
    {
        this->setWindowTitle(tr("Copy VM"));
        this->ui->labelSrHint->setText(tr("Select a Storage Repository for the copied disks:"));
    }

    // Set default name
    this->ui->nameTextBox->setText(this->getDefaultCopyName(this->m_vm));

    // Set description if available
    // Set description if VM has one
    QString vmDescription = this->m_vm->GetDescription();
    if (!vmDescription.isEmpty())
    {
        this->ui->descriptionTextBox->setPlainText(vmDescription);
    }

    // Determine if copy and fast clone are allowed
    bool allowCopy = !isTemplate || this->m_vm->GetAllowedOperations().contains("copy");
    bool anyDiskFastCloneable = this->m_vm->AnyDiskFastClonable();
    bool hasAtLeastOneDisk = this->m_vm->HasAtLeastOneDisk();

    // Enable/disable copy radio button
    this->ui->copyRadioButton->setEnabled(allowCopy && hasAtLeastOneDisk);
    
    // Enable/disable fast clone panel
    bool fastCloneEnabled = !allowCopy || anyDiskFastCloneable || !hasAtLeastOneDisk;
    this->ui->fastClonePanel->setEnabled(fastCloneEnabled);

    // Set initial radio button state
    if (!fastCloneEnabled)
    {
        this->ui->cloneRadioButton->setChecked(false);
    }

    if (!this->ui->copyRadioButton->isEnabled())
    {
        this->ui->cloneRadioButton->setChecked(true);
    }

    // Enable/disable SR picker panel based on copy radio button state
    this->ui->srPickerPanel->setEnabled(this->ui->copyRadioButton->isEnabled() && 
                                        this->ui->copyRadioButton->isChecked());

    // Set tooltip for fast clone panel if disabled
    if (!fastCloneEnabled)
    {
        this->ui->fastClonePanel->setToolTip(tr("Fast clone is not available for this VM"));
    }

    // Update fast clone description text
    if (isTemplate && !(anyDiskFastCloneable || allowCopy))
    {
        this->ui->cloneRadioButton->setText(tr("Clone template (slow)"));
        this->ui->fastCloneDescription->setText(tr("Creates a new template as a slow clone of the template."));
    } else
    {
        if (!isTemplate)
        {
            this->ui->fastCloneDescription->setText(tr("Creates a new VM as a fast clone of the VM. Fast cloning is almost instantaneous and takes up minimal extra disk space."));
        } else
        {
            this->ui->fastCloneDescription->setText(tr("Creates a new template as a fast clone of the template. Fast cloning is almost instantaneous and takes up minimal extra disk space."));
        }
    }

    // Populate SR picker
    if (this->m_connection)
    {
        // Get all VDIs attached to this VM
        QStringList vdiRefs;
        QList<QSharedPointer<VBD>> vbds = this->m_vm->GetVBDs();
        
        for (const QSharedPointer<VBD>& vbd : vbds)
        {
            if (vbd && vbd->GetType() != "CD")
            {
                QString vdiRef = vbd->GetVDIRef();
                if (!vdiRef.isEmpty())
                {
                    vdiRefs.append(vdiRef);
                }
            }
        }

        this->ui->srPicker->Populate(SrPicker::SRPickerType::Copy, 
                                     this->m_connection, 
                                     VMHelpers::getVMHome(this->m_connection, this->m_vm->GetData()), 
                                     QString(), 
                                     vdiRefs);
    }

    this->enableMoveButton();
}

void CopyVMDialog::enableMoveButton()
{
    QPushButton* okButton = this->ui->buttonBox->button(QDialogButtonBox::Ok);
    if (!okButton)
        return;

    bool enabled = !this->ui->nameTextBox->text().trimmed().isEmpty();
    
    // If copy mode is selected, also check if SR is selected
    if (this->ui->copyRadioButton->isChecked() && this->ui->srPickerPanel->isEnabled())
    {
        enabled = enabled && !this->ui->srPicker->GetSelectedSR().isEmpty();
    }

    okButton->setEnabled(enabled);
}

void CopyVMDialog::enableRescanButton()
{
    this->ui->buttonRescan->setEnabled(this->ui->srPickerPanel->isEnabled() && this->ui->srPicker->CanBeScanned());
}

QString CopyVMDialog::getDefaultCopyName(QSharedPointer<VM> vmToCopy)
{
    if (!vmToCopy || !this->m_connection)
        return QString();

    QStringList takenNames;
    QList<QSharedPointer<VM>> vms = this->m_connection->GetCache()->GetAll<VM>(XenObjectType::VM);
    foreach (const QSharedPointer<VM>& vm, vms)
    {
        takenNames.append(vm->GetName());
    }

    QString baseName = tr("Copy of %1").arg(vmToCopy->GetName());
    QString uniqueName = baseName;
    int counter = 1;

    while (takenNames.contains(uniqueName))
    {
        uniqueName = QString("%1 (%2)").arg(baseName).arg(counter);
        counter++;
    }

    return uniqueName;
}

void CopyVMDialog::onNameTextChanged()
{
    this->enableMoveButton();
}

void CopyVMDialog::onDescriptionTextChanged()
{
    this->enableMoveButton();
}

void CopyVMDialog::onCloneRadioToggled(bool checked)
{
    if (checked)
    {
        // Since the radio buttons aren't in the same group, do manual mutual exclusion
        this->ui->copyRadioButton->setChecked(false);
    }
}

void CopyVMDialog::onCopyRadioToggled(bool checked)
{
    this->ui->srPickerPanel->setEnabled(checked);
    this->enableRescanButton();
    this->enableMoveButton();
    
    // Since the radio buttons aren't in the same group, do manual mutual exclusion
    if (checked)
    {
        this->ui->cloneRadioButton->setChecked(false);
    }
}

void CopyVMDialog::onRescanClicked()
{
    this->ui->srPicker->ScanSRs();
}

void CopyVMDialog::onSrPickerSelectionChanged()
{
    this->enableMoveButton();
}

void CopyVMDialog::onSrPickerCanBeScannedChanged()
{
    this->enableRescanButton();
    this->enableMoveButton();
}

void CopyVMDialog::accept()
{
    if (!this->m_vm || !this->m_connection)
    {
        QDialog::reject();
        return;
    }

    QString name = this->getName();
    QString description = this->getDescription();

    // If fast clone is selected or SR picker is disabled
    if (this->ui->cloneRadioButton->isChecked() || !this->ui->srPickerPanel->isEnabled())
    {
        // Perform fast clone
        VMCloneAction* action = new VMCloneAction(this->m_vm, name, description, this);
        connect(action, &AsyncOperation::completed, this, &QDialog::accept);
        action->RunAsync(true);
        return;
    }

    // Otherwise, perform full copy
    QString srRef = this->getSelectedSR();
    if (srRef.isEmpty())
    {
        QMessageBox::warning(this, tr("No Storage Repository Selected"), tr("Please select a Storage Repository for the copied disks."));
        return;
    }

    // Get storage host and SR objects
    QString hostRef = VMHelpers::getVMStorageHost(this->m_connection, this->m_vm->GetData(), false);
    QSharedPointer<Host> host;
    if (!hostRef.isEmpty())
    {
        host = this->m_connection->GetCache()->ResolveObject<Host>(XenObjectType::Host, hostRef);
    }
    
    QSharedPointer<SR> sr = this->m_connection->GetCache()->ResolveObject<SR>(srRef);
    
    VMCopyAction* action = new VMCopyAction(this->m_vm, host, sr, name, description, this);
    connect(action, &AsyncOperation::completed, this, &QDialog::accept);
    action->RunAsync(true);
}
