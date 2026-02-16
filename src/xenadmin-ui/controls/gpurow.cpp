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


#include "gpurow.h"

#include "gpuconfiguration.h"
#include "gpushinybar.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/actions/gpu/gpuhelpers.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pgpu.h"
#include "xenlib/xen/vgputype.h"
#include "xenlib/xen/xenobject.h"

#include <QCheckBox>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <algorithm>

GpuRow::GpuRow(const QSharedPointer<XenObject>& scopeObject, const QList<QSharedPointer<PGPU>>& pGpus, QWidget* parent) : QWidget(parent), m_scopeObject(scopeObject), m_pGpus(pGpus)
{
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    QFrame* panel = new QFrame(this);
    panel->setFrameShape(QFrame::StyledPanel);

    QVBoxLayout* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(8, 8, 8, 8);
    panelLayout->setSpacing(8);

    QHBoxLayout* header = new QHBoxLayout();
    QLabel* icon = new QLabel(panel);
    icon->setPixmap(QIcon(":/icons/cpu_16.png").pixmap(16, 16));
    header->addWidget(icon, 0, Qt::AlignTop);

    this->m_nameLabel = new QLabel(panel);
    this->m_nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QSharedPointer<PGPU> firstPgpu = this->m_pGpus.isEmpty() ? QSharedPointer<PGPU>() : this->m_pGpus.first();
    this->m_nameLabel->setText(firstPgpu ? firstPgpu->GetName() : tr("Physical GPU"));
    header->addWidget(this->m_nameLabel, 1);

    QLabel* allowedLabel = new QLabel(tr("Allowed vGPU types"), panel);
    header->addWidget(allowedLabel, 0, Qt::AlignRight | Qt::AlignTop);
    panelLayout->addLayout(header);

    QHBoxLayout* content = new QHBoxLayout();
    content->setSpacing(12);

    QWidget* left = new QWidget(panel);
    QVBoxLayout* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(6);

    this->m_barsContainer = new QWidget(left);
    this->m_barsLayout = new QGridLayout(this->m_barsContainer);
    this->m_barsLayout->setContentsMargins(0, 0, 0, 0);
    this->m_barsLayout->setHorizontalSpacing(8);
    this->m_barsLayout->setVerticalSpacing(4);
    leftLayout->addWidget(this->m_barsContainer);

    this->m_multiSelectPanel = new QWidget(left);
    QHBoxLayout* multiLayout = new QHBoxLayout(this->m_multiSelectPanel);
    multiLayout->setContentsMargins(0, 0, 0, 0);
    this->m_selectAllButton = new QPushButton(tr("Select All"), this->m_multiSelectPanel);
    this->m_clearAllButton = new QPushButton(tr("Clear All"), this->m_multiSelectPanel);
    multiLayout->addWidget(this->m_selectAllButton);
    multiLayout->addWidget(this->m_clearAllButton);
    multiLayout->addStretch();
    leftLayout->addWidget(this->m_multiSelectPanel);

    content->addWidget(left, 1);

    QWidget* right = new QWidget(panel);
    QVBoxLayout* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(6);

    this->m_allowedTypesGrid = new QTableWidget(right);
    this->m_allowedTypesGrid->setColumnCount(2);
    this->m_allowedTypesGrid->verticalHeader()->setVisible(false);
    this->m_allowedTypesGrid->horizontalHeader()->setVisible(false);
    this->m_allowedTypesGrid->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    this->m_allowedTypesGrid->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    this->m_allowedTypesGrid->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->m_allowedTypesGrid->setSelectionMode(QAbstractItemView::NoSelection);
    this->m_allowedTypesGrid->setShowGrid(false);
    this->m_allowedTypesGrid->setFocusPolicy(Qt::NoFocus);
    this->m_allowedTypesGrid->setMinimumWidth(260);
    rightLayout->addWidget(this->m_allowedTypesGrid, 1);

    this->m_editButton = new QPushButton(tr("Edit Allowed Types"), right);
    rightLayout->addWidget(this->m_editButton, 0, Qt::AlignLeft);

    content->addWidget(right, 0);
    panelLayout->addLayout(content);

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(panel);

    connect(this->m_selectAllButton, &QPushButton::clicked, this, &GpuRow::onSelectAll);
    connect(this->m_clearAllButton, &QPushButton::clicked, this, &GpuRow::onClearAll);
    connect(this->m_editButton, &QPushButton::clicked, this, &GpuRow::onEditClicked);

    this->m_vgpuCapability = this->m_scopeObject
                             && !GpuHelpers::FeatureForbidden(this->m_scopeObject.data(), &Host::RestrictVgpu)
                             && !this->m_pGpus.isEmpty()
                             && this->m_pGpus.first()
                             && this->m_pGpus.first()->HasVGpu();

    this->RebuildBars();
    if (firstPgpu)
        this->RepopulateAllowedTypes(firstPgpu);

    const bool showMultiSelect = this->m_vgpuCapability && this->m_pGpus.size() > 1;
    this->m_multiSelectPanel->setVisible(showMultiSelect);
    this->m_editButton->setVisible(this->m_vgpuCapability);
    this->m_editButton->setText(this->m_pGpus.size() > 1 ? tr("Edit Allowed Types (Selected GPUs)")
                                                         : tr("Edit Allowed Types"));
    this->onSelectionChanged();
}

