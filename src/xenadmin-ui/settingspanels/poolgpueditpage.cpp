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


#include "poolgpueditpage.h"

#include "xenlib/operations/multipleaction.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/actions/gpu/gpuhelpers.h"
#include "xenlib/xen/actions/host/updateintegratedgpupassthroughaction.h"
#include "xenlib/xen/actions/pool/setgpuplacementpolicyaction.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pgpu.h"
#include "xenlib/xen/pool.h"

#include <QGroupBox>
#include <QIcon>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

PoolGpuEditPage::PoolGpuEditPage(QWidget* parent) : IEditPage(parent)
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(10);

    this->m_placementGroup = new QGroupBox(tr("GPU Placement Policy"), this);
    QVBoxLayout* placementLayout = new QVBoxLayout(this->m_placementGroup);
    this->m_radioDensity = new QRadioButton(tr("Max Density"), this->m_placementGroup);
    this->m_radioPerformance = new QRadioButton(tr("Max Performance"), this->m_placementGroup);
    this->m_radioMixed = new QRadioButton(tr("Mixed"), this->m_placementGroup);
    this->m_radioMixed->setEnabled(false);
    placementLayout->addWidget(this->m_radioDensity);
    placementLayout->addWidget(this->m_radioPerformance);
    placementLayout->addWidget(this->m_radioMixed);
    root->addWidget(this->m_placementGroup);

    this->m_integratedGroup = new QGroupBox(tr("Integrated GPU Passthrough"), this);
    QVBoxLayout* integratedLayout = new QVBoxLayout(this->m_integratedGroup);
    this->m_integratedCurrentState = new QLabel(this->m_integratedGroup);
    this->m_radioEnableIntegrated = new QRadioButton(tr("Enable on next reboot"), this->m_integratedGroup);
    this->m_radioDisableIntegrated = new QRadioButton(tr("Disable on next reboot"), this->m_integratedGroup);
    integratedLayout->addWidget(this->m_integratedCurrentState);
    integratedLayout->addWidget(this->m_radioEnableIntegrated);
    integratedLayout->addWidget(this->m_radioDisableIntegrated);
    root->addWidget(this->m_integratedGroup);

    root->addStretch();
}

QString PoolGpuEditPage::GetText() const
{
    return tr("GPU");
}

QString PoolGpuEditPage::GetSubText() const
{
    QStringList parts;
    if (this->m_showPlacementPolicy)
        parts.append(this->allocationAlgorithmSummary());
    if (this->m_showIntegratedGpu)
        parts.append(this->m_integratedGpuEnabledNow ? tr("Integrated passthrough enabled")
                                                     : tr("Integrated passthrough disabled"));
    return parts.join("; ");
}

QIcon PoolGpuEditPage::GetImage() const
{
    return QIcon(":/icons/cpu_16.png");
}

void PoolGpuEditPage::SetXenObject(QSharedPointer<XenObject> object,
                                   const QVariantMap& objectDataBefore,
                                   const QVariantMap& objectDataCopy)
{
    this->m_object = object;
    Q_UNUSED(objectDataBefore);
    Q_UNUSED(objectDataCopy);

    this->m_pool.clear();
    this->m_host.clear();
    this->m_showPlacementPolicy = false;
    this->m_showIntegratedGpu = false;

    XenConnection* conn = this->connection();
    XenCache* cache = conn ? conn->GetCache() : nullptr;
    if (!cache)
    {
        this->populatePage();
        emit populated();
        return;
    }

    this->m_pool = cache->GetPoolOfOne();
    this->m_host = (!object.isNull() && object->GetObjectType() == XenObjectType::Host)
                       ? qSharedPointerDynamicCast<Host>(object)
                       : QSharedPointer<Host>();

    const bool isPoolObject = (!object.isNull() && object->GetObjectType() == XenObjectType::Pool);
    const bool isStandaloneHost = this->m_host && !this->m_pool;
    this->m_showPlacementPolicy = (isPoolObject || isStandaloneHost) && GpuHelpers::VGpuCapability(conn);
    this->m_showIntegratedGpu = this->m_host && this->m_host->CanEnableDisableIntegratedGpu();

    this->populatePage();
    emit populated();
}

AsyncOperation* PoolGpuEditPage::SaveSettings()
{
    QList<AsyncOperation*> actions;

    if (this->m_showPlacementPolicy && this->m_pool && this->m_pool->IsValid())
    {
        XenAPI::AllocationAlgorithm target = XenAPI::AllocationAlgorithm::Unknown;
        if (this->m_radioDensity->isChecked())
            target = XenAPI::AllocationAlgorithm::DepthFirst;
        else if (this->m_radioPerformance->isChecked())
            target = XenAPI::AllocationAlgorithm::BreadthFirst;

        if (target != XenAPI::AllocationAlgorithm::Unknown && target != this->m_currentAlgorithm)
            actions.append(new SetGpuPlacementPolicyAction(this->m_pool, target, this));
    }

    if (this->m_showIntegratedGpu && this->m_host && this->m_host->IsValid())
    {
        const bool targetEnabled = this->m_radioEnableIntegrated->isChecked();
        if (targetEnabled != this->m_integratedGpuEnabledOnNextReboot)
            actions.append(new UpdateIntegratedGpuPassthroughAction(this->m_host, targetEnabled, true, this));
    }

    if (actions.isEmpty())
        return nullptr;
    if (actions.size() == 1)
        return actions.first();

    return new MultipleAction(this->connection(),
                              tr("Update GPU settings"),
                              QString(),
                              QString(),
                              actions,
                              false,
                              false,
                              false,
                              this);
}

