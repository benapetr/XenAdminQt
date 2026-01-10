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
#include "../commands/vm/newtemplatefromsnapshotcommand.h"
#include "../commands/vm/exportsnapshotastemplatecommand.h"
#include "../commands/vm/newvmfromsnapshotcommand.h"
#include "../dialogs/snapshotpropertiesdialog.h"
#include "../controls/snapshottreeview.h"
#include "../operations/operationmanager.h"
#include "xenlib/xen/actions/vm/vmsnapshotdeleteaction.h"
#include "xenlib/xen/actions/vm/vmsnapshotrevertaction.h"
#include "xenlib/xen/session.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/actions/vm/vmsnapshotcreateaction.h"
#include "xenlib/xen/xenapi/xenapi_Blob.h"
#include "xenlib/xen/xenapi/xenapi_VM.h"
#include "xenlib/xen/vm.h"
#include <QDateTime>
#include <QDebug>
#include <QMessageBox>
#include <QTimer>
#include <QMenu>
#include <QActionGroup>
#include <QHeaderView>
#include <QSet>
#include <QPainter>

QHash<QString, SnapshotsTabPage::SnapshotsView> SnapshotsTabPage::s_viewByVmRef;

SnapshotsTabPage::SnapshotsTabPage(QWidget* parent)
    : BaseTabPage(parent), ui(new Ui::SnapshotsTabPage)
{
    this->ui->setupUi(this);

    // Enable context menu
    this->ui->snapshotTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this->ui->snapshotTree, &QListWidget::customContextMenuRequested, this, &SnapshotsTabPage::onSnapshotContextMenu);

    // Connect signals
    connect(this->ui->takeSnapshotButton, &QPushButton::clicked, this, &SnapshotsTabPage::onTakeSnapshot);
    connect(this->ui->deleteSnapshotButton, &QPushButton::clicked, this, &SnapshotsTabPage::onDeleteSnapshot);
    connect(this->ui->revertButton, &QPushButton::clicked, this, &SnapshotsTabPage::onRevertToSnapshot);
    connect(this->ui->refreshButton, &QPushButton::clicked, this, &SnapshotsTabPage::refreshSnapshotList);
    connect(this->ui->snapshotTree, &QListWidget::itemSelectionChanged, this, &SnapshotsTabPage::onSnapshotSelectionChanged);
    connect(this->ui->snapshotTable, &QTableWidget::itemSelectionChanged, this, &SnapshotsTabPage::onSnapshotSelectionChanged);
    connect(this->ui->propertiesButton, &QPushButton::clicked, this, [this]()
    {
        QString snapshotRef = this->selectedSnapshotRef();
        if (snapshotRef.isEmpty() || !this->m_connection)
            return;
        QSharedPointer<VM> snapshot = this->m_connection->GetCache()->ResolveObject<VM>("vm", snapshotRef);
        SnapshotPropertiesDialog dialog(snapshot, this);
        dialog.exec();
    });

    this->ui->snapshotTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this->ui->snapshotTable, &QTableWidget::customContextMenuRequested, this, &SnapshotsTabPage::onSnapshotContextMenu);

    this->ui->snapshotTable->horizontalHeader()->setStretchLastSection(true);
    this->ui->snapshotTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    this->ui->snapshotTable->verticalHeader()->setVisible(false);
    this->ui->snapshotTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->ui->snapshotTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->ui->snapshotTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    this->ui->snapshotTree->setSelectionMode(QAbstractItemView::ExtendedSelection);

    auto* operationManager = OperationManager::instance();
    connect(operationManager, &OperationManager::recordAdded, this, &SnapshotsTabPage::onOperationRecordUpdated);
    connect(operationManager, &OperationManager::recordUpdated, this, &SnapshotsTabPage::onOperationRecordUpdated);
    connect(operationManager, &OperationManager::recordRemoved, this, &SnapshotsTabPage::onOperationRecordUpdated);

    QMenu* viewMenu = new QMenu(this);
    this->m_treeViewAction = viewMenu->addAction(tr("Tree View"));
    this->m_treeViewAction->setCheckable(true);
    this->m_treeViewAction->setChecked(true);
    this->m_listViewAction = viewMenu->addAction(tr("List View"));
    this->m_listViewAction->setCheckable(true);
    viewMenu->addSeparator();
    this->m_scheduledSnapshotsAction = viewMenu->addAction(tr("Scheduled snapshots"));
    this->m_scheduledSnapshotsAction->setCheckable(true);
    this->m_scheduledSnapshotsAction->setChecked(true);

    QActionGroup* viewGroup = new QActionGroup(this);
    viewGroup->addAction(this->m_treeViewAction);
    viewGroup->addAction(this->m_listViewAction);
    this->ui->viewButton->setMenu(viewMenu);

    connect(this->m_treeViewAction, &QAction::triggered, this, [this]() {
        this->setViewMode(SnapshotsView::TreeView);
    });
    connect(this->m_listViewAction, &QAction::triggered, this, [this]() {
        this->setViewMode(SnapshotsView::ListView);
    });
    connect(this->m_scheduledSnapshotsAction, &QAction::triggered, this, &SnapshotsTabPage::onScheduledSnapshotsToggled);

    this->m_sortByTypeAction = new QAction(tr("Type"), this);
    this->m_sortByNameAction = new QAction(tr("Name"), this);
    this->m_sortByCreatedAction = new QAction(tr("Created"), this);
    this->m_sortBySizeAction = new QAction(tr("Size"), this);

    connect(this->m_sortByTypeAction, &QAction::triggered, this, [this]() {
        this->ui->snapshotTable->sortItems(0, Qt::AscendingOrder);
    });
    connect(this->m_sortByNameAction, &QAction::triggered, this, [this]() {
        this->ui->snapshotTable->sortItems(1, Qt::AscendingOrder);
    });
    connect(this->m_sortByCreatedAction, &QAction::triggered, this, [this]() {
        this->ui->snapshotTable->sortItems(2, Qt::AscendingOrder);
    });
    connect(this->m_sortBySizeAction, &QAction::triggered, this, [this]() {
        this->ui->snapshotTable->sortItems(3, Qt::AscendingOrder);
    });

    this->updateButtonStates();
    this->showDisabledDetails();
}

