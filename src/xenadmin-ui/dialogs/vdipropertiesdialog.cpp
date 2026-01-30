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

#include "vdipropertiesdialog.h"
#include "ui_vdipropertiesdialog.h"
#include "xenlib/utils/misc.h"
#include "xenlib/xen/vdi.h"
#include "xenlib/xen/sr.h"
#include <QMessageBox>
#include <QPushButton>
#include <QDebug>

VdiPropertiesDialog::VdiPropertiesDialog(QSharedPointer<VDI> vdi, QWidget* parent) : QDialog(parent), ui(new Ui::VdiPropertiesDialog), m_vdi(vdi), m_originalSize(0), m_canResize(false)
{
    this->ui->setupUi(this);

    // Connect size change signal
    connect(this->ui->sizeSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &VdiPropertiesDialog::onSizeChanged);

    // Connect OK button to validation
    disconnect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &VdiPropertiesDialog::validateAndAccept);

    this->populateDialog();
}

VdiPropertiesDialog::~VdiPropertiesDialog()
{
    delete this->ui;
}

void VdiPropertiesDialog::populateDialog()
{
    if (!this->m_vdi || !this->m_vdi->IsValid())
    {
        qWarning() << "VdiPropertiesDialog: Invalid VDI";
        return;
    }

    // Populate name and description
    QString name = this->m_vdi->GetName();
    QString description = this->m_vdi->GetDescription();
    this->m_originalName = name;
    this->m_originalDescription = description;

    this->ui->nameLineEdit->setText(name);
    this->ui->descriptionTextEdit->setPlainText(description);

    // Get SR information
    this->m_sr = this->m_vdi->GetSR();
    this->ui->srValueLabel->setText(this->m_sr ? this->m_sr->GetName() : tr("Unknown"));

    // Get size information
    this->m_originalSize = this->m_vdi->VirtualSize();
    double sizeGB = this->m_originalSize / (1024.0 * 1024.0 * 1024.0);

    this->ui->currentSizeValueLabel->setText(Misc::FormatSize(this->m_originalSize));
    this->ui->sizeSpinBox->setValue(sizeGB);

    // Check if resize is allowed
    QStringList allowedOps = this->m_vdi->AllowedOperations();
    this->m_canResize = false;

    for (const QString& opStr : allowedOps)
    {
        if (opStr == "resize" || opStr == "resize_online")
        {
            this->m_canResize = true;
            break;
        }
    }

    // Enable/disable resize controls
    this->ui->sizeSpinBox->setEnabled(this->m_canResize);
    this->ui->newSizeLabel->setEnabled(this->m_canResize);

    if (!this->m_canResize)
    {
        this->ui->resizeWarningLabel->setText(
            "Resizing is not available for this virtual disk. "
            "It may be in use or the storage repository does not support resizing.");
    }

    // Set window title with VDI name
    this->setWindowTitle(QString("Properties: %1").arg(name));
}

QString VdiPropertiesDialog::getVdiName() const
{
    return this->ui->nameLineEdit->text().trimmed();
}

QString VdiPropertiesDialog::getVdiDescription() const
{
    return this->ui->descriptionTextEdit->toPlainText().trimmed();
}

qint64 VdiPropertiesDialog::getNewSize() const
{
    // Convert GB to bytes
    double sizeGB = this->ui->sizeSpinBox->value();
    return static_cast<qint64>(sizeGB * 1024.0 * 1024.0 * 1024.0);
}

bool VdiPropertiesDialog::hasChanges() const
{
    QString currentName = this->m_originalName;
    QString currentDesc = this->m_originalDescription;

    bool nameChanged = (this->getVdiName() != currentName);
    bool descChanged = (this->getVdiDescription() != currentDesc);

    // Only consider size changed if difference is > 10MB (to avoid floating point issues)
    bool sizeChanged = (this->getNewSize() - this->m_originalSize) > (10 * 1024 * 1024);

    return nameChanged || descChanged || sizeChanged;
}

void VdiPropertiesDialog::onSizeChanged(double value)
{
    Q_UNUSED(value);
    this->validateResize();
}

void VdiPropertiesDialog::validateResize()
{
    if (!this->m_canResize)
    {
        return;
    }

    qint64 newSize = this->getNewSize();
    qint64 sizeDiff = newSize - this->m_originalSize;

    // Clear previous warnings
    this->ui->resizeWarningLabel->clear();
    this->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);

    // Check if size is decreasing
    if (newSize < this->m_originalSize)
    {
        this->ui->resizeWarningLabel->setText("Error: Cannot decrease virtual disk size. Only increases are supported.");
        this->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        return;
    }

    // Check if size change is too small (< 10MB)
    if (sizeDiff > 0 && sizeDiff < (10 * 1024 * 1024))
    {
        this->ui->resizeWarningLabel->setText("Warning: Size increase is less than 10 MB. This may not be detected.");
    }

    // Get SR to check free space
    if (this->m_sr)
    {
        qint64 physicalSize = this->m_sr->PhysicalSize();
        qint64 physicalUtilisation = this->m_sr->PhysicalUtilisation();
        qint64 freeSpace = physicalSize - physicalUtilisation;

        // Check if SR has enough free space (for non-thin-provisioned SRs)
        // Note: We don't know if SR is thin-provisioned, so just warn if space is low
        if (sizeDiff > freeSpace && freeSpace > 0)
        {
            double freeGB = freeSpace / (1024.0 * 1024.0 * 1024.0);
            this->ui->resizeWarningLabel->setText(
                QString("Warning: Storage repository may not have enough free space. "
                        "Available: %1 GB")
                    .arg(QString::number(freeGB, 'f', 2)));
        }

        // Check maximum VDI size (2TB for most SRs)
        const qint64 MAX_VDI_SIZE = 2LL * 1024 * 1024 * 1024 * 1024; // 2TB
        if (newSize > MAX_VDI_SIZE)
        {
            this->ui->resizeWarningLabel->setText("Error: Maximum virtual disk size is 2 TB for most storage types.");
            this->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
            return;
        }
    }
}

void VdiPropertiesDialog::validateAndAccept()
{
    // Validate name is not empty
    QString name = this->getVdiName();
    if (name.isEmpty())
    {
        QMessageBox::warning(this, "Validation Error", "Virtual disk name cannot be empty.");
        this->ui->nameLineEdit->setFocus();
        return;
    }

    // Validate resize if changed
    if (this->m_canResize)
    {
        qint64 newSize = this->getNewSize();
        if (newSize < this->m_originalSize)
        {
            QMessageBox::warning(this, "Validation Error", "Cannot decrease virtual disk size. Only increases are supported.");
            return;
        }
    }

    // All validation passed
    accept();
}
