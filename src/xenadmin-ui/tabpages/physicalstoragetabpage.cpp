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

#include "physicalstoragetabpage.h"
#include "ui_physicalstoragetabpage.h"
#include "xenlib.h"
#include "xencache.h"
#include "../mainwindow.h"
#include "../commands/storage/newsrcommand.h"
#include "../commands/storage/trimsrcommand.h"
#include "../commands/storage/storagepropertiescommand.h"
#include <QTableWidgetItem>
#include <QMenu>
#include <QDebug>
#include <QMessageBox>
#include <algorithm>

PhysicalStorageTabPage::PhysicalStorageTabPage(QWidget* parent) : BaseTabPage(parent), ui(new Ui::PhysicalStorageTabPage)
{
    this->ui->setupUi(this);
    this->ui->storageTable->horizontalHeader()->setStretchLastSection(true);

    // Make table read-only (C# PhysicalStoragePage has ReadOnly = true)
    this->ui->storageTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Connect button signals
    connect(this->ui->newSRButton, &QPushButton::clicked, this, &PhysicalStorageTabPage::onNewSRButtonClicked);
    connect(this->ui->trimButton, &QPushButton::clicked, this, &PhysicalStorageTabPage::onTrimButtonClicked);
    connect(this->ui->propertiesButton, &QPushButton::clicked, this, &PhysicalStorageTabPage::onPropertiesButtonClicked);

    // Connect table signals
    connect(this->ui->storageTable, &QTableWidget::customContextMenuRequested, this, &PhysicalStorageTabPage::onStorageTableCustomContextMenuRequested);
    connect(this->ui->storageTable, &QTableWidget::itemSelectionChanged, this, &PhysicalStorageTabPage::onStorageTableSelectionChanged);
    connect(this->ui->storageTable, &QTableWidget::doubleClicked, this, &PhysicalStorageTabPage::onStorageTableDoubleClicked);

    // Update button states
    this->updateButtonStates();
}

PhysicalStorageTabPage::~PhysicalStorageTabPage()
{
    delete this->ui;
}

bool PhysicalStorageTabPage::isApplicableForObjectType(const QString& objectType) const
{
    // Physical Storage tab is applicable to Hosts and Pools
    // C# Reference: xenadmin/XenAdmin/MainWindow.cs line 1337
    //   if (!multi && !SearchMode && ((isHostSelected && isHostLive) || isPoolSelected))
    //       newTabs.Add(TabPagePhysicalStorage);
    return objectType == "host" || objectType == "pool";
}

void PhysicalStorageTabPage::setXenObject(const QString& objectType, const QString& objectRef, const QVariantMap& objectData)
{
    // Call base implementation
    BaseTabPage::setXenObject(objectType, objectRef, objectData);
}

void PhysicalStorageTabPage::refreshContent()
{
    // Clear table
    this->ui->storageTable->setRowCount(0);

    if (!this->m_xenLib || this->m_objectRef.isEmpty())
    {
        this->updateButtonStates();
        return;
    }

    if (this->m_objectType == "host")
    {
        this->populateHostStorage();
    }
    else if (this->m_objectType == "pool")
    {
        this->populatePoolStorage();
    }

    // Update button states after populating table
    this->updateButtonStates();
}

