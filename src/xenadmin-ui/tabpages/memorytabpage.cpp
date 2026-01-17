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
#include "xen/host.h"
#include "xen/hostmetrics.h"
#include "xen/vmmetrics.h"
#include "xencache.h"
#include "xenlib/xen/vm.h"
#include "../dialogs/ballooningdialog.h"
#include <cmath>
#include <QDebug>
#include <QGroupBox>
#include <QFormLayout>
#include <QLabel>
#include <QFont>

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
    if (this->m_objectData.isEmpty())
    {
        this->ui->memoryBar->clearSegments();
        this->ui->memoryBar->setTotalMemory(0);
        this->ui->memoryStatsGroup->setVisible(false);
        return;
    }

    this->ui->memoryStatsGroup->setVisible(true);

    if (this->m_objectType == "vm")
    {
        this->populateVMMemory();
    } else if (this->m_objectType == "host")
    {
        this->populateHostMemory();
    }
}

void MemoryTabPage::populateVMMemory()
{
    QSharedPointer<VM> vm = this->GetVM();
    if (!vm)
        return;

    // Get memory values
    qint64 memoryStaticMin = vm->GetMemoryStaticMin();
    qint64 memoryStaticMax = vm->GetMemoryStaticMax();
    qint64 memoryDynamicMin = vm->GetMemoryDynamicMin();
    qint64 memoryDynamicMax = vm->GetMemoryDynamicMax();

    // Set total memory for the bar
    this->ui->memoryBar->setTotalMemory(memoryStaticMax);
    this->ui->memoryBar->clearSegments();

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

    QString tooltip = vmName + "\n" + QString("Current memory usage: %1").arg(this->formatMemorySize(memoryActual));
    bool hasBallooning = this->supportsBallooning(vm);
    if (hasBallooning)
    {
        tooltip += QString("\nDynamic Min: %1").arg(this->formatMemorySize(memoryDynamicMin));
        tooltip += QString("\nDynamic Max: %1").arg(this->formatMemorySize(memoryDynamicMax));
        if (memoryDynamicMax != memoryStaticMax)
            tooltip += QString("\nStatic Max: %1").arg(this->formatMemorySize(memoryStaticMax));
    }

    // For VMs, show current usage against static max
    this->ui->memoryBar->addSegment(vmName, memoryActual, vmColor, tooltip);

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

    this->ui->staticMinValue->setText(this->formatMemorySize(memoryStaticMin));
    this->ui->staticMaxValue->setText(this->formatMemorySize(memoryStaticMax));
    this->ui->dynamicMinValue->setText(this->formatMemorySize(memoryDynamicMin));
    this->ui->dynamicMaxValue->setText(this->formatMemorySize(memoryDynamicMax));

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
        this->ui->dynamicMinValue->setText(this->formatMemorySize(memoryStaticMax));
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
    if (!this->m_connection || !host)
        return;

    auto vmSupportsBallooning = [this](const QSharedPointer<VM>& vm) -> bool {
        return this->supportsBallooning(vm);
    };

    QSharedPointer<HostMetrics> metrics = host->GetMetrics();
    qint64 totalMemory = metrics ? metrics->GetMemoryTotal() : 0;
    qint64 hostMemoryOverhead = host->MemoryOverhead();

    // Set up memory bar
    this->ui->memoryBar->setTotalMemory(totalMemory);
    this->ui->memoryBar->clearSegments();

    // Get all VMs resident on this host
    QList<QSharedPointer<VM>> residentVMs = host->GetResidentVMs();

    qint64 xenMemory = hostMemoryOverhead;  // Memory used by Xen hypervisor (includes host overhead)
    qint64 dom0Memory = 0; // Control domain memory
    qint64 totalVmOverhead = 0;
    qint64 totalVmActual = 0;
    qint64 totalDynMax = 0; // Total dynamic max for all VMs
    qint64 totalDynMin = 0;
    int vmCount = 0;

    // Structure to hold VM info for display
    struct VMInfo
    {
        QString name;
        QSharedPointer<VM> vm;
        qint64 memoryActual;
        qint64 memoryDynamicMin;
        qint64 memoryDynamicMax;
        qint64 memoryStaticMax;
    };
    QList<VMInfo> vmInfoList;

    // First pass: collect all VM data
    for (const QSharedPointer<VM>& vm : residentVMs)
    {
        if (!vm)
            continue;

        bool isControlDomain = vm->IsControlDomain();
        QString vmName = vm->GetName();
        qint64 memoryOverhead = vm->MemoryOverhead();
        qint64 memoryDynamicMin = vm->GetMemoryDynamicMin();
        qint64 memoryDynamicMax = vm->GetMemoryDynamicMax();
        qint64 memoryStaticMax = vm->GetMemoryStaticMax();

        // Get VM metrics for actual memory usage
        qint64 memoryActual = 0;
        QSharedPointer<VMMetrics> vmMetrics = vm->GetMetrics();
        if (vmMetrics)
            memoryActual = vmMetrics->GetMemoryActual();

        // Accumulate Xen overhead
        xenMemory += memoryOverhead;
        totalVmOverhead += memoryOverhead;
        totalVmActual += memoryActual;

        if (isControlDomain)
        {
            // Control domain (dom0)
            dom0Memory = (memoryActual > 0) ? memoryActual : memoryDynamicMin;
            if (memoryActual > 0)
                xenMemory += memoryActual;
        } else
        {
            if (vmSupportsBallooning(vm))
            {
                totalDynMax += memoryDynamicMax;
                totalDynMin += memoryDynamicMin;
            } else
            {
                totalDynMax += memoryStaticMax;
                totalDynMin += memoryStaticMax;
            }

            if (memoryActual > 0)
            {
                // Regular VM - add to list for display
                VMInfo info;
                info.name = vmName;
                info.vm = vm;
                info.memoryActual = memoryActual;
                info.memoryDynamicMin = memoryDynamicMin;
                info.memoryDynamicMax = memoryDynamicMax;
                info.memoryStaticMax = memoryStaticMax;
                vmInfoList.append(info);

            }
        }
    }

    // Add segments to memory bar in C# order: Xen → Control domain → VMs → Free

    // 1. Xen overhead segment (hypervisor memory - not including dom0)
    qint64 xenOverhead = xenMemory - dom0Memory;
    if (xenOverhead > 0)
    {
        this->ui->memoryBar->addSegment("XCP-ng", xenOverhead,
                                        QColor(169, 169, 169), // DarkGray
                                        QString("XCP-ng\n%1").arg(this->formatMemorySize(xenOverhead)));
    }

    // 2. Control domain segment
    if (dom0Memory > 0)
    {
        this->ui->memoryBar->addSegment("Control domain", dom0Memory,
                                        QColor(105, 105, 105), // DimGray
                                        QString("Control domain\n%1").arg(this->formatMemorySize(dom0Memory)));
    }

    // 3. VM segments (alternate colors)
    vmCount = 0;
    for (const VMInfo& vmInfo : vmInfoList)
    {
        QColor vmColor = (vmCount % 2 == 0) ? QColor(25, 25, 112) : QColor(70, 130, 180); // MidnightBlue / SteelBlue
        QString vmTooltip = QString("%1\nCurrent memory usage: %2")
                                .arg(vmInfo.name, this->formatMemorySize(vmInfo.memoryActual));

        if (vmInfo.vm && vmSupportsBallooning(vmInfo.vm))
        {
            qint64 vmDynamicMin = vmInfo.vm->GetMemoryDynamicMin();
            qint64 vmDynamicMax = vmInfo.vm->GetMemoryDynamicMax();
            qint64 vmStaticMax = vmInfo.vm->GetMemoryStaticMax();
            vmTooltip += QString("\nDynamic Min: %1").arg(this->formatMemorySize(vmDynamicMin));
            vmTooltip += QString("\nDynamic Max: %1").arg(this->formatMemorySize(vmDynamicMax));
            if (vmDynamicMax != vmStaticMax)
                vmTooltip += QString("\nStatic Max: %1").arg(this->formatMemorySize(vmStaticMax));
        }

        this->ui->memoryBar->addSegment(vmInfo.name, vmInfo.memoryActual, vmColor, vmTooltip);
        vmCount++;
    }

    qint64 freeMemory = totalMemory - (hostMemoryOverhead + totalVmOverhead + totalVmActual);
    if (freeMemory < 0)
        freeMemory = 0;
    qint64 usedMemory = totalMemory - freeMemory;

    // Calculate total dynamic max including Xen memory (matches C# logic)
    qint64 totDynMaxWithXen = totalDynMax + xenMemory;

    // Calculate available memory (matches C# Host.memory_available_calc)
    qint64 availableMemory = totalMemory - totalDynMin - xenMemory;
    if (availableMemory < 0)
        availableMemory = 0;

    // Display memory statistics
    this->ui->totalMemoryLabel->setVisible(true);
    this->ui->totalMemoryValue->setVisible(true);
    this->ui->usedMemoryLabel->setVisible(true);
    this->ui->usedMemoryValue->setVisible(true);
    this->ui->controlDomainMemoryLabel->setVisible(true);
    this->ui->controlDomainMemoryValue->setVisible(true);
    this->ui->availableMemoryLabel->setVisible(true);
    this->ui->availableMemoryValue->setVisible(true);
    this->ui->totalMaxMemoryLabel->setVisible(true);
    this->ui->totalMaxMemoryValue->setVisible(true);

    this->ui->totalMemoryValue->setText(this->formatMemorySize(totalMemory));
    this->ui->usedMemoryValue->setText(this->formatMemorySize(usedMemory));
    this->ui->controlDomainMemoryValue->setText(this->formatMemorySize(dom0Memory));
    this->ui->availableMemoryValue->setText(this->formatMemorySize(availableMemory));

    // Show total max memory with overcommit percentage
    qint64 overcommit = totalMemory > 0
                            ? (qint64) std::round((double) totDynMaxWithXen / (double) totalMemory * 100.0)
                            : 0;
    this->ui->totalMaxMemoryValue->setText(
        QString("%1 (%2% of total memory)").arg(this->formatMemorySize(totDynMaxWithXen)).arg(overcommit));

    // Hide VM-specific labels for hosts
    this->ui->staticMinLabel->setVisible(false);
    this->ui->staticMinValue->setVisible(false);
    this->ui->staticMaxLabel->setVisible(false);
    this->ui->staticMaxValue->setVisible(false);
    this->ui->dynamicMinLabel->setVisible(false);
    this->ui->dynamicMinValue->setVisible(false);
    this->ui->dynamicMaxLabel->setVisible(false);
    this->ui->dynamicMaxValue->setVisible(false);

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

    // Add VM rows to the list
    for (const VMInfo& vmInfo : vmInfoList)
    {
        QGroupBox* vmGroup = new QGroupBox(vmInfo.name, this);
        QFormLayout* vmLayout = new QFormLayout(vmGroup);

        // Memory actual
        QLabel* actualLabel = new QLabel("Current memory usage:");
        QFont boldFont;
        boldFont.setBold(true);
        actualLabel->setFont(boldFont);
        QLabel* actualValue = new QLabel(this->formatMemorySize(vmInfo.memoryActual));
        vmLayout->addRow(actualLabel, actualValue);

        // Dynamic min
        QLabel* dynMinLabel = new QLabel("Dynamic minimum:");
        dynMinLabel->setFont(boldFont);
        QLabel* dynMinValue = new QLabel(this->formatMemorySize(vmInfo.memoryDynamicMin));
        vmLayout->addRow(dynMinLabel, dynMinValue);

        // Dynamic max
        QLabel* dynMaxLabel = new QLabel("Dynamic maximum:");
        dynMaxLabel->setFont(boldFont);
        QLabel* dynMaxValue = new QLabel(this->formatMemorySize(vmInfo.memoryDynamicMax));
        vmLayout->addRow(dynMaxLabel, dynMaxValue);

        // Static max
        QLabel* statMaxLabel = new QLabel("Static maximum:");
        statMaxLabel->setFont(boldFont);
        QLabel* statMaxValue = new QLabel(this->formatMemorySize(vmInfo.memoryStaticMax));
        vmLayout->addRow(statMaxLabel, statMaxValue);

        this->ui->vmListLayout->addWidget(vmGroup);
    }

    // Add spacer at the end
    this->ui->vmListLayout->addStretch();
}

