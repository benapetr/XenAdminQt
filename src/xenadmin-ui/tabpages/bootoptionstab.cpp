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

#include "bootoptionstab.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QDebug>
#include "xen/xenobject.h"

BootOptionsTab::BootOptionsTab(QWidget* parent)
    : BaseTabPage(parent), m_originalAutoBoot(false), m_originalPVBootFromCD(false)
{
    createUI();
}

void BootOptionsTab::createUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // Auto-boot setting (common for both HVM and PV)
    QGroupBox* autoBootGroup = new QGroupBox("Auto Power On", this);
    QVBoxLayout* autoBootLayout = new QVBoxLayout(autoBootGroup);

    this->m_autoBootCheckBox = new QCheckBox("Start VM automatically when server starts", this);
    connect(this->m_autoBootCheckBox, &QCheckBox::stateChanged, this, &BootOptionsTab::onAutoBootChanged);
    autoBootLayout->addWidget(this->m_autoBootCheckBox);

    this->m_autoBootInfoLabel = new QLabel("The VM will be started automatically when the host boots.", this);
    this->m_autoBootInfoLabel->setWordWrap(true);
    this->m_autoBootInfoLabel->setStyleSheet("QLabel { color: #666; font-style: italic; }");
    autoBootLayout->addWidget(this->m_autoBootInfoLabel);

    mainLayout->addWidget(autoBootGroup);

    // HVM Boot Order section
    QGroupBox* hvmGroup = new QGroupBox("Boot Order", this);
    QHBoxLayout* hvmLayout = new QHBoxLayout(hvmGroup);

    this->m_hvmWidget = new QWidget(this);
    QHBoxLayout* hvmWidgetLayout = new QHBoxLayout(this->m_hvmWidget);
    hvmWidgetLayout->setContentsMargins(0, 0, 0, 0);

    // Boot order list
    QVBoxLayout* listLayout = new QVBoxLayout();
    this->m_hvmInfoLabel = new QLabel("Select and order the boot devices (checked items will be used):", this);
    listLayout->addWidget(this->m_hvmInfoLabel);

    this->m_bootOrderList = new QListWidget(this);
    this->m_bootOrderList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(this->m_bootOrderList, &QListWidget::itemSelectionChanged, this, &BootOptionsTab::onBootOrderSelectionChanged);
    listLayout->addWidget(this->m_bootOrderList);

    hvmWidgetLayout->addLayout(listLayout);

    // Up/Down buttons
    QVBoxLayout* buttonLayout = new QVBoxLayout();
    buttonLayout->addStretch();

    this->m_moveUpButton = new QPushButton("Move Up", this);
    this->m_moveUpButton->setEnabled(false);
    connect(this->m_moveUpButton, &QPushButton::clicked, this, &BootOptionsTab::onMoveUpClicked);
    buttonLayout->addWidget(this->m_moveUpButton);

    this->m_moveDownButton = new QPushButton("Move Down", this);
    this->m_moveDownButton->setEnabled(false);
    connect(this->m_moveDownButton, &QPushButton::clicked, this, &BootOptionsTab::onMoveDownClicked);
    buttonLayout->addWidget(this->m_moveDownButton);

    buttonLayout->addStretch();
    hvmWidgetLayout->addLayout(buttonLayout);

    hvmLayout->addWidget(this->m_hvmWidget);
    mainLayout->addWidget(hvmGroup);

    // PV Boot Device section
    QGroupBox* pvGroup = new QGroupBox("Boot Device", this);
    QVBoxLayout* pvLayout = new QVBoxLayout(pvGroup);

    this->m_pvWidget = new QWidget(this);
    QVBoxLayout* pvWidgetLayout = new QVBoxLayout(this->m_pvWidget);
    pvWidgetLayout->setContentsMargins(0, 0, 0, 0);

    this->m_pvInfoLabel = new QLabel("Select the boot device for this paravirtualized VM:", this);
    pvWidgetLayout->addWidget(this->m_pvInfoLabel);

    this->m_pvBootDeviceCombo = new QComboBox(this);
    connect(this->m_pvBootDeviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BootOptionsTab::onPVBootDeviceChanged);
    pvWidgetLayout->addWidget(this->m_pvBootDeviceCombo);

    QLabel* osParamsLabel = new QLabel("OS Boot Parameters:", this);
    pvWidgetLayout->addWidget(osParamsLabel);

    this->m_osParamsEdit = new QTextEdit(this);
    this->m_osParamsEdit->setMaximumHeight(80);
    this->m_osParamsEdit->setPlaceholderText("Optional kernel boot parameters (e.g., console=tty0)");
    connect(this->m_osParamsEdit, &QTextEdit::textChanged, this, &BootOptionsTab::onOsParamsChanged);
    pvWidgetLayout->addWidget(this->m_osParamsEdit);

    pvLayout->addWidget(this->m_pvWidget);
    mainLayout->addWidget(pvGroup);

    mainLayout->addStretch();
}

