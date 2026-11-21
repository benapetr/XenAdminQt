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

#include "vmsnapshotdialog.h"
#include "ui_vmsnapshotdialog.h"
#include "xen/host.h"
#include "xen/vm.h"
#include <QDateTime>
#include <QPushButton>

VmSnapshotDialog::VmSnapshotDialog(QSharedPointer<VM> vm, QWidget* parent) : QDialog(parent), ui(new Ui::VmSnapshotDialog)
{
    this->m_vm = vm;

    this->ui->setupUi(this);
    this->setupDialog();

    // Connect signals
    connect(this->ui->nameLineEdit, &QLineEdit::textChanged, this, &VmSnapshotDialog::onNameChanged);
    connect(this->ui->diskRadioButton, &QRadioButton::toggled, this, &VmSnapshotDialog::onDiskRadioToggled);
    connect(this->ui->memoryRadioButton, &QRadioButton::toggled, this, &VmSnapshotDialog::onMemoryRadioToggled);

    // Set focus to name field
    this->ui->nameLineEdit->setFocus();
}

VmSnapshotDialog::~VmSnapshotDialog()
{
    delete this->ui;
}

bool VmSnapshotDialog::makeExportable() const
{
    return this->ui->makeExportableCheckBox->isChecked();
}

QString VmSnapshotDialog::snapshotName() const
{
    return this->ui->nameLineEdit->text().trimmed();
}

QString VmSnapshotDialog::snapshotDescription() const
{
    return this->ui->descriptionTextEdit->toPlainText().trimmed();
}

VmSnapshotDialog::SnapshotType VmSnapshotDialog::snapshotType() const
{
    if (this->ui->diskRadioButton->isChecked())
        return DISK;

    return DISK_AND_MEMORY;
}

void VmSnapshotDialog::setupDialog()
{
    // C# VmSnapshotDialog constructor. Its licence checks (RestrictVss, RestrictCheckpoint) are not ported.

    // Default snapshot name with a timestamp (a Qt addition)
    QString defaultName = QString("%1_%2")
                              .arg(this->m_vm->GetName(), QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss"));
    this->ui->nameLineEdit->setText(defaultName);

    this->updateWarnings();

    const QStringList allowedOps = this->m_vm->GetAllowedOperations();

    this->ui->diskRadioButton->setEnabled(allowedOps.contains("snapshot"));
    this->ui->diskSnapshotInfoLabel->setVisible(!this->ui->diskRadioButton->isEnabled());

    // C# also has a "Quiesce" checkbox, shown only before XenServer 7.6 (Quebec), which removed
    // quiesced (VSS) snapshots from xapi. It is deliberately not ported; see docs/status.md.

    this->ui->memoryRadioButton->setEnabled(allowedOps.contains("checkpoint"));
    this->ui->memorySnapshotInfoLabel->setVisible(!this->ui->memoryRadioButton->isEnabled());

    this->updateOkButton();
}

void VmSnapshotDialog::updateOkButton()
{
    // C# UpdateOK
    const bool hasName = !this->ui->nameLineEdit->text().trimmed().isEmpty();
    const bool hasValidType = this->ui->diskRadioButton->isEnabled() || this->ui->memoryRadioButton->isEnabled();

    if (QPushButton* okButton = this->ui->buttonBox->button(QDialogButtonBox::Ok))
        okButton->setEnabled(hasName && hasValidType);
}

void VmSnapshotDialog::updateWarnings()
{
    // C# UpdateWarnings, without its licence checks and its quiesce message
    const bool running = this->m_vm->GetPowerState() == "Running";
    const bool ioDriversInstalled = this->m_vm->GetVirtualizationStatus() & 4;  // VirtualizationStatus.IoDriversInstalled
    // C# HasNewVirtualizationStates: Windows VMs report the agent and the drivers separately
    const bool newStates = this->m_vm->IsWindows();
    const QString vmTools = tr("XCP-ng VM Tools");

    this->ui->diskSnapshotInfoLabel->setText(tr("Disk snapshots are not currently available for this VM"));

    if (!running)
        this->ui->memorySnapshotInfoLabel->setText(tr("Disk and memory snapshots need the VM to be running"));
    else if (!ioDriversInstalled)
        this->ui->memorySnapshotInfoLabel->setText(newStates ? tr("Disk and memory snapshots need I/O drivers to be installed")
                                                             : tr("Disk and memory snapshots need %1 to be installed").arg(vmTools));
    else
        this->ui->memorySnapshotInfoLabel->setText(tr("Disk and memory snapshots are not currently available for this VM"));
}

void VmSnapshotDialog::onNameChanged()
{
    this->updateOkButton();
}

void VmSnapshotDialog::onDiskRadioToggled(bool checked)
{
    if (checked)
    {
        // Disk mode selected, memory mode deselected
        this->ui->memoryRadioButton->setChecked(false);
    }
    this->updateOkButton();
}

void VmSnapshotDialog::onMemoryRadioToggled(bool checked)
{
    if (checked)
    {
        // Memory mode selected, disk mode deselected
        this->ui->diskRadioButton->setChecked(false);
    }
    this->updateOkButton();
}
