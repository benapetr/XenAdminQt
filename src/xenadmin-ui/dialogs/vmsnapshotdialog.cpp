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
#include <QDateTime>
#include <QPushButton>

VmSnapshotDialog::VmSnapshotDialog(const QVariantMap& vmData, QWidget* parent)
    : QDialog(parent), ui(new Ui::VmSnapshotDialog), m_vmData(vmData)
{
    ui->setupUi(this);
    setupDialog();

    // Connect signals
    connect(ui->nameLineEdit, &QLineEdit::textChanged, this, &VmSnapshotDialog::onNameChanged);
    connect(ui->diskRadioButton, &QRadioButton::toggled, this, &VmSnapshotDialog::onDiskRadioToggled);
    connect(ui->memoryRadioButton, &QRadioButton::toggled, this, &VmSnapshotDialog::onMemoryRadioToggled);
    connect(ui->quiesceCheckBox, &QCheckBox::toggled, this, &VmSnapshotDialog::onQuiesceCheckBoxToggled);

    // Set focus to name field
    ui->nameLineEdit->setFocus();
}

VmSnapshotDialog::~VmSnapshotDialog()
{
    delete ui;
}

QString VmSnapshotDialog::snapshotName() const
{
    return ui->nameLineEdit->text().trimmed();
}

QString VmSnapshotDialog::snapshotDescription() const
{
    return ui->descriptionTextEdit->toPlainText().trimmed();
}

VmSnapshotDialog::SnapshotType VmSnapshotDialog::snapshotType() const
{
    if (ui->quiesceCheckBox->isChecked())
        return QUIESCED_DISK;

    if (ui->diskRadioButton->isChecked())
        return DISK;

    return DISK_AND_MEMORY;
}

void VmSnapshotDialog::setupDialog()
{
    // C#: VmSnapshotDialog.cs lines 42-61

    // Set default snapshot name with timestamp
    QString defaultName = QString("Snapshot_%1")
                              .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss"));
    ui->nameLineEdit->setText(defaultName);

    // Update warning messages
    updateWarnings();

    // Check what snapshot operations are allowed
    QVariantList allowedOps = m_vmData.value("allowed_operations").toList();

    bool canSnapshot = allowedOps.contains("snapshot");
    bool canCheckpoint = allowedOps.contains("checkpoint");
    bool canQuiesce = allowedOps.contains("snapshot_with_quiesce");

    // C#: diskRadioButton.Enabled = _VM.allowed_operations.Contains(vm_operations.snapshot);
    ui->diskRadioButton->setEnabled(canSnapshot);

    // C#: pictureBoxSnapshotsInfo.Visible = labelSnapshotInfo.Visible = !diskRadioButton.Enabled;
    // Show info label when disk snapshot is NOT available
    ui->diskSnapshotInfoLabel->setVisible(!canSnapshot);

    // C#: var quiesceAvailable = !Helpers.QuebecOrGreater(_VM.Connection);
    // For now, always show quiesce option (TODO: check server version)
    bool quiesceVisible = true;

    // C#: quiesceCheckBox.Visible = quiesceAvailable;
    ui->quiesceCheckBox->setVisible(quiesceVisible);

    // C#: quiesceCheckBox.Enabled = quiesceAvailable && _VM.allowed_operations.Contains(vm_operations.snapshot_with_quiesce)
    //     && !Helpers.FeatureForbidden(_VM, Host.RestrictVss);
    // TODO: Check Host.RestrictVss restriction
    ui->quiesceCheckBox->setEnabled(quiesceVisible && canQuiesce);

    // C#: pictureBoxQuiesceInfo.Visible = labelQuiesceInfo.Visible = quiesceAvailable && !quiesceCheckBox.Enabled;
    // Show quiesce info when quiesce is visible but not enabled
    ui->quiesceInfoLabel->setVisible(quiesceVisible && !ui->quiesceCheckBox->isEnabled());

    // C#: memoryRadioButton.Enabled = _VM.allowed_operations.Contains(vm_operations.checkpoint)
    //     && !Helpers.FeatureForbidden(_VM, Host.RestrictCheckpoint);
    // TODO: Check Host.RestrictCheckpoint restriction
    ui->memoryRadioButton->setEnabled(canCheckpoint);

    // C#: CheckpointInfoPictureBox.Visible = labelCheckpointInfo.Visible = !memoryRadioButton.Enabled;
    // Show checkpoint info when memory snapshot is NOT available
    ui->memorySnapshotInfoLabel->setVisible(!canCheckpoint);

    // C#: UpdateOK();
    updateOkButton();
}

void VmSnapshotDialog::updateOkButton()
{
    bool hasName = !ui->nameLineEdit->text().trimmed().isEmpty();
    bool hasValidType = ui->diskRadioButton->isEnabled() ||
                        ui->memoryRadioButton->isEnabled();

    QPushButton* okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    if (okButton)
    {
        okButton->setEnabled(hasName && hasValidType);
    }
}