void PhysicalStorageTabPage::populateHostStorage()
{
    // C# Reference: PhysicalStoragePage.BuildList() lines 218-279
    // Shows storage repositories attached to this host
    this->ui->titleLabel->setText("Storage Repositories");

    if (!this->m_xenLib || this->m_objectRef.isEmpty())
    {
        return;
    }

    // Get host record to find PBDs
    QVariantMap hostData = this->m_xenLib->getCache()->resolve("host", this->m_objectRef);
    QVariantList pbdRefs = hostData.value("PBDs", QVariantList()).toList();

    if (pbdRefs.isEmpty())
    {
        return;
    }

    // Build list of SRs from PBDs
    // C# uses host.PBDs to find connected SRs (lines 230-250)
    QList<QString> srRefsList;
    QHash<QString, bool> srPluggedStatus;

    for (const QVariant& pbdVar : pbdRefs)
    {
        QString pbdRef = pbdVar.toString();
        QVariantMap pbdData = this->m_xenLib->getCache()->resolve("pbd", pbdRef);

        if (pbdData.isEmpty())
            continue;

        QString srRef = pbdData.value("SR").toString();
        QVariantMap srData = this->m_xenLib->getCache()->resolve("sr", srRef);

        if (srData.isEmpty())
            continue;

        // Filter out Tools SR and hidden SRs
        // C# checks: sr.IsToolsSR() || !sr.Show(Settings.ShowHiddenVMs) (line 242)
        QString srType = srData.value("type", "").toString();
        bool isToolsSR = (srType == "udev"); // Tools SR has type "udev"

        // Check if SR is marked as hidden in other_config
        QVariantMap otherConfig = srData.value("other_config").toMap();
        bool isHidden = otherConfig.value("XenCenter.CustomFields.hidden", false).toBool();

        if (isToolsSR || isHidden)
            continue;

        // Don't add duplicates (C# line 246: if (index >= 0) continue;)
        if (srRefsList.contains(srRef))
            continue;

        srRefsList.append(srRef);

        // Track if this PBD is plugged (currently_attached)
        bool isPlugged = pbdData.value("currently_attached", false).toBool();
        srPluggedStatus[srRef] = isPlugged;
    }

    // Sort SR list (C# inserts in sorted order using BinarySearch)
    std::sort(srRefsList.begin(), srRefsList.end());

    // Now add rows for each SR
    for (const QString& srRef : srRefsList)
    {
        QVariantMap srData = this->m_xenLib->getCache()->resolve("sr", srRef);

        if (srData.isEmpty())
            continue;

        QString name = srData.value("name_label", "Unknown").toString();
        QString description = srData.value("name_description", "").toString();
        QString type = srData.value("type", "").toString();

        // Shared: check if SR is shared across multiple hosts
        // C# checks sr.shared (line in SRRow.UpdateDetails)
        bool shared = srData.value("shared", false).toBool();
        QString sharedText = shared ? "Yes" : "No";

        // Calculate usage, size, and virtual allocation
        // C# uses sr.PhysicalSize, sr.PhysicalUtilisation, sr.VirtualAllocation
        qint64 physicalSize = srData.value("physical_size", 0).toLongLong();
        qint64 physicalUtilisation = srData.value("physical_utilisation", 0).toLongLong();

        // Calculate virtual allocation (sum of all VDI virtual_size in this SR)
        qint64 virtualAllocation = 0;
        QVariantList vdiRefs = srData.value("VDIs", QVariantList()).toList();
        for (const QVariant& vdiVar : vdiRefs)
        {
            QString vdiRef = vdiVar.toString();
            QVariantMap vdiData = this->m_xenLib->getCache()->resolve("vdi", vdiRef);
            if (!vdiData.isEmpty())
            {
                virtualAllocation += vdiData.value("virtual_size", 0).toLongLong();
            }
        }

        // Format sizes in human-readable format
        auto formatSize = [](qint64 bytes) -> QString {
            if (bytes <= 0)
                return "N/A";
            double gb = bytes / (1024.0 * 1024.0 * 1024.0);
            return QString::number(gb, 'f', 2) + " GB";
        };

        QString sizeText = formatSize(physicalSize);
        QString usageText = "N/A";
        if (physicalSize > 0)
        {
            double usedGB = physicalUtilisation / (1024.0 * 1024.0 * 1024.0);
            double percent = (double)physicalUtilisation / (double)physicalSize * 100.0;
            usageText = QString::number(usedGB, 'f', 2) + " GB (" +
                        QString::number(percent, 'f', 1) + "%)";
        }
        QString virtAllocText = formatSize(virtualAllocation);

        // Add row to table
        int row = this->ui->storageTable->rowCount();
        this->ui->storageTable->insertRow(row);

        // Column 0: Icon (placeholder for now - C# uses SR icon based on type)
        QTableWidgetItem* iconItem = new QTableWidgetItem();
        iconItem->setData(Qt::UserRole, srRef); // Store SR ref for context menu
        this->ui->storageTable->setItem(row, 0, iconItem);

        // Column 1: Name
        QTableWidgetItem* nameItem = new QTableWidgetItem(name);
        this->ui->storageTable->setItem(row, 1, nameItem);

        // Column 2: Description
        this->ui->storageTable->setItem(row, 2, new QTableWidgetItem(description));

        // Column 3: Type
        this->ui->storageTable->setItem(row, 3, new QTableWidgetItem(type));

        // Column 4: Shared
        this->ui->storageTable->setItem(row, 4, new QTableWidgetItem(sharedText));

        // Column 5: Usage
        this->ui->storageTable->setItem(row, 5, new QTableWidgetItem(usageText));

        // Column 6: Size
        this->ui->storageTable->setItem(row, 6, new QTableWidgetItem(sizeText));

        // Column 7: Virtual Allocation
        this->ui->storageTable->setItem(row, 7, new QTableWidgetItem(virtAllocText));
    }

    // Resize columns to content
    for (int i = 0; i < this->ui->storageTable->columnCount(); ++i)
    {
        this->ui->storageTable->resizeColumnToContents(i);
    }
}

