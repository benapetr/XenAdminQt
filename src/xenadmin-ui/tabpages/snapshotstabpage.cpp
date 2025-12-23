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

#include "snapshotstabpage.h"
#include "ui_snapshotstabpage.h"
#include "../mainwindow.h"
#include "../commands/vm/takesnapshotcommand.h"
#include "../commands/vm/deletesnapshotcommand.h"
#include "../commands/vm/reverttosnapshotcommand.h"
#include "xenlib.h"
#include "xencache.h"
#include <QDateTime>
#include <QDebug>
#include <QMessageBox>
#include <QTimer>
#include <QMenu>

SnapshotsTabPage::SnapshotsTabPage(QWidget* parent)
    : BaseTabPage(parent), ui(new Ui::SnapshotsTabPage)
{
    this->ui->setupUi(this);

    // Configure snapshot tree
    this->ui->snapshotTree->setHeaderLabels(QStringList() << "Snapshot Name"
                                                          << "Created"
                                                          << "Description");
    this->ui->snapshotTree->setSelectionMode(QAbstractItemView::SingleSelection);
    this->ui->snapshotTree->setAlternatingRowColors(true);

    // Enable context menu
    this->ui->snapshotTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this->ui->snapshotTree, &QTreeWidget::customContextMenuRequested,
            this, &SnapshotsTabPage::onSnapshotContextMenu);

    // Connect signals
    connect(this->ui->takeSnapshotButton, &QPushButton::clicked, this, &SnapshotsTabPage::onTakeSnapshot);
    connect(this->ui->deleteSnapshotButton, &QPushButton::clicked, this, &SnapshotsTabPage::onDeleteSnapshot);
    connect(this->ui->revertButton, &QPushButton::clicked, this, &SnapshotsTabPage::onRevertToSnapshot);
    connect(this->ui->refreshButton, &QPushButton::clicked, this, &SnapshotsTabPage::refreshSnapshotList);
    connect(this->ui->snapshotTree, &QTreeWidget::itemSelectionChanged, this, &SnapshotsTabPage::onSnapshotSelectionChanged);

    updateButtonStates();
}

SnapshotsTabPage::~SnapshotsTabPage()
{
    delete this->ui;
}

void SnapshotsTabPage::setXenLib(XenLib* xenLib)
{
    BaseTabPage::setXenLib(xenLib);

    // Connect to virtualMachinesReceived signal for automatic refresh
    if (xenLib)
    {
        connect(xenLib, &XenLib::virtualMachinesReceived,
                this, &SnapshotsTabPage::onVirtualMachinesDataUpdated,
                Qt::UniqueConnection);
    }
}

bool SnapshotsTabPage::isApplicableForObjectType(const QString& objectType) const
{
    // Snapshots are only applicable to VMs
    return objectType == "vm";
}

void SnapshotsTabPage::refreshContent()
{
    if (this->m_objectData.isEmpty() || this->m_objectType != "vm")
    {
        this->ui->snapshotTree->clear();
        this->updateButtonStates();
        return;
    }

    this->populateSnapshotTree();
    this->updateButtonStates();
}

