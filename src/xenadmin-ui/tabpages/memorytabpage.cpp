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
#include "xenlib.h"
#include "xencache.h"
#include "../dialogs/ballooningdialog.h"
#include <QDebug>
#include <QGroupBox>
#include <QFormLayout>
#include <QLabel>
#include <QFont>

MemoryTabPage::MemoryTabPage(QWidget* parent)
    : BaseTabPage(parent), ui(new Ui::MemoryTabPage)
{
    this->ui->setupUi(this);

    // Connect edit button
    connect(this->ui->editButton, &QPushButton::clicked,
            this, &MemoryTabPage::onEditButtonClicked);
}

MemoryTabPage::~MemoryTabPage()
{
    delete this->ui;
}

bool MemoryTabPage::isApplicableForObjectType(const QString& objectType) const
{
    // Memory tab is applicable to VMs and Hosts
    return objectType == "vm" || objectType == "host";
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
    // Get memory values
    qint64 memoryStaticMin = this->m_objectData.value("memory_static_min", 0).toLongLong();
    qint64 memoryStaticMax = this->m_objectData.value("memory_static_max", 0).toLongLong();
    qint64 memoryDynamicMin = this->m_objectData.value("memory_dynamic_min", 0).toLongLong();
    qint64 memoryDynamicMax = this->m_objectData.value("memory_dynamic_max", 0).toLongLong();

    // Set total memory for the bar
    this->ui->memoryBar->setTotalMemory(memoryStaticMax);
    this->ui->memoryBar->clearSegments();

    // Add memory segment for the VM
    QString vmName = this->m_objectData.value("name_label", "VM").toString();
    QString powerState = this->m_objectData.value("power_state", "unknown").toString();

    // Use different colors based on power state
    QColor vmColor;
    if (powerState == "Running")
    {
        vmColor = QColor(34, 139, 34); // ForestGreen
    } else
    {
        vmColor = QColor(169, 169, 169); // DarkGray
    }

    QString tooltip = vmName + "\n";
    if (this->supportsBallooning())
    {
        tooltip += QString("Dynamic Min: %1\n").arg(formatMemorySize(memoryDynamicMin));
        tooltip += QString("Dynamic Max: %1\n").arg(formatMemorySize(memoryDynamicMax));
    }
    tooltip += QString("Static Max: %1").arg(formatMemorySize(memoryStaticMax));

    // For VMs, show allocated memory
    this->ui->memoryBar->addSegment(vmName, memoryStaticMax, vmColor, tooltip);

    // Display memory information in labels
    this->ui->totalMemoryLabel->setVisible(false);
    this->ui->totalMemoryValue->setVisible(false);
    this->ui->usedMemoryLabel->setVisible(false);
    this->ui->usedMemoryValue->setVisible(false);
    this->ui->availableMemoryLabel->setVisible(false);
    this->ui->availableMemoryValue->setVisible(false);

    this->ui->staticMinValue->setText(this->formatMemorySize(memoryStaticMin));
    this->ui->staticMaxValue->setText(this->formatMemorySize(memoryStaticMax));
    this->ui->dynamicMinValue->setText(this->formatMemorySize(memoryDynamicMin));
    this->ui->dynamicMaxValue->setText(this->formatMemorySize(memoryDynamicMax));

    // Show/hide dynamic memory based on ballooning support
    bool hasBallooning = this->supportsBallooning();
    this->ui->dynamicMinLabel->setVisible(hasBallooning);
    this->ui->dynamicMinValue->setVisible(hasBallooning);
    this->ui->dynamicMaxLabel->setVisible(hasBallooning);
    this->ui->dynamicMaxValue->setVisible(hasBallooning);

    // C# Reference: VMMemoryControlsNoEdit.cs OnPaint (line 91-96)
    // Edit button: don't show if VM has just been rebooted (unknown virtualization status)
    // or if VM is suspended (can't be edited). Show for halted or running VMs.
    bool showEditButton = (powerState == "Halted" || powerState == "Running");
    this->ui->editButton->setVisible(showEditButton);

    // Hide VM list for VM view (only shown for hosts)
    this->ui->vmListScrollArea->setVisible(false);
}

