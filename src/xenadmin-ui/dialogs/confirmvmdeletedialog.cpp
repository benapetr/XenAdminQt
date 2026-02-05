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

#include "confirmvmdeletedialog.h"
#include "ui_confirmvmdeletedialog.h"
#include "xen/network/connection.h"
#include "xen/vbd.h"
#include "xen/vdi.h"
#include "xen/vm.h"
#include "xencache.h"
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QHeaderView>
#include <QFontMetrics>
#include <QResizeEvent>

ConfirmVMDeleteDialog::ConfirmVMDeleteDialog(const QList<QSharedPointer<VM>> &vms, QWidget* parent) : QDialog(parent), ui(new Ui::ConfirmVMDeleteDialog), deleteButton_(nullptr)
{
    this->ui->setupUi(this);
    this->initialize(vms);
}

ConfirmVMDeleteDialog::ConfirmVMDeleteDialog(QSharedPointer<VM> vm, QWidget* parent) : QDialog(parent), ui(new Ui::ConfirmVMDeleteDialog), deleteButton_(nullptr)
{
    QList<QSharedPointer<VM>> vms;
    vms << vm;
    this->ui->setupUi(this);
    this->initialize(vms);
}

ConfirmVMDeleteDialog::~ConfirmVMDeleteDialog()
{
    delete this->ui;
}