void GpuRow::RebuildBars()
{
    while (QLayoutItem* item = this->m_barsLayout->takeAt(0))
    {
        if (QWidget* w = item->widget())
            w->deleteLater();
        delete item;
    }
    this->m_barsByPgpuRef.clear();
    this->m_checkByPgpuRef.clear();

    const bool showHostLabel = this->m_scopeObject
                               && this->m_scopeObject->GetObjectType() == XenObjectType::Pool
                               && this->m_scopeObject->GetConnection()
                               && this->m_scopeObject->GetConnection()->GetCache()
                               && this->m_scopeObject->GetConnection()->GetCache()->GetAll<Host>(XenObjectType::Host).size() > 1;

    QString currentHostRef;
    int row = 0;
    for (const QSharedPointer<PGPU>& pgpu : std::as_const(this->m_pGpus))
    {
        if (!pgpu || !pgpu->IsValid())
            continue;

        QSharedPointer<Host> host = pgpu->GetHost();
        if (showHostLabel && host && host->IsValid() && host->OpaqueRef() != currentHostRef)
        {
            currentHostRef = host->OpaqueRef();
            QLabel* hostLabel = new QLabel(tr("On host: %1").arg(host->GetName()), this->m_barsContainer);
            QFont f = hostLabel->font();
            f.setBold(true);
            hostLabel->setFont(f);
            this->m_barsLayout->addWidget(hostLabel, row, 0, 1, 2);
            ++row;
        }

        QCheckBox* cb = nullptr;
        if (this->m_pGpus.size() > 1 && this->m_vgpuCapability)
        {
            cb = new QCheckBox(this->m_barsContainer);
            cb->setChecked(true);
            connect(cb, &QCheckBox::toggled, this, &GpuRow::onSelectionChanged);
            this->m_barsLayout->addWidget(cb, row, 0, Qt::AlignTop);
            this->m_checkByPgpuRef.insert(pgpu->OpaqueRef(), cb);
        }

        GpuShinyBar* bar = new GpuShinyBar(this->m_barsContainer);
        bar->Initialize(pgpu);
        this->m_barsLayout->addWidget(bar, row, 1);
        this->m_barsByPgpuRef.insert(pgpu->OpaqueRef(), bar);
        ++row;
        Q_UNUSED(cb);
    }

    this->m_barsLayout->setColumnStretch(1, 1);
}