SnapshotsTabPage::~SnapshotsTabPage()
{
    delete this->ui;
}

void SnapshotsTabPage::removeObject()
{
    if (!this->m_connection)
        return;

    XenCache* cache = this->m_connection->GetCache();
    disconnect(cache, &XenCache::objectChanged, this, &SnapshotsTabPage::onCacheObjectChanged);
}

void SnapshotsTabPage::updateObject()
{
    XenCache* cache = this->m_connection ? this->m_connection->GetCache() : nullptr;
    connect(cache, &XenCache::objectChanged, this, &SnapshotsTabPage::onCacheObjectChanged, Qt::UniqueConnection);
    if (!this->m_objectRef.isEmpty())
        this->setViewMode(this->s_viewByVmRef.value(this->m_objectRef, SnapshotsView::TreeView));
}

bool SnapshotsTabPage::IsApplicableForObjectType(const QString& objectType) const
{
    // Snapshots are only applicable to VMs
    return objectType == "vm";
}

void SnapshotsTabPage::refreshContent()
{
    if (this->m_objectData.isEmpty() || this->m_objectType != "vm")
    {
        this->ui->snapshotTree->Clear();
        this->ui->snapshotTable->setRowCount(0);
        this->updateButtonStates();
        return;
    }

    if (!this->m_objectRef.isEmpty())
        this->setViewMode(this->s_viewByVmRef.value(this->m_objectRef, SnapshotsView::TreeView));

    this->refreshVmssPanel();
    this->populateSnapshotTree();
    this->updateButtonStates();
    this->updateDetailsPanel(true);
    this->updateSpinningIcon();
}

void SnapshotsTabPage::populateSnapshotTree()
{
    SnapshotTreeView* tree = this->ui->snapshotTree;
    tree->setUpdatesEnabled(false);
    tree->Clear();
    this->ui->snapshotTable->setRowCount(0);

    if (!this->m_objectData.contains("snapshots") || !this->m_connection || !this->m_connection->GetCache())
    {
        tree->setUpdatesEnabled(true);
        return;
    }

    QVariantList snapshotRefs = this->m_objectData.value("snapshots").toList();
    if (snapshotRefs.isEmpty())
    {
        tree->setUpdatesEnabled(true);
        return;
    }

    XenCache* cache = this->m_connection->GetCache();
    QHash<QString, QVariantMap> snapshots;
    QSet<QString> snapshotRefSet;

    for (const QVariant& refVariant : snapshotRefs)
    {
        QString snapshotRef = refVariant.toString();
        if (snapshotRef.isEmpty())
            continue;

        QVariantMap snapshot = cache->ResolveObjectData("vm", snapshotRef);
        if (snapshot.isEmpty() || !snapshot.value("is_a_snapshot").toBool())
            continue;

        snapshots.insert(snapshotRef, snapshot);
        snapshotRefSet.insert(snapshotRef);
    }

    if (snapshots.isEmpty())
    {
        tree->setUpdatesEnabled(true);
        return;
    }

    int row = 0;
    for (auto it = snapshots.constBegin(); it != snapshots.constEnd(); ++it)
    {
        const QString snapshotRef = it.key();
        const QVariantMap snapshot = it.value();
        if (!this->shouldShowSnapshot(snapshot))
            continue;

        const QString powerState = snapshot.value("power_state").toString();
        const bool isSuspended = powerState == "Suspended";
        const QString typeText = isSuspended ? tr("Disk and memory") : tr("Disks only");

        QString createdText;
        const QString timestamp = snapshot.value("snapshot_time").toString();
        if (!timestamp.isEmpty())
        {
            QDateTime dt = QDateTime::fromString(timestamp, Qt::ISODate);
            if (dt.isValid())
            {
                dt = dt.toLocalTime().addSecs(this->m_connection->GetServerTimeOffsetSeconds());
                createdText = dt.toString("yyyy-MM-dd HH:mm:ss");
            } else
            {
                createdText = timestamp;
            }
        }

        QString nameText = snapshot.value("name_label").toString();
        if (nameText.isEmpty())
            nameText = tr("Unnamed Snapshot");

        this->ui->snapshotTable->insertRow(row);
        QTableWidgetItem* typeItem = new QTableWidgetItem(typeText);
        QTableWidgetItem* nameItem = new QTableWidgetItem(nameText);
        QTableWidgetItem* createdItem = new QTableWidgetItem(createdText);
        QTableWidgetItem* sizeItem = new QTableWidgetItem(QString());

        QStringList tags;
        const QVariantList tagList = snapshot.value("tags").toList();
        for (const QVariant& tagVar : tagList)
        {
            const QString tag = tagVar.toString();
            if (!tag.isEmpty())
                tags.append(tag);
        }
        QTableWidgetItem* tagsItem = new QTableWidgetItem(tags.join(", "));

        typeItem->setData(Qt::UserRole, snapshotRef);
        nameItem->setData(Qt::UserRole, snapshotRef);
        createdItem->setData(Qt::UserRole, snapshotRef);
        sizeItem->setData(Qt::UserRole, snapshotRef);
        tagsItem->setData(Qt::UserRole, snapshotRef);

        this->ui->snapshotTable->setItem(row, 0, typeItem);
        this->ui->snapshotTable->setItem(row, 1, nameItem);
        this->ui->snapshotTable->setItem(row, 2, createdItem);
        this->ui->snapshotTable->setItem(row, 3, sizeItem);
        this->ui->snapshotTable->setItem(row, 4, tagsItem);
        ++row;
    }

    QSet<QString> childRefs;
    QMultiHash<QString, QString> childrenByParent;
    for (auto it = snapshots.constBegin(); it != snapshots.constEnd(); ++it)
    {
        const QString parentRef = it.key();
        QVariantList children = it.value().value("children").toList();
        for (const QVariant& childVar : children)
        {
            const QString childRef = childVar.toString();
            if (!snapshotRefSet.contains(childRef))
                continue;

            childrenByParent.insert(parentRef, childRef);
            childRefs.insert(childRef);
        }
    }

    QList<QString> roots;
    for (const QString& snapshotRef : snapshotRefSet)
    {
        if (!childRefs.contains(snapshotRef))
            roots.append(snapshotRef);
    }

    const QString vmName = this->m_objectData.value("name_label").toString();
    SnapshotIcon* rootIcon = new SnapshotIcon(vmName.isEmpty() ? tr("VM") : vmName,
                                              tr("Base"), nullptr, tree, SnapshotIcon::Template);
    tree->AddSnapshot(rootIcon);

    bool parentIsSnapshot = false;
    const QString parentRef = this->m_objectData.value("parent").toString();
    if (!parentRef.isEmpty())
    {
        QVariantMap parentData = cache->ResolveObjectData("vm", parentRef);
        parentIsSnapshot = parentData.value("is_a_snapshot").toBool();
    }

    if (!parentIsSnapshot)
    {
        SnapshotIcon* vmIcon = new SnapshotIcon(tr("Now"), QString(), rootIcon, tree, SnapshotIcon::VMImageIndex);
        tree->AddSnapshot(vmIcon);
    }

    for (const QString& rootRef : roots)
    {
        this->buildSnapshotTree(rootRef, rootIcon, snapshots, childrenByParent);
    }

    tree->setUpdatesEnabled(true);
    if (tree->selectedItems().isEmpty())
        tree->setCurrentItem(rootIcon);
    tree->update();
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
    cmd->Run();

    // No manual refresh needed - cache will be automatically updated via event polling
    // This matches C# behavior where SnapshotsPage relies on VM_BatchCollectionChanged events
}

