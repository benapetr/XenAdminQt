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
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QTimer>
#include "../settingsmanager.h"
#include "xenlib/xen/host.h"
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

    connect(this->ui->modeComboBox, QOverload<int>::of(&QComboBox::activated), this, [this](int index)
    {
        this->m_preferReadOnly = index == 1;
    });

    this->m_refreshTimer = new QTimer(this);
    this->m_refreshTimer->setSingleShot(true);
    connect(this->m_refreshTimer, &QTimer::timeout, this, [this]()
    {
        if (!this->isVisible())
            return;
        this->populateSRFilter();
        this->populateVDITable();
    });
    if (this->m_vm && this->m_vm->GetCache())
    {
        auto refresh = [this](QSharedPointer<XenObject> object)
        {
            if (!object || !this->isVisible())
                return;
            const XenObjectType type = object->GetObjectType();
            if (type == XenObjectType::VDI || type == XenObjectType::VBD ||
                type == XenObjectType::SR || type == XenObjectType::PBD ||
                (type == XenObjectType::VM && object->OpaqueRef() == this->m_vm->OpaqueRef()))
                this->m_refreshTimer->start(0);
        };
        connect(this->m_vm->GetCache(), &XenCache::objectChanged, this, refresh);
        connect(this->m_vm->GetCache(), &XenCache::objectRemoved, this, refresh);
    }

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
    const QString selectedSR = this->ui->srComboBox->currentData().toString();
    const QSignalBlocker blocker(this->ui->srComboBox);
    this->ui->srComboBox->clear();

    // Add "All Storage Repositories" option
    this->ui->srComboBox->addItem("All Storage Repositories", QString());

    if (!this->m_vm)
        return;

    // Get all SRs
    QList<QSharedPointer<SR>> allSRs = this->m_vm->GetCache()->GetAll<SR>(XenObjectType::SR);

    for (const QSharedPointer<SR>& sr : allSRs)
    {
        if (!this->isSRVisible(sr))
            continue;

        const QString reason = this->srUnavailableReason(sr);
        this->ui->srComboBox->addItem(sr->GetName(), sr->OpaqueRef());
        const int index = this->ui->srComboBox->count() - 1;
        if (!reason.isEmpty())
        {
            auto* model = qobject_cast<QStandardItemModel*>(this->ui->srComboBox->model());
            if (model && model->item(index))
            {
                model->item(index)->setEnabled(false);
                model->item(index)->setToolTip(reason);
            }
        }
    }
    const int previousIndex = this->ui->srComboBox->findData(selectedSR);
    if (previousIndex >= 0)
        this->ui->srComboBox->setCurrentIndex(previousIndex);
}

bool AttachVirtualDiskDialog::isSRVisible(QSharedPointer<SR> sr) const
{
    return sr && sr->IsVisible(SettingsManager::instance().GetShowHiddenObjects()) &&
           sr->HasPBDs() && sr->ContentType() != "iso" && sr->GetType() != "iso";
}

QString AttachVirtualDiskDialog::srUnavailableReason(QSharedPointer<SR> sr) const
{
    if (!sr || !this->m_vm)
        return tr("Storage repository is unavailable.");
    const auto affinity = this->m_vm->GetAffinityHost();
    const auto resident = this->m_vm->GetResidentOnHost();
    if (affinity && !sr->CanBeSeenFrom(affinity))
        return tr("Storage repository cannot be accessed from home server '%1'.").arg(affinity->GetName());
    if (this->m_vm->IsRunning() && resident && !sr->CanBeSeenFrom(resident))
        return tr("Storage repository cannot be accessed from the VM's current server '%1'.").arg(resident->GetName());
    if (sr->IsBroken(false))
        return tr("Storage repository is broken.");
    return QString();
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

    for (const QSharedPointer<VDI>& vdi : allVDIs)
    {
        if (!vdi || !vdi->CanAttach(SettingsManager::instance().GetShowHiddenObjects()))
            continue;

        QString vdiRef = vdi->OpaqueRef();
        QString srRef = vdi->SRRef();

        // Skip if filtering by SR and this VDI is not in selected SR
        if (!selectedSR.isEmpty() && srRef != selectedSR)
        {
            continue;
        }

        // Get SR data
        QSharedPointer<SR> sr = cache->ResolveObject<SR>(srRef);
        if (!this->isSRVisible(sr) || !this->srUnavailableReason(sr).isEmpty())
            continue;
        const QString srName = sr->GetName();

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

    this->onVDISelectionChanged();

    // Resize columns to content
    for (int i = 0; i < this->ui->vdiTable->columnCount(); ++i)
    {
        this->ui->vdiTable->resizeColumnToContents(i);
    }
}

void AttachVirtualDiskDialog::onSRFilterChanged(int index)
{
    Q_UNUSED(index);
    this->populateVDITable();
}

void AttachVirtualDiskDialog::onVDISelectionChanged()
{
    const QString ref = this->getSelectedVDIRef();
    const auto vdi = !ref.isEmpty() && this->m_vm && this->m_vm->GetCache()
        ? this->m_vm->GetCache()->ResolveObject<VDI>(ref) : QSharedPointer<VDI>();
    bool forceReadOnly = false;
    bool eligible = vdi && vdi->CanAttach(SettingsManager::instance().GetShowHiddenObjects(), &forceReadOnly);
    if (eligible)
    {
        const auto sr = vdi->GetSR();
        eligible = this->isSRVisible(sr) && this->srUnavailableReason(sr).isEmpty();
    }
    const bool hvm = this->m_vm && this->m_vm->IsHVM();
    this->ui->modeComboBox->setCurrentIndex(forceReadOnly || (!hvm && this->m_preferReadOnly) ? 1 : 0);
    this->ui->modeComboBox->setEnabled(eligible && !forceReadOnly && !hvm);
    QString reason;
    if (hvm)
        reason = tr("Read-only disk attachment is not supported for HVM virtual machines.");
    else if (forceReadOnly)
        reason = tr("This disk must be attached read-only because it is read-only or already referenced by another attachment.");
    this->ui->modeComboBox->setToolTip(reason);
    this->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(eligible && !(hvm && forceReadOnly));
    this->ui->buttonBox->button(QDialogButtonBox::Ok)->setToolTip(hvm && forceReadOnly ? reason : QString());
}

void AttachVirtualDiskDialog::accept()
{
    // Recheck cached eligibility at confirmation, including read-only restrictions.
    this->onVDISelectionChanged();
    if (this->ui->buttonBox->button(QDialogButtonBox::Ok)->isEnabled())
        QDialog::accept();
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

QString AttachVirtualDiskDialog::getMode() const
{
    return (this->ui->modeComboBox->currentIndex() == 1) ? "RO" : "RW";
}