void ConfirmVMDeleteDialog::initialize(const QList<QSharedPointer<VM>> &vms)
{
    if (vms.isEmpty())
        return;

    // Add Delete button
    this->deleteButton_ = this->ui->buttonBox->addButton(tr("&Delete"), QDialogButtonBox::AcceptRole);
    this->deleteButton_->setDefault(true);
    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);

    // Set window title based on VM count
    if (vms.count() == 1)
    {
        QString type = vms.first()->IsTemplate() ? tr("Template") : tr("VM");
        this->setWindowTitle(tr("Confirm %1 Delete").arg(type));
    } else
    {
        this->setWindowTitle(tr("Confirm Delete Items"));
    }

    // Setup tree widget
    this->ui->listView->setHeaderLabels({tr("Name"), tr("VM")});
    this->ui->listView->header()->setStretchLastSection(true);
    this->ui->listView->setRootIsDecorated(false);

    // Create groups (we'll use top-level items as group headers)
    QTreeWidgetItem* attachedDisksGroup = new QTreeWidgetItem(this->ui->listView);
    attachedDisksGroup->setText(0, tr("Attached virtual disks"));
    attachedDisksGroup->setFlags(Qt::ItemIsEnabled);

    QFont groupFont = attachedDisksGroup->font(0);
    groupFont.setBold(true);
    attachedDisksGroup->setFont(0, groupFont);
    attachedDisksGroup->setFont(1, groupFont);

    QTreeWidgetItem* snapshotsGroup = new QTreeWidgetItem(this->ui->listView);
    snapshotsGroup->setText(0, tr("Snapshots"));
    snapshotsGroup->setFlags(Qt::ItemIsEnabled);
    snapshotsGroup->setFont(0, groupFont);
    snapshotsGroup->setFont(1, groupFont);

    QList<QSharedPointer<VDI>> sharedVDIsCouldBeDeleted;

    // Process each VM
    for (const QSharedPointer<VM>& vm : vms)
    {
        XenCache* cache = vm->GetCache();
        if (!cache)
            continue;

        QString vmRef = vm->OpaqueRef();
        QString vmName = vm->GetName();
        QList<QString> vbdRefs = vm->GetVBDRefs();

        // Process VBDs (Virtual Block Devices - disks)
        for (QString vbdRef : vbdRefs)
        {
            QSharedPointer<VBD> vbd = cache->ResolveObject<VBD>(vbdRef);
            
            if (!vbd)
                continue;

            // Skip CD-ROM devices
            if (vbd->IsCD())
                continue;

            QString vdiRef = vbd->GetVDIRef();
            if (vdiRef.isEmpty() || vdiRef == XENOBJECT_NULL)
                continue;

            QSharedPointer<VDI> vdi = cache->ResolveObject<VDI>(vdiRef);
            if (!vdi)
                continue;

            // Get all VBDs that reference this VDI
            QList<QString> vdiVBDs = vdi->GetVBDRefs();
            
            if (vdiVBDs.count() > 1)
            {
                // This VDI is shared among multiple VMs
                // Check if all VMs using this VDI are being deleted
                bool allVMsBeingDeleted = true;
                for (QString otherVbdRef : vdiVBDs)
                {
                    QSharedPointer<VBD> other_vbd = cache->ResolveObject<VBD>(otherVbdRef);
                    if (!other_vbd)
                        continue;

                    QString otherVmRef = other_vbd->GetVMRef();
                    
                    bool found = false;
                    for (const QSharedPointer<VM>& other_vm : vms)
                    {
                        if (other_vm->OpaqueRef() == otherVmRef)
                        {
                            found = true;
                            break;
                        }
                    }
                    
                    if (!found)
                    {
                        allVMsBeingDeleted = false;
                        break;
                    }
                }

                if (allVMsBeingDeleted)
                {
                    // Add to shared VDIs list if not already there
                    bool alreadyAdded = false;
                    for (const QSharedPointer<VDI>& sharedVdi : sharedVDIsCouldBeDeleted)
                    {
                        if (sharedVdi->OpaqueRef() == vdiRef)
                        {
                            alreadyAdded = true;
                            break;
                        }
                    }
                    if (!alreadyAdded)
                        sharedVDIsCouldBeDeleted.append(vdi);
                }
            } else
            {
                // VDI is used by only one VM - add it to the list
                QTreeWidgetItem* item = new QTreeWidgetItem(attachedDisksGroup);
                QString vdiName = vdi->GetName();
                if (vdiName.isEmpty())
                    vdiName = vdi->GetDescription();
                if (vdiName.isEmpty())
                    vdiName = vdiRef;

                item->setText(0, vdiName);
                item->setText(1, vmName);
                item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
                
                // Check by default if this VBD is the owner
                bool isOwner = !vbd->GetData().value("empty").toBool();
                item->setCheckState(0, isOwner ? Qt::Checked : Qt::Unchecked);
                
                // Store VBD ref in item data
                item->setData(0, Qt::UserRole, vbd->OpaqueRef());
                item->setData(0, Qt::UserRole + 1, ItemType_Disk);
            }
        }

        // Process snapshots
        // TODO add GetSnapshots to VM class
        QVariantList snapshotRefs = vm->GetData().value("snapshots").toList();
        for (const QVariant& snapshotRefVar : snapshotRefs)
        {
            QString snapshotRef = snapshotRefVar.toString();
            QVariantMap snapshotData = cache->ResolveObjectData("vm", snapshotRef);
            
            if (snapshotData.isEmpty())
                continue;

            QTreeWidgetItem* item = new QTreeWidgetItem(snapshotsGroup);
            QString snapshotName = snapshotData.value("name_label").toString();
            if (snapshotName.isEmpty())
                snapshotName = snapshotRef;

            item->setText(0, snapshotName);
            item->setText(1, vmName);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
            item->setCheckState(0, Qt::Unchecked);
            
            // Store snapshot ref in item data
            item->setData(0, Qt::UserRole, snapshotRef);
            item->setData(0, Qt::UserRole + 1, ItemType_Snapshot);
        }
    }

    // Add shared VDIs
    for (const QSharedPointer<VDI>& vdi : sharedVDIsCouldBeDeleted)
    {
        QTreeWidgetItem* item = new QTreeWidgetItem(attachedDisksGroup);
        QString vdiName = vdi->GetName();
        if (vdiName.isEmpty())
            vdiName = vdi->GetDescription();
        if (vdiName.isEmpty())
            vdiName = vdi->OpaqueRef();

        // Build list of VMs using this VDI
        QStringList vmNames;
        QList<QString> vbdRefs = vdi->GetVBDRefs();
        QStringList vbdRefsList;
        
        for (QString vbdRef : vbdRefs)
        {
            vbdRefsList << vbdRef;
            QSharedPointer<VBD> vbd = vdi->GetCache()->ResolveObject<VBD>(vbdRef);
            if (!vbd)
                continue;
            QSharedPointer<VM> vm = vbd->GetVM();
            if (!vm)
                continue;
            QString vmName = vm->GetName();
            if (!vmName.isEmpty() && !vmNames.contains(vmName))
                vmNames.append(vmName);
        }

        item->setText(0, vdiName);
        item->setText(1, vmNames.join(", "));
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        item->setCheckState(0, Qt::Unchecked);
        
        // Store VBD refs list in item data
        item->setData(0, Qt::UserRole, vbdRefsList);
        item->setData(0, Qt::UserRole + 1, ItemType_SharedDisk);
    }

    // Expand all groups
    this->ui->listView->expandAll();

    // Remove groups if they have no children
    if (attachedDisksGroup->childCount() == 0)
        delete attachedDisksGroup;
    if (snapshotsGroup->childCount() == 0)
        delete snapshotsGroup;

    // If no items at all, show a message
    if (this->ui->listView->topLevelItemCount() == 0 || 
        (this->ui->listView->topLevelItemCount() > 0 && 
         this->ui->listView->topLevelItem(0)->childCount() == 0 &&
         (this->ui->listView->topLevelItemCount() == 1 || this->ui->listView->topLevelItem(1)->childCount() == 0)))
    {
        this->ui->label1->setText(tr("No associated disks or snapshots to delete."));
        this->ui->selectAllButton->setEnabled(false);
        this->ui->clearButton->setEnabled(false);
    }

    // Connect signals
    connect(this->ui->selectAllButton, &QPushButton::clicked, this, &ConfirmVMDeleteDialog::onSelectAllClicked);
    connect(this->ui->clearButton, &QPushButton::clicked, this, &ConfirmVMDeleteDialog::onClearClicked);
    connect(this->ui->listView, &QTreeWidget::itemChanged, this, &ConfirmVMDeleteDialog::onItemChanged);

    this->enableSelectAllClear();
    this->updateColumnWidths();
}