void SnapshotsTabPage::onDeleteSnapshot()
{
    QList<QString> snapshotRefs = this->selectedSnapshotRefs();

    if (snapshotRefs.isEmpty())
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

    if (snapshotRefs.size() == 1)
    {
        DeleteSnapshotCommand* cmd = new DeleteSnapshotCommand(snapshotRefs.first(), mainWindow);
        cmd->Run();
        QTimer::singleShot(1000, this, &SnapshotsTabPage::refreshSnapshotList);
        return;
    }

    if (!this->m_connection || !this->m_connection->IsConnected())
    {
        QMessageBox::critical(this, tr("Delete Error"), tr("Not connected to XenServer."));
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Delete Snapshots"),
        tr("Are you sure you want to delete %1 snapshots?\n\nThis action cannot be undone.")
            .arg(snapshotRefs.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    for (const QString& ref : snapshotRefs)
    {
        VMSnapshotDeleteAction* action = new VMSnapshotDeleteAction(this->m_connection, ref, this);
        OperationManager::instance()->RegisterOperation(action);
        connect(action, &AsyncOperation::completed, action, &QObject::deleteLater);
        action->RunAsync();
    }
}

void SnapshotsTabPage::onRevertToSnapshot()
{
    QString snapshotName;
    QString snapshotRef = this->selectedSnapshotRef(&snapshotName);

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
    cmd->Run();

    // Refresh after a short delay to allow the operation to complete
    QTimer::singleShot(1000, this, &SnapshotsTabPage::refreshSnapshotList);
}

void SnapshotsTabPage::onSnapshotSelectionChanged()
{
    updateButtonStates();
    updateDetailsPanel();
    updateSpinningIcon();
}

void SnapshotsTabPage::refreshSnapshotList()
{
    if (!this->m_connection || !this->m_connection->GetCache() || this->m_objectRef.isEmpty())
    {
        return;
    }

    qDebug() << "SnapshotsTabPage: Manually refreshing snapshot list for VM:" << this->m_objectRef;

    XenAPI::Session* session = this->m_connection->GetSession();
    if (!session || !session->IsLoggedIn())
    {
        qWarning() << "SnapshotsTabPage: No active session for refresh";
        return;
    }

    try
    {
        QVariantMap records = XenAPI::VM::get_all_records(session);
        this->m_connection->GetCache()->UpdateBulk("vm", records);
    }
    catch (const std::exception& ex)
    {
        qWarning() << "SnapshotsTabPage: Failed to refresh VM records:" << ex.what();
    }
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

void SnapshotsTabPage::onCacheObjectChanged(XenConnection* connection, const QString& type, const QString& ref)
{
    (void) connection;
    (void) ref;

    if (this->m_objectType == "vm" && (type == "vm" || type == "vdi" || type == "vbd"))
    {
        this->populateSnapshotTree();
        this->updateButtonStates();
        this->updateDetailsPanel(true);
        this->updateSpinningIcon();
    }

    if (this->m_objectType == "vm" && (type == "vm" || type == "vmss"))
        this->refreshVmssPanel();
}

void SnapshotsTabPage::updateButtonStates()
{
    const QList<QString> refs = this->selectedSnapshotRefs();
    const bool hasVM = !m_objectRef.isEmpty() && m_objectType == "vm";

    MainWindow* mainWindow = qobject_cast<MainWindow*>(this->window());

    bool canTake = false;
    if (hasVM)
    {
        canTake = true;
        if (mainWindow)
        {
            TakeSnapshotCommand takeCmd(this->m_objectRef, mainWindow);
            canTake = takeCmd.CanRun();
        }
    }

    bool canDelete = false;
    bool canRevert = false;
    if (mainWindow && refs.size() == 1)
    {
        DeleteSnapshotCommand deleteCmd(refs.first(), mainWindow);
        RevertToSnapshotCommand revertCmd(refs.first(), mainWindow);
        canDelete = deleteCmd.CanRun();
        canRevert = revertCmd.CanRun();
    }
    else if (refs.size() > 1)
    {
        canDelete = this->canDeleteSnapshots(refs);
    }

    ui->takeSnapshotButton->setEnabled(canTake);
    ui->deleteSnapshotButton->setEnabled(canDelete);
    ui->revertButton->setEnabled(canRevert);
}

void SnapshotsTabPage::onOperationRecordUpdated(OperationManager::OperationRecord*)
{
    updateSpinningIcon();
}

void SnapshotsTabPage::updateDetailsPanel(bool force)
{
    QList<QString> refs = this->selectedSnapshotRefs();
    if (!this->m_connection || refs.isEmpty())
    {
        this->showDisabledDetails();
        return;
    }

    XenCache* cache = this->m_connection->GetCache();
    if (!cache)
    {
        this->showDisabledDetails();
        return;
    }

    QList<QVariantMap> snapshots;
    for (const QString& ref : refs)
    {
        QVariantMap snapshot = cache->ResolveObjectData("vm", ref);
        if (!snapshot.isEmpty() && snapshot.value("is_a_snapshot").toBool())
            snapshots.append(snapshot);
    }

    if (snapshots.isEmpty())
    {
        this->showDisabledDetails();
    }
    else if (snapshots.size() == 1)
    {
        this->showDetailsForSnapshot(snapshots.first(), force);
    }
    else
    {
        this->showDetailsForMultiple(snapshots);
    }
}

void SnapshotsTabPage::showDisabledDetails()
{
    this->ui->detailsGroupBox->setEnabled(false);
    this->ui->detailsGroupBox->setTitle(tr("Snapshot created on"));
    this->ui->snapshotNameLabel->clear();
    this->ui->descriptionValueLabel->clear();
    this->ui->modeValueLabel->clear();
    this->ui->sizeValueLabel->clear();
    this->ui->tagsValueLabel->clear();
    this->ui->folderValueLabel->clear();
    this->ui->customFieldTitleLabel1->clear();
    this->ui->customFieldValueLabel1->clear();
    this->ui->customFieldTitleLabel2->clear();
    this->ui->customFieldValueLabel2->clear();
    this->ui->propertiesButton->setEnabled(false);
    this->ui->screenshotLabel->setPixmap(this->noScreenshotPixmap());
}

void SnapshotsTabPage::showDetailsForSnapshot(const QVariantMap& snapshot, bool force)
{
    (void) force;
    this->ui->detailsGroupBox->setEnabled(true);

    QString createdText;
    const QString timestamp = snapshot.value("snapshot_time").toString();
    if (!timestamp.isEmpty())
    {
        QDateTime dt = QDateTime::fromString(timestamp, Qt::ISODate);
        if (dt.isValid())
        {
            dt = dt.toLocalTime().addSecs(this->m_connection->GetServerTimeOffsetSeconds());
            createdText = dt.toString("yyyy-MM-dd HH:mm:ss");
        } else
        {
            createdText = timestamp;
        }
    }
    this->ui->detailsGroupBox->setTitle(tr("Snapshot created on %1").arg(createdText));

    QString nameText = snapshot.value("name_label").toString();
    if (nameText.isEmpty())
        nameText = tr("Snapshot");
    this->ui->snapshotNameLabel->setText(nameText);

    const QString powerState = snapshot.value("power_state").toString();
    const bool isSuspended = powerState == "Suspended";
    this->ui->modeValueLabel->setText(isSuspended ? tr("Disks and memory") : tr("Disks only"));

    const QString description = snapshot.value("name_description").toString();
    this->ui->descriptionValueLabel->setText(description.isEmpty() ? tr("<None>") : description);

    const qint64 sizeBytes = this->snapshotSizeBytes(snapshot);
    this->ui->sizeValueLabel->setText(sizeBytes > 0 ? this->formatSize(sizeBytes) : tr("<None>"));

    QStringList tags;
    const QVariantList tagList = snapshot.value("tags").toList();
    for (const QVariant& tagVar : tagList)
    {
        const QString tag = tagVar.toString();
        if (!tag.isEmpty())
            tags.append(tag);
    }
    this->ui->tagsValueLabel->setText(tags.isEmpty() ? tr("<None>") : tags.join(", "));

    const QVariantMap otherConfig = snapshot.value("other_config").toMap();
    const QString folderPath = otherConfig.value("folder").toString();
    this->ui->folderValueLabel->setText(folderPath.isEmpty() ? tr("<None>") : folderPath);

    QList<QPair<QString, QString>> customFields;
    for (auto it = otherConfig.constBegin(); it != otherConfig.constEnd(); ++it)
    {
        if (!it.key().startsWith("XenCenter.CustomFields."))
            continue;
        QString name = it.key().mid(23);
        if (!name.isEmpty())
            customFields.append(qMakePair(name, it.value().toString()));
    }
    std::sort(customFields.begin(), customFields.end(), [](const auto& a, const auto& b) {
        return a.first.toLower() < b.first.toLower();
    });

    this->ui->customFieldTitleLabel1->clear();
    this->ui->customFieldValueLabel1->clear();
    this->ui->customFieldTitleLabel2->clear();
    this->ui->customFieldValueLabel2->clear();

    if (!customFields.isEmpty())
    {
        this->ui->customFieldTitleLabel1->setText(customFields.at(0).first + ":");
        this->ui->customFieldValueLabel1->setText(customFields.at(0).second);
        if (customFields.size() > 1)
        {
            this->ui->customFieldTitleLabel2->setText(customFields.at(1).first + ":");
            this->ui->customFieldValueLabel2->setText(customFields.at(1).second);
        }
    }

    QPixmap screenshot = this->noScreenshotPixmap();
    const QVariantMap blobs = snapshot.value("blobs").toMap();
    const QString blobRef = blobs.value(VMSnapshotCreateAction::VNC_SNAPSHOT_NAME).toString();
    if (!blobRef.isEmpty() && this->m_connection)
    {
        XenAPI::Session* session = this->m_connection->GetSession();
        if (session && session->IsLoggedIn())
        {
            try
            {
                const QByteArray data = XenAPI::Blob::load(session, blobRef);
                QPixmap loaded;
                if (loaded.loadFromData(data, "JPEG"))
                    screenshot = loaded;
            }
            catch (const std::exception& ex)
            {
                qWarning() << "SnapshotsTabPage: Failed to load snapshot screenshot:" << ex.what();
            }
        }
    }
    this->ui->screenshotLabel->setPixmap(screenshot);
    this->ui->propertiesButton->setEnabled(true);
}

void SnapshotsTabPage::showDetailsForMultiple(const QList<QVariantMap>& snapshots)
{
    if (snapshots.isEmpty())
    {
        this->showDisabledDetails();
        return;
    }

    this->ui->detailsGroupBox->setEnabled(true);
    this->ui->snapshotNameLabel->setText(tr("%1 snapshots selected").arg(snapshots.size()));

    qint64 totalSize = 0;
    QStringList tags;
    QDateTime earliest = QDateTime::currentDateTime();
    QDateTime latest = QDateTime::fromSecsSinceEpoch(0);

    for (const QVariantMap& snapshot : snapshots)
    {
        totalSize += this->snapshotSizeBytes(snapshot);

        const QVariantList tagList = snapshot.value("tags").toList();
        for (const QVariant& tagVar : tagList)
        {
            const QString tag = tagVar.toString();
            if (!tag.isEmpty() && !tags.contains(tag))
                tags.append(tag);
        }

        const QString timestamp = snapshot.value("snapshot_time").toString();
        QDateTime dt = QDateTime::fromString(timestamp, Qt::ISODate);
        if (dt.isValid())
        {
            dt = dt.toLocalTime().addSecs(this->m_connection->GetServerTimeOffsetSeconds());
            if (!earliest.isValid() || dt < earliest)
                earliest = dt;
            if (!latest.isValid() || dt > latest)
                latest = dt;
        }
    }

    QString rangeText;
    if (earliest.isValid() && latest.isValid())
        rangeText = tr("%1 - %2").arg(earliest.toString("yyyy-MM-dd HH:mm:ss"),
                                      latest.toString("yyyy-MM-dd HH:mm:ss"));
    this->ui->detailsGroupBox->setTitle(rangeText.isEmpty() ? tr("Snapshots") : rangeText);

    this->ui->descriptionValueLabel->setText(tr("<None>"));
    this->ui->modeValueLabel->setText(tr("<Multiple>"));
    this->ui->sizeValueLabel->setText(totalSize > 0 ? this->formatSize(totalSize) : tr("<None>"));
    this->ui->tagsValueLabel->setText(tags.isEmpty() ? tr("<None>") : tags.join(", "));
    this->ui->folderValueLabel->setText(tr("<Multiple>"));
    this->ui->customFieldTitleLabel1->clear();
    this->ui->customFieldValueLabel1->clear();
    this->ui->customFieldTitleLabel2->clear();
    this->ui->customFieldValueLabel2->clear();
    this->ui->propertiesButton->setEnabled(false);
    this->ui->screenshotLabel->setPixmap(this->noScreenshotPixmap());
}

QList<QString> SnapshotsTabPage::selectedSnapshotRefs() const
{
    QList<QString> refs;
    if (this->ui->viewStack->currentIndex() == 0)
    {
        for (QListWidgetItem* item : this->ui->snapshotTree->selectedItems())
        {
            auto* icon = dynamic_cast<SnapshotIcon*>(item);
            if (icon && icon->IsSelectable())
                refs.append(icon->data(Qt::UserRole).toString());
        }
    }
    else
    {
        QList<QTableWidgetItem*> selection = this->ui->snapshotTable->selectedItems();
        for (QTableWidgetItem* item : selection)
        {
            const QString ref = item->data(Qt::UserRole).toString();
            if (!ref.isEmpty() && !refs.contains(ref))
                refs.append(ref);
        }
    }
    return refs;
}

bool SnapshotsTabPage::canDeleteSnapshots(const QList<QString>& snapshotRefs) const
{
    if (snapshotRefs.isEmpty() || !this->m_connection)
        return false;

    XenCache* cache = this->m_connection->GetCache();
    if (!cache)
        return false;

    for (const QString& ref : snapshotRefs)
    {
        QVariantMap snapshotData = cache->ResolveObjectData("vm", ref);
        if (snapshotData.isEmpty())
            return false;
        if (!snapshotData.value("is_a_snapshot").toBool())
            return false;
        if (!snapshotData.value("current_operations").toList().isEmpty())
            return false;
        if (!snapshotData.value("allowed_operations").toList().contains("destroy"))
            return false;
    }

    return true;
}

void SnapshotsTabPage::updateSpinningIcon()
{
    if (!this->ui->snapshotTree)
        return;

    bool spinning = false;
    QString message;

    const QList<OperationManager::OperationRecord*>& records = OperationManager::instance()->GetRecords();
    for (OperationManager::OperationRecord* record : records)
    {
        if (!record || !record->operation)
            continue;

        if (record->state == AsyncOperation::Completed ||
            record->state == AsyncOperation::Cancelled ||
            record->state == AsyncOperation::Failed)
        {
            continue;
        }

        QString candidateMessage;
        if (!isSpinningActionForCurrentVm(record->operation, &candidateMessage))
            continue;

        spinning = true;
        if (message.isEmpty())
            message = candidateMessage;
        if (candidateMessage == tr("Snapshotting..."))
            break; // Prefer snapshot create message if both are running.
    }

    this->ui->snapshotTree->ChangeVMToSpinning(spinning, message);
}

bool SnapshotsTabPage::isSpinningActionForCurrentVm(AsyncOperation* operation, QString* message) const
{
    if (!operation || this->m_objectRef.isEmpty() || this->m_objectType != "vm")
        return false;

    if (auto* createAction = qobject_cast<VMSnapshotCreateAction*>(operation))
    {
        if (createAction->vmRef() != this->m_objectRef)
            return false;
        if (message)
            *message = tr("Snapshotting...");
        return true;
    }

    if (auto* revertAction = qobject_cast<VMSnapshotRevertAction*>(operation))
    {
        if (revertAction->vmRef() != this->m_objectRef)
            return false;
        if (message)
            *message = tr("Reverting VM...");
        return true;
    }

    return false;
}

qint64 SnapshotsTabPage::snapshotSizeBytes(const QVariantMap& snapshot) const
{
    if (!this->m_connection || !this->m_connection->GetCache())
        return 0;

    XenCache* cache = this->m_connection->GetCache();
    qint64 total = 0;

    const QVariantList vbdRefs = snapshot.value("VBDs").toList();
    for (const QVariant& vbdVar : vbdRefs)
    {
        const QString vbdRef = vbdVar.toString();
        if (vbdRef.isEmpty())
            continue;

        QVariantMap vbd = cache->ResolveObjectData("vbd", vbdRef);
        if (vbd.isEmpty())
            continue;

        if (vbd.value("type").toString() != "Disk")
            continue;

        const QString vdiRef = vbd.value("VDI").toString();
        if (vdiRef.isEmpty())
            continue;

        QVariantMap vdi = cache->ResolveObjectData("vdi", vdiRef);
        if (vdi.isEmpty())
            continue;

        qint64 utilisation = vdi.value("physical_utilisation").toLongLong();
        if (utilisation <= 0)
            utilisation = vdi.value("physical_utilization").toLongLong();
        if (utilisation > 0)
            total += utilisation;
    }

    const QString suspendVdiRef = snapshot.value("suspend_VDI").toString();
    if (!suspendVdiRef.isEmpty())
    {
        QVariantMap vdi = cache->ResolveObjectData("vdi", suspendVdiRef);
        qint64 utilisation = vdi.value("physical_utilisation").toLongLong();
        if (utilisation <= 0)
            utilisation = vdi.value("physical_utilization").toLongLong();
        if (utilisation > 0)
            total += utilisation;
    }

    return total;
}

QString SnapshotsTabPage::formatSize(qint64 bytes) const
{
    if (bytes <= 0)
        return tr("<None>");

    const double kb = 1024.0;
    const double mb = kb * 1024.0;
    const double gb = mb * 1024.0;
    const double tb = gb * 1024.0;

    if (bytes >= tb)
        return QString("%1 TB").arg(bytes / tb, 0, 'f', 2);
    if (bytes >= gb)
        return QString("%1 GB").arg(bytes / gb, 0, 'f', 2);
    if (bytes >= mb)
        return QString("%1 MB").arg(bytes / mb, 0, 'f', 2);
    if (bytes >= kb)
        return QString("%1 KB").arg(bytes / kb, 0, 'f', 2);
    return QString("%1 B").arg(bytes);
}

QPixmap SnapshotsTabPage::noScreenshotPixmap() const
{
    const int width = 100;
    const int height = 75;
    QPixmap pixmap(width, height);
    pixmap.fill(Qt::black);

    QPainter painter(&pixmap);
    painter.setPen(Qt::white);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, tr("No screenshot"));
    return pixmap;
}

SnapshotsTabPage::SnapshotsView SnapshotsTabPage::currentViewMode() const
{
    return this->ui->viewStack->currentIndex() == 0 ? SnapshotsView::TreeView : SnapshotsView::ListView;
}

void SnapshotsTabPage::setViewMode(SnapshotsView view)
{
    if (view == SnapshotsView::TreeView)
    {
        this->ui->viewStack->setCurrentIndex(0);
        this->ui->snapshotTree->SetTreeMode(true);
        if (this->m_treeViewAction)
            this->m_treeViewAction->setChecked(true);
    }
    else
    {
        this->ui->viewStack->setCurrentIndex(1);
        if (this->m_listViewAction)
            this->m_listViewAction->setChecked(true);
    }

    if (!this->m_objectRef.isEmpty())
        this->s_viewByVmRef[this->m_objectRef] = view;

    this->updateButtonStates();
}

QString SnapshotsTabPage::selectedSnapshotRef(QString* snapshotName) const
{
    if (this->ui->viewStack->currentIndex() == 0)
    {
        if (ui->snapshotTree->selectedItems().isEmpty())
            return QString();

        auto* icon = dynamic_cast<SnapshotIcon*>(ui->snapshotTree->selectedItems().first());
        if (!icon || !icon->IsSelectable())
            return QString();

        if (snapshotName)
            *snapshotName = icon->text();

        return icon->data(Qt::UserRole).toString();
    }

    QList<QTableWidgetItem*> selection = this->ui->snapshotTable->selectedItems();
    if (selection.isEmpty())
        return QString();

    QTableWidgetItem* item = selection.first();
    if (snapshotName)
        *snapshotName = this->ui->snapshotTable->item(item->row(), 1)->text();

    return item->data(Qt::UserRole).toString();
}

void SnapshotsTabPage::onSnapshotContextMenu(const QPoint& pos)
{
    const bool treeView = this->ui->viewStack->currentIndex() == 0;
    QString snapshotRef;
    if (treeView)
    {
        QListWidgetItem* item = this->ui->snapshotTree->itemAt(pos);
        auto* icon = dynamic_cast<SnapshotIcon*>(item);
        if (icon && icon->IsSelectable())
        {
            if (!item->isSelected())
            {
                this->ui->snapshotTree->setCurrentItem(item);
            }
            snapshotRef = icon->data(Qt::UserRole).toString();
        }
    } else
    {
        QTableWidgetItem* item = this->ui->snapshotTable->itemAt(pos);
        if (item)
        {
            if (!item->isSelected())
            {
                this->ui->snapshotTable->selectRow(item->row());
            }
            snapshotRef = item->data(Qt::UserRole).toString();
        }
    }
    QSharedPointer<VM> snapshot = this->m_connection->GetCache()->ResolveObject<VM>("vm", snapshotRef);

    QMenu menu(this);
    QAction* takeSnapshotAction = menu.addAction(tr("Take Snapshot..."));
    QAction* revertAction = menu.addAction(tr("Revert to Snapshot..."));
    QMenu* saveMenu = menu.addMenu(tr("Save"));
    QAction* saveVmAction = saveMenu->addAction(tr("New VM from Snapshot..."));
    QAction* saveTemplateAction = saveMenu->addAction(tr("New Template from Snapshot..."));
    QAction* exportAction = saveMenu->addAction(tr("Export Snapshot as Template..."));
    menu.addSeparator();

    QMenu* viewMenu = menu.addMenu(tr("View"));
    viewMenu->addAction(this->m_treeViewAction);
    viewMenu->addAction(this->m_listViewAction);
    viewMenu->addSeparator();
    viewMenu->addAction(this->m_scheduledSnapshotsAction);

    if (!treeView)
    {
        QMenu* sortMenu = menu.addMenu(tr("Sort By"));
        sortMenu->addAction(this->m_sortByTypeAction);
        sortMenu->addAction(this->m_sortByNameAction);
        sortMenu->addAction(this->m_sortByCreatedAction);
        sortMenu->addAction(this->m_sortBySizeAction);
    }

    menu.addSeparator();
    QAction* deleteAction = menu.addAction(tr("Delete Snapshot"));
    QAction* propertiesAction = menu.addAction(tr("Properties..."));

    bool canRevert = false;
    bool canDelete = false;
    bool canSave = false;
    bool canProperties = false;
    bool canTake = false;

    MainWindow* mainWindow = qobject_cast<MainWindow*>(this->window());
    if (mainWindow && !snapshotRef.isEmpty())
    {
        RevertToSnapshotCommand revertCmd(snapshotRef, mainWindow);
        DeleteSnapshotCommand deleteCmd(snapshotRef, mainWindow);
        NewVMFromSnapshotCommand newVmCmd(snapshotRef, this->m_connection, mainWindow);
        NewTemplateFromSnapshotCommand newTemplateCmd(snapshotRef, this->m_connection, mainWindow);
        ExportSnapshotAsTemplateCommand exportCmd(snapshotRef, this->m_connection, mainWindow);

        canRevert = revertCmd.CanRun();
        canDelete = deleteCmd.CanRun();
        canSave = newVmCmd.CanRun() || newTemplateCmd.CanRun() || exportCmd.CanRun();
        canProperties = true;
    }

    if (mainWindow && this->m_objectType == "vm" && !this->m_objectRef.isEmpty())
    {
        TakeSnapshotCommand takeCmd(this->m_objectRef, mainWindow);
        canTake = takeCmd.CanRun();
    }

    takeSnapshotAction->setEnabled(canTake);
    revertAction->setEnabled(canRevert);
    saveMenu->setEnabled(canSave);
    deleteAction->setEnabled(canDelete);
    propertiesAction->setEnabled(canProperties);

    QWidget* menuAnchor = treeView
        ? static_cast<QWidget*>(this->ui->snapshotTree)
        : static_cast<QWidget*>(this->ui->snapshotTable);
    QAction* selectedAction = menu.exec(menuAnchor->mapToGlobal(pos));

    if (selectedAction == takeSnapshotAction)
    {
        this->onTakeSnapshot();
    }
    else if (selectedAction == revertAction)
    {
        this->onRevertToSnapshot();
    }
    else if (selectedAction == saveVmAction)
    {
        QWidget* window = this->window();
        MainWindow* mainWindow = window ? qobject_cast<MainWindow*>(window) : nullptr;
        if (mainWindow)
        {
            NewVMFromSnapshotCommand cmd(snapshotRef, this->m_connection, mainWindow);
            cmd.Run();
        }
    }
    else if (selectedAction == saveTemplateAction)
    {
        QWidget* window = this->window();
        MainWindow* mainWindow = window ? qobject_cast<MainWindow*>(window) : nullptr;
        if (mainWindow)
        {
            NewTemplateFromSnapshotCommand cmd(snapshotRef, this->m_connection, mainWindow);
            cmd.Run();
        }
    }
    else if (selectedAction == exportAction)
    {
        QWidget* window = this->window();
        MainWindow* mainWindow = window ? qobject_cast<MainWindow*>(window) : nullptr;
        if (mainWindow)
        {
            ExportSnapshotAsTemplateCommand cmd(snapshotRef, this->m_connection, mainWindow);
            cmd.Run();
        }
    }
    else if (selectedAction == deleteAction)
    {
        this->onDeleteSnapshot();
    }
    else if (selectedAction == propertiesAction)
    {
        if (!snapshotRef.isEmpty() && this->m_connection)
        {
            SnapshotPropertiesDialog dialog(snapshot, this->window());
            dialog.exec();
        }
    }
}

void SnapshotsTabPage::onScheduledSnapshotsToggled()
{
    if (!this->m_scheduledSnapshotsAction)
        return;

    this->m_showScheduledSnapshots = this->m_scheduledSnapshotsAction->isChecked();
    this->populateSnapshotTree();
    this->updateButtonStates();
}

void SnapshotsTabPage::onVmssLinkClicked()
{
    QMessageBox::information(this,
                             tr("VM Snapshot Schedules"),
                             tr("VM snapshot schedules are not available yet in this version."));
}

bool SnapshotsTabPage::isScheduledSnapshot(const QVariantMap& snapshot) const
{
    return snapshot.value("is_snapshot_from_vmpp").toBool()
        || snapshot.value("is_vmss_snapshot").toBool();
}

bool SnapshotsTabPage::shouldShowSnapshot(const QVariantMap& snapshot) const
{
    return this->m_showScheduledSnapshots || !this->isScheduledSnapshot(snapshot);
}

void SnapshotsTabPage::buildSnapshotTree(const QString& snapshotRef,
                                         SnapshotIcon* parentIcon,
                                         const QHash<QString, QVariantMap>& snapshots,
                                         const QMultiHash<QString, QString>& childrenByParent)
{
    const QVariantMap snapshot = snapshots.value(snapshotRef);
    const bool showSnapshot = this->shouldShowSnapshot(snapshot);
    SnapshotIcon* currentIcon = parentIcon;

    if (showSnapshot)
    {
        const QString powerState = snapshot.value("power_state").toString();
        const bool isSuspended = powerState == "Suspended";
        const bool isScheduled = this->isScheduledSnapshot(snapshot);
        const int iconIndex = isScheduled
            ? (isSuspended ? SnapshotIcon::ScheduledDiskMemorySnapshot : SnapshotIcon::ScheduledDiskSnapshot)
            : (isSuspended ? SnapshotIcon::DiskAndMemorySnapshot : SnapshotIcon::DiskSnapshot);

        QString labelTime;
        const QString timestamp = snapshot.value("snapshot_time").toString();
        if (!timestamp.isEmpty())
        {
            QDateTime dt = QDateTime::fromString(timestamp, Qt::ISODate);
            if (dt.isValid())
            {
                dt = dt.toLocalTime().addSecs(this->m_connection->GetServerTimeOffsetSeconds());
                labelTime = dt.toString("yyyy-MM-dd HH:mm:ss");
            } else
            {
                labelTime = timestamp;
            }
        }

        QString labelName = snapshot.value("name_label").toString();
        if (labelName.isEmpty())
            labelName = tr("Unnamed Snapshot");

        SnapshotIcon* snapshotIcon = new SnapshotIcon(labelName, labelTime, parentIcon, this->ui->snapshotTree, iconIndex);
        snapshotIcon->setData(Qt::UserRole, snapshotRef);
        this->ui->snapshotTree->AddSnapshot(snapshotIcon);
        currentIcon = snapshotIcon;
    }

    const QList<QString> children = childrenByParent.values(snapshotRef);
    for (const QString& childRef : children)
        this->buildSnapshotTree(childRef, currentIcon, snapshots, childrenByParent);
}

void SnapshotsTabPage::refreshVmssPanel()
{
    if (!this->m_connection || this->m_objectType != "vm" || this->m_objectData.isEmpty())
    {
        if (this->m_scheduledSnapshotsAction)
            this->m_scheduledSnapshotsAction->setVisible(false);
        return;
    }

    XenCache* cache = this->m_connection->GetCache();
    if (!cache)
    {
        if (this->m_scheduledSnapshotsAction)
            this->m_scheduledSnapshotsAction->setVisible(false);
        return;
    }

    const bool hasVmssSupport = !cache->GetAllData("vmss").isEmpty() || this->m_objectData.contains("snapshot_schedule");

    if (this->m_scheduledSnapshotsAction)
        this->m_scheduledSnapshotsAction->setVisible(hasVmssSupport);

    // TODO: C# shows VMSS status inside the details panel (not a top banner).
    // Move VMSS info there once the details panel is ported.
}
