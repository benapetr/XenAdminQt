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

#include "memorytabpage.h"
#include "ui_memorytabpage.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/hostmetrics.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/vmmetrics.h"
#include "xenlib/xencache.h"
#include "xenlib/utils/misc.h"
#include "xenlib/xen/vm.h"
#include "../controls/hostmemoryrow.h"
#include "../widgets/vmmemoryrow.h"
#include "../dialogs/ballooningdialog.h"
#include <algorithm>

MemoryTabPage::MemoryTabPage(QWidget* parent) : BaseTabPage(parent), ui(new Ui::MemoryTabPage)
{
    this->ui->setupUi(this);

    // Connect edit button
    connect(this->ui->editButton, &QPushButton::clicked, this, &MemoryTabPage::onEditButtonClicked);
}

MemoryTabPage::~MemoryTabPage()
{
    delete this->ui;
}

void MemoryTabPage::removeObject()
{
    if (!this->m_connection)
        return;

    XenCache* cache = this->m_connection->GetCache();
    if (!cache)
        return;

    disconnect(cache, &XenCache::objectChanged, this, &MemoryTabPage::onCacheObjectChanged);
    disconnect(cache, &XenCache::objectRemoved, this, &MemoryTabPage::onCacheObjectRemoved);
    disconnect(cache, &XenCache::bulkUpdateComplete, this, &MemoryTabPage::onCacheBulkUpdateComplete);
    disconnect(cache, &XenCache::cacheCleared, this, &MemoryTabPage::onCacheCleared);
}

void MemoryTabPage::updateObject()
{
    XenCache* cache = this->m_connection ? this->m_connection->GetCache() : nullptr;
    if (!cache)
        return;

    connect(cache, &XenCache::objectChanged, this, &MemoryTabPage::onCacheObjectChanged, Qt::UniqueConnection);
    connect(cache, &XenCache::objectRemoved, this, &MemoryTabPage::onCacheObjectRemoved, Qt::UniqueConnection);
    connect(cache, &XenCache::bulkUpdateComplete, this, &MemoryTabPage::onCacheBulkUpdateComplete, Qt::UniqueConnection);
    connect(cache, &XenCache::cacheCleared, this, &MemoryTabPage::onCacheCleared, Qt::UniqueConnection);
}

bool MemoryTabPage::IsApplicableForObjectType(const QString& objectType) const
{
    // Memory tab is applicable to VMs, Hosts, and Pools
    return objectType == "vm" || objectType == "host" || objectType == "pool";
}

QSharedPointer<VM> MemoryTabPage::GetVM()
{
    return qSharedPointerDynamicCast<VM>(this->m_object);
}

void MemoryTabPage::refreshContent()
{
    if (!this->m_object)
    {
        this->ui->memoryBar->ClearSegments();
        this->ui->memoryBar->SetTotalMemory(0);
        this->ui->memoryStatsGroup->setVisible(false);
        return;
    }

    this->ui->memoryStatsGroup->setVisible(true);

    if (this->m_object->GetObjectType() == XenObjectType::VM)
    {
        this->ui->horizontalSpacer->changeSize(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
        this->ui->editButton->setVisible(true);
        this->ui->verticalSpacer->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
        this->ui->verticalLayout->invalidate();
        this->populateVMMemory();
    } else if (this->m_object->GetObjectType() == XenObjectType::Host)
    {
        this->ui->editButton->setVisible(false);
        this->ui->horizontalSpacer->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum);
        this->ui->verticalSpacer->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum);
        this->ui->verticalLayout->invalidate();
        this->populateHostMemory();
    } else if (this->m_object->GetObjectType() == XenObjectType::Pool)
    {
        this->ui->editButton->setVisible(false);
        this->ui->horizontalSpacer->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum);
        this->ui->verticalSpacer->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum);
        this->ui->verticalLayout->invalidate();
        this->populatePoolMemory();
    } else
    {
        this->ui->memoryBar->ClearSegments();
        this->ui->memoryBar->SetTotalMemory(0);
        this->ui->memoryStatsGroup->setVisible(false);
        this->ui->vmListScrollArea->setVisible(false);
    }
}

