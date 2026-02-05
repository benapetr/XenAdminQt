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

#include "srpicker.h"
#include "ui_srpicker.h"
#include "xencache.h"
#include "xen/network/connection.h"
#include "xen/actions/sr/srrefreshaction.h"
#include "xen/pbd.h"
#include "xen/pool.h"
#include "xen/sr.h"
#include "xen/vdi.h"
#include <QHeaderView>
#include <QTableWidgetItem>

SrPicker::SrPicker(QWidget* parent) : QWidget(parent), ui(new Ui::SrPicker), m_connection(nullptr), m_usage(VM), m_runningScans(0)
{
    this->ui->setupUi(this);

    // Configure table
    this->ui->srTable->horizontalHeader()->setStretchLastSection(true);
    this->ui->srTable->setSelectionMode(QAbstractItemView::SingleSelection);
    this->ui->srTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    // Connect signals
    connect(this->ui->srTable, &QTableWidget::itemSelectionChanged, this, &SrPicker::onSelectionChanged);
    connect(this->ui->srTable, &QTableWidget::cellDoubleClicked, this, &SrPicker::onItemDoubleClicked);
}

SrPicker::~SrPicker()
{
    // Cleanup pending actions
    for (SrRefreshAction* action : this->m_refreshQueue)
    {
        if (action)
        {
            disconnect(action, nullptr, this, nullptr);
            // Actions are parented and will be cleaned up automatically
        }
    }
    this->m_refreshQueue.clear();

    delete this->ui;
}

void SrPicker::Populate(SRPickerType usage, XenConnection* connection, const QString& affinityRef, const QString& preselectedSRRef, const QStringList& existingVDIRefs)
{
    // Cleanup existing state
    for (SrRefreshAction* action : this->m_refreshQueue)
    {
        if (action)
        {
            disconnect(action, &SrRefreshAction::completed, this, &SrPicker::onSrRefreshCompleted);
        }
    }
    this->m_refreshQueue.clear();
    this->m_runningScans = 0;

    this->m_connection = connection;
    this->m_usage = usage;
    this->m_affinityRef = affinityRef;
    if (!this->m_affinityRef.isEmpty() && XenObject::ValueIsNULL(this->m_affinityRef))
        this->m_affinityRef.clear();
    this->m_preselectedSRRef = preselectedSRRef;
    this->m_existingVDIRefs = existingVDIRefs;
    this->m_srItems.clear();

    if (!this->m_connection)
        return;

    // Get pool default SR
    this->m_defaultSRRef.clear();
    QSharedPointer<Pool> pool = this->m_connection->GetCache()->GetPoolOfOne();
    if (pool)
        this->m_defaultSRRef = pool->GetDefaultSRRef();

    // Listen for cache updates
    XenCache* cache = this->m_connection->GetCache();
    connect(cache, &XenCache::objectChanged, this, &SrPicker::onCacheUpdated, Qt::UniqueConnection);
    connect(cache, &XenCache::objectRemoved, this, &SrPicker::onCacheRemoved, Qt::UniqueConnection);

    // Populate SR list
    this->populateSRList();
}

void SrPicker::populateSRList()
{
    this->ui->srTable->setRowCount(0);
    this->m_srItems.clear();

    if (!this->m_connection)
        return;

    QList<QSharedPointer<SR>> allSRs = this->m_connection->GetCache()->GetAll<SR>();
    for (const QSharedPointer<SR>& sr : allSRs)
    {
        if (sr && sr->IsValid() && this->isValidSR(sr))
        {
            this->addSR(sr);
        }
    }

    // Auto-select default or preselected SR
    this->selectDefaultSR();
    this->onCanBeScannedChanged();
}