void SnapshotsTabPage::populateSnapshotTree()
{
    this->ui->snapshotTree->clear();

    if (!this->m_objectData.contains("snapshots") || !this->m_xenLib)
    {
        return;
    }

    // The "snapshots" field contains a list of snapshot references (OpaqueRefs)
    // We need to resolve each reference to get the actual snapshot data
    QVariantList snapshotRefs = this->m_objectData.value("snapshots").toList();

    for (const QVariant& refVariant : snapshotRefs)
    {
        QString snapshotRef = refVariant.toString();

        // Resolve the snapshot reference to get its full data
        QVariantMap snapshot = this->m_xenLib->getCache()->ResolveObjectData("vm", snapshotRef);

        if (snapshot.isEmpty())
        {
            qDebug() << "SnapshotsTabPage: Could not resolve snapshot ref:" << snapshotRef;
            continue;
        }

        // Verify this is actually a snapshot
        bool isSnapshot = snapshot.value("is_a_snapshot").toBool();
        if (!isSnapshot)
        {
            continue;
        }

        QTreeWidgetItem* item = new QTreeWidgetItem(this->ui->snapshotTree);

        // Set snapshot name
        QString name = snapshot.value("name_label").toString();
        item->setText(0, name.isEmpty() ? "Unnamed Snapshot" : name);

        // Set snapshot type based on power_state
        // If power_state is "Suspended", it's a disk+memory snapshot (checkpoint)
        // Otherwise it's a disk-only snapshot
        QString powerState = snapshot.value("power_state").toString();
        QString snapshotType = (powerState == "Suspended") ? tr("Disk and memory") : tr("Disks only");
        item->setText(1, snapshotType);

        // Format the timestamp
        QString timestamp = snapshot.value("snapshot_time").toString();
        if (!timestamp.isEmpty())
        {
            // Parse ISO 8601 timestamp from Xen API (format: 20240101T12:34:56Z)
            QDateTime dt = QDateTime::fromString(timestamp, Qt::ISODate);
            if (dt.isValid())
            {
                item->setText(2, dt.toString("yyyy-MM-dd HH:mm:ss"));
            } else
            {
                item->setText(2, timestamp);
            }
        }

        // Set description
        QString description = snapshot.value("name_description").toString();
        item->setText(3, description);

        // Store snapshot reference for operations
        item->setData(0, Qt::UserRole, snapshotRef);

        // Store power state for debugging
        item->setData(0, Qt::UserRole + 1, powerState);
    }

    // Resize columns to content
    for (int i = 0; i < 4; ++i)
    {
        this->ui->snapshotTree->resizeColumnToContents(i);
    }

    qDebug() << "SnapshotsTabPage: Populated" << this->ui->snapshotTree->topLevelItemCount() << "snapshots";
}

void SnapshotsTabPage::onTakeSnapshot()
{
    if (this->m_objectRef.isEmpty())
    {
        return;
    }

    // Get main window to execute command
    QWidget* window = this->window();
    if (!window)
    {
        return;
    }

    MainWindow* mainWindow = qobject_cast<MainWindow*>(window);
    if (!mainWindow)
    {
        return;
    }

    TakeSnapshotCommand* cmd = new TakeSnapshotCommand(this->m_objectRef, mainWindow);
    cmd->run();

    // No manual refresh needed - cache will be automatically updated via event polling
    // This matches C# behavior where SnapshotsPage relies on VM_BatchCollectionChanged events
}

void SnapshotsTabPage::onDeleteSnapshot()
{
    QList<QTreeWidgetItem*> selectedItems = this->ui->snapshotTree->selectedItems();
    if (selectedItems.isEmpty())
    {
        return;
    }

    QTreeWidgetItem* item = selectedItems.first();
    QString snapshotRef = item->data(0, Qt::UserRole).toString();
    QString snapshotName = item->text(0);

    if (snapshotRef.isEmpty())
    {
        return;
    }

    // Get main window to execute command
    QWidget* window = this->window();
    if (!window)
    {
        return;
    }

    MainWindow* mainWindow = qobject_cast<MainWindow*>(window);
    if (!mainWindow)
    {
        return;
    }

    DeleteSnapshotCommand* cmd = new DeleteSnapshotCommand(snapshotRef, mainWindow);
    cmd->run();

    // Refresh after a short delay to allow the operation to complete
    QTimer::singleShot(1000, this, &SnapshotsTabPage::refreshSnapshotList);
}