void PhysicalStorageTabPage::populatePoolStorage()
{
    // C# Reference: PhysicalStoragePage.BuildList() for Pool (lines 218-279)
    // Shows all storage repositories in the pool (when host == null, shows all PBDs in pool)
    this->ui->titleLabel->setText("Storage Repositories");

    if (!this->m_xenLib || this->m_objectRef.isEmpty())
    {
        return;
    }

    // For pools, show all SRs in the pool
    // C#: List<PBD> pbds = host != null ? connection.ResolveAll(host.PBDs) : connection.Cache.PBDs (line 230)
    QList<QVariantMap> allSRs = this->m_xenLib->getCache()->getAll("sr");

    QList<QString> srRefsList;

    for (const QVariantMap& srData : allSRs)
    {
        QString srRef = srData.value("ref", "").toString();

        // Filter out Tools SR and hidden SRs
        // C# checks: sr.IsToolsSR() || !sr.Show(Settings.ShowHiddenVMs) (line 242)
        QString srType = srData.value("type", "").toString();
        bool isToolsSR = (srType == "udev"); // Tools SR has type "udev"

        // Check if SR is marked as hidden in other_config
        QVariantMap otherConfig = srData.value("other_config").toMap();
        bool isHidden = otherConfig.value("XenCenter.CustomFields.hidden", false).toBool();

        if (isToolsSR || isHidden)
            continue;

        srRefsList.append(srRef);
    }

    // Sort SR list (C# inserts in sorted order using BinarySearch)
    std::sort(srRefsList.begin(), srRefsList.end());

    // Now add rows for each SR
    for (const QString& srRef : srRefsList)
    {
        QVariantMap srData = this->m_xenLib->getCache()->resolve("sr", srRef);

        if (srData.isEmpty())
            continue;

        QString name = srData.value("name_label", "Unknown").toString();
        QString description = srData.value("name_description", "").toString();
        QString type = srData.value("type", "").toString();

        // Shared: check if SR is shared across multiple hosts
        bool shared = srData.value("shared", false).toBool();
        QString sharedText = shared ? "Yes" : "No";

        // Calculate usage, size, and virtual allocation
        qint64 physicalSize = srData.value("physical_size", 0).toLongLong();
        qint64 physicalUtilisation = srData.value("physical_utilisation", 0).toLongLong();

        // Calculate virtual allocation (sum of all VDI virtual_size in this SR)
        qint64 virtualAllocation = 0;
        QVariantList vdiRefs = srData.value("VDIs", QVariantList()).toList();
        for (const QVariant& vdiVar : vdiRefs)
        {
            QString vdiRef = vdiVar.toString();
            QVariantMap vdiData = this->m_xenLib->getCache()->resolve("vdi", vdiRef);
            if (!vdiData.isEmpty())
            {
                virtualAllocation += vdiData.value("virtual_size", 0).toLongLong();
            }
        }

        // Format sizes in human-readable format
        auto formatSize = [](qint64 bytes) -> QString {
            if (bytes <= 0)
                return "N/A";
            double gb = bytes / (1024.0 * 1024.0 * 1024.0);
            return QString::number(gb, 'f', 2) + " GB";
        };

        QString sizeText = formatSize(physicalSize);
        QString usageText = "N/A";
        if (physicalSize > 0)
        {
            double usedGB = physicalUtilisation / (1024.0 * 1024.0 * 1024.0);
            double percent = (double)physicalUtilisation / (double)physicalSize * 100.0;
            usageText = QString::number(usedGB, 'f', 2) + " GB (" +
                        QString::number(percent, 'f', 1) + "%)";
        }
        QString virtAllocText = formatSize(virtualAllocation);

        // Add row to table
        int row = this->ui->storageTable->rowCount();
        this->ui->storageTable->insertRow(row);

        // Column 0: Icon (placeholder for now - C# uses SR icon based on type)
        QTableWidgetItem* iconItem = new QTableWidgetItem();
        iconItem->setData(Qt::UserRole, srRef); // Store SR ref for context menu
        this->ui->storageTable->setItem(row, 0, iconItem);

        // Column 1: Name
        QTableWidgetItem* nameItem = new QTableWidgetItem(name);
        this->ui->storageTable->setItem(row, 1, nameItem);

        // Column 2: Description
        this->ui->storageTable->setItem(row, 2, new QTableWidgetItem(description));

        // Column 3: Type
        this->ui->storageTable->setItem(row, 3, new QTableWidgetItem(type));

        // Column 4: Shared
        this->ui->storageTable->setItem(row, 4, new QTableWidgetItem(sharedText));

        // Column 5: Usage
        this->ui->storageTable->setItem(row, 5, new QTableWidgetItem(usageText));

        // Column 6: Size
        this->ui->storageTable->setItem(row, 6, new QTableWidgetItem(sizeText));

        // Column 7: Virtual Allocation
        this->ui->storageTable->setItem(row, 7, new QTableWidgetItem(virtAllocText));
    }

    // Resize columns to content
    for (int i = 0; i < this->ui->storageTable->columnCount(); ++i)
    {
        this->ui->storageTable->resizeColumnToContents(i);
    }
}

