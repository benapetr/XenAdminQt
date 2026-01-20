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

bool MemoryTabPage::IsApplicableForObjectType(const QString& objectType) const
{
    // Memory tab is applicable to VMs and Hosts
    return objectType == "vm" || objectType == "host";
}

QSharedPointer<VM> MemoryTabPage::GetVM()
{
    return qSharedPointerCast<VM>(this->m_object);
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

    if (this->m_object->GetObjectType() == "vm")
    {
        this->ui->horizontalSpacer->changeSize(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
        this->ui->editButton->setVisible(true);
        this->ui->verticalSpacer->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
        this->ui->verticalLayout->invalidate();
        this->populateVMMemory();
    } else if (this->m_object->GetObjectType() == "host")
    {
        this->ui->editButton->setVisible(false);
        this->ui->horizontalSpacer->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum);
        this->ui->verticalSpacer->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum);
        this->ui->verticalLayout->invalidate();
        this->populateHostMemory();
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

    QString tooltip = vmName + "\n" + QString("Current memory usage: %1").arg(Misc::FormatMemorySize(memoryActual));
    bool hasBallooning = vm->SupportsBallooning();
    if (hasBallooning)
    {
        tooltip += QString("\nDynamic Min: %1").arg(Misc::FormatMemorySize(memoryDynamicMin));
        tooltip += QString("\nDynamic Max: %1").arg(Misc::FormatMemorySize(memoryDynamicMax));
        if (memoryDynamicMax != memoryStaticMax)
            tooltip += QString("\nStatic Max: %1").arg(Misc::FormatMemorySize(memoryStaticMax));
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

    this->ui->staticMinValue->setText(Misc::FormatMemorySize(memoryStaticMin));
    this->ui->staticMaxValue->setText(Misc::FormatMemorySize(memoryStaticMax));
    this->ui->dynamicMinValue->setText(Misc::FormatMemorySize(memoryDynamicMin));
    this->ui->dynamicMaxValue->setText(Misc::FormatMemorySize(memoryDynamicMax));

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
        this->ui->dynamicMinValue->setText(Misc::FormatMemorySize(memoryStaticMax));
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
    QSharedPointer<Host> host = qSharedPointerCast<Host>(this->m_object);
    if (!host || !host->GetConnection())
        return;

    this->ui->memoryBar->setVisible(false);
    this->ui->memoryStatsGroup->setVisible(false);

    // Hide edit button for hosts (only shown for VMs)
    this->ui->editButton->setVisible(false);

    // Show VM list for host view
    this->ui->vmListScrollArea->setVisible(true);

    // Clear previous VM list
    QLayoutItem* item;
    while ((item = this->ui->vmListLayout->takeAt(0)) != nullptr)
    {
        if (item->widget())
        {
            delete item->widget();
        }
        delete item;
    }

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
    QList<QSharedPointer<VM>> vmList = cache ? cache->GetAll<VM>("vm") : QList<QSharedPointer<VM>>();
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
