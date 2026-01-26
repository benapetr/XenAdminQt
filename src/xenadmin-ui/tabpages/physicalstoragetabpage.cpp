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

#include <QTableWidgetItem>
#include <QMenu>
#include <QDebug>
#include <QMessageBox>
#include <algorithm>
#include <QSet>
#include <QItemSelectionModel>
#include "physicalstoragetabpage.h"
#include "dialogs/actionprogressdialog.h"
#include "ui_physicalstoragetabpage.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pbd.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/vdi.h"
#include "xenlib/xen/actions/sr/srtrimaction.h"
#include "xenlib/utils/misc.h"
#include "../iconmanager.h"
#include "../mainwindow.h"
#include "../commands/storage/newsrcommand.h"
#include "../commands/storage/trimsrcommand.h"
#include "../commands/storage/detachsrcommand.h"
#include "../commands/storage/storagepropertiescommand.h"
#include "../operations/operationmanager.h"
#include "xenlib/operations/parallelaction.h"

PhysicalStorageTabPage::PhysicalStorageTabPage(QWidget* parent) : BaseTabPage(parent), ui(new Ui::PhysicalStorageTabPage)
{
    this->ui->setupUi(this);
    this->ui->storageTable->horizontalHeader()->setStretchLastSection(true);
    this->ui->storageTable->setIconSize(QSize(16, 16));
    this->ui->storageTable->setColumnWidth(0, 24);

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

bool PhysicalStorageTabPage::IsApplicableForObjectType(const QString& objectType) const
{
    // Physical Storage tab is applicable to Hosts and Pools
    // C# Reference: xenadmin/XenAdmin/MainWindow.cs line 1337
    //   if (!multi && !SearchMode && ((isHostSelected && isHostLive) || isPoolSelected))
    //       newTabs.Add(TabPagePhysicalStorage);
    return objectType == "host" || objectType == "pool";
}

void PhysicalStorageTabPage::refreshContent()
{
    // Clear table
    this->ui->storageTable->setRowCount(0);

    if (!this->m_connection || this->m_objectRef.isEmpty())
    {
        this->updateButtonStates();
        return;
    }

    if (this->m_objectType == "host")
    {
        this->populateHostStorage();
    } else if (this->m_objectType == "pool")
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

    if (!this->m_connection || this->m_objectRef.isEmpty())
    {
        return;
    }

    QSharedPointer<Host> host = this->m_connection->GetCache()->ResolveObject<Host>("host", this->m_objectRef);
    if (!host || !host->IsValid())
        return;

    // Build list of SRs from PBDs
    // C# uses host.PBDs to find connected SRs (lines 230-250)
    QList<QString> srRefsList;
    QHash<QString, bool> srPluggedStatus;

    const QList<QSharedPointer<PBD>> pbds = host->GetPBDs();
    for (const QSharedPointer<PBD>& pbd : pbds)
    {
        if (!pbd || !pbd->IsValid())
            continue;

        QSharedPointer<SR> sr = pbd->GetSR();
        if (!sr || !sr->IsValid())
            continue;

        // Filter out Tools SR and hidden SRs
        // C# checks: sr.IsToolsSR() || !sr.Show(Settings.ShowHiddenVMs) (line 242)
        if (sr->IsToolsSR() || sr->IsHidden())
            continue;

        // Don't add duplicates (C# line 246: if (index >= 0) continue;)
        QString srRef = sr->OpaqueRef();
        if (srRefsList.contains(srRef))
            continue;

        srRefsList.append(srRef);

        // Track if this PBD is plugged (currently_attached)
        bool isPlugged = pbd->IsCurrentlyAttached();
        srPluggedStatus[srRef] = isPlugged;
    }

    // Sort SR list (C# inserts in sorted order using BinarySearch)
    std::sort(srRefsList.begin(), srRefsList.end());

    // Now add rows for each SR
    for (const QString& srRef : srRefsList)
    {
        QSharedPointer<SR> sr = this->m_connection->GetCache()->ResolveObject<SR>("sr", srRef);
        if (!sr || !sr->IsValid())
            continue;

        QString name = sr->GetName();
        QString description = sr->GetDescription();
        QString type = sr->GetType();

        // Shared: check if SR is shared across multiple hosts
        // C# checks sr.shared (line in SRRow.UpdateDetails)
        bool shared = sr->IsShared();
        QString sharedText = shared ? "Yes" : "No";

        // Calculate usage, size, and virtual allocation
        // C# uses sr.PhysicalSize, sr.PhysicalUtilisation, sr.VirtualAllocation
        qint64 physicalSize = sr->PhysicalSize();
        qint64 physicalUtilisation = sr->PhysicalUtilisation();

        // Calculate virtual allocation (sum of all VDI virtual_size in this SR)
        qint64 virtualAllocation = 0;
        const QList<QSharedPointer<VDI>> vdis = sr->GetVDIs();
        for (const QSharedPointer<VDI>& vdi : vdis)
        {
            if (vdi && vdi->IsValid())
                virtualAllocation += vdi->VirtualSize();
        }

        QString sizeText = Misc::FormatMemorySize(physicalSize);
        QString usageText = "N/A";
        if (physicalSize > 0)
        {
            double usedGB = physicalUtilisation / (1024.0 * 1024.0 * 1024.0);
            double percent = (double)physicalUtilisation / (double)physicalSize * 100.0;
            usageText = QString::number(usedGB, 'f', 2) + " GB (" +
                        QString::number(percent, 'f', 1) + "%)";
        }
        QString virtAllocText = Misc::FormatMemorySize(virtualAllocation);

        // Add row to table
        int row = this->ui->storageTable->rowCount();
        this->ui->storageTable->insertRow(row);

        // Column 0: Icon (C# uses SR icon based on type/state)
        QTableWidgetItem* iconItem = new QTableWidgetItem();
        iconItem->setIcon(IconManager::instance().GetIconForSR(sr->GetData(), this->m_connection));
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

    if (!this->m_connection || this->m_objectRef.isEmpty())
    {
        return;
    }

    // For pools, show all SRs in the pool
    // C#: List<PBD> pbds = host != null ? connection.ResolveAll(host.PBDs) : connection.Cache.PBDs (line 230)
    QList<QSharedPointer<SR>> allSRs = this->m_connection->GetCache()->GetAll<SR>("sr");

    QList<QString> srRefsList;

    for (const QSharedPointer<SR>& sr : allSRs)
    {
        if (!sr || !sr->IsValid())
            continue;

        // Filter out Tools SR and hidden SRs
        // C# checks: sr.IsToolsSR() || !sr.Show(Settings.ShowHiddenVMs) (line 242)
        if (sr->IsToolsSR() || sr->IsHidden())
            continue;

        srRefsList.append(sr->OpaqueRef());
    }

    // Sort SR list (C# inserts in sorted order using BinarySearch)
    std::sort(srRefsList.begin(), srRefsList.end());

    // Now add rows for each SR
    for (const QString& srRef : srRefsList)
    {
        QSharedPointer<SR> sr = this->m_connection->GetCache()->ResolveObject<SR>("sr", srRef);
        if (!sr || !sr->IsValid())
            continue;

        QString name = sr->GetName();
        QString description = sr->GetDescription();
        QString type = sr->GetType();

        // Shared: check if SR is shared across multiple hosts
        bool shared = sr->IsShared();
        QString sharedText = shared ? "Yes" : "No";

        // Calculate usage, size, and virtual allocation
        qint64 physicalSize = sr->PhysicalSize();
        qint64 physicalUtilisation = sr->PhysicalUtilisation();

        // Calculate virtual allocation (sum of all VDI virtual_size in this SR)
        qint64 virtualAllocation = 0;
        const QList<QSharedPointer<VDI>> vdis = sr->GetVDIs();
        for (const QSharedPointer<VDI>& vdi : vdis)
        {
            if (vdi && vdi->IsValid())
                virtualAllocation += vdi->VirtualSize();
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

        // Column 0: Icon (C# uses SR icon based on type/state)
        QTableWidgetItem* iconItem = new QTableWidgetItem();
        iconItem->setIcon(IconManager::instance().GetIconForSR(sr->GetData(), this->m_connection));
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
    const QStringList selectedSrRefs = this->getSelectedSRRefs();
    const int selectionCount = selectedSrRefs.size();

    if (mainWindow)
    {
        NewSRCommand newSrCmd(mainWindow);
        this->ui->newSRButton->setEnabled(newSrCmd.CanRun());
    } else
    {
        this->ui->newSRButton->setEnabled(false);
    }

    bool canTrim = false;
    bool canShowProperties = false;

    if (mainWindow && selectionCount == 1)
    {
        const QString selectedSrRef = selectedSrRefs.first();
        TrimSRCommand trimCmd(mainWindow);
        trimCmd.setTargetSR(selectedSrRef, this->m_connection);
        canTrim = trimCmd.CanRun();

        StoragePropertiesCommand propsCmd(mainWindow);
        propsCmd.setTargetSR(selectedSrRef, this->m_connection);
        canShowProperties = propsCmd.CanRun();
    }
    else if (mainWindow && selectionCount > 1 && this->m_connection)
    {
        // Enable trim if at least one selected SR supports it and is attached.
        for (const QString& srRef : selectedSrRefs)
        {
            QSharedPointer<SR> sr = this->m_connection->GetCache()->ResolveObject<SR>("sr", srRef);
            if (!sr || !sr->IsValid())
                continue;
            if (sr->SupportsTrim() && !sr->IsDetached())
            {
                canTrim = true;
                break;
            }
        }
        canShowProperties = false;
    }

    this->ui->trimButton->setEnabled(canTrim);
    this->ui->propertiesButton->setEnabled(canShowProperties);
}

QString PhysicalStorageTabPage::getSelectedSRRef() const
{
    const QStringList refs = this->getSelectedSRRefs();
    return refs.isEmpty() ? QString() : refs.first();
}

QStringList PhysicalStorageTabPage::getSelectedSRRefs() const
{
    QStringList refs;
    QList<QTableWidgetItem*> selectedItems = this->ui->storageTable->selectedItems();
    if (selectedItems.isEmpty())
        return refs;

    QSet<int> rows;
    for (QTableWidgetItem* item : selectedItems)
        rows.insert(item->row());

    for (int row : rows)
    {
        QTableWidgetItem* iconItem = this->ui->storageTable->item(row, 0);
        if (!iconItem)
            continue;
        const QString srRef = iconItem->data(Qt::UserRole).toString();
        if (!srRef.isEmpty())
            refs.append(srRef);
    }

    return refs;
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
    if (!command.CanRun())
    {
        QMessageBox::warning(this, tr("Cannot Create Storage Repository"), tr("Storage repository creation is not available right now."));
        return;
    }

    command.Run();
}

void PhysicalStorageTabPage::onTrimButtonClicked()
{
    const QStringList selectedSrRefs = this->getSelectedSRRefs();
    if (selectedSrRefs.isEmpty())
        return;

    MainWindow* mainWindow = this->getMainWindow();
    if (!mainWindow)
        return;

    QList<QSharedPointer<SR>> eligibleSrs;
    if (this->m_connection)
    {
        for (const QString& srRef : selectedSrRefs)
        {
            QSharedPointer<SR> sr = this->m_connection->GetCache()->ResolveObject<SR>("sr", srRef);
            if (!sr || !sr->IsValid())
                continue;
            if (sr->SupportsTrim() && !sr->IsDetached())
                eligibleSrs.append(sr);
        }
    }

    if (eligibleSrs.isEmpty())
    {
        QMessageBox::warning(this, tr("Cannot Trim Storage Repository"), tr("The selected storage repository cannot be trimmed at this time."));
        return;
    }

    QString confirmationText;
    if (eligibleSrs.size() == 1)
    {
        confirmationText = tr("Are you sure you want to trim storage repository '%1'?")
                               .arg(eligibleSrs.first()->GetName());
    }
    else
    {
        confirmationText = tr("Are you sure you want to trim the selected storage repositories?");
    }

    QMessageBox confirm(this);
    confirm.setWindowTitle(tr("Trim Storage Repository"));
    confirm.setText(confirmationText);
    confirm.setInformativeText(tr("Trimming will reclaim freed space from the storage repository.\n\n"
                                  "This operation may take some time depending on the amount of space to reclaim.\n\n"
                                  "Do you want to continue?"));
    confirm.setIcon(QMessageBox::Question);
    confirm.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    confirm.setDefaultButton(QMessageBox::Yes);

    if (confirm.exec() != QMessageBox::Yes)
        return;

    QList<AsyncOperation*> actions;
    for (const QSharedPointer<SR>& sr : eligibleSrs)
    {
        XenConnection* conn = sr ? sr->GetConnection() : nullptr;
        if (!conn || !conn->IsConnected())
            continue;

        SrTrimAction* action = new SrTrimAction(conn, sr, nullptr);
        OperationManager::instance()->RegisterOperation(action);
        actions.append(action);
    }

    if (actions.isEmpty())
        return;

    AsyncOperation* groupedAction = nullptr;
    if (actions.size() == 1)
    {
        groupedAction = actions.first();
    }
    else
    {
        groupedAction = new ParallelAction(
            QString(),
            tr("Reclaiming freed space..."),
            tr("Reclaim freed space completed"),
            actions,
            nullptr,
            false,
            false,
            ParallelAction::DEFAULT_MAX_PARALLEL_OPERATIONS,
            this);
        OperationManager::instance()->RegisterOperation(groupedAction);
    }

    ActionProgressDialog dialog(groupedAction, this);
    dialog.exec();
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
    command.setTargetSR(srRef, this->m_connection);

    if (!command.CanRun())
        return;

    command.Run();
}

void PhysicalStorageTabPage::onStorageTableCustomContextMenuRequested(const QPoint& pos)
{
    MainWindow* mainWindow = this->getMainWindow();
    if (!mainWindow)
        return;

    int row = this->ui->storageTable->rowAt(pos.y());
    if (row >= 0)
    {
        QItemSelectionModel* selectionModel = this->ui->storageTable->selectionModel();
        const bool rowSelected = selectionModel && selectionModel->isRowSelected(row, QModelIndex());
        if (rowSelected)
        {
            this->ui->storageTable->setCurrentCell(row, 0, QItemSelectionModel::NoUpdate);
        } else
        {
            this->ui->storageTable->setCurrentCell(row, 0, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        }
    }

    const QStringList selectedSrRefs = this->getSelectedSRRefs();
    const int selectionCount = selectedSrRefs.size();

    QMenu menu(this);

    NewSRCommand newCmd(mainWindow);
    QAction* newAction = menu.addAction(tr("New Storage Repository..."));
    newAction->setEnabled(newCmd.CanRun());
    connect(newAction, &QAction::triggered, this, [mainWindow]()
    {
        NewSRCommand cmd(mainWindow);
        if (cmd.CanRun())
            cmd.Run();
    });

    if (selectionCount >= 1)
    {
        bool canTrim = false;
        if (this->m_connection)
        {
            for (const QString& srRef : selectedSrRefs)
            {
                QSharedPointer<SR> sr = this->m_connection->GetCache()->ResolveObject<SR>("sr", srRef);
                if (!sr || !sr->IsValid())
                    continue;
                if (sr->SupportsTrim() && !sr->IsDetached())
                {
                    canTrim = true;
                    break;
                }
            }
        }

        QAction* trimAction = menu.addAction(tr("Reclaim Freed Space..."));
        trimAction->setEnabled(canTrim);
        connect(trimAction, &QAction::triggered, this, &PhysicalStorageTabPage::onTrimButtonClicked);

        if (selectionCount == 1)
        {
            const QString srRef = selectedSrRefs.first();

            DetachSRCommand detachCmd(mainWindow);
            detachCmd.setTargetSR(srRef);
            QAction* detachAction = menu.addAction(tr("Detach Storage Repository"));
            detachAction->setEnabled(detachCmd.CanRun());
            connect(detachAction, &QAction::triggered, this, [mainWindow, srRef]()
            {
                DetachSRCommand cmd(mainWindow);
                cmd.setTargetSR(srRef);
                if (cmd.CanRun())
                    cmd.Run();
            });

            StoragePropertiesCommand propsCmd(mainWindow);
            propsCmd.setTargetSR(srRef, this->m_connection);
            QAction* propsAction = menu.addAction(tr("Properties..."));
            propsAction->setEnabled(propsCmd.CanRun());
            XenConnection *cn = this->m_connection;
            connect(propsAction, &QAction::triggered, this, [mainWindow, srRef, cn]()
            {
                StoragePropertiesCommand cmd(mainWindow);
                cmd.setTargetSR(srRef, cn);
                if (cmd.CanRun())
                    cmd.Run();
            });
        }
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