void MemoryTabPage::populateVMMemory()
{
    QSharedPointer<VM> vm = this->GetVM();
    if (!vm)
        return;

    this->ui->memoryBar->setVisible(true);
    this->ui->memoryStatsGroup->setVisible(true);

    // Get memory values
    qint64 memoryStaticMin = vm->GetMemoryStaticMin();
    qint64 memoryStaticMax = vm->GetMemoryStaticMax();
    qint64 memoryDynamicMin = vm->GetMemoryDynamicMin();
    qint64 memoryDynamicMax = vm->GetMemoryDynamicMax();

    // Set total memory for the bar
    this->ui->memoryBar->SetTotalMemory(memoryStaticMax);
    this->ui->memoryBar->ClearSegments();

    // Add memory segment for the VM
    QString vmName = vm->GetName();
    QString powerState = vm->GetPowerState();

    qint64 memoryActual = 0;
    if (powerState == "Running" || powerState == "Paused")
    {
        QSharedPointer<VMMetrics> metrics = vm->GetMetrics();
        if (!metrics.isNull())
            memoryActual = metrics->GetMemoryActual();
    }

    // Use different colors based on power state
    QColor vmColor;
    if (powerState == "Running")
    {
        vmColor = QColor(34, 139, 34); // ForestGreen
    } else
    {
        vmColor = QColor(169, 169, 169); // DarkGray
    }

    QString tooltip = vmName + "\n" + QString("Current memory usage: %1").arg(Misc::FormatSize(memoryActual));
    bool hasBallooning = vm->SupportsBallooning();
    if (hasBallooning)
    {
        tooltip += QString("\nDynamic Min: %1").arg(Misc::FormatSize(memoryDynamicMin));
        tooltip += QString("\nDynamic Max: %1").arg(Misc::FormatSize(memoryDynamicMax));
        if (memoryDynamicMax != memoryStaticMax)
            tooltip += QString("\nStatic Max: %1").arg(Misc::FormatSize(memoryStaticMax));
    }

    // For VMs, show current usage against static max
    this->ui->memoryBar->AddSegment(vmName, memoryActual, vmColor, tooltip);

    // Display memory information in labels
    this->ui->totalMemoryLabel->setVisible(false);
    this->ui->totalMemoryValue->setVisible(false);
    this->ui->usedMemoryLabel->setVisible(false);
    this->ui->usedMemoryValue->setVisible(false);
    this->ui->availableMemoryLabel->setVisible(false);
    this->ui->availableMemoryValue->setVisible(false);
    this->ui->controlDomainMemoryLabel->setVisible(false);
    this->ui->controlDomainMemoryValue->setVisible(false);
    this->ui->totalMaxMemoryLabel->setVisible(false);
    this->ui->totalMaxMemoryValue->setVisible(false);

    this->ui->staticMinValue->setText(Misc::FormatSize(memoryStaticMin));
    this->ui->staticMaxValue->setText(Misc::FormatSize(memoryStaticMax));
    this->ui->dynamicMinValue->setText(Misc::FormatSize(memoryDynamicMin));
    this->ui->dynamicMaxValue->setText(Misc::FormatSize(memoryDynamicMax));

    // Show/hide dynamic memory based on ballooning support
    this->ui->dynamicMinLabel->setVisible(hasBallooning);
    this->ui->dynamicMinValue->setVisible(hasBallooning);
    this->ui->dynamicMaxLabel->setVisible(hasBallooning);
    this->ui->dynamicMaxValue->setVisible(hasBallooning);
    this->ui->staticMinLabel->setVisible(hasBallooning && memoryStaticMin != memoryDynamicMin);
    this->ui->staticMinValue->setVisible(hasBallooning && memoryStaticMin != memoryDynamicMin);
    this->ui->staticMaxLabel->setVisible(hasBallooning && memoryDynamicMax != memoryStaticMax);
    this->ui->staticMaxValue->setVisible(hasBallooning && memoryDynamicMax != memoryStaticMax);

    if (!hasBallooning)
    {
        this->ui->dynamicMinLabel->setText(tr("Memory:"));
        this->ui->dynamicMinValue->setText(Misc::FormatSize(memoryStaticMax));
        this->ui->dynamicMinLabel->setVisible(true);
        this->ui->dynamicMinValue->setVisible(true);
        this->ui->staticMinLabel->setVisible(false);
        this->ui->staticMinValue->setVisible(false);
        this->ui->dynamicMaxLabel->setVisible(false);
        this->ui->dynamicMaxValue->setVisible(false);
        this->ui->staticMaxLabel->setVisible(false);
        this->ui->staticMaxValue->setVisible(false);
    } else
    {
        this->ui->dynamicMinLabel->setText(tr("Dynamic Minimum:"));
    }

    // C# Reference: VMMemoryControlsNoEdit.cs OnPaint (line 91-96)
    // Edit button: don't show if VM has just been rebooted (unknown virtualization status)
    // or if VM is suspended (can't be edited). Show for halted or running VMs.
    bool hasUnknownVirtualizationStatus = false;
    {
        int status = vm->GetVirtualizationStatus();
        hasUnknownVirtualizationStatus = (status & 1) != 0;
    }

    bool showEditButton = (powerState == "Halted") || (powerState == "Running" && !hasUnknownVirtualizationStatus);
    this->ui->editButton->setVisible(showEditButton);

    // Hide VM list for VM view (only shown for hosts)
    this->ui->vmListScrollArea->setVisible(false);
}