void BootOptionsTab::SetObject(QSharedPointer<XenObject> object)
{
    BaseTabPage::SetObject(object);

    if (!object || object->GetObjectType() != XenObjectType::VM)
    {
        this->setEnabled(false);
        return;
    }

    this->m_vmRef = object->OpaqueRef();
    this->m_vmData = object->GetData();

    this->setEnabled(true);

    // Determine if VM is HVM or PV
    bool hvmMode = this->isHVM();

    // Show/hide appropriate sections
    this->m_hvmWidget->setVisible(hvmMode);
    this->m_pvWidget->setVisible(!hvmMode);

    // Get auto-boot setting
    QVariantMap otherConfig = this->m_vmData.value("other_config").toMap();
    this->m_originalAutoBoot = (otherConfig.value("auto_poweron", "false").toString() == "true");
    this->m_autoBootCheckBox->setChecked(this->m_originalAutoBoot);

    if (hvmMode)
    {
        // HVM: Configure boot order list
        this->m_bootOrderList->clear();

        // Get boot order from VM
        QVariantMap bootParams = this->m_vmData.value("HVM_boot_params").toMap();
        this->m_originalBootOrder = bootParams.value("order", "cd").toString().toUpper();

        // Add boot order items (checked items first)
        QStringList bootDevices;
        bootDevices << "C"
                    << "D"
                    << "N"; // Hard disk, DVD, Network

        // Add checked items in order
        for (int i = 0; i < this->m_originalBootOrder.length(); i++)
        {
            QString device = this->m_originalBootOrder.mid(i, 1);
            QString deviceName;
            if (device == "C")
                deviceName = "Hard Disk";
            else if (device == "D")
                deviceName = "DVD Drive";
            else if (device == "N")
                deviceName = "Network";
            else
                continue;

            QListWidgetItem* item = new QListWidgetItem(deviceName, this->m_bootOrderList);
            item->setData(Qt::UserRole, device);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Checked);
            bootDevices.removeOne(device);
        }

        // Add unchecked items
        for (const QString& device : bootDevices)
        {
            QString deviceName;
            if (device == "C")
                deviceName = "Hard Disk";
            else if (device == "D")
                deviceName = "DVD Drive";
            else if (device == "N")
                deviceName = "Network";

            QListWidgetItem* item = new QListWidgetItem(deviceName, this->m_bootOrderList);
            item->setData(Qt::UserRole, device);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
        }
    } else
    {
        // PV: Configure boot device combo
        this->m_pvBootDeviceCombo->clear();
        this->m_pvBootDeviceCombo->addItem("Hard Disk", "disk");

        // Check if VM has CD/DVD
        QVariantList vbds = this->m_vmData.value("VBDs").toList();
        bool hasCD = false;

        // Simple check - just see if any VBD type is "CD"
        for (const QVariant& vbdRef : vbds)
        {
            // TODO unfinished logic
            // We would need API access here to check VBD details
            // For now, assume there might be a CD
            hasCD = true; // Placeholder
            break;
        }

        if (hasCD)
        {
            this->m_pvBootDeviceCombo->addItem("DVD Drive", "cd");
            this->m_pvBootDeviceCombo->setCurrentIndex(0); // Default to hard disk
        }

        // Get OS boot parameters
        this->m_originalOsParams = this->m_vmData.value("PV_args", "").toString();
        this->m_osParamsEdit->setPlainText(this->m_originalOsParams);
    }
}