void GpuRow::RepopulateAllowedTypes(const QSharedPointer<PGPU>& pgpu)
{
    this->m_allowedTypesGrid->setRowCount(0);
    if (!pgpu || !pgpu->IsValid() || !pgpu->GetConnection() || !pgpu->GetConnection()->GetCache())
        return;

    XenCache* cache = pgpu->GetConnection()->GetCache();
    QList<QSharedPointer<VGPUType>> types;
    for (const QString& typeRef : pgpu->SupportedVGPUTypeRefs())
    {
        QSharedPointer<VGPUType> type = cache->ResolveObject<VGPUType>(typeRef);
        if (type && type->IsValid())
            types.append(type);
    }

    std::sort(types.begin(), types.end(), [](const QSharedPointer<VGPUType>& a, const QSharedPointer<VGPUType>& b) {
        if (!a)
            return false;
        if (!b)
            return true;
        return QString::compare(a->DisplayName(), b->DisplayName(), Qt::CaseInsensitive) < 0;
    });

    for (const QSharedPointer<VGPUType>& type : std::as_const(types))
    {
        const int row = this->m_allowedTypesGrid->rowCount();
        this->m_allowedTypesGrid->insertRow(row);

        const bool enabled = pgpu->EnabledVGPUTypeRefs().contains(type->OpaqueRef());
        QTableWidgetItem* iconItem = new QTableWidgetItem();
        iconItem->setIcon(QIcon(enabled ? QStringLiteral(":/icons/tick_16.png")
                                        : QStringLiteral(":/icons/error_16.png")));
        this->m_allowedTypesGrid->setItem(row, 0, iconItem);

        QTableWidgetItem* nameItem = new QTableWidgetItem(type->DisplayName());
        this->m_allowedTypesGrid->setItem(row, 1, nameItem);
    }

    this->m_allowedTypesGrid->resizeRowsToContents();
}

void GpuRow::RefreshGpu(const QSharedPointer<PGPU>& pgpu)
{
    if (!pgpu)
        return;
    GpuShinyBar* bar = this->m_barsByPgpuRef.value(pgpu->OpaqueRef(), nullptr);
    if (bar)
    {
        bar->Initialize(pgpu);
        bar->update();
    }
}

void GpuRow::onSelectAll()
{
    for (QCheckBox* cb : std::as_const(this->m_checkByPgpuRef))
    {
        if (cb)
            cb->setChecked(true);
    }
}

void GpuRow::onClearAll()
{
    for (QCheckBox* cb : std::as_const(this->m_checkByPgpuRef))
    {
        if (cb)
            cb->setChecked(false);
    }
}

void GpuRow::onSelectionChanged()
{
    if (this->m_checkByPgpuRef.isEmpty())
        return;

    bool anyChecked = false;
    bool anyUnchecked = false;
    for (QCheckBox* cb : std::as_const(this->m_checkByPgpuRef))
    {
        if (!cb)
            continue;
        anyChecked = anyChecked || cb->isChecked();
        anyUnchecked = anyUnchecked || !cb->isChecked();
    }

    this->m_editButton->setEnabled(anyChecked);
    this->m_clearAllButton->setEnabled(anyChecked);
    this->m_selectAllButton->setEnabled(anyUnchecked);
}

void GpuRow::onEditClicked()
{
    if (this->m_pGpus.isEmpty())
        return;

    QList<QSharedPointer<PGPU>> selected;
    if (this->m_checkByPgpuRef.isEmpty())
    {
        selected = this->m_pGpus;
    } else
    {
        for (const QSharedPointer<PGPU>& pgpu : std::as_const(this->m_pGpus))
        {
            QCheckBox* cb = this->m_checkByPgpuRef.value(pgpu->OpaqueRef(), nullptr);
            if (cb && cb->isChecked())
                selected.append(pgpu);
        }
    }

    if (selected.isEmpty())
        return;

    GpuConfiguration dialog(selected, this);
    dialog.exec();
}