void PhysicalStorageTabPage::updateButtonStates()
{
    MainWindow* mainWindow = this->getMainWindow();
    QString selectedSrRef = this->getSelectedSRRef();

    if (mainWindow)
    {
        NewSRCommand newSrCmd(mainWindow);
        this->ui->newSRButton->setEnabled(newSrCmd.canRun());
    } else
    {
        this->ui->newSRButton->setEnabled(false);
    }

    bool canTrim = false;
    bool canShowProperties = false;

    if (mainWindow && !selectedSrRef.isEmpty())
    {
        TrimSRCommand trimCmd(mainWindow);
        trimCmd.setTargetSR(selectedSrRef);
        canTrim = trimCmd.canRun();

        StoragePropertiesCommand propsCmd(mainWindow);
        propsCmd.setTargetSR(selectedSrRef);
        canShowProperties = propsCmd.canRun();
    }

    this->ui->trimButton->setEnabled(canTrim);
    this->ui->propertiesButton->setEnabled(canShowProperties);
}

QString PhysicalStorageTabPage::getSelectedSRRef() const
{
    QList<QTableWidgetItem*> selectedItems = this->ui->storageTable->selectedItems();
    if (selectedItems.isEmpty())
        return QString();

    // Get the first item in the selected row (Icon column stores SR ref)
    int row = selectedItems.first()->row();
    QTableWidgetItem* iconItem = this->ui->storageTable->item(row, 0);
    if (!iconItem)
        return QString();

    return iconItem->data(Qt::UserRole).toString();
}