bool BootOptionsTab::isHVM() const
{
    // HVM VMs have HVM_boot_policy set
    QString bootPolicy = this->m_vmData.value("HVM_boot_policy").toString();
    return !bootPolicy.isEmpty();
}

bool BootOptionsTab::isPVBootableDVD() const
{
    // TODO: Would need API access to check VBD bootable flags
    // For now, return false
    return false;
}

QString BootOptionsTab::getBootOrderString() const
{
    QString order;
    for (int i = 0; i < this->m_bootOrderList->count(); i++)
    {
        QListWidgetItem* item = this->m_bootOrderList->item(i);
        if (item->checkState() == Qt::Checked)
        {
            order += item->data(Qt::UserRole).toString();
        }
    }
    return order;
}

bool BootOptionsTab::hasChanges() const
{
    if (this->m_autoBootCheckBox->isChecked() != this->m_originalAutoBoot)
    {
        return true;
    }

    if (isHVM())
    {
        QString currentOrder = this->getBootOrderString();
        return currentOrder != this->m_originalBootOrder;
    } else
    {
        // PV changes
        if (this->m_osParamsEdit->toPlainText() != this->m_originalOsParams)
        {
            return true;
        }

        if (this->m_pvBootDeviceCombo->count() > 1)
        {
            bool currentBootFromCD = (this->m_pvBootDeviceCombo->currentIndex() == 1);
            if (currentBootFromCD != this->m_originalPVBootFromCD)
            {
                return true;
            }
        }
    }

    return false;
}

void BootOptionsTab::applyChanges()
{
    // TODO: Implement apply through a command
    // For now, just show informational message
    QMessageBox::information(this, "Boot Options",
                             "Boot options display implemented. "
                             "Apply functionality will be added in a future update through a dedicated command.");
}

void BootOptionsTab::onBootOrderSelectionChanged()
{
    int currentIndex = this->m_bootOrderList->currentRow();
    this->m_moveUpButton->setEnabled(currentIndex > 0);
    this->m_moveDownButton->setEnabled(currentIndex >= 0 && currentIndex < this->m_bootOrderList->count() - 1);
}

void BootOptionsTab::onMoveUpClicked()
{
    moveBootOrderItem(true);
}

void BootOptionsTab::onMoveDownClicked()
{
    moveBootOrderItem(false);
}

void BootOptionsTab::moveBootOrderItem(bool moveUp)
{
    int currentIndex = this->m_bootOrderList->currentRow();
    if (currentIndex < 0 || currentIndex >= this->m_bootOrderList->count())
    {
        return;
    }

    if (moveUp && currentIndex == 0)
    {
        return;
    }

    if (!moveUp && currentIndex == this->m_bootOrderList->count() - 1)
    {
        return;
    }

    int newIndex = moveUp ? currentIndex - 1 : currentIndex + 1;

    // Save item data
    QListWidgetItem* item = this->m_bootOrderList->takeItem(currentIndex);

    // Reinsert at new position
    this->m_bootOrderList->insertItem(newIndex, item);
    this->m_bootOrderList->setCurrentRow(newIndex);

    onBootOrderSelectionChanged();
}

void BootOptionsTab::onAutoBootChanged(int state)
{
    Q_UNUSED(state);
    // Change will be detected by hasChanges()
}

void BootOptionsTab::onPVBootDeviceChanged(int index)
{
    Q_UNUSED(index);
    // Change will be detected by hasChanges()
}

void BootOptionsTab::onOsParamsChanged()
{
    // Change will be detected by hasChanges()
}