void MemoryTabPage::populateHostMemory()
{
    QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(this->m_object);
    if (!host || !host->GetConnection())
        return;

    this->ui->memoryBar->setVisible(false);
    this->ui->memoryStatsGroup->setVisible(false);

    // Hide edit button for hosts (only shown for VMs)
    this->ui->editButton->setVisible(false);

    // Show VM list for host view
    this->ui->vmListScrollArea->setVisible(true);

    this->clearVmListLayout();

    QSharedPointer<HostMetrics> metrics = host->GetMetrics();
    if (metrics && metrics->IsLive())
    {
        HostMemoryRow* hostRow = new HostMemoryRow(host, this);
        this->ui->vmListLayout->addWidget(hostRow);
    }

    struct MemSettings
    {
        bool hasBallooning;
        QString powerState;
        qint64 staticMin;
        qint64 staticMax;
        qint64 dynamicMin;
        qint64 dynamicMax;
    };

    auto settingsEqual = [](const MemSettings& left, const MemSettings& right) -> bool {
        return left.hasBallooning == right.hasBallooning
            && left.powerState == right.powerState
            && left.staticMin == right.staticMin
            && left.staticMax == right.staticMax
            && left.dynamicMin == right.dynamicMin
            && left.dynamicMax == right.dynamicMax;
    };

    XenCache* cache = host->GetCache();
    QList<QSharedPointer<VM>> vmList = cache ? cache->GetAll<VM>(XenObjectType::VM) : QList<QSharedPointer<VM>>();
    QList<QSharedPointer<VM>> hostVms;
    QString hostRef = host->OpaqueRef();

    for (const QSharedPointer<VM>& vm : vmList)
    {
        if (!vm || vm->IsEvicted() || !vm->IsRealVM())
            continue;

        if (vm->GetHomeRef() != hostRef)
            continue;

        hostVms.append(vm);
    }

    std::sort(hostVms.begin(), hostVms.end(), [](const QSharedPointer<VM>& left, const QSharedPointer<VM>& right) {
        QString leftName = left ? left->GetName() : QString();
        QString rightName = right ? right->GetName() : QString();
        return QString::compare(leftName, rightName, Qt::CaseInsensitive) < 0;
    });

    QVector<MemSettings> settingsOrder;
    QVector<QList<QSharedPointer<VM>>> groupedVms;
    for (const QSharedPointer<VM>& vm : hostVms)
    {
        if (!vm)
            continue;

        MemSettings settings;
        if (vm->SupportsBallooning())
        {
            settings.hasBallooning = true;
            settings.powerState = vm->GetPowerState();
            settings.staticMin = vm->GetMemoryStaticMin();
            settings.staticMax = vm->GetMemoryStaticMax();
            settings.dynamicMin = vm->GetMemoryDynamicMin();
            settings.dynamicMax = vm->GetMemoryDynamicMax();
        } else
        {
            settings.hasBallooning = false;
            settings.powerState = vm->GetPowerState();
            settings.staticMin = 0;
            settings.staticMax = vm->GetMemoryStaticMax();
            settings.dynamicMin = 0;
            settings.dynamicMax = 0;
        }

        int settingsIndex = -1;
        for (int i = 0; i < settingsOrder.size(); ++i)
        {
            if (settingsEqual(settingsOrder.at(i), settings))
            {
                settingsIndex = i;
                break;
            }
        }

        if (settingsIndex < 0)
        {
            settingsOrder.append(settings);
            groupedVms.append(QList<QSharedPointer<VM>>());
            settingsIndex = settingsOrder.size() - 1;
        }

        groupedVms[settingsIndex].append(vm);
    }

    QStringList powerStateOrder = {"Running", "Paused", "Suspended", "Halted", "unknown"};
    for (const QString& powerState : powerStateOrder)
    {
        for (int i = 0; i < settingsOrder.size(); ++i)
        {
            const MemSettings& settings = settingsOrder.at(i);
            if (settings.powerState.compare(powerState, Qt::CaseInsensitive) != 0)
                continue;

            const QList<QSharedPointer<VM>>& rowVms = groupedVms.at(i);
            if (rowVms.isEmpty())
                continue;

            VMMemoryRow* vmRow = new VMMemoryRow(rowVms, false, this);
            this->ui->vmListLayout->addWidget(vmRow);
        }
    }

    this->ui->vmListLayout->addStretch();
}