void SrPicker::addSR(const QSharedPointer<SR>& sr)
{
    if (!sr)
        return;

    SRItem item;
    item.ref = sr->OpaqueRef();
    item.name = sr->GetName();
    item.description = sr->GetDescription();
    item.type = sr->GetType();
    item.physicalSize = sr->PhysicalSize();
    item.freeSpace = this->calculateFreeSpace(sr);
    item.shared = sr->IsShared();
    item.scanning = false;

    QString disableReason;
    item.enabled = this->canBeEnabled(sr, disableReason);
    item.disableReason = disableReason;

    this->m_srItems.append(item);

    // Add to table
    int row = this->ui->srTable->rowCount();
    this->ui->srTable->insertRow(row);

    // Column 0: Name
    QTableWidgetItem* nameItem = new QTableWidgetItem(item.name);
    nameItem->setData(Qt::UserRole, sr->OpaqueRef());
    if (!item.enabled)
    {
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEnabled);
    }
    this->ui->srTable->setItem(row, 0, nameItem);

    // Column 1: Description/Status
    QString statusText = item.enabled ? 
        QString("Free: %1 of %2").arg(this->formatSize(item.freeSpace)).arg(this->formatSize(item.physicalSize)) :
        item.disableReason;

    QTableWidgetItem* statusItem = new QTableWidgetItem(statusText);
    if (!item.enabled)
    {
        statusItem->setFlags(statusItem->flags() & ~Qt::ItemIsEnabled);
    }
    this->ui->srTable->setItem(row, 1, statusItem);
}

void SrPicker::updateSRItem(const QString& srRef)
{
    // Find item
    int itemIndex = -1;
    for (int i = 0; i < this->m_srItems.count(); ++i)
    {
        if (this->m_srItems[i].ref == srRef)
        {
            itemIndex = i;
            break;
        }
    }

    if (itemIndex == -1)
        return;

    QSharedPointer<SR> sr = this->m_connection->GetCache()->ResolveObject<SR>(srRef);
    if (!sr)
        return;

    SRItem& item = this->m_srItems[itemIndex];
    item.name = sr->GetName();
    item.description = sr->GetDescription();
    item.physicalSize = sr->PhysicalSize();
    item.freeSpace = this->calculateFreeSpace(sr);
    item.shared = sr->IsShared();

    QString disableReason;
    item.enabled = this->canBeEnabled(sr, disableReason);
    item.disableReason = disableReason;

    // Update table row
    for (int row = 0; row < this->ui->srTable->rowCount(); ++row)
    {
        QTableWidgetItem* nameItem = this->ui->srTable->item(row, 0);
        if (nameItem && nameItem->data(Qt::UserRole).toString() == srRef)
        {
            // Update name
            nameItem->setText(item.name);
            if (!item.enabled)
                nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEnabled);
            else
                nameItem->setFlags(nameItem->flags() | Qt::ItemIsEnabled | Qt::ItemIsSelectable);

            // Update status
            QString statusText = item.scanning ? "Scanning..." :
                                 (item.enabled ? 
                                  QString("Free: %1 of %2").arg(this->formatSize(item.freeSpace)).arg(this->formatSize(item.physicalSize)) :
                                  item.disableReason);

            QTableWidgetItem* statusItem = this->ui->srTable->item(row, 1);
            if (statusItem)
            {
                statusItem->setText(statusText);
                if (!item.enabled)
                    statusItem->setFlags(statusItem->flags() & ~Qt::ItemIsEnabled);
                else
                    statusItem->setFlags(statusItem->flags() | Qt::ItemIsEnabled);
            }

            break;
        }
    }
}

void SrPicker::removeSR(const QString& srRef)
{
    // Remove from item list
    for (int i = 0; i < this->m_srItems.count(); ++i)
    {
        if (this->m_srItems[i].ref == srRef)
        {
            this->m_srItems.removeAt(i);
            break;
        }
    }

    // Remove from table
    for (int row = 0; row < this->ui->srTable->rowCount(); ++row)
    {
        QTableWidgetItem* item = this->ui->srTable->item(row, 0);
        if (item && item->data(Qt::UserRole).toString() == srRef)
        {
            this->ui->srTable->removeRow(row);
            break;
        }
    }
}

