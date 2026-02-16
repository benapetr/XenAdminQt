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

#include "gpuconfiguration.h"

#include "xenlib/utils/misc.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/actions/gpu/vgpuconfigurationaction.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/pgpu.h"
#include "xenlib/xen/vgpu.h"
#include "xenlib/xen/vgputype.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

GpuConfiguration::GpuConfiguration(const QList<QSharedPointer<PGPU>>& pGpus, QWidget* parent) : QDialog(parent), m_pGpus(pGpus)
{
    if (!this->m_pGpus.isEmpty() && this->m_pGpus.first())
        this->m_connection = this->m_pGpus.first()->GetConnection();

    this->setWindowTitle(tr("GPU Configuration"));
    this->resize(720, 460);

    QVBoxLayout* root = new QVBoxLayout(this);
    root->addWidget(new QLabel(tr("Select which vGPU types are enabled on the selected physical GPUs."), this));

    this->m_table = new QTableWidget(this);
    this->m_table->setColumnCount(4);
    this->m_table->setHorizontalHeaderLabels(QStringList() << tr("Enabled")
                                                            << tr("Name")
                                                            << tr("vGPUs / GPU")
                                                            << tr("Video RAM"));
    this->m_table->verticalHeader()->setVisible(false);
    this->m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    this->m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    this->m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    this->m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    this->m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    root->addWidget(this->m_table, 1);

    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &GpuConfiguration::onAccepted);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);

    this->populateGrid();
}

void GpuConfiguration::populateGrid()
{
    this->m_table->setRowCount(0);
    this->m_rowStates.clear();

    XenCache* cache = this->m_connection ? this->m_connection->GetCache() : nullptr;
    if (!cache || this->m_pGpus.isEmpty() || !this->m_pGpus.first())
        return;

    const QSharedPointer<PGPU> first = this->m_pGpus.first();
    const QStringList supportedTypeRefs = first->SupportedVGPUTypeRefs();

    for (const QString& typeRef : supportedTypeRefs)
    {
        QSharedPointer<VGPUType> type = cache->ResolveObject<VGPUType>(typeRef);
        if (!type || !type->IsValid())
            continue;

        const bool enabled = first->EnabledVGPUTypeRefs().contains(typeRef);

        bool isInUse = false;
        for (const QSharedPointer<PGPU>& pGpu : this->m_pGpus)
        {
            if (!pGpu || !pGpu->IsValid())
                continue;

            const QList<QSharedPointer<VGPU>> resident = pGpu->GetResidentVGPUs();
            for (const QSharedPointer<VGPU>& vgpu : resident)
            {
                if (vgpu && vgpu->IsValid() && vgpu->TypeRef() == typeRef)
                {
                    isInUse = true;
                    break;
                }
            }

            if (isInUse)
                break;
        }

        const int row = this->m_table->rowCount();
        this->m_table->insertRow(row);

        QTableWidgetItem* check = new QTableWidgetItem();
        check->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
        if (isInUse && enabled)
            check->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        check->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
        this->m_table->setItem(row, 0, check);

        QString displayName = type->IsPassthrough() ? tr("Pass-through") : type->ModelName();
        this->m_table->setItem(row, 1, new QTableWidgetItem(displayName));

        if (type->IsPassthrough())
            this->m_table->setItem(row, 2, new QTableWidgetItem(QString()));
        else
            this->m_table->setItem(row, 2, new QTableWidgetItem(QString::number(type->Capacity())));

        if (type->FramebufferSize() > 0)
            this->m_table->setItem(row, 3, new QTableWidgetItem(Misc::FormatSize(type->FramebufferSize())));
        else
            this->m_table->setItem(row, 3, new QTableWidgetItem(QString()));

        RowState state;
        state.typeRef = typeRef;
        state.originalEnabled = enabled;
        state.isInUse = isInUse;
        this->m_rowStates.insert(row, state);
    }
}

void GpuConfiguration::onAccepted()
{
    if (!this->m_connection)
    {
        this->accept();
        return;
    }

    QMap<QString, QStringList> updatedEnabledByPGpu;
    for (const QSharedPointer<PGPU>& pGpu : this->m_pGpus)
    {
        if (!pGpu || !pGpu->IsValid())
            continue;

        updatedEnabledByPGpu.insert(pGpu->OpaqueRef(), pGpu->EnabledVGPUTypeRefs());
    }

    bool hasChanges = false;
    for (int row = 0; row < this->m_table->rowCount(); ++row)
    {
        if (!this->m_rowStates.contains(row))
            continue;

        QTableWidgetItem* check = this->m_table->item(row, 0);
        if (!check)
            continue;

        const RowState state = this->m_rowStates.value(row);
        const bool checkedNow = check->checkState() == Qt::Checked;
        if (checkedNow == state.originalEnabled)
            continue;

        hasChanges = true;
        for (auto it = updatedEnabledByPGpu.begin(); it != updatedEnabledByPGpu.end(); ++it)
        {
            QStringList refs = it.value();
            if (checkedNow)
            {
                if (!refs.contains(state.typeRef))
                    refs.append(state.typeRef);
            } else
            {
                refs.removeAll(state.typeRef);
            }
            it.value() = refs;
        }
    }

    if (hasChanges)
    {
        VgpuConfigurationAction* action = new VgpuConfigurationAction(updatedEnabledByPGpu, this->m_connection, this);
        action->RunAsync();
    }

    this->accept();
}
