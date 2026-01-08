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

#include "newvirtualdiskdialog.h"
#include "ui_newvirtualdiskdialog.h"
#include "xen/network/connection.h"
#include "xencache.h"
#include "controls/srpicker.h"
#include <QMessageBox>
#include <QPushButton>
#include <QDebug>

NewVirtualDiskDialog::NewVirtualDiskDialog(XenConnection* connection, const QString& vmRef, QWidget* parent)
    : QDialog(parent), ui(new Ui::NewVirtualDiskDialog), m_connection(connection), m_vmRef(vmRef)
{
    this->ui->setupUi(this);

    // Get VM data
    if (this->m_connection && this->m_connection->GetCache())
        this->m_vmData = this->m_connection->GetCache()->ResolveObjectData("vm", this->m_vmRef);

    // Connect signals
    connect(this->ui->srPicker, &SrPicker::selectedIndexChanged, this, &NewVirtualDiskDialog::onSRChanged);
    connect(this->ui->srPicker, &SrPicker::canBeScannedChanged, this, [this]()
    {
        this->ui->rescanButton->setEnabled(this->ui->srPicker->canBeScanned());
    });
    connect(this->ui->sizeSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &NewVirtualDiskDialog::onSizeChanged);
    connect(this->ui->rescanButton, &QPushButton::clicked, this, &NewVirtualDiskDialog::onRescanClicked);
    connect(this->ui->addButton, &QPushButton::clicked, this, &NewVirtualDiskDialog::validateAndAccept);

    if (!this->m_vmData.isEmpty())
    {
        this->m_vmNameOverride = this->m_vmData.value("name_label", "VM").toString();
        QVariantList vbdRefs = this->m_vmData.value("VBDs", QVariantList()).toList();
        for (const QVariant& vbdRefVar : vbdRefs)
        {
            QString vbdRef = vbdRefVar.toString();
            QVariantMap vbdData = this->m_connection->GetCache()->ResolveObjectData("vbd", vbdRef);
            QString userdevice = vbdData.value("userdevice", "").toString();
            if (!userdevice.isEmpty())
                this->m_usedDevices.append(userdevice);
        }
    }

    // Populate SR list widget
    this->populateSRList();

    this->updateDefaultName();
    this->applyInitialDisk();
}

NewVirtualDiskDialog::~NewVirtualDiskDialog()
{
    delete this->ui;
}

void NewVirtualDiskDialog::populateSRList()
{
    if (!this->m_connection)
        return;

    QString homeHost = this->m_homeHostRef;
    if (homeHost.isEmpty())
    {
        QString vmResidentOn = this->m_vmData.value("resident_on", "").toString();
        QString vmAffinity = this->m_vmData.value("affinity", "").toString();
        homeHost = vmResidentOn.isEmpty() ? vmAffinity : vmResidentOn;
    }

    this->ui->srPicker->populate(SrPicker::VM,
                                 this->m_connection,
                                 homeHost,
                                 this->m_initialSrRef,
                                 QStringList());
    this->ui->rescanButton->setEnabled(this->ui->srPicker->canBeScanned());
}

int NewVirtualDiskDialog::findNextAvailableDevice() const
{
    if (!this->m_usedDevices.isEmpty())
    {
        int maxDevice = -1;
        for (const QString& device : this->m_usedDevices)
        {
            bool ok = false;
            int deviceNum = device.toInt(&ok);
            if (ok && deviceNum > maxDevice)
                maxDevice = deviceNum;
        }
        return maxDevice + 1;
    }

    if (!this->m_connection || !this->m_connection->GetCache())
        return 0;

    // Find highest device number in use
    int maxDevice = -1;

    QVariantList vbdRefs = this->m_vmData.value("VBDs", QVariantList()).toList();
    for (const QVariant& vbdRefVar : vbdRefs)
    {
        QString vbdRef = vbdRefVar.toString();
        QVariantMap vbdData = this->m_connection->GetCache()->ResolveObjectData("vbd", vbdRef);

        QString userdevice = vbdData.value("userdevice", "").toString();
        bool ok;
        int deviceNum = userdevice.toInt(&ok);
        if (ok && deviceNum > maxDevice)
        {
            maxDevice = deviceNum;
        }
    }

    return maxDevice + 1;
}