void SrPicker::ScanSRs()
{
    if (!this->m_connection)
        return;

    // C# ScanSRs() implementation: iterate items, create SrRefreshAction for each non-scanning SR
    for (SRItem& item : this->m_srItems)
    {
        if (item.scanning)
            continue;

        QSharedPointer<SR> sr = this->m_connection->GetCache()->ResolveObject<SR>(item.ref);
        if (!sr || this->isDetached(sr))
            continue;

        bool alreadyQueued = false;
        for (const SrRefreshAction* action : this->m_refreshQueue)
        {
            if (action && action->srRef() == item.ref)
            {
                alreadyQueued = true;
                break;
            }
        }
        if (alreadyQueued)
            continue;

        // Mark as scanning
        item.scanning = true;
        this->updateSRItem(item.ref);

        // Create SrRefreshAction
        SrRefreshAction* action = new SrRefreshAction(this->m_connection, item.ref, this);
        connect(action, &SrRefreshAction::completed, this, &SrPicker::onSrRefreshCompleted);

        this->m_refreshQueue.append(action);

        // Start action if we have capacity (C# runs MAX_SCANS_PER_CONNECTION in parallel)
        if (this->m_runningScans < MAX_SCANS_PER_CONNECTION)
        {
            this->m_runningScans++;
            action->RunAsync();
        }
    }

    this->onCanBeScannedChanged();
}

QString SrPicker::GetSelectedSR() const
{
    QList<QTableWidgetItem*> selectedItems = this->ui->srTable->selectedItems();
    if (selectedItems.isEmpty())
        return QString();

    QTableWidgetItem* firstItem = this->ui->srTable->item(selectedItems.first()->row(), 0);
    if (!firstItem)
        return QString();

    return firstItem->data(Qt::UserRole).toString();
}

bool SrPicker::CanBeScanned() const
{
    if (!this->m_connection)
        return false;

    for (const SRItem& item : this->m_srItems)
    {
        if (!item.scanning)
        {
            QSharedPointer<SR> sr = this->m_connection->GetCache()->ResolveObject<SR>(item.ref);
            if (sr && !this->isDetached(sr))
                return true;
        }
    }

    return false;
}

void SrPicker::onSelectionChanged()
{
    emit this->selectedIndexChanged();
}

void SrPicker::onItemDoubleClicked(int row, int column)
{
    Q_UNUSED(row);
    Q_UNUSED(column);
    emit this->doubleClickOnRow();
}

void SrPicker::onCacheUpdated(XenConnection* connection, const QString& type, const QString& ref)
{
    if (!this->m_connection || this->m_connection != connection)
        return;

    if (type == "sr")
    {
        QSharedPointer<SR> sr = this->m_connection->GetCache()->ResolveObject<SR>(ref);

        // Check if this SR is already in our list
        bool found = false;
        for (const SRItem& item : this->m_srItems)
        {
            if (item.ref == ref)
            {
                found = true;
                break;
            }
        }

        if (!sr || !sr->IsValid())
        {
            // SR was deleted - remove it if we have it
            if (found)
            {
                this->removeSR(ref);
                this->onCanBeScannedChanged();
            }
        }
        else if (found)
        {
            // SR exists and we have it - update it
            this->updateSRItem(ref);
        }
        else
        {
            // SR is new and valid - add it
            if (this->isValidSR(sr))
            {
                this->addSR(sr);
                this->selectDefaultSR(); // Re-check default selection in case this is the default SR
                this->onCanBeScannedChanged();
            }
        }
        return;
    }

    if (type == "pbd")
    {
        QSharedPointer<PBD> pbd = this->m_connection->GetCache()->ResolveObject<PBD>(ref);
        QString srRef = pbd ? pbd->GetSRRef() : QString();
        if (!srRef.isEmpty())
        {
            this->updateSRItem(srRef);
            this->onCanBeScannedChanged();
        }
        return;
    }

    if (type == "pool")
    {
        QSharedPointer<Pool> pool = this->m_connection->GetCache()->ResolveObject<Pool>(ref);
        if (pool)
        {
            this->m_defaultSRRef = pool->GetDefaultSRRef();
            this->selectDefaultSR();
        }
        return;
    }
}

void SrPicker::onCacheRemoved(XenConnection* connection, const QString& type, const QString& ref)
{
    if (!this->m_connection || this->m_connection != connection || type != "sr")
        return;

    this->removeSR(ref);
    this->onCanBeScannedChanged();
}

