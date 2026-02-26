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

#include <QTableWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QDebug>
#include "attachvirtualdiskdialog.h"
#include "ui_attachvirtualdiskdialog.h"
#include "../widgets/tableclipboardutils.h"
#include "xenlib/utils/misc.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/vdi.h"
#include "xenlib/xen/vbd.h"
#include "xenlib/xencache.h"

namespace
{
class SizeTableWidgetItem : public QTableWidgetItem
{
    public:
        explicit SizeTableWidgetItem(const QString& text, qint64 sizeBytes) : QTableWidgetItem(text)
        {
            this->setData(Qt::UserRole, sizeBytes);
        }

        bool operator<(const QTableWidgetItem& other) const override
        {
            return this->data(Qt::UserRole).toLongLong() < other.data(Qt::UserRole).toLongLong();
        }
};
}

AttachVirtualDiskDialog::AttachVirtualDiskDialog(QSharedPointer<VM> vm, QWidget* parent) : QDialog(parent), ui(new Ui::AttachVirtualDiskDialog), m_vm(vm)
{
    this->ui->setupUi(this);

    // Set table properties
    this->ui->vdiTable->horizontalHeader()->setStretchLastSection(true);
    this->ui->vdiTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->ui->vdiTable->setSortingEnabled(true);
    this->ui->vdiTable->horizontalHeader()->setSortIndicatorShown(true);

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

    if (!this->m_vm)
        return;

    // Get all SRs
    QList<QSharedPointer<SR>> allSRs = this->m_vm->GetCache()->GetAll<SR>(XenObjectType::SR);

    for (const QSharedPointer<SR>& sr : allSRs)
    {
        if (!sr || !sr->IsValid())
            continue;

        QString srRef = sr->OpaqueRef();
        QString srName = sr->GetName();
        QString srType = sr->GetType();
        QString contentType = sr->ContentType();

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
    if (!this->m_vm)
    {
        this->ui->vdiTable->setRowCount(0);
        return;
    }

    const QString previouslySelectedVdiRef = this->getSelectedVDIRef();
    const TableClipboardUtils::SortState sortState = TableClipboardUtils::CaptureSortState(this->ui->vdiTable);

    this->ui->vdiTable->setSortingEnabled(false);
    this->ui->vdiTable->setRowCount(0);

    QString selectedSR = this->ui->srComboBox->currentData().toString();
    XenCache* cache = this->m_vm->GetCache();

    // Get all VDIs from cache
    QList<QSharedPointer<VDI>> allVDIs = cache->GetAll<VDI>();

    // Get VBDs already attached to this VM to filter them out
    QStringList attachedVDIs;
    QList<QSharedPointer<VBD>> vbds = this->m_vm->GetVBDs();
    for (const QSharedPointer<VBD>& vbd : vbds)
    {
        if (!vbd || !vbd->IsValid())
            continue;

        QString vdiRef = vbd->GetVDIRef();
        if (!vdiRef.isEmpty() && vdiRef != XENOBJECT_NULL)
        {
            attachedVDIs.append(vdiRef);
        }
    }

    for (const QSharedPointer<VDI>& vdi : allVDIs)
    {
        if (!vdi || !vdi->IsValid())
            continue;

        QString vdiRef = vdi->OpaqueRef();
        QString srRef = vdi->SRRef();

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
        QSharedPointer<SR> sr = cache->ResolveObject<SR>(srRef);
        if (!sr || !sr->IsValid())
            continue;

        QString srName = sr->GetName();
        QString srType = sr->GetType();
        QString contentType = sr->ContentType();

        // Skip ISOs and tools disks
        if (srType == "iso" || contentType == "iso")
        {
            continue;
        }

        QString vdiType = vdi->GetType();
        if (vdiType == "user" && srType == "iso")
        {
            continue; // Skip ISOs
        }

        // Check if VDI is in use by other VMs
        QList<QSharedPointer<VBD>> vdiVbds = vdi->GetVBDs();
        bool inUseByOthers = false;
        for (const QSharedPointer<VBD>& vbd : vdiVbds)
        {
            if (!vbd || !vbd->IsValid())
                continue;

            QString vmRef = vbd->GetVMRef();
            if (vmRef != this->m_vm->OpaqueRef())
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
        QString name = vdi->GetName().isEmpty() ? "Unnamed" : vdi->GetName();
        QString description = vdi->GetDescription();
        qint64 virtualSize = vdi->VirtualSize();
        QString size = Misc::FormatSize(virtualSize);

        int row = this->ui->vdiTable->rowCount();
        this->ui->vdiTable->insertRow(row);

        QTableWidgetItem* nameItem = new QTableWidgetItem(name);
        nameItem->setData(Qt::UserRole, vdiRef); // Store VDI ref
        this->ui->vdiTable->setItem(row, 0, nameItem);

        this->ui->vdiTable->setItem(row, 1, new QTableWidgetItem(description));
        this->ui->vdiTable->setItem(row, 2, new SizeTableWidgetItem(size, virtualSize));
        this->ui->vdiTable->setItem(row, 3, new QTableWidgetItem(srName));
    }

    TableClipboardUtils::RestoreSortState(this->ui->vdiTable, sortState, 0, Qt::AscendingOrder);

    if (!previouslySelectedVdiRef.isEmpty())
    {
        for (int row = 0; row < this->ui->vdiTable->rowCount(); ++row)
        {
            QTableWidgetItem* nameItem = this->ui->vdiTable->item(row, 0);
            if (nameItem && nameItem->data(Qt::UserRole).toString() == previouslySelectedVdiRef)
            {
                this->ui->vdiTable->selectRow(row);
                break;
            }
        }
    }

    // Resize columns to content
    for (int i = 0; i < this->ui->vdiTable->columnCount(); ++i)
    {
        this->ui->vdiTable->resizeColumnToContents(i);
    }
}

int AttachVirtualDiskDialog::findNextAvailableDevice()
{
    if (!this->m_vm)
        return 0;

    // Find highest device number in use
    int maxDevice = -1;

    QList<QSharedPointer<VBD>> vbds = this->m_vm->GetVBDs();
    for (const QSharedPointer<VBD>& vbd : vbds)
    {
        if (!vbd || !vbd->IsValid())
            continue;

        QString userdevice = vbd->GetUserdevice();
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