void NewVirtualDiskDialog::onSRChanged()
{
    this->validateInput();
}

void NewVirtualDiskDialog::onSizeChanged(double value)
{
    Q_UNUSED(value);
    this->validateInput();
}

void NewVirtualDiskDialog::onRescanClicked()
{
    this->ui->srPicker->scanSRs();
}

void NewVirtualDiskDialog::validateInput()
{
    this->ui->warningLabel->clear();
    this->ui->addButton->setEnabled(true);

    if (!this->m_connection || !this->m_connection->GetCache())
    {
        this->ui->warningLabel->setText("Error: No connection available.");
        this->ui->addButton->setEnabled(false);
        return;
    }

    // Check if SR is selected
    if (this->ui->srPicker->selectedSR().isEmpty())
    {
        this->ui->warningLabel->setText("Error: Please select a storage repository.");
        this->ui->addButton->setEnabled(false);
        return;
    }

    QString srRef = this->ui->srPicker->selectedSR();
    QVariantMap srData = this->m_connection->GetCache()->ResolveObjectData("sr", srRef);

    if (srData.isEmpty())
    {
        this->ui->warningLabel->setText("Error: Selected storage repository not found.");
        this->ui->addButton->setEnabled(false);
        return;
    }

    // Get requested size in bytes
    qint64 requestedSize = this->getSize();

    // Check minimum size
    if (requestedSize < (10 * 1024 * 1024)) // 10 MB minimum
    {
        this->ui->warningLabel->setText("Error: Minimum disk size is 10 MB.");
        this->ui->addButton->setEnabled(false);
        return;
    }

    if (this->m_minSizeBytes > 0 && requestedSize < this->m_minSizeBytes)
    {
        this->ui->warningLabel->setText(
            QString("Error: Minimum disk size is %1 MB.")
                .arg(QString::number(this->m_minSizeBytes / (1024.0 * 1024.0), 'f', 0)));
        this->ui->addButton->setEnabled(false);
        return;
    }

    // Check maximum size (2TB for most SRs)
    const qint64 MAX_VDI_SIZE = 2LL * 1024 * 1024 * 1024 * 1024; // 2TB
    if (requestedSize > MAX_VDI_SIZE)
    {
        this->ui->warningLabel->setText("Error: Maximum disk size is 2 TB for most storage types.");
        this->ui->addButton->setEnabled(false);
        return;
    }

    // Check SR free space
    qint64 physicalSize = srData.value("physical_size", 0).toLongLong();
    qint64 physicalUtilisation = srData.value("physical_utilisation", 0).toLongLong();
    qint64 freeSpace = physicalSize - physicalUtilisation;

    if (physicalSize > 0 && requestedSize > freeSpace)
    {
        double freeGB = freeSpace / (1024.0 * 1024.0 * 1024.0);
        this->ui->warningLabel->setText(
            QString("Warning: Storage repository may not have enough free space. "
                    "Available: %1 GB")
                .arg(QString::number(freeGB, 'f', 2)));
        // Still allow creation for thin-provisioned SRs
    }
}

void NewVirtualDiskDialog::validateAndAccept()
{
    // Validate name
    QString name = this->getVDIName();
    if (name.isEmpty())
    {
        QMessageBox::warning(this, "Validation Error", "Please enter a name for the virtual disk.");
        this->ui->nameLineEdit->setFocus();
        return;
    }

    // Validate SR selection
    if (this->ui->srPicker->selectedSR().isEmpty())
    {
        QMessageBox::warning(this, "Validation Error", "Please select a storage repository.");
        return;
    }

    // Validate size
    qint64 size = this->getSize();
    if (size < (10 * 1024 * 1024))
    {
        QMessageBox::warning(this, "Validation Error", "Minimum disk size is 10 MB.");
        return;
    }

    const qint64 MAX_VDI_SIZE = 2LL * 1024 * 1024 * 1024 * 1024;
    if (size > MAX_VDI_SIZE)
    {
        QMessageBox::warning(this, "Validation Error", "Maximum disk size is 2 TB.");
        return;
    }

    if (this->m_minSizeBytes > 0 && size < this->m_minSizeBytes)
    {
        QMessageBox::warning(this, "Validation Error", "Selected size is below the template minimum.");
        return;
    }

    // All validation passed
    accept();
}