void SrPicker::onSrRefreshCompleted()
{
    SrRefreshAction* action = qobject_cast<SrRefreshAction*>(sender());
    if (!action)
        return;

    // Remove from queue and decrement running count
    this->m_refreshQueue.removeOne(action);
    this->m_runningScans--;

    // Start next queued scan if available (C# SrRefreshAction_Completed logic)
    this->startNextScan();

    // Find the SR that was scanned and mark as no longer scanning
    QString scannedSRRef = action->srRef();
    for (SRItem& item : this->m_srItems)
    {
        if (item.ref == scannedSRRef)
        {
            item.scanning = false;
            this->updateSRItem(item.ref);
            break;
        }
    }

    // Auto-select if needed (C# SrRefreshAction_Completed logic)
    if (!scannedSRRef.isEmpty())
    {
        if (!this->m_preselectedSRRef.isEmpty() && scannedSRRef == this->m_preselectedSRRef)
        {
            // Select the preselected SR
            for (int row = 0; row < this->ui->srTable->rowCount(); ++row)
            {
                QTableWidgetItem* item = this->ui->srTable->item(row, 0);
                if (item && item->data(Qt::UserRole).toString() == scannedSRRef)
                {
                    this->ui->srTable->selectRow(row);
                    break;
                }
            }
        } else if (!this->m_defaultSRRef.isEmpty() && scannedSRRef == this->m_defaultSRRef)
        {
            // Select the default SR
            for (int row = 0; row < this->ui->srTable->rowCount(); ++row)
            {
                QTableWidgetItem* item = this->ui->srTable->item(row, 0);
                if (item && item->data(Qt::UserRole).toString() == scannedSRRef)
                {
                    this->ui->srTable->selectRow(row);
                    break;
                }
            }
        } else if (this->ui->srTable->selectedItems().isEmpty())
        {
            // Select first enabled SR if nothing is selected
            this->selectDefaultSR();
        }
    }

    this->onCanBeScannedChanged();

    // Action will be deleted by its parent
}

void SrPicker::startNextScan()
{
    // C# SrRefreshAction_Completed logic: start next queued scan if we have capacity
    if (this->m_runningScans < MAX_SCANS_PER_CONNECTION)
    {
        for (SrRefreshAction* action : this->m_refreshQueue)
        {
            if (!action->IsRunning() && !action->IsCompleted())
            {
                this->m_runningScans++;
                action->RunAsync();
                break;
            }
        }
    }
}

bool SrPicker::isValidSR(const QSharedPointer<SR>& sr) const
{
    // Don't show ISO SRs
    if (!sr || sr->ContentType() == "iso")
        return false;

    // Basic filtering - individual items handle enable/disable logic
    return true;
}