void MemoryTabPage::populatePoolMemory()
{
    QSharedPointer<Pool> pool = qSharedPointerDynamicCast<Pool>(this->m_object);
    if (!pool || !pool->GetConnection())
        return;

    this->ui->memoryBar->setVisible(false);
    this->ui->memoryStatsGroup->setVisible(false);
    this->ui->editButton->setVisible(false);
    this->ui->vmListScrollArea->setVisible(true);

    this->clearVmListLayout();

    XenCache* cache = pool->GetCache();
    QList<QSharedPointer<Host>> hosts = cache ? cache->GetAll<Host>(XenObjectType::Host) : QList<QSharedPointer<Host>>();
    std::sort(hosts.begin(), hosts.end(), [](const QSharedPointer<Host>& left, const QSharedPointer<Host>& right)
    {
        QString leftName = left ? left->GetName() : QString();
        QString rightName = right ? right->GetName() : QString();
        return QString::compare(leftName, rightName, Qt::CaseInsensitive) < 0;
    });

    for (const QSharedPointer<Host>& host : hosts)
    {
        if (!host)
            continue;
        QSharedPointer<HostMetrics> metrics = host->GetMetrics();
        if (!metrics || !metrics->IsLive())
            continue;
        HostMemoryRow* hostRow = new HostMemoryRow(host, this);
        this->ui->vmListLayout->addWidget(hostRow);
    }

    this->ui->vmListLayout->addStretch();
}

void MemoryTabPage::onCacheObjectChanged(XenConnection* connection, const QString& type, const QString& ref)
{
    if (this->m_connection != connection)
        return;

    if (this->m_objectType == XenObjectType::VM)
    {
        if (type == "vm" && ref == this->m_objectRef)
        {
            this->refreshContent();
            return;
        }

        QSharedPointer<VM> vm = this->GetVM();
        if (vm && type == "vm_metrics" && ref == vm->MetricsRef())
        {
            this->refreshContent();
            return;
        }
    } else if (this->m_objectType == XenObjectType::Host)
    {
        if (type == "host" && ref == this->m_objectRef)
        {
            this->refreshContent();
            return;
        }

        QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(this->m_object);
        if (host && type == "host_metrics" && ref == host->GetMetricsRef())
        {
            this->refreshContent();
            return;
        }

        if (type == "vm" || type == "vm_metrics")
        {
            this->refreshContent();
            return;
        }
    } else if (this->m_objectType == XenObjectType::Pool)
    {
        if (type == "pool" && ref == this->m_objectRef)
        {
            this->refreshContent();
            return;
        }

        if (type == "host" || type == "host_metrics" || type == "vm" || type == "vm_metrics")
        {
            this->refreshContent();
            return;
        }
    }
}

void MemoryTabPage::onCacheObjectRemoved(XenConnection* connection, const QString& type, const QString& ref)
{
    Q_UNUSED(ref);

    if (this->m_connection != connection)
        return;

    if (this->m_objectType == XenObjectType::VM)
    {
        if (type == "vm_metrics" || type == "vm")
            this->refreshContent();
    } else if (this->m_objectType == XenObjectType::Host)
    {
        if (type == "host" || type == "host_metrics" || type == "vm" || type == "vm_metrics")
            this->refreshContent();
    } else if (this->m_objectType == XenObjectType::Pool)
    {
        if (type == "pool" || type == "host" || type == "host_metrics" || type == "vm" || type == "vm_metrics")
            this->refreshContent();
    }
}

void MemoryTabPage::onCacheBulkUpdateComplete(const QString& type, int count)
{
    Q_UNUSED(count);

    if (this->m_objectType == XenObjectType::VM)
    {
        if (type == "vm" || type == "vm_metrics")
            this->refreshContent();
    } else if (this->m_objectType == XenObjectType::Host)
    {
        if (type == "host" || type == "host_metrics" || type == "vm" || type == "vm_metrics")
            this->refreshContent();
    } else if (this->m_objectType == XenObjectType::Pool)
    {
        if (type == "pool" || type == "host" || type == "host_metrics" || type == "vm" || type == "vm_metrics")
            this->refreshContent();
    }
}

void MemoryTabPage::onCacheCleared()
{
    this->refreshContent();
}

void MemoryTabPage::clearVmListLayout()
{
    QLayoutItem* item;
    while ((item = this->ui->vmListLayout->takeAt(0)) != nullptr)
    {
        if (item->widget())
            delete item->widget();
        delete item;
    }
}

void MemoryTabPage::onEditButtonClicked()
{
    // C# Reference: VMMemoryControlsNoEdit.cs editButton_Click (line 140-144)
    // Opens BallooningDialog for single VM or BallooningWizard for multiple VMs

    // Open ballooning dialog
    BallooningDialog dialog(this->GetVM(), this);
    dialog.exec();

    // Refresh the tab to show updated values
    if (dialog.result() == QDialog::Accepted)
    {
        this->refreshContent();
    }
}
