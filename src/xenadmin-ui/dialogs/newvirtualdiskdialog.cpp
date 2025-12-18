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
#include "xenlib.h"
#include "xencache.h"
#include "iconmanager.h"
#include <QMessageBox>
#include <QPushButton>
#include <QDebug>

NewVirtualDiskDialog::NewVirtualDiskDialog(XenLib* xenLib, const QString& vmRef, QWidget* parent)
    : QDialog(parent), ui(new Ui::NewVirtualDiskDialog), m_xenLib(xenLib), m_vmRef(vmRef)
{
    this->ui->setupUi(this);

    // Get VM data
    this->m_vmData = this->m_xenLib->getCache()->resolve("vm", this->m_vmRef);

    // Connect signals
    connect(this->ui->srListWidget, &QListWidget::currentRowChanged, this, &NewVirtualDiskDialog::onSRChanged);
    connect(this->ui->sizeSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &NewVirtualDiskDialog::onSizeChanged);
    connect(this->ui->rescanButton, &QPushButton::clicked, this, &NewVirtualDiskDialog::onRescanClicked);
    connect(this->ui->addButton, &QPushButton::clicked, this, &NewVirtualDiskDialog::validateAndAccept);

    // Populate SR list widget
    this->populateSRList();

    // Find next available device position for default naming
    int nextDevice = this->findNextAvailableDevice();

    // Generate default name
    QString vmName = this->m_vmData.value("name_label", "VM").toString();
    this->ui->nameLineEdit->setText(QString("%1 Disk %2").arg(vmName).arg(nextDevice));
}

NewVirtualDiskDialog::~NewVirtualDiskDialog()
{
    delete this->ui;
}

void NewVirtualDiskDialog::populateSRList()
{
    // C# pattern: SrPicker control shows detailed SR information with warnings
    // Each item shows: SR name, free space, and availability warnings
    this->ui->srListWidget->clear();

    // Get all SRs
    QList<QVariantMap> allSRs = this->m_xenLib->getCache()->getAll("sr");

    // Get VM's resident host to check SR visibility
    QString vmResidentOn = this->m_vmData.value("resident_on", "").toString();
    QString vmAffinity = this->m_vmData.value("affinity", "").toString();
    QString homeHost = vmResidentOn.isEmpty() ? vmAffinity : vmResidentOn;

    // Get all hosts to check PBD connections
    QList<QVariantMap> allHosts = this->m_xenLib->getCache()->getAll("host");

    for (const QVariantMap& srData : allSRs)
    {
        QString srRef = srData.value("ref", "").toString();
        QString srName = srData.value("name_label", "Unknown").toString();
        QString srType = srData.value("type", "").toString();
        QString contentType = srData.value("content_type", "").toString();

        // C# SR.SupportsVdiCreate() checks (SR.cs lines 443-457):
        // 1. Skip ISO SRs (content_type == "iso")
        if (contentType == "iso")
        {
            continue;
        }

        // 2. Skip tmpfs (memory) SRs
        if (srType == "tmpfs")
        {
            continue;
        }

        // 3. Check if SM (Storage Manager) supports VDI_CREATE capability
        QList<QVariantMap> allSMs = this->m_xenLib->getCache()->getAll("SM");
        bool supportsVdiCreate = false;

        for (const QVariantMap& smData : allSMs)
        {
            QString smType = smData.value("type", "").toString();
            if (smType == srType)
            {
                // Check if "VDI_CREATE" is in capabilities list
                QVariantList capabilities = smData.value("capabilities", QVariantList()).toList();
                for (const QVariant& cap : capabilities)
                {
                    if (cap.toString() == "VDI_CREATE")
                    {
                        supportsVdiCreate = true;
                        break;
                    }
                }
                break;
            }
        }

        if (!supportsVdiCreate)
        {
            continue;
        }

        // Get size information
        qint64 physicalSize = srData.value("physical_size", 0).toLongLong();
        qint64 physicalUtilisation = srData.value("physical_utilisation", 0).toLongLong();
        qint64 freeSpace = physicalSize - physicalUtilisation;

        // Check allowed operations to detect broken SR
        QVariantList allowedOps = srData.value("allowed_operations", QVariantList()).toList();
        bool isBroken = allowedOps.isEmpty();

        // Check if SR is full (less than 1MB free)
        bool isFull = (physicalSize > 0 && freeSpace < 1024 * 1024);

        // Check SR visibility from VM's home host (C# SrPickerItem pattern)
        // Get PBD connections for this SR
        QVariantList pbdRefs = srData.value("PBDs", QVariantList()).toList();
        QStringList hostsWithAccess;
        QStringList hostsWithoutAccess;

        for (const QVariantMap& hostData : allHosts)
        {
            QString hostRef = hostData.value("ref", "").toString();
            QString hostName = hostData.value("name_label", "").toString();

            // Check if this host has a PBD to this SR that's attached
            bool hasAttachedPBD = false;
            for (const QVariant& pbdRefVar : pbdRefs)
            {
                QString pbdRef = pbdRefVar.toString();
                QVariantMap pbdData = this->m_xenLib->getCache()->resolve("pbd", pbdRef);

                if (pbdData.value("host", "").toString() == hostRef &&
                    pbdData.value("currently_attached", false).toBool())
                {
                    hasAttachedPBD = true;
                    break;
                }
            }

            if (hasAttachedPBD)
            {
                hostsWithAccess.append(hostName);
            } else
            {
                hostsWithoutAccess.append(hostName);
            }
        }

        // Determine if SR is available to the VM
        bool isAvailable = true;
        QString warningMessage;

        if (isBroken)
        {
            isAvailable = false;
            warningMessage = "This storage repository is broken";
        } else if (isFull)
        {
            isAvailable = false;
            warningMessage = "This storage repository is full";
        } else if (!homeHost.isEmpty())
        {
            // Check if SR is visible from VM's home host
            QVariantMap homeHostData = this->m_xenLib->getCache()->resolve("host", homeHost);
            QString homeHostName = homeHostData.value("name_label", "").toString();

            if (!hostsWithoutAccess.isEmpty() && hostsWithoutAccess.contains(homeHostName))
            {
                isAvailable = false;
                warningMessage = QString("This storage repository cannot be seen from %1").arg(homeHostName);
            } else if (!hostsWithoutAccess.isEmpty())
            {
                // SR is accessible but not from all hosts
                warningMessage = QString("This storage repository cannot be seen from %1").arg(hostsWithoutAccess.join(", "));
            }
        }

        // Create list item with SR name and detailed description
        QListWidgetItem* item = new QListWidgetItem();
        item->setText(srName);
        item->setData(Qt::UserRole, srRef);

        // Set icon
        QIcon srIcon = IconManager::instance().getIconForSR(srData);
        item->setIcon(srIcon);

        // Build description text (shown below SR name)
        QString description;
        if (!warningMessage.isEmpty())
        {
            description = warningMessage;
        } else if (physicalSize > 0)
        {
            double freeGB = freeSpace / (1024.0 * 1024.0 * 1024.0);
            double totalGB = physicalSize / (1024.0 * 1024.0 * 1024.0);
            description = QString("%1 GB free of %2 GB").arg(QString::number(freeGB, 'f', 2), QString::number(totalGB, 'f', 2));
        }

        // Store description as tooltip and use custom item text formatting
        item->setToolTip(QString("%1\n%2").arg(srName, description));

        // Format text to show name on first line, description on second line
        item->setText(QString("%1\n%2").arg(srName, description));

        // Disable item if not available
        if (!isAvailable)
        {
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            // Use red color for warnings
            item->setForeground(QColor(Qt::red));
        }

        this->ui->srListWidget->addItem(item);
    }

    // Select first available (enabled) SR
    for (int i = 0; i < this->ui->srListWidget->count(); ++i)
    {
        QListWidgetItem* item = this->ui->srListWidget->item(i);
        if (item->flags() & Qt::ItemIsEnabled)
        {
            this->ui->srListWidget->setCurrentRow(i);
            break;
        }
    }
}