bool SrPicker::canBeEnabled(const QSharedPointer<SR>& sr, QString& reason) const
{
    // C# SrPickerItem::CanBeEnabled() logic - varies by picker type
    if (!sr)
    {
        reason = "SR is unavailable";
        return false;
    }
    const QString srRef = sr->OpaqueRef();

    switch (this->m_usage)
    {
        case Move:
            // SrPickerMoveItem: !IsDetached && !IsCurrentLocation && SupportsVdiCreate && CanFitDisks
            if (this->isDetached(sr))
            {
                reason = "SR is detached";
                return false;
            }
            if (this->isCurrentLocation(srRef))
            {
                reason = "Current location";
                return false;
            }
            if (!this->supportsVdiCreate(sr))
            {
                reason = "Storage is read-only";
                return false;
            }
            if (!this->canFitDisks(sr))
            {
                reason = "Insufficient free space";
                return false;
            }
            return true;

        case Migrate:
            // SrPickerMigrateItem: more complex logic with local SR checks
            if (this->isCurrentLocation(srRef))
            {
                reason = "Current location";
                return false;
            }
            if (!this->supportsStorageMigration(sr))
            {
                reason = "Unsupported SR type";
                return false;
            }
            if (!this->supportsVdiCreate(sr))
            {
                reason = "Storage is read-only";
                return false;
            }
            if (this->isDetached(sr))
            {
                reason = "SR is detached";
                return false;
            }
            if (!this->canFitDisks(sr))
            {
                reason = "Insufficient free space";
                return false;
            }
            return true;

        case Copy:
            // SrPickerCopyItem: !IsDetached && SupportsVdiCreate && CanFitDisks
            if (this->isDetached(sr))
            {
                reason = "SR is detached";
                return false;
            }
            if (!this->supportsVdiCreate(sr))
            {
                reason = "Storage is read-only";
                return false;
            }
            if (!this->canFitDisks(sr))
            {
                reason = "Insufficient free space";
                return false;
            }
            return true;

        case InstallFromTemplate:
            // SrPickerInstallFromTemplateItem: SupportsVdiCreate && !IsDetached && CanFitDisks
            if (!this->supportsVdiCreate(sr))
            {
                reason = "Storage is read-only";
                return false;
            }
            if (this->isDetached(sr))
            {
                reason = "SR is detached";
                return false;
            }
            if (!this->canFitDisks(sr))
            {
                reason = "Insufficient free space";
                return false;
            }
            return true;

        case VM:
        case LunPerVDI:
            // SrPickerVmItem: CanBeSeenFromAffinity && SupportsVdiCreate && !IsBroken && CanFitDisks
            if (!this->canBeSeenFromAffinity(sr))
            {
                reason = "SR cannot be seen from affinity host";
                return false;
            }
            if (!this->supportsVdiCreate(sr))
            {
                reason = "Storage is read-only";
                return false;
            }
            if (this->isBroken(sr))
            {
                reason = "SR is broken";
                return false;
            }
            if (!this->canFitDisks(sr))
            {
                reason = "Insufficient free space";
                return false;
            }
            return true;
    }

    return true;
}

qint64 SrPicker::calculateFreeSpace(const QSharedPointer<SR>& sr) const
{
    if (!sr)
        return 0;
    qint64 physicalSize = sr->PhysicalSize();
    qint64 utilisation = sr->PhysicalUtilisation();
    return physicalSize - utilisation;
}

QString SrPicker::formatSize(qint64 bytes) const
{
    if (bytes < 0)
        return "Unknown";

    const qint64 KB = 1024;
    const qint64 MB = 1024 * KB;
    const qint64 GB = 1024 * MB;
    const qint64 TB = 1024 * GB;

    if (bytes >= TB)
        return QString("%1 TB").arg(bytes / (double) TB, 0, 'f', 2);
    else if (bytes >= GB)
        return QString("%1 GB").arg(bytes / (double) GB, 0, 'f', 2);
    else if (bytes >= MB)
        return QString("%1 MB").arg(bytes / (double) MB, 0, 'f', 2);
    else if (bytes >= KB)
        return QString("%1 KB").arg(bytes / (double) KB, 0, 'f', 2);
    else
        return QString("%1 bytes").arg(bytes);
}

void SrPicker::selectDefaultSR()
{
    if (this->ui->srTable->rowCount() == 0)
        return;

    // Priority 1: Preselected SR (if specified)
    if (!this->m_preselectedSRRef.isEmpty())
    {
        for (int row = 0; row < this->ui->srTable->rowCount(); ++row)
        {
            QTableWidgetItem* item = this->ui->srTable->item(row, 0);
            if (item && item->data(Qt::UserRole).toString() == this->m_preselectedSRRef)
            {
                // Check if enabled
                if ((item->flags() & Qt::ItemIsEnabled) && !this->m_srItems[row].scanning)
                {
                    this->ui->srTable->selectRow(row);
                    return;
                }
            }
        }
    }

    // Priority 2: Pool default SR
    if (!this->m_defaultSRRef.isEmpty())
    {
        for (int row = 0; row < this->ui->srTable->rowCount(); ++row)
        {
            QTableWidgetItem* item = this->ui->srTable->item(row, 0);
            if (item && item->data(Qt::UserRole).toString() == this->m_defaultSRRef)
            {
                // Check if enabled
                if ((item->flags() & Qt::ItemIsEnabled) && !this->m_srItems[row].scanning)
                {
                    this->ui->srTable->selectRow(row);
                    return;
                }
            }
        }
    }

    // Priority 3: First enabled, non-scanning SR
    for (int row = 0; row < this->ui->srTable->rowCount(); ++row)
    {
        QTableWidgetItem* item = this->ui->srTable->item(row, 0);
        if (item && (item->flags() & Qt::ItemIsEnabled) && !this->m_srItems[row].scanning)
        {
            this->ui->srTable->selectRow(row);
            return;
        }
    }
}