QString NewVirtualDiskDialog::getVDIName() const
{
    return this->ui->nameLineEdit->text().trimmed();
}

QString NewVirtualDiskDialog::getVDIDescription() const
{
    return this->ui->descriptionTextEdit->toPlainText().trimmed();
}

QString NewVirtualDiskDialog::getSelectedSR() const
{
    return this->ui->srPicker->selectedSR();
}

qint64 NewVirtualDiskDialog::getSize() const
{
    // Convert GB to bytes
    double sizeGB = this->ui->sizeSpinBox->value();
    return static_cast<qint64>(sizeGB * 1024.0 * 1024.0 * 1024.0);
}

void NewVirtualDiskDialog::setDialogMode(DialogMode mode)
{
    this->m_mode = mode;
    if (this->m_mode == DialogMode::Edit)
    {
        this->setWindowTitle(tr("Edit Virtual Disk"));
        this->ui->addButton->setText(tr("OK"));
    }
    else
    {
        this->setWindowTitle(tr("Add Virtual Disk"));
        this->ui->addButton->setText(tr("&Add"));
    }
}

void NewVirtualDiskDialog::setWizardContext(const QString& vmName, const QStringList& usedDevices, const QString& homeHostRef)
{
    this->m_vmNameOverride = vmName;
    this->m_usedDevices = usedDevices;
    this->m_homeHostRef = homeHostRef;
    this->updateDefaultName();
}

void NewVirtualDiskDialog::setInitialDisk(const QString& name,
                                          const QString& description,
                                          qint64 sizeBytes,
                                          const QString& srRef)
{
    this->m_initialName = name;
    this->m_initialDescription = description;
    this->m_initialSizeBytes = sizeBytes;
    this->m_initialSrRef = srRef;
    this->applyInitialDisk();
}

void NewVirtualDiskDialog::setMinSizeBytes(qint64 minSizeBytes)
{
    this->m_minSizeBytes = minSizeBytes;
    if (this->m_minSizeBytes > 0)
        this->ui->sizeSpinBox->setMinimum(this->m_minSizeBytes / (1024.0 * 1024.0 * 1024.0));
}

void NewVirtualDiskDialog::setCanResize(bool canResize)
{
    this->m_canResize = canResize;
    this->ui->sizeSpinBox->setEnabled(this->m_canResize);
}

void NewVirtualDiskDialog::updateDefaultName()
{
    if (!this->m_initialName.isEmpty())
        return;

    QString vmName = this->m_vmNameOverride.isEmpty() ? QStringLiteral("VM") : this->m_vmNameOverride;
    int nextDevice = this->findNextAvailableDevice();
    this->ui->nameLineEdit->setText(QString("%1 Disk %2").arg(vmName).arg(nextDevice));
}

void NewVirtualDiskDialog::applyInitialDisk()
{
    if (!this->m_initialName.isEmpty())
        this->ui->nameLineEdit->setText(this->m_initialName);
    if (!this->m_initialDescription.isEmpty())
        this->ui->descriptionTextEdit->setText(this->m_initialDescription);
    if (this->m_initialSizeBytes > 0)
    {
        double sizeGB = this->m_initialSizeBytes / (1024.0 * 1024.0 * 1024.0);
        this->ui->sizeSpinBox->setValue(sizeGB);
    }

    Q_UNUSED(this->m_initialSrRef);
}

// Device position, mode, and bootable are determined by the calling command
// These methods are kept for compatibility but return defaults
QString NewVirtualDiskDialog::getDevicePosition() const
{
    return QString::number(findNextAvailableDevice());
}

QString NewVirtualDiskDialog::getMode() const
{
    // Default to read-write
    return "RW";
}

bool NewVirtualDiskDialog::isBootable() const
{
    // Default to non-bootable
    return false;
}