void MemoryTabPage::populateHostMemory()
{
    if (!this->m_xenLib)
        return;

    // Get host metrics
    QString metricsRef = this->m_objectData.value("metrics", "OpaqueRef:NULL").toString();
    QVariantMap metricsData;

    if (metricsRef != "OpaqueRef:NULL")
    {
        metricsData = this->m_xenLib->getCache()->resolve("host_metrics", metricsRef);
    }

    qint64 totalMemory = metricsData.value("memory_total", 0).toLongLong();
    qint64 freeMemory = metricsData.value("memory_free", 0).toLongLong();
    qint64 usedMemory = totalMemory - freeMemory;

    // Set up memory bar
    this->ui->memoryBar->setTotalMemory(totalMemory);
    this->ui->memoryBar->clearSegments();

    // Get all VMs resident on this host
    QVariantList residentVMs = this->m_objectData.value("resident_VMs", QVariantList()).toList();

    qint64 xenMemory = 0;  // Memory used by Xen hypervisor
    qint64 dom0Memory = 0; // Control domain memory
    qint64 vmsTotalMemory = 0;
    qint64 totalDynMax = 0; // Total dynamic max for all VMs
    int vmCount = 0;

    // Structure to hold VM info for display
    struct VMInfo
    {
        QString name;
        QString ref;
        qint64 memoryActual;
        qint64 memoryDynamicMin;
        qint64 memoryDynamicMax;
        qint64 memoryStaticMax;
    };
    QList<VMInfo> vmInfoList;

    // First pass: collect all VM data
    for (const QVariant& vmRefVar : residentVMs)
    {
        QString vmRef = vmRefVar.toString();
        QVariantMap vmData = this->m_xenLib->getCache()->resolve("vm", vmRef);

        bool isControlDomain = vmData.value("is_control_domain", false).toBool();
        QString vmName = vmData.value("name_label", "VM").toString();
        qint64 memoryOverhead = vmData.value("memory_overhead", 0).toLongLong();
        qint64 memoryDynamicMin = vmData.value("memory_dynamic_min", 0).toLongLong();
        qint64 memoryDynamicMax = vmData.value("memory_dynamic_max", 0).toLongLong();
        qint64 memoryStaticMax = vmData.value("memory_static_max", 0).toLongLong();

        // Get VM metrics for actual memory usage
        QString vmMetricsRef = vmData.value("metrics", "OpaqueRef:NULL").toString();
        qint64 memoryActual = 0;

        if (vmMetricsRef != "OpaqueRef:NULL")
        {
            QVariantMap vmMetricsData = this->m_xenLib->getCache()->resolve("vm_metrics", vmMetricsRef);
            memoryActual = vmMetricsData.value("memory_actual", 0).toLongLong();
        }

        // Accumulate Xen overhead
        xenMemory += memoryOverhead;

        if (isControlDomain)
        {
            // Control domain (dom0)
            dom0Memory = memoryActual;
        } else if (memoryActual > 0)
        {
            // Regular VM - add to list for display
            VMInfo info;
            info.name = vmName;
            info.ref = vmRef;
            info.memoryActual = memoryActual;
            info.memoryDynamicMin = memoryDynamicMin;
            info.memoryDynamicMax = memoryDynamicMax;
            info.memoryStaticMax = memoryStaticMax;
            vmInfoList.append(info);

            vmsTotalMemory += memoryActual;
            totalDynMax += memoryDynamicMax;
        }
    }

    // Add segments to memory bar in C# order: Xen → Control domain → VMs → Free

    // 1. Xen overhead segment (hypervisor memory - not including dom0)
    qint64 xenOverhead = xenMemory - dom0Memory;
    if (xenOverhead > 0)
    {
        this->ui->memoryBar->addSegment("XCP-ng", xenOverhead,
                                        QColor(169, 169, 169), // DarkGray
                                        QString("XCP-ng\n%1").arg(formatMemorySize(xenOverhead)));
    }

    // 2. Control domain segment
    if (dom0Memory > 0)
    {
        this->ui->memoryBar->addSegment("Control domain", dom0Memory,
                                        QColor(105, 105, 105), // DimGray
                                        QString("Control domain\n%1").arg(formatMemorySize(dom0Memory)));
    }

    // 3. VM segments (alternate colors)
    vmCount = 0;
    for (const VMInfo& vmInfo : vmInfoList)
    {
        QColor vmColor = (vmCount % 2 == 0) ? QColor(25, 25, 112) : QColor(70, 130, 180); // MidnightBlue / SteelBlue
        this->ui->memoryBar->addSegment(vmInfo.name, vmInfo.memoryActual, vmColor,
                                        QString("%1\n%2").arg(vmInfo.name, formatMemorySize(vmInfo.memoryActual)));
        vmCount++;
    }

    // Calculate total dynamic max including Xen memory (matches C# logic)
    qint64 totDynMaxWithXen = totalDynMax + xenMemory;

    // Calculate available memory (matches C# Host.memory_available_calc)
    qint64 availableMemory = freeMemory;

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

    this->ui->totalMemoryValue->setText(formatMemorySize(totalMemory));
    this->ui->usedMemoryValue->setText(formatMemorySize(usedMemory));
    this->ui->controlDomainMemoryValue->setText(formatMemorySize(dom0Memory));
    this->ui->availableMemoryValue->setText(formatMemorySize(availableMemory));

    // Show total max memory with overcommit percentage
    qint64 overcommit = totalMemory > 0
                            ? (qint64) std::round((double) totDynMaxWithXen / (double) totalMemory * 100.0)
                            : 0;
    this->ui->totalMaxMemoryValue->setText(
        QString("%1 (%2% of total memory)").arg(formatMemorySize(totDynMaxWithXen)).arg(overcommit));

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
        QLabel* actualValue = new QLabel(formatMemorySize(vmInfo.memoryActual));
        vmLayout->addRow(actualLabel, actualValue);

        // Dynamic min
        QLabel* dynMinLabel = new QLabel("Dynamic minimum:");
        dynMinLabel->setFont(boldFont);
        QLabel* dynMinValue = new QLabel(formatMemorySize(vmInfo.memoryDynamicMin));
        vmLayout->addRow(dynMinLabel, dynMinValue);

        // Dynamic max
        QLabel* dynMaxLabel = new QLabel("Dynamic maximum:");
        dynMaxLabel->setFont(boldFont);
        QLabel* dynMaxValue = new QLabel(formatMemorySize(vmInfo.memoryDynamicMax));
        vmLayout->addRow(dynMaxLabel, dynMaxValue);

        // Static max
        QLabel* statMaxLabel = new QLabel("Static maximum:");
        statMaxLabel->setFont(boldFont);
        QLabel* statMaxValue = new QLabel(formatMemorySize(vmInfo.memoryStaticMax));
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
    // Check if VM supports memory ballooning
    // In XenServer, ballooning is supported if dynamic min/max differ from static min/max
    qint64 memoryStaticMin = this->m_objectData.value("memory_static_min", 0).toLongLong();
    qint64 memoryStaticMax = this->m_objectData.value("memory_static_max", 0).toLongLong();
    qint64 memoryDynamicMin = this->m_objectData.value("memory_dynamic_min", 0).toLongLong();
    qint64 memoryDynamicMax = this->m_objectData.value("memory_dynamic_max", 0).toLongLong();

    // If dynamic values are set and differ from static, ballooning is supported
    return (memoryDynamicMin > 0 || memoryDynamicMax > 0) &&
           (memoryDynamicMin != memoryStaticMin || memoryDynamicMax != memoryStaticMax);
}

void MemoryTabPage::onEditButtonClicked()
{
    // C# Reference: VMMemoryControlsNoEdit.cs editButton_Click (line 140-144)
    // Opens BallooningDialog for single VM or BallooningWizard for multiple VMs

    if (this->m_objectType != "vm" || !this->m_xenLib)
        return;

    // Open ballooning dialog
    BallooningDialog dialog(this->m_objectRef, this->m_xenLib, this);
    dialog.exec();

    // Refresh the tab to show updated values
    if (dialog.result() == QDialog::Accepted)
    {
        refreshContent();
    }
}