void SnapshotsTabPage::onRevertToSnapshot()
{
    QList<QTreeWidgetItem*> selectedItems = ui->snapshotTree->selectedItems();
    if (selectedItems.isEmpty())
    {
        return;
    }

    QTreeWidgetItem* item = selectedItems.first();
    QString snapshotRef = item->data(0, Qt::UserRole).toString();
    QString snapshotName = item->text(0);

    if (snapshotRef.isEmpty())
    {
        return;
    }

    // Confirm revert
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Revert to Snapshot",
        QString("Are you sure you want to revert to snapshot '%1'?\n\n"
                "This will restore the VM to the state when the snapshot was taken. "
                "The current state will be lost unless you take a new snapshot first.")
            .arg(snapshotName),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
    {
        return;
    }

    // Get main window to execute command
    QWidget* window = this->window();
    if (!window)
    {
        return;
    }

    MainWindow* mainWindow = qobject_cast<MainWindow*>(window);
    if (!mainWindow)
    {
        return;
    }

    RevertToSnapshotCommand* cmd = new RevertToSnapshotCommand(snapshotRef, mainWindow);
    cmd->run();

    // Refresh after a short delay to allow the operation to complete
    QTimer::singleShot(1000, this, &SnapshotsTabPage::refreshSnapshotList);
}

void SnapshotsTabPage::onSnapshotSelectionChanged()
{
    updateButtonStates();
}

void SnapshotsTabPage::refreshSnapshotList()
{
    if (!this->m_xenLib || this->m_objectRef.isEmpty())
    {
        return;
    }

    qDebug() << "SnapshotsTabPage: Manually refreshing snapshot list for VM:" << this->m_objectRef;

    // Invalidate the VM cache to force fresh data from server
    this->m_xenLib->getCache()->ClearType("vm");

    // Request fresh VM data - this will trigger virtualMachinesReceived signal
    this->m_xenLib->requestVirtualMachines();
}

void SnapshotsTabPage::onVirtualMachinesDataUpdated(QVariantList vms)
{
    // Check if the updated VMs include our current VM
    if (this->m_objectRef.isEmpty() || this->m_objectType != "vm")
    {
        return;
    }

    // Find our VM in the updated list
    for (const QVariant& vmVar : vms)
    {
        QVariantMap vm = vmVar.toMap();
        QString vmRef = vm.value("ref").toString();

        if (vmRef == this->m_objectRef)
        {
            qDebug() << "SnapshotsTabPage: Auto-refreshing snapshots for VM:" << vmRef;

            // Update our local object data
            this->m_objectData = vm;

            // Refresh the snapshot tree
            this->populateSnapshotTree();
            this->updateButtonStates();
            break;
        }
    }
}

void SnapshotsTabPage::updateButtonStates()
{
    bool hasSelection = !ui->snapshotTree->selectedItems().isEmpty();
    bool hasVM = !m_objectRef.isEmpty() && m_objectType == "vm";

    ui->takeSnapshotButton->setEnabled(hasVM);
    ui->deleteSnapshotButton->setEnabled(hasSelection);
    ui->revertButton->setEnabled(hasSelection);
}

void SnapshotsTabPage::onSnapshotContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = this->ui->snapshotTree->itemAt(pos);
    if (!item)
    {
        return;
    }

    QString snapshotRef = item->data(0, Qt::UserRole).toString();
    if (snapshotRef.isEmpty())
    {
        return;
    }

    // Create context menu
    QMenu menu(this);

    QAction* revertAction = menu.addAction("Revert to Snapshot");
    revertAction->setEnabled(true);

    QAction* deleteAction = menu.addAction("Delete Snapshot");
    deleteAction->setEnabled(true);

    // Show menu and handle selection
    QAction* selectedAction = menu.exec(this->ui->snapshotTree->mapToGlobal(pos));

    if (selectedAction == revertAction)
    {
        // Select the item and trigger revert
        this->ui->snapshotTree->setCurrentItem(item);
        this->onRevertToSnapshot();
    } else if (selectedAction == deleteAction)
    {
        // Select the item and trigger delete
        this->ui->snapshotTree->setCurrentItem(item);
        this->onDeleteSnapshot();
    }
}
