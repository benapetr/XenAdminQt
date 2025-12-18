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

#include "attachvirtualdiskdialog.h"
#include "ui_attachvirtualdiskdialog.h"
#include "xenlib.h"
#include "xencache.h"
#include <QTableWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QDebug>

AttachVirtualDiskDialog::AttachVirtualDiskDialog(XenLib* xenLib, const QString& vmRef, QWidget* parent)
    : QDialog(parent), ui(new Ui::AttachVirtualDiskDialog), m_xenLib(xenLib), m_vmRef(vmRef)
{
    this->ui->setupUi(this);

    // Get VM data
    this->m_vmData = this->m_xenLib->getCache()->resolve("vm", this->m_vmRef);

    // Set table properties
    this->ui->vdiTable->horizontalHeader()->setStretchLastSection(true);
    this->ui->vdiTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Connect signals
    connect(this->ui->srComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AttachVirtualDiskDialog::onSRFilterChanged);
    connect(this->ui->vdiTable, &QTableWidget::itemSelectionChanged, this, &AttachVirtualDiskDialog::onVDISelectionChanged);

    // Find next available device position
    int nextDevice = this->findNextAvailableDevice();
    this->ui->deviceSpinBox->setValue(nextDevice);

    // Populate SR filter and VDI list
    this->populateSRFilter();
    this->populateVDITable();

    // Initial OK button state
    this->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
}

AttachVirtualDiskDialog::~AttachVirtualDiskDialog()
{
    delete this->ui;
}

void AttachVirtualDiskDialog::populateSRFilter()
{
    this->ui->srComboBox->clear();

    // Add "All Storage Repositories" option
    this->ui->srComboBox->addItem("All Storage Repositories", QString());

    // Get all SRs
    QList<QVariantMap> allSRs = this->m_xenLib->getCache()->getAll("sr");

    for (const QVariantMap& srData : allSRs)
    {
        QString srRef = srData.value("ref", "").toString();
        QString srName = srData.value("name_label", "Unknown").toString();
        QString srType = srData.value("type", "").toString();
        QString contentType = srData.value("content_type", "").toString();

        // Skip ISO SRs
        if (srType == "iso" || contentType == "iso")
        {
            continue;
        }

        this->ui->srComboBox->addItem(srName, srRef);
    }
}

void AttachVirtualDiskDialog::populateVDITable()
{
    this->ui->vdiTable->setRowCount(0);

    QString selectedSR = this->ui->srComboBox->currentData().toString();

    // Get all VDIs from cache
    QList<QVariantMap> allVDIs = this->m_xenLib->getCache()->getAll("vdi");

    // Get VBDs already attached to this VM to filter them out
    QStringList attachedVDIs;
    QVariantList vbdRefs = this->m_vmData.value("VBDs", QVariantList()).toList();
    for (const QVariant& vbdRefVar : vbdRefs)
    {
        QString vbdRef = vbdRefVar.toString();
        QVariantMap vbdData = this->m_xenLib->getCache()->resolve("vbd", vbdRef);
        QString vdiRef = vbdData.value("VDI", "").toString();
        if (!vdiRef.isEmpty() && vdiRef != "OpaqueRef:NULL")
        {
            attachedVDIs.append(vdiRef);
        }
    }

    for (const QVariantMap& vdiData : allVDIs)
    {
        QString vdiRef = vdiData.value("ref", "").toString();
        QString srRef = vdiData.value("SR", "").toString();

        // Skip if already attached to this VM
        if (attachedVDIs.contains(vdiRef))
        {
            continue;
        }

        // Skip if filtering by SR and this VDI is not in selected SR
        if (!selectedSR.isEmpty() && srRef != selectedSR)
        {
            continue;
        }

        // Get SR data
        QVariantMap srData = this->m_xenLib->getCache()->resolve("sr", srRef);
        QString srName = srData.value("name_label", "Unknown").toString();
        QString srType = srData.value("type", "").toString();
        QString contentType = srData.value("content_type", "").toString();

        // Skip ISOs and tools disks
        if (srType == "iso" || contentType == "iso")
        {
            continue;
        }

        QString vdiType = vdiData.value("type", "").toString();
        if (vdiType == "user" && srType == "iso")
        {
            continue; // Skip ISOs
        }

        // Check if VDI is in use by other VMs
        QVariantList vbdRefs = vdiData.value("VBDs", QVariantList()).toList();
        bool inUseByOthers = false;
        for (const QVariant& vbdVar : vbdRefs)
        {
            QString vbdRef = vbdVar.toString();
            QVariantMap vbdData = this->m_xenLib->getCache()->resolve("vbd", vbdRef);
            QString vmRef = vbdData.value("VM", "").toString();
            if (vmRef != this->m_vmRef)
            {
                inUseByOthers = true;
                break;
            }
        }

        // Skip if in use by other VMs (we could allow read-only sharing later)
        if (inUseByOthers)
        {
            continue;
        }

        // Add to table
        QString name = vdiData.value("name_label", "Unnamed").toString();
        QString description = vdiData.value("name_description", "").toString();
        qint64 virtualSize = vdiData.value("virtual_size", 0).toLongLong();

        QString size = "N/A";
        if (virtualSize > 0)
        {
            double sizeGB = virtualSize / (1024.0 * 1024.0 * 1024.0);
            size = QString::number(sizeGB, 'f', 2) + " GB";
        }

        int row = this->ui->vdiTable->rowCount();
        this->ui->vdiTable->insertRow(row);

        QTableWidgetItem* nameItem = new QTableWidgetItem(name);
        nameItem->setData(Qt::UserRole, vdiRef); // Store VDI ref
        this->ui->vdiTable->setItem(row, 0, nameItem);

        this->ui->vdiTable->setItem(row, 1, new QTableWidgetItem(description));
        this->ui->vdiTable->setItem(row, 2, new QTableWidgetItem(size));
        this->ui->vdiTable->setItem(row, 3, new QTableWidgetItem(srName));
    }

    // Resize columns to content
    for (int i = 0; i < this->ui->vdiTable->columnCount(); ++i)
    {
        this->ui->vdiTable->resizeColumnToContents(i);
    }
}

int AttachVirtualDiskDialog::findNextAvailableDevice()
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

void AttachVirtualDiskDialog::onSRFilterChanged(int index)
{
    Q_UNUSED(index);
    this->populateVDITable();
}

void AttachVirtualDiskDialog::onVDISelectionChanged()
{
    bool hasSelection = !this->ui->vdiTable->selectedItems().isEmpty();
    this->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(hasSelection);
}

QString AttachVirtualDiskDialog::getSelectedVDIRef() const
{
    QList<QTableWidgetItem*> selectedItems = this->ui->vdiTable->selectedItems();
    if (selectedItems.isEmpty())
        return QString();

    int row = selectedItems.first()->row();
    QTableWidgetItem* item = this->ui->vdiTable->item(row, 0);
    if (item)
        return item->data(Qt::UserRole).toString();

    return QString();
}

QString AttachVirtualDiskDialog::getDevicePosition() const
{
    return QString::number(this->ui->deviceSpinBox->value());
}

QString AttachVirtualDiskDialog::getMode() const
{
    return (this->ui->modeComboBox->currentIndex() == 1) ? "RO" : "RW";
}

bool AttachVirtualDiskDialog::isBootable() const
{
    return this->ui->bootableCheckBox->isChecked();
}