MainWindow* PhysicalStorageTabPage::getMainWindow() const
{
    return qobject_cast<MainWindow*>(this->window());
}

void PhysicalStorageTabPage::onNewSRButtonClicked()
{
    MainWindow* mainWindow = this->getMainWindow();
    if (!mainWindow)
        return;

    NewSRCommand command(mainWindow);
    if (!command.canRun())
    {
        QMessageBox::warning(this, tr("Cannot Create Storage Repository"),
                             tr("Storage repository creation is not available right now."));
        return;
    }

    command.run();
}

void PhysicalStorageTabPage::onTrimButtonClicked()
{
    QString srRef = this->getSelectedSRRef();
    if (srRef.isEmpty())
        return;

    MainWindow* mainWindow = this->getMainWindow();
    if (!mainWindow)
        return;

    TrimSRCommand command(mainWindow);
    command.setTargetSR(srRef);

    if (!command.canRun())
    {
        QMessageBox::warning(this, tr("Cannot Trim Storage Repository"),
                             tr("The selected storage repository cannot be trimmed at this time."));
        return;
    }

    command.run();
}

void PhysicalStorageTabPage::onPropertiesButtonClicked()
{
    QString srRef = this->getSelectedSRRef();
    if (srRef.isEmpty())
        return;

    MainWindow* mainWindow = this->getMainWindow();
    if (!mainWindow)
        return;

    StoragePropertiesCommand command(mainWindow);
    command.setTargetSR(srRef);

    if (!command.canRun())
        return;

    command.run();
}

void PhysicalStorageTabPage::onStorageTableCustomContextMenuRequested(const QPoint& pos)
{
    MainWindow* mainWindow = this->getMainWindow();
    if (!mainWindow)
        return;

    int row = this->ui->storageTable->rowAt(pos.y());
    if (row >= 0)
    {
        this->ui->storageTable->setCurrentCell(row, 0);
    }

    QString srRef = this->getSelectedSRRef();

    QMenu menu(this);

    NewSRCommand newCmd(mainWindow);
    QAction* newAction = menu.addAction(tr("New Storage Repository..."));
    newAction->setEnabled(newCmd.canRun());
    connect(newAction, &QAction::triggered, this, [mainWindow]() {
        NewSRCommand cmd(mainWindow);
        if (cmd.canRun())
            cmd.run();
    });

    if (!srRef.isEmpty())
    {
        TrimSRCommand trimCmd(mainWindow);
        trimCmd.setTargetSR(srRef);
        QAction* trimAction = menu.addAction(tr("Reclaim Freed Space..."));
        trimAction->setEnabled(trimCmd.canRun());
        connect(trimAction, &QAction::triggered, this, [mainWindow, srRef]() {
            TrimSRCommand cmd(mainWindow);
            cmd.setTargetSR(srRef);
            if (cmd.canRun())
                cmd.run();
        });

        StoragePropertiesCommand propsCmd(mainWindow);
        propsCmd.setTargetSR(srRef);
        QAction* propsAction = menu.addAction(tr("Properties..."));
        propsAction->setEnabled(propsCmd.canRun());
        connect(propsAction, &QAction::triggered, this, [mainWindow, srRef]() {
            StoragePropertiesCommand cmd(mainWindow);
            cmd.setTargetSR(srRef);
            if (cmd.canRun())
                cmd.run();
        });
    }

    menu.exec(this->ui->storageTable->mapToGlobal(pos));
}

void PhysicalStorageTabPage::onStorageTableSelectionChanged()
{
    // Update button states when selection changes
    this->updateButtonStates();
}

void PhysicalStorageTabPage::onStorageTableDoubleClicked(const QModelIndex& index)
{
    Q_UNUSED(index);
    
    // Double-click opens properties (matches C# behavior)
    QString srRef = this->getSelectedSRRef();
    if (!srRef.isEmpty())
    {
        this->onPropertiesButtonClicked();
    }
}