void VmSnapshotDialog::updateWarnings()
{
    // C#: VmSnapshotDialog.cs lines 135-156
    QString powerState = m_vmData.value("power_state").toString();
    bool isRunning = (powerState == "Running");

    // Get virtualization status
    // TODO: Implement proper GetVirtualizationStatus check
    bool hasManagementInstalled = true; // Assume true for now
    bool hasIoDriversInstalled = true;  // Assume true for now

    // C#: labelSnapshotInfo.Text = Messages.INFO_DISK_MODE;
    if (!ui->diskRadioButton->isEnabled())
    {
        ui->diskSnapshotInfoLabel->setText(tr("Disk-only snapshots are not supported for this VM."));
    } else
    {
        ui->diskSnapshotInfoLabel->clear();
    }

    // Quiesce warning messages (C#: lines 138-145)
    if (ui->quiesceInfoLabel->isVisible())
    {
        // C#: if (Helpers.FeatureForbidden(_VM, Host.RestrictVss))
        //     labelQuiesceInfo.Text = Messages.FIELD_DISABLED;
        // TODO: Check Host.RestrictVss
        bool vssRestricted = false; // TODO: Implement restriction check

        if (vssRestricted)
        {
            ui->quiesceInfoLabel->setText(tr("This feature is restricted."));
        }
        // C#: else if (_VM.power_state != vm_power_state.Running)
        //     labelQuiesceInfo.Text = Messages.INFO_QUIESCE_MODE_POWER_STATE;
        else if (!isRunning)
        {
            ui->quiesceInfoLabel->setText(tr("Quiesced snapshots require the VM to be running."));
        }
        // C#: else if (!_VM.GetVirtualizationStatus(out _).HasFlag(VM.VirtualizationStatus.ManagementInstalled))
        else if (!hasManagementInstalled)
        {
            ui->quiesceInfoLabel->setText(tr("Quiesced snapshots require XenServer VM Tools to be installed."));
        }
        // C#: else
        //     labelQuiesceInfo.Text = Messages.INFO_QUIESCE_MODE; // This says that VSS must be enabled
        else
        {
            ui->quiesceInfoLabel->setText(tr("Quiesced snapshots require VSS to be enabled in the VM."));
        }
    }

    // Checkpoint warning messages (C#: lines 147-156)
    if (ui->memorySnapshotInfoLabel->isVisible())
    {
        // C#: if (Helpers.FeatureForbidden(_VM, Host.RestrictCheckpoint))
        //     labelCheckpointInfo.Text = Messages.FIELD_DISABLED;
        // TODO: Check Host.RestrictCheckpoint
        bool checkpointRestricted = false; // TODO: Implement restriction check

        if (checkpointRestricted)
        {
            ui->memorySnapshotInfoLabel->setText(tr("This feature is restricted."));
        }
        // C#: else if (_VM.power_state != vm_power_state.Running)
        //     labelCheckpointInfo.Text = Messages.INFO_DISKMEMORY_MODE_POWER_STATE;
        else if (!isRunning)
        {
            ui->memorySnapshotInfoLabel->setText(tr("Memory snapshots (checkpoints) require the VM to be running."));
        }
        // C#: else if (!_VM.GetVirtualizationStatus(out _).HasFlag(VM.VirtualizationStatus.IoDriversInstalled))
        else if (!hasIoDriversInstalled)
        {
            ui->memorySnapshotInfoLabel->setText(tr("Memory snapshots require XenServer VM Tools with I/O drivers installed."));
        }
        // C#: else
        //     labelCheckpointInfo.Text = Messages.INFO_DISKMEMORY_MODE_MISC;
        else
        {
            ui->memorySnapshotInfoLabel->setText(tr("Memory snapshots capture the VM's current state including memory contents."));
        }
    }
}

void VmSnapshotDialog::onNameChanged()
{
    updateOkButton();
}

void VmSnapshotDialog::onDiskRadioToggled(bool checked)
{
    if (checked)
    {
        // Disk mode selected, memory mode deselected
        ui->memoryRadioButton->setChecked(false);
    }
    updateOkButton();
}

void VmSnapshotDialog::onMemoryRadioToggled(bool checked)
{
    if (checked)
    {
        // Memory mode selected, disk mode and quiesce deselected
        ui->diskRadioButton->setChecked(false);
        ui->quiesceCheckBox->setChecked(false);
    }
    updateOkButton();
}

void VmSnapshotDialog::onQuiesceCheckBoxToggled(bool checked)
{
    if (checked)
    {
        // Quiesce requires disk mode
        ui->diskRadioButton->setChecked(true);
    }
}

bool VmSnapshotDialog::canUseQuiesce() const
{
    QVariantList allowedOps = m_vmData.value("allowed_operations").toList();
    QString powerState = m_vmData.value("power_state").toString();

    return allowedOps.contains("snapshot_with_quiesce") &&
           powerState == "Running";
}

bool VmSnapshotDialog::canUseCheckpoint() const
{
    QVariantList allowedOps = m_vmData.value("allowed_operations").toList();
    QString powerState = m_vmData.value("power_state").toString();

    return allowedOps.contains("checkpoint") &&
           powerState == "Running";
}