void SrPicker::onCanBeScannedChanged()
{
    emit this->canBeScannedChanged();
}

// Validation helper methods (C# SrPickerItem logic)

bool SrPicker::isCurrentLocation(const QString& srRef) const
{
    if (this->m_existingVDIRefs.isEmpty())
        return false;

    // Check if all existing VDIs are in this SR
    for (const QString& vdiRef : this->m_existingVDIRefs)
    {
        QSharedPointer<VDI> vdi = this->m_connection->GetCache()->ResolveObject<VDI>(vdiRef);
        QSharedPointer<SR> vdiSr = vdi ? vdi->GetSR() : QSharedPointer<SR>();
        if (!vdiSr || vdiSr->OpaqueRef() != srRef)
            return false;
    }

    return true;
}

bool SrPicker::isBroken(const QSharedPointer<SR>& sr) const
{
    // C# SR.IsBroken() checks if SR is unavailable/broken
    // For now, check if all PBDs are detached
    if (!sr)
        return true;

    const QList<QSharedPointer<PBD>> pbds = sr->GetPBDs();
    if (pbds.isEmpty())
        return true;

    // If at least one PBD is attached, SR is not broken
    for (const QSharedPointer<PBD>& pbd : pbds)
    {
        if (pbd && pbd->IsCurrentlyAttached())
            return false;
    }

    return true;
}

bool SrPicker::isDetached(const QSharedPointer<SR>& sr) const
{
    // C# SR.IsDetached() checks if SR is detached
    if (!sr)
        return true;

    const QList<QSharedPointer<PBD>> pbds = sr->GetPBDs();
    if (pbds.isEmpty())
        return true;

    // If at least one PBD is attached, SR is not detached
    for (const QSharedPointer<PBD>& pbd : pbds)
    {
        if (pbd && pbd->IsCurrentlyAttached())
            return false;
    }

    return true;
}

bool SrPicker::supportsVdiCreate(const QSharedPointer<SR>& sr) const
{
    // Check allowed_operations for "vdi_create"
    return sr && sr->AllowedOperations().contains("vdi_create");
}

bool SrPicker::supportsStorageMigration(const QSharedPointer<SR>& sr) const
{
    return sr ? sr->SupportsStorageMigration() : false;
}

bool SrPicker::canBeSeenFromAffinity(const QSharedPointer<SR>& sr) const
{
    if (this->m_affinityRef.isEmpty())
    {
        // No affinity - SR must be shared
        return sr && sr->IsShared();
    }

    // Check if SR has a PBD on the affinity host
    if (!sr)
        return false;

    const QList<QSharedPointer<PBD>> pbds = sr->GetPBDs();
    for (const QSharedPointer<PBD>& pbd : pbds)
    {
        if (!pbd)
            continue;
        QString hostRef = pbd->GetHostRef();

        if (hostRef == this->m_affinityRef)
        {
            // Found matching PBD - check if attached
            return pbd->IsCurrentlyAttached();
        }
    }

    return false;
}

bool SrPicker::canFitDisks(const QSharedPointer<SR>& sr) const
{
    if (this->m_existingVDIRefs.isEmpty())
        return true;

    qint64 requiredSpace = 0;
    for (const QString& vdiRef : this->m_existingVDIRefs)
    {
        QSharedPointer<VDI> vdi = this->m_connection->GetCache()->ResolveObject<VDI>(vdiRef);
        if (vdi)
            requiredSpace += vdi->VirtualSize();
    }

    qint64 freeSpace = this->calculateFreeSpace(sr);
    return freeSpace >= requiredSpace;
}