bool PoolGpuEditPage::IsValidToSave() const
{
    return true;
}

void PoolGpuEditPage::ShowLocalValidationMessages()
{
}

void PoolGpuEditPage::HideLocalValidationMessages()
{
}

void PoolGpuEditPage::Cleanup()
{
}

bool PoolGpuEditPage::HasChanged() const
{
    if (this->m_showPlacementPolicy)
    {
        XenAPI::AllocationAlgorithm current = XenAPI::AllocationAlgorithm::Unknown;
        if (this->m_radioDensity->isChecked())
            current = XenAPI::AllocationAlgorithm::DepthFirst;
        else if (this->m_radioPerformance->isChecked())
            current = XenAPI::AllocationAlgorithm::BreadthFirst;
        if (current != this->m_currentAlgorithm)
            return true;
    }

    if (this->m_showIntegratedGpu)
        return this->m_radioEnableIntegrated->isChecked() != this->m_integratedGpuEnabledOnNextReboot;

    return false;
}

void PoolGpuEditPage::populatePage()
{
    this->m_placementGroup->setVisible(this->m_showPlacementPolicy);
    this->m_integratedGroup->setVisible(this->m_showIntegratedGpu);

    if (this->m_showPlacementPolicy)
        this->populatePlacementPolicy();
    if (this->m_showIntegratedGpu)
        this->populateIntegratedGpu();
}

void PoolGpuEditPage::populatePlacementPolicy()
{
    this->m_currentAlgorithm = XenAPI::AllocationAlgorithm::Unknown;

    XenCache* cache = this->connection() ? this->connection()->GetCache() : nullptr;
    const QList<QSharedPointer<GPUGroup>> groups = cache ? cache->GetAll<GPUGroup>(XenObjectType::GPUGroup)
                                                          : QList<QSharedPointer<GPUGroup>>();

    bool first = true;
    for (const QSharedPointer<GPUGroup>& group : groups)
    {
        if (!group || !group->IsValid())
            continue;
        const QString value = group->AllocationAlgorithm();
        XenAPI::AllocationAlgorithm next = XenAPI::AllocationAlgorithm::Unknown;
        if (value == QLatin1String("depth_first"))
            next = XenAPI::AllocationAlgorithm::DepthFirst;
        else if (value == QLatin1String("breadth_first"))
            next = XenAPI::AllocationAlgorithm::BreadthFirst;

        if (first)
        {
            this->m_currentAlgorithm = next;
            first = false;
            continue;
        }
        if (this->m_currentAlgorithm != next)
        {
            this->m_currentAlgorithm = XenAPI::AllocationAlgorithm::Unknown;
            break;
        }
    }

    this->m_radioDensity->setChecked(this->m_currentAlgorithm == XenAPI::AllocationAlgorithm::DepthFirst);
    this->m_radioPerformance->setChecked(this->m_currentAlgorithm == XenAPI::AllocationAlgorithm::BreadthFirst);
    this->m_radioMixed->setVisible(this->m_currentAlgorithm == XenAPI::AllocationAlgorithm::Unknown);
    this->m_radioMixed->setChecked(this->m_currentAlgorithm == XenAPI::AllocationAlgorithm::Unknown);
}

void PoolGpuEditPage::populateIntegratedGpu()
{
    if (!this->m_host || !this->m_host->IsValid())
        return;

    const QString display = this->m_host->Display();
    const bool hostEnabledNow = display == QLatin1String("enabled") || display == QLatin1String("disable_on_reboot");
    const bool hostEnabledNext = display == QLatin1String("enabled") || display == QLatin1String("enable_on_reboot");

    bool gpuEnabledNow = false;
    bool gpuEnabledNext = false;
    QSharedPointer<PGPU> systemDisplay = this->m_host->SystemDisplayDevice();
    if (systemDisplay && systemDisplay->IsValid())
    {
        const QString dom0 = systemDisplay->Dom0Access();
        gpuEnabledNow = dom0 == QLatin1String("enabled") || dom0 == QLatin1String("disable_on_reboot");
        gpuEnabledNext = dom0 == QLatin1String("enabled") || dom0 == QLatin1String("enable_on_reboot");
    }

    this->m_integratedGpuEnabledNow = hostEnabledNow && gpuEnabledNow;
    this->m_integratedGpuEnabledOnNextReboot = hostEnabledNext && gpuEnabledNext;
    this->m_integratedCurrentState->setText(this->m_integratedGpuEnabledNow ? tr("Current state: Enabled")
                                                                             : tr("Current state: Disabled"));
    this->m_radioEnableIntegrated->setChecked(this->m_integratedGpuEnabledOnNextReboot);
    this->m_radioDisableIntegrated->setChecked(!this->m_integratedGpuEnabledOnNextReboot);
}

QString PoolGpuEditPage::allocationAlgorithmSummary() const
{
    if (this->m_radioDensity->isChecked())
        return tr("Max Density");
    if (this->m_radioPerformance->isChecked())
        return tr("Max Performance");
    return tr("Mixed");
}
