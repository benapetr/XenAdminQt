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


#include "gpueditpage.h"

#include "../dialogs/addvgpudialog.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/actions/gpu/gpuhelpers.h"
#include "xenlib/xen/actions/vm/gpuassignaction.h"
#include "xenlib/xen/gpugroup.h"
#include "xenlib/xen/vgpu.h"
#include "xenlib/xen/vgputype.h"
#include "xenlib/xen/vm.h"

#include <QPointer>
#include <QHeaderView>
#include <QIcon>
#include <QItemSelectionModel>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QDebug>
#include <algorithm>

GpuEditPage::GpuEditPage(QWidget* parent) : IEditPage(parent)
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    this->m_infoLabel = new QLabel(this);
    this->m_infoLabel->setWordWrap(true);
    this->m_infoLabel->setVisible(false);
    root->addWidget(this->m_infoLabel);

    this->m_table = new QTableWidget(this);
    this->m_table->setColumnCount(3);
    this->m_table->setHorizontalHeaderLabels(QStringList() << tr("Device") << tr("GPU Group") << tr("vGPU Type"));
    this->m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    this->m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    this->m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    this->m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    root->addWidget(this->m_table, 1);

    QHBoxLayout* buttons = new QHBoxLayout();
    this->m_addButton = new QPushButton(tr("Add"), this);
    this->m_removeButton = new QPushButton(tr("Remove"), this);
    buttons->addWidget(this->m_addButton);
    buttons->addWidget(this->m_removeButton);
    buttons->addStretch();
    root->addLayout(buttons);

    connect(this->m_addButton, &QPushButton::clicked, this, &GpuEditPage::onAddGpuClicked);
    connect(this->m_removeButton, &QPushButton::clicked, this, &GpuEditPage::onRemoveGpuClicked);
    connect(this->m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this, &GpuEditPage::onSelectionChanged);
}

QString GpuEditPage::GetText() const
{
    return tr("GPU");
}

QString GpuEditPage::GetSubText() const
{
    if (this->m_rows.isEmpty())
        return tr("NONE");

    QStringList labels;
    for (const RowData& row : this->m_rows)
        labels.append(row.displayType);
    return labels.join(", ");
}

QIcon GpuEditPage::GetImage() const
{
    return QIcon(":/icons/cpu_16.png");
}

void GpuEditPage::SetXenObjects(const QString& objectRef, const QString& objectType, const QVariantMap& objectDataBefore, const QVariantMap& objectDataCopy)
{
    Q_UNUSED(objectRef);
    Q_UNUSED(objectType);
    Q_UNUSED(objectDataBefore);
    Q_UNUSED(objectDataCopy);

    this->disconnectCacheSignals();
    this->m_vm = qSharedPointerDynamicCast<VM>(this->m_object);
    this->connectCacheSignals();
    qDebug() << "[GpuEditPage] SetXenObjects:"
             << "objectRef=" << objectRef
             << "objectType=" << objectType
             << "vmRef=" << (this->m_vm ? this->m_vm->OpaqueRef() : QString())
             << "vmValid=" << (this->m_vm && this->m_vm->IsValid());
    this->populateRowsFromVm();
    this->m_waitingForCacheSync = false;
    this->m_originalStateKey = this->normalizedStateKey();
    qDebug() << "[GpuEditPage] SetXenObjects done:"
             << "rows=" << this->m_rows.size()
             << "stateKey=" << this->m_originalStateKey;
    this->updateEnablement();
    emit populated();
}

AsyncOperation* GpuEditPage::SaveSettings()
{
    if (!this->m_vm || !this->HasChanged())
        return nullptr;

    GpuAssignAction* action = new GpuAssignAction(this->m_vm, this->buildVgpuDataForSave(), this);
    this->m_waitingForCacheSync = true;
    qDebug() << "[GpuEditPage] SaveSettings:"
             << "vmRef=" << this->m_vm->OpaqueRef()
             << "rowsToSave=" << this->m_rows.size()
             << "waitingForCacheSync=true";

    QPointer<GpuEditPage> self(this);
    connect(action, &AsyncOperation::completed, this, [self, action]() {
        if (!self)
            return;
        qDebug() << "[GpuEditPage] SaveSettings completed:"
                 << "hasError=" << action->HasError()
                 << "error=" << action->GetErrorMessage();
        if (action->HasError())
            self->m_waitingForCacheSync = false;
    });

    return action;
}