QString MemoryTabPage::formatMemorySize(qint64 bytes) const
{
    if (bytes == 0)
        return "0 B";

    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;

    if (bytes >= GB)
    {
        double gb = bytes / static_cast<double>(GB);
        return QString("%1 GB").arg(gb, 0, 'f', 2);
    } else if (bytes >= MB)
    {
        double mb = bytes / static_cast<double>(MB);
        return QString("%1 MB").arg(mb, 0, 'f', 0);
    } else if (bytes >= KB)
    {
        double kb = bytes / static_cast<double>(KB);
        return QString("%1 KB").arg(kb, 0, 'f', 0);
    } else
    {
        return QString("%1 B").arg(bytes);
    }
}

bool MemoryTabPage::supportsBallooning() const
{
    QSharedPointer<VM> vm = qSharedPointerCast<VM>(this->m_object);
    return this->supportsBallooning(vm);
}

bool MemoryTabPage::supportsBallooning(const QSharedPointer<VM>& vm) const
{
    if (!vm || !this->m_connection || !this->m_connection->GetCache())
        return false;

    if (vm->IsTemplate())
        return vm->GetMemoryDynamicMin() != vm->GetMemoryStaticMax();

    QString guestMetricsRef = vm->GetGuestMetricsRef();
    if (guestMetricsRef.isEmpty() || guestMetricsRef == "OpaqueRef:NULL")
        return false;

    QVariantMap guestMetrics = this->m_connection->GetCache()->ResolveObjectData("vm_guest_metrics", guestMetricsRef);
    QVariantMap other = guestMetrics.value("other").toMap();
    if (!other.contains("feature-balloon"))
        return false;

    QString value = other.value("feature-balloon").toString().toLower();
    return value == "1" || value == "true" || value == "yes";
}

void MemoryTabPage::onEditButtonClicked()
{
    // C# Reference: VMMemoryControlsNoEdit.cs editButton_Click (line 140-144)
    // Opens BallooningDialog for single VM or BallooningWizard for multiple VMs

    // Open ballooning dialog
    BallooningDialog dialog(this->GetVM(), this->m_connection, this);
    dialog.exec();

    // Refresh the tab to show updated values
    if (dialog.result() == QDialog::Accepted)
    {
        this->refreshContent();
    }
}
