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

#include "addvgpudialog.h"

#include "xenlib/xencache.h"
#include "xenlib/xen/actions/gpu/gpuhelpers.h"
#include "xenlib/xen/gpugroup.h"
#include "xenlib/xen/vgpu.h"
#include "xenlib/xen/vgputype.h"
#include "xenlib/xen/vm.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QVBoxLayout>

AddVGPUDialog::AddVGPUDialog(QSharedPointer<VM> vm,
                             const QList<QSharedPointer<VGPU>>& existingVGpus,
                             QWidget* parent)
    : QDialog(parent),
      m_vm(vm),
      m_existingVGpus(existingVGpus)
{
    this->setWindowTitle(tr("Add vGPU"));
    this->resize(520, 150);

    QVBoxLayout* root = new QVBoxLayout(this);
    root->addWidget(new QLabel(tr("Select the GPU type to add:"), this));

    this->m_combo = new VgpuComboBox(this);
    root->addWidget(this->m_combo);
    connect(this->m_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AddVGPUDialog::onComboSelectionChanged);

    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    this->m_addButton = box->button(QDialogButtonBox::Ok);
    this->m_addButton->setText(tr("Add"));
    this->m_addButton->setEnabled(false);
    connect(box, &QDialogButtonBox::accepted, this, &AddVGPUDialog::onAccepted);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);

    this->populateComboBox();
}

GpuTuple AddVGPUDialog::SelectedTuple() const
{
    return this->m_selectedTuple;
}

void AddVGPUDialog::onComboSelectionChanged()
{
    if (!this->m_addButton)
        return;

    const GpuTuple tuple = this->m_combo->CurrentTuple();
    this->m_addButton->setEnabled(tuple.enabled && !tuple.isGpuHeaderItem);
}

void AddVGPUDialog::onAccepted()
{
    const GpuTuple tuple = this->m_combo->CurrentTuple();
    if (tuple.vgpuTypeRefs.isEmpty())
        this->m_selectedTuple = GpuTuple();
    else
        this->m_selectedTuple = tuple;

    this->accept();
}

bool AddVGPUDialog::compareVGpuTypeForDisplay(const QSharedPointer<VGPUType>& left,
                                              const QSharedPointer<VGPUType>& right)
{
    if (!left)
        return false;
    if (!right)
        return true;

    const bool leftPassthrough = left->IsPassthrough();
    const bool rightPassthrough = right->IsPassthrough();
    if (leftPassthrough != rightPassthrough)
        return leftPassthrough;

    const qint64 leftCapacity = left->Capacity();
    const qint64 rightCapacity = right->Capacity();
    if (leftCapacity != rightCapacity)
        return leftCapacity < rightCapacity;

    const qint64 leftResolution = left->MaxResolutionX() * left->MaxResolutionY();
    const qint64 rightResolution = right->MaxResolutionX() * right->MaxResolutionY();
    if (leftResolution != rightResolution)
        return leftResolution > rightResolution;

    return QString::compare(left->ModelName(), right->ModelName(), Qt::CaseInsensitive) < 0;
}

void AddVGPUDialog::populateComboBox()
{
    this->m_combo->ClearTuples();

    XenCache* cache = this->m_vm ? this->m_vm->GetCache() : nullptr;
    if (!cache)
    {
        this->m_combo->setEnabled(false);
        return;
    }

    QList<QSharedPointer<GPUGroup>> groups = cache->GetAll<GPUGroup>(XenObjectType::GPUGroup);
    groups.erase(std::remove_if(groups.begin(), groups.end(), [](const QSharedPointer<GPUGroup>& group) {
                     return !group || !group->IsValid() || group->GetPGPURefs().isEmpty() || group->SupportedVGPUTypeRefs().isEmpty();
                 }),
                 groups.end());

    std::sort(groups.begin(), groups.end(), [](const QSharedPointer<GPUGroup>& left,
                                               const QSharedPointer<GPUGroup>& right) {
        const QString ln = left ? left->Name() : QString();
        const QString rn = right ? right->Name() : QString();
        return QString::compare(ln, rn, Qt::CaseInsensitive) < 0;
    });

    const bool vgpuRestricted = GpuHelpers::FeatureForbidden(this->m_vm.data(), &Host::RestrictVgpu)
                                || !this->m_vm->CanHaveVGpu();

    for (const QSharedPointer<GPUGroup>& group : groups)
    {
        if (!group)
            continue;

        if (vgpuRestricted)
        {
            if (group->HasPassthrough())
            {
                GpuTuple tuple;
                tuple.gpuGroupRef = group->OpaqueRef();
                tuple.displayName = group->Name();
                this->m_combo->AddTuple(tuple);
            }
            continue;
        }

        const QStringList supportedTypeRefsList = group->SupportedVGPUTypeRefs();
        QSet<QString> commonTypeRefs(supportedTypeRefsList.begin(),
                                     supportedTypeRefsList.end());

        for (const QSharedPointer<VGPU>& existingVGpu : this->m_existingVGpus)
        {
            if (!existingVGpu || !existingVGpu->IsValid())
                continue;

            QSharedPointer<VGPUType> existingType = cache->ResolveObject<VGPUType>(existingVGpu->TypeRef());
            if (!existingType || !existingType->IsValid())
                continue;

            const QStringList compatibleTypeRefsList = existingType->CompatibleTypesInVmRefs();
            const QSet<QString> compatibleRefs(compatibleTypeRefsList.begin(),
                                               compatibleTypeRefsList.end());
            commonTypeRefs = commonTypeRefs.intersect(compatibleRefs);
        }

        QList<QSharedPointer<VGPUType>> commonTypes;
        for (const QString& typeRef : commonTypeRefs)
        {
            QSharedPointer<VGPUType> type = cache->ResolveObject<VGPUType>(typeRef);
            if (type && type->IsValid())
                commonTypes.append(type);
        }

        std::sort(commonTypes.begin(), commonTypes.end(), &AddVGPUDialog::compareVGpuTypeForDisplay);

        if (group->HasVGpu() && !commonTypes.isEmpty())
        {
            GpuTuple header;
            header.gpuGroupRef = group->OpaqueRef();
            header.vgpuTypeRefs = group->SupportedVGPUTypeRefs();
            header.isGpuHeaderItem = true;
            header.enabled = false;
            header.displayName = group->Name();
            this->m_combo->AddTuple(header);
        }

        const QStringList enabledTypeRefsList = group->EnabledVGPUTypeRefs();
        QSet<QString> disabledTypeRefs(supportedTypeRefsList.begin(),
                                       supportedTypeRefsList.end());
        disabledTypeRefs.subtract(QSet<QString>(enabledTypeRefsList.begin(),
                                                enabledTypeRefsList.end()));

        for (const QSharedPointer<VGPUType>& type : commonTypes)
        {
            if (!type)
                continue;

            GpuTuple tuple;
            tuple.gpuGroupRef = group->OpaqueRef();
            tuple.vgpuTypeRefs = QStringList(type->OpaqueRef());
            tuple.isVgpuSubitem = group->HasVGpu();
            tuple.enabled = !disabledTypeRefs.contains(type->OpaqueRef());
            tuple.displayName = type->DisplayDescription();
            this->m_combo->AddTuple(tuple);

            if (commonTypes.size() == 1 && tuple.enabled)
                this->m_combo->SetCurrentTuple(tuple);
        }
    }

    this->m_combo->setEnabled(this->m_combo->count() > 0);
}