bool GpuEditPage::IsValidToSave() const
{
    return true;
}

void GpuEditPage::ShowLocalValidationMessages()
{
}

void GpuEditPage::HideLocalValidationMessages()
{
}

void GpuEditPage::Cleanup()
{
    this->disconnectCacheSignals();
}

bool GpuEditPage::HasChanged() const
{
    return this->normalizedStateKey() != this->m_originalStateKey;
}

QVariantList GpuEditPage::GetVGpuData() const
{
    return this->buildVgpuDataForSave();
}

void GpuEditPage::onAddGpuClicked()
{
    if (!this->m_vm || !this->m_vm->IsValid())
        return;

    AddVGPUDialog dialog(this->m_vm, this->existingVgpusForDialog(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const GpuTuple tuple = dialog.SelectedTuple();
    if (tuple.gpuGroupRef.isEmpty())
        return;

    RowData row;
    row.opaqueRef.clear();
    row.gpuGroupRef = tuple.gpuGroupRef;
    row.typeRef = tuple.vgpuTypeRefs.isEmpty() ? QString() : tuple.vgpuTypeRefs.first();

    int maxDevice = -1;
    for (const RowData& existing : this->m_rows)
        maxDevice = qMax(maxDevice, existing.device.toInt());
    row.device = QString::number(maxDevice + 1);

    XenCache* cache = this->connection() ? this->connection()->GetCache() : nullptr;
    if (cache)
    {
        QSharedPointer<GPUGroup> group = cache->ResolveObject<GPUGroup>(row.gpuGroupRef);
        QSharedPointer<VGPUType> type = row.typeRef.isEmpty() ? QSharedPointer<VGPUType>() : cache->ResolveObject<VGPUType>(row.typeRef);
        row.displayType = type ? type->DisplayDescription() : (group ? group->Name() : tr("Pass-through"));
    } else
    {
        row.displayType = tr("Pass-through");
    }

    this->m_rows.append(row);
    this->addRow(row);
    this->updateEnablement();
    emit populated();
}

void GpuEditPage::onRemoveGpuClicked()
{
    const int row = this->m_table->currentRow();
    if (row < 0 || row >= this->m_rows.size())
        return;

    this->m_rows.removeAt(row);
    this->m_table->removeRow(row);
    this->updateEnablement();
    emit populated();
}

void GpuEditPage::onSelectionChanged()
{
    this->updateEnablement();
}

void GpuEditPage::clearRows()
{
    qDebug() << "[GpuEditPage] clearRows:" << "oldRows=" << this->m_rows.size();
    this->m_rows.clear();
    this->m_table->setRowCount(0);
}

void GpuEditPage::populateRowsFromVm()
{
    this->clearRows();

    if (!this->m_vm || !this->m_vm->IsValid())
    {
        qDebug() << "[GpuEditPage] populateRowsFromVm skipped: vm invalid";
        return;
    }

    XenCache* cache = this->m_vm->GetCache();
    if (!cache)
    {
        qDebug() << "[GpuEditPage] populateRowsFromVm skipped: cache missing";
        return;
    }

    const QStringList vmVgpuRefs = this->m_vm->VGPURefs();
    qDebug() << "[GpuEditPage] populateRowsFromVm:"
             << "vmRef=" << this->m_vm->OpaqueRef()
             << "vmVgpuRefsCount=" << vmVgpuRefs.size()
             << "vmVgpuRefs=" << vmVgpuRefs;

    for (const QString& vgpuRef : vmVgpuRefs)
    {
        QSharedPointer<VGPU> vgpu = cache->ResolveObject<VGPU>(XenObjectType::VGPU, vgpuRef);
        if (!vgpu || !vgpu->IsValid())
        {
            qDebug() << "[GpuEditPage] populateRowsFromVm unresolved/invalid VGPU ref:" << vgpuRef;
            continue;
        }

        RowData row;
        row.opaqueRef = vgpu->OpaqueRef();
        row.gpuGroupRef = vgpu->GetGPUGroupRef();
        row.typeRef = vgpu->TypeRef();
        row.device = vgpu->GetDevice();

        QSharedPointer<VGPUType> type = row.typeRef.isEmpty() ? QSharedPointer<VGPUType>() : cache->ResolveObject<VGPUType>(row.typeRef);
        QSharedPointer<GPUGroup> group = cache->ResolveObject<GPUGroup>(row.gpuGroupRef);
        row.displayType = type ? type->DisplayDescription() : (group ? group->Name() : tr("Pass-through"));

        this->m_rows.append(row);
        this->addRow(row);
    }

    qDebug() << "[GpuEditPage] populateRowsFromVm done:"
             << "rows=" << this->m_rows.size();
}

void GpuEditPage::addRow(const RowData& row)
{
    const int tableRow = this->m_table->rowCount();
    this->m_table->insertRow(tableRow);

    XenCache* cache = this->connection() ? this->connection()->GetCache() : nullptr;
    QSharedPointer<GPUGroup> group = cache ? cache->ResolveObject<GPUGroup>(row.gpuGroupRef) : QSharedPointer<GPUGroup>();

    this->m_table->setItem(tableRow, 0, new QTableWidgetItem(row.device));
    this->m_table->setItem(tableRow, 1, new QTableWidgetItem(group ? group->Name() : row.gpuGroupRef));
    this->m_table->setItem(tableRow, 2, new QTableWidgetItem(row.displayType));
}

QVariantList GpuEditPage::buildVgpuDataForSave() const
{
    QVariantList result;
    for (const RowData& row : this->m_rows)
    {
        QVariantMap map;
        map.insert("opaque_ref", row.opaqueRef);
        map.insert("GPU_group", row.gpuGroupRef);
        map.insert("type", row.typeRef);
        map.insert("device", row.device);
        result.append(map);
    }
    return result;
}

QList<QSharedPointer<VGPU>> GpuEditPage::existingVgpusForDialog() const
{
    QList<QSharedPointer<VGPU>> result;
    XenCache* cache = this->connection() ? this->connection()->GetCache() : nullptr;
    if (!cache)
        return result;

    for (const RowData& row : this->m_rows)
    {
        if (row.opaqueRef.isEmpty())
            continue;
        QSharedPointer<VGPU> vgpu = cache->ResolveObject<VGPU>(XenObjectType::VGPU, row.opaqueRef);
        if (vgpu && vgpu->IsValid())
            result.append(vgpu);
    }
    return result;
}

QString GpuEditPage::normalizedStateKey() const
{
    QStringList tokens;
    for (const RowData& row : this->m_rows)
        tokens.append(QStringLiteral("%1|%2|%3|%4").arg(row.opaqueRef, row.gpuGroupRef, row.typeRef, row.device));
    std::sort(tokens.begin(), tokens.end());
    return tokens.join(QLatin1Char(';'));
}

void GpuEditPage::updateEnablement()
{
    const bool vmUsable = this->m_vm && this->m_vm->IsValid() && this->m_vm->IsHalted();
    const bool gpuAvailable = vmUsable && this->m_vm->CanHaveGpu() && GpuHelpers::GpusAvailable(this->connection());
    this->m_addButton->setEnabled(gpuAvailable);
    this->m_removeButton->setEnabled(gpuAvailable && this->m_table->currentRow() >= 0);

    if (!vmUsable)
    {
        this->m_infoLabel->setVisible(true);
        this->m_infoLabel->setText(tr("The VM must be halted to change GPU assignments."));
    } else if (!gpuAvailable)
    {
        this->m_infoLabel->setVisible(true);
        this->m_infoLabel->setText(tr("No assignable GPUs are available for this VM."));
    } else
    {
        this->m_infoLabel->setVisible(false);
    }
}

void GpuEditPage::connectCacheSignals()
{
    XenCache* cache = this->m_vm ? this->m_vm->GetCache() : nullptr;
    if (!cache)
        return;

    connect(cache, &XenCache::objectChanged, this, &GpuEditPage::onCacheObjectChanged, Qt::UniqueConnection);
    connect(cache, &XenCache::objectRemoved, this, &GpuEditPage::onCacheObjectRemoved, Qt::UniqueConnection);
    connect(cache, &XenCache::bulkUpdateComplete, this, &GpuEditPage::onCacheBulkUpdateComplete, Qt::UniqueConnection);
    connect(cache, &XenCache::cacheCleared, this, &GpuEditPage::onCacheCleared, Qt::UniqueConnection);
    qDebug() << "[GpuEditPage] cache signals connected for vmRef=" << (this->m_vm ? this->m_vm->OpaqueRef() : QString());
}

void GpuEditPage::disconnectCacheSignals()
{
    XenCache* cache = this->m_vm ? this->m_vm->GetCache() : nullptr;
    if (!cache)
        return;

    disconnect(cache, &XenCache::objectChanged, this, &GpuEditPage::onCacheObjectChanged);
    disconnect(cache, &XenCache::objectRemoved, this, &GpuEditPage::onCacheObjectRemoved);
    disconnect(cache, &XenCache::bulkUpdateComplete, this, &GpuEditPage::onCacheBulkUpdateComplete);
    disconnect(cache, &XenCache::cacheCleared, this, &GpuEditPage::onCacheCleared);
    qDebug() << "[GpuEditPage] cache signals disconnected";
}

void GpuEditPage::onCacheObjectChanged(XenConnection* connection, const QString& type, const QString& ref)
{
    if (!this->m_vm || connection != this->m_vm->GetConnection())
        return;

    qDebug() << "[GpuEditPage] cache objectChanged:"
             << "type=" << type
             << "ref=" << ref
             << "waitingForCacheSync=" << this->m_waitingForCacheSync;
    this->applyCacheRefreshIfNeeded(type, ref);
}

void GpuEditPage::onCacheObjectRemoved(XenConnection* connection, const QString& type, const QString& ref)
{
    this->onCacheObjectChanged(connection, type, ref);
}

void GpuEditPage::onCacheBulkUpdateComplete(const QString& type, int count)
{
    qDebug() << "[GpuEditPage] cache bulkUpdateComplete:"
             << "type=" << type
             << "count=" << count
             << "waitingForCacheSync=" << this->m_waitingForCacheSync;
    this->applyCacheRefreshIfNeeded(type, QString());
}

void GpuEditPage::onCacheCleared()
{
    qDebug() << "[GpuEditPage] cache cleared:"
             << "waitingForCacheSync(before)=" << this->m_waitingForCacheSync;
    if (!this->m_waitingForCacheSync)
        return;

    this->m_waitingForCacheSync = false;
}

void GpuEditPage::applyCacheRefreshIfNeeded(const QString& type, const QString& ref)
{
    if (!this->m_waitingForCacheSync || !this->m_vm)
        return;

    const QString normalizedType = type.toLower();
    const QString vmRef = this->m_vm->OpaqueRef();

    const bool vmChanged = (normalizedType == QLatin1String("vm") && ref == vmRef);
    const bool vgpuChanged = (normalizedType == QLatin1String("vgpu"));
    const bool vmBulk = (normalizedType == QLatin1String("vm") && ref.isEmpty());

    // Ignore early VGPU-only events while waiting for cache sync.
    // VM.vGPUs in cache may still be stale at that point and would wipe UI rows.
    if (vgpuChanged)
    {
        qDebug() << "[GpuEditPage] applyCacheRefreshIfNeeded:"
                 << "type=" << type
                 << "ref=" << ref
                 << "vmRef=" << vmRef
                 << "vmChanged=" << vmChanged
                 << "vgpuChanged=" << vgpuChanged
                 << "vmBulk=" << vmBulk
                 << "-> waiting for VM cache update";
        return;
    }

    if (!vmChanged && !vmBulk)
        return;

    qDebug() << "[GpuEditPage] applyCacheRefreshIfNeeded:"
             << "type=" << type
             << "ref=" << ref
             << "vmRef=" << vmRef
             << "vmChanged=" << vmChanged
             << "vgpuChanged=" << vgpuChanged
             << "vmBulk=" << vmBulk
             << "-> refreshing rows from cache";
    this->populateRowsFromVm();
    this->m_originalStateKey = this->normalizedStateKey();
    this->m_waitingForCacheSync = false;
    qDebug() << "[GpuEditPage] cache refresh complete:"
             << "rows=" << this->m_rows.size()
             << "stateKey=" << this->m_originalStateKey
             << "waitingForCacheSync=false";
    this->updateEnablement();
    emit populated();
}