int NewVirtualDiskDialog::findNextAvailableDevice()
{
    // Find highest device number in use
    int maxDevice = -1;

    QVariantList vbdRefs = this->m_vmData.value("VBDs", QVariantList()).toList();
    for (const QVariant& vbdRefVar : vbdRefs)
    {
        QString vbdRef = vbdRefVar.toString();
        QVariantMap vbdData = this->m_xenLib->getCache()->resolve("vbd", vbdRef);

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

void NewVirtualDiskDialog::onSRChanged(int row)
{
    Q_UNUSED(row);
    this->validateInput();
}

void NewVirtualDiskDialog::onSizeChanged(double value)
{
    Q_UNUSED(value);
    this->validateInput();
}

void NewVirtualDiskDialog::onRescanClicked()
{
    // Refresh the SR list
    this->populateSRList();
}

void NewVirtualDiskDialog::validateInput()
{
    this->ui->warningLabel->clear();
    this->ui->addButton->setEnabled(true);

    // Check if SR is selected
    if (this->ui->srListWidget->currentRow() < 0)
    {
        this->ui->warningLabel->setText("Error: Please select a storage repository.");
        this->ui->addButton->setEnabled(false);
        return;
    }

    QListWidgetItem* selectedItem = this->ui->srListWidget->currentItem();
    if (!selectedItem)
    {
        this->ui->warningLabel->setText("Error: Please select a storage repository.");
        this->ui->addButton->setEnabled(false);
        return;
    }

    QString srRef = selectedItem->data(Qt::UserRole).toString();
    QVariantMap srData = this->m_xenLib->getCache()->resolve("sr", srRef);

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
    if (this->ui->srListWidget->currentRow() < 0)
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
    QListWidgetItem* selectedItem = this->ui->srListWidget->currentItem();
    if (selectedItem)
    {
        return selectedItem->data(Qt::UserRole).toString();
    }
    return QString();
}

qint64 NewVirtualDiskDialog::getSize() const
{
    // Convert GB to bytes
    double sizeGB = this->ui->sizeSpinBox->value();
    return static_cast<qint64>(sizeGB * 1024.0 * 1024.0 * 1024.0);
}

// Device position, mode, and bootable are determined by the calling command
// These methods are kept for compatibility but return defaults
QString NewVirtualDiskDialog::getDevicePosition() const
{
    // Find next available device automatically
    int maxDevice = -1;

    QVariantList vbdRefs = this->m_vmData.value("VBDs", QVariantList()).toList();
    for (const QVariant& vbdRefVar : vbdRefs)
    {
        QString vbdRef = vbdRefVar.toString();
        QVariantMap vbdData = this->m_xenLib->getCache()->resolve("vbd", vbdRef);

        QString userdevice = vbdData.value("userdevice", "").toString();
        bool ok;
        int deviceNum = userdevice.toInt(&ok);
        if (ok && deviceNum > maxDevice)
        {
            maxDevice = deviceNum;
        }
    }

    return QString::number(maxDevice + 1);
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