QStringList ConfirmVMDeleteDialog::GetDeleteDisks() const
{
    QStringList vbdRefs;
    
    for (int i = 0; i < this->ui->listView->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* group = this->ui->listView->topLevelItem(i);
        for (int j = 0; j < group->childCount(); ++j)
        {
            QTreeWidgetItem* item = group->child(j);
            if (item->checkState(0) == Qt::Checked)
            {
                ItemType type = static_cast<ItemType>(item->data(0, Qt::UserRole + 1).toInt());
                if (type == ItemType_Disk)
                {
                    QString vbdRef = item->data(0, Qt::UserRole).toString();
                    if (!vbdRef.isEmpty())
                        vbdRefs.append(vbdRef);
                } else if (type == ItemType_SharedDisk)
                {
                    QStringList vbdList = item->data(0, Qt::UserRole).toStringList();
                    vbdRefs.append(vbdList);
                }
            }
        }
    }
    
    return vbdRefs;
}

QStringList ConfirmVMDeleteDialog::GetDeleteSnapshots() const
{
    QStringList snapshotRefs;
    
    for (int i = 0; i < this->ui->listView->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* group = this->ui->listView->topLevelItem(i);
        for (int j = 0; j < group->childCount(); ++j)
        {
            QTreeWidgetItem* item = group->child(j);
            if (item->checkState(0) == Qt::Checked)
            {
                ItemType type = static_cast<ItemType>(item->data(0, Qt::UserRole + 1).toInt());
                if (type == ItemType_Snapshot)
                {
                    QString snapshotRef = item->data(0, Qt::UserRole).toString();
                    if (!snapshotRef.isEmpty())
                        snapshotRefs.append(snapshotRef);
                }
            }
        }
    }
    
    return snapshotRefs;
}

void ConfirmVMDeleteDialog::onSelectAllClicked()
{
    for (int i = 0; i < this->ui->listView->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* group = this->ui->listView->topLevelItem(i);
        for (int j = 0; j < group->childCount(); ++j)
        {
            QTreeWidgetItem* item = group->child(j);
            if (item->flags() & Qt::ItemIsUserCheckable)
                item->setCheckState(0, Qt::Checked);
        }
    }
}

void ConfirmVMDeleteDialog::onClearClicked()
{
    for (int i = 0; i < this->ui->listView->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* group = this->ui->listView->topLevelItem(i);
        for (int j = 0; j < group->childCount(); ++j)
        {
            QTreeWidgetItem* item = group->child(j);
            if (item->flags() & Qt::ItemIsUserCheckable)
                item->setCheckState(0, Qt::Unchecked);
        }
    }
}

void ConfirmVMDeleteDialog::onItemChanged(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(item);
    Q_UNUSED(column);
    this->enableSelectAllClear();
}

void ConfirmVMDeleteDialog::enableSelectAllClear()
{
    bool allChecked = true;
    bool allUnchecked = true;
    int itemCount = 0;

    for (int i = 0; i < this->ui->listView->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* group = this->ui->listView->topLevelItem(i);
        for (int j = 0; j < group->childCount(); ++j)
        {
            QTreeWidgetItem* item = group->child(j);
            if (item->flags() & Qt::ItemIsUserCheckable)
            {
                itemCount++;
                if (item->checkState(0) == Qt::Checked)
                    allUnchecked = false;
                else
                    allChecked = false;
            }
        }
    }

    this->ui->selectAllButton->setEnabled(!allChecked && itemCount > 0);
    this->ui->clearButton->setEnabled(!allUnchecked && itemCount > 0);
}

QString ConfirmVMDeleteDialog::elideText(const QString& text, int maxWidth, const QFont& font) const
{
    QFontMetrics metrics(font);
    return metrics.elidedText(text, Qt::ElideRight, maxWidth);
}

void ConfirmVMDeleteDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    this->updateColumnWidths();
}

void ConfirmVMDeleteDialog::updateColumnWidths()
{
    // Resize columns to contents with minimum width
    for (int col = 0; col < this->ui->listView->columnCount(); ++col)
    {
        this->ui->listView->resizeColumnToContents(col);
        if (this->ui->listView->columnWidth(col) < MINIMUM_COL_WIDTH)
            this->ui->listView->setColumnWidth(col, MINIMUM_COL_WIDTH);
    }
}
