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

#include "srstoragetabpage.h"
#include "ui_srstoragetabpage.h"
#include "xenlib.h"
#include "xencache.h"
#include "xen/network/connection.h"
#include "xen/api.h"
#include "xen/xenapi/xenapi_VDI.h"
#include "dialogs/movevirtualdiskdialog.h"
#include "dialogs/vdipropertiesdialog.h"
#include "dialogs/operationprogressdialog.h"
#include "xen/actions/sr/srrefreshaction.h"
#include "xen/actions/vdi/destroydiskaction.h"
#include "../operations/operationmanager.h"
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>
#include <stdexcept>

SrStorageTabPage::SrStorageTabPage(QWidget* parent)
    : BaseTabPage(parent), ui(new Ui::SrStorageTabPage)
{
    this->ui->setupUi(this);
    this->ui->storageTable->horizontalHeader()->setStretchLastSection(true);
    this->ui->storageTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(this->ui->storageTable, &QTableWidget::customContextMenuRequested,
            this, &SrStorageTabPage::onStorageTableCustomContextMenuRequested);
    connect(this->ui->storageTable, &QTableWidget::itemSelectionChanged,
            this, &SrStorageTabPage::onStorageTableSelectionChanged);
    connect(this->ui->storageTable, &QTableWidget::doubleClicked,
            this, &SrStorageTabPage::onStorageTableDoubleClicked);

    connect(this->ui->addButton, &QPushButton::clicked, this, &SrStorageTabPage::onAddButtonClicked);
    connect(this->ui->rescanButton, &QPushButton::clicked, this, &SrStorageTabPage::onRescanButtonClicked);
    connect(this->ui->moveButton, &QPushButton::clicked, this, &SrStorageTabPage::onMoveButtonClicked);
    connect(this->ui->deleteButton, &QPushButton::clicked, this, &SrStorageTabPage::onDeleteButtonClicked);
    connect(this->ui->editButton, &QPushButton::clicked, this, &SrStorageTabPage::onEditButtonClicked);

    this->updateButtonStates();
}

SrStorageTabPage::~SrStorageTabPage()
{
    delete this->ui;
}

bool SrStorageTabPage::isApplicableForObjectType(const QString& objectType) const
{
    return objectType == "sr";
}

void SrStorageTabPage::SetXenObject(const QString& objectType, const QString& objectRef, const QVariantMap& objectData)
{
    BaseTabPage::SetXenObject(objectType, objectRef, objectData);
}

void SrStorageTabPage::refreshContent()
{
    this->ui->storageTable->setRowCount(0);

    if (this->m_objectType != "sr" || this->m_objectData.isEmpty())
    {
        this->updateButtonStates();
        return;
    }

    this->populateSRStorage();
    this->updateButtonStates();
}

void SrStorageTabPage::populateSRStorage()
{
    this->ui->titleLabel->setText("Virtual Disks");

    if (!this->m_xenLib)
    {
        return;
    }

    QVariantList vdiRefs = this->m_objectData.value("VDIs").toList();

    this->ui->storageTable->setColumnCount(5);
    this->ui->storageTable->setHorizontalHeaderLabels(
        QStringList() << "Name" << "Description" << "Size" << "VM" << "CBT");

    for (const QVariant& vdiVar : vdiRefs)
    {
        QString vdiRef = vdiVar.toString();
        QVariantMap vdiRecord = this->m_xenLib->getCache()->ResolveObjectData("vdi", vdiRef);

        if (vdiRecord.isEmpty())
        {
            continue;
        }

        bool isSnapshot = vdiRecord.value("is_a_snapshot", false).toBool();
        QVariantMap smConfig = vdiRecord.value("sm_config").toMap();
        if (isSnapshot || smConfig.contains("base_mirror"))
        {
            continue;
        }

        QString vdiName = vdiRecord.value("name_label", "").toString();
        QString vdiDescription = vdiRecord.value("name_description", "").toString();

        qint64 virtualSize = vdiRecord.value("virtual_size", 0).toLongLong();
        QString sizeText = "N/A";
        if (virtualSize > 0)
        {
            double sizeGB = virtualSize / (1024.0 * 1024.0 * 1024.0);
            sizeText = QString::number(sizeGB, 'f', 2) + " GB";
        }

        QStringList vmNames;
        QVariantList vbdRefs = vdiRecord.value("VBDs").toList();
        for (const QVariant& vbdVar : vbdRefs)
        {
            QString vbdRef = vbdVar.toString();
            QVariantMap vbdData = this->m_xenLib->getCache()->ResolveObjectData("vbd", vbdRef);
            if (vbdData.isEmpty())
            {
                continue;
            }

            QString vmRef = vbdData.value("VM").toString();
            QVariantMap vmData = this->m_xenLib->getCache()->ResolveObjectData("vm", vmRef);
            if (!vmData.isEmpty())
            {
                QString vmName = vmData.value("name_label", "Unknown").toString();
                if (!vmNames.contains(vmName))
                {
                    vmNames.append(vmName);
                }
            }
        }

        QString vmString = vmNames.isEmpty() ? "-" : vmNames.join(", ");

        bool cbtEnabled = vdiRecord.value("cbt_enabled", false).toBool();
        QString cbtStatus = cbtEnabled ? "Enabled" : "-";

        int row = this->ui->storageTable->rowCount();
        this->ui->storageTable->insertRow(row);

        QTableWidgetItem* nameItem = new QTableWidgetItem(vdiName);
        nameItem->setData(Qt::UserRole, vdiRef);
        this->ui->storageTable->setItem(row, 0, nameItem);
        this->ui->storageTable->setItem(row, 1, new QTableWidgetItem(vdiDescription));
        this->ui->storageTable->setItem(row, 2, new QTableWidgetItem(sizeText));
        this->ui->storageTable->setItem(row, 3, new QTableWidgetItem(vmString));
        this->ui->storageTable->setItem(row, 4, new QTableWidgetItem(cbtStatus));
    }

    for (int i = 0; i < this->ui->storageTable->columnCount(); ++i)
    {
        this->ui->storageTable->resizeColumnToContents(i);
    }
}

QString SrStorageTabPage::getSelectedVDIRef() const
{
    QList<QTableWidgetItem*> selectedItems = this->ui->storageTable->selectedItems();
    if (selectedItems.isEmpty())
    {
        return QString();
    }

    int row = selectedItems.first()->row();
    QTableWidgetItem* item = this->ui->storageTable->item(row, 0);
    if (!item)
    {
        return QString();
    }

    return item->data(Qt::UserRole).toString();
}

void SrStorageTabPage::updateButtonStates()
{
    bool hasSelection = !this->getSelectedVDIRef().isEmpty();
    bool srAvailable = !this->m_objectData.isEmpty();

    QVariantList srAllowedOps = this->m_objectData.value("allowed_operations", QVariantList()).toList();
    bool srLocked = srAllowedOps.isEmpty();

    bool srDetached = true;
    QVariantList pbdRefs = this->m_objectData.value("PBDs", QVariantList()).toList();
    if (this->m_xenLib)
    {
        for (const QVariant& pbdVar : pbdRefs)
        {
            QString pbdRef = pbdVar.toString();
            QVariantMap pbdData = this->m_xenLib->getCache()->ResolveObjectData("pbd", pbdRef);
            if (pbdData.value("currently_attached", false).toBool())
            {
                srDetached = false;
                break;
            }
        }
    }

    this->ui->rescanButton->setEnabled(srAvailable && !srLocked && !srDetached);
    this->ui->addButton->setEnabled(srAvailable && !srLocked);
    this->ui->moveButton->setEnabled(hasSelection);

    if (hasSelection && this->m_xenLib)
    {
        QVariantMap vdiData = this->m_xenLib->getCache()->ResolveObjectData("vdi", this->getSelectedVDIRef());
        bool isSnapshot = vdiData.value("is_a_snapshot", false).toBool();
        QVariantList vdiAllowed = vdiData.value("allowed_operations", QVariantList()).toList();
        bool vdiLocked = vdiAllowed.isEmpty();
        this->ui->editButton->setEnabled(!isSnapshot && !vdiLocked && !srLocked);
        this->ui->deleteButton->setEnabled(!srLocked);
    }
    else
    {
        this->ui->editButton->setEnabled(false);
        this->ui->deleteButton->setEnabled(false);
    }
}

void SrStorageTabPage::onRescanButtonClicked()
{
    if (!this->m_xenLib || this->m_objectRef.isEmpty())
    {
        return;
    }

    SrRefreshAction* action = new SrRefreshAction(this->m_xenLib->getConnection(), this->m_objectRef);
    OperationManager::instance()->registerOperation(action);
    action->runAsync();

    this->requestSrRefresh(2000);
}

void SrStorageTabPage::onAddButtonClicked()
{
    QMessageBox::information(this, tr("New Virtual Disk"),
                             tr("New disk wizard should be triggered by MainWindow."));
}

void SrStorageTabPage::onMoveButtonClicked()
{
    QString vdiRef = this->getSelectedVDIRef();
    if (vdiRef.isEmpty() || !this->m_xenLib)
    {
        return;
    }

    MoveVirtualDiskDialog dialog(this->m_xenLib->getConnection(), vdiRef, this);
    if (dialog.exec() == QDialog::Accepted)
    {
        this->requestSrRefresh(1000);
    }
}

void SrStorageTabPage::onDeleteButtonClicked()
{
    QString vdiRef = this->getSelectedVDIRef();
    if (vdiRef.isEmpty() || !this->m_xenLib)
    {
        return;
    }

    QVariantMap vdiData = this->m_xenLib->getCache()->ResolveObjectData("vdi", vdiRef);
    QString vdiName = vdiData.value("name_label", tr("Virtual Disk")).toString();

    QMessageBox::StandardButton confirm = QMessageBox::question(
        this,
        tr("Delete Virtual Disk"),
        tr("Are you sure you want to permanently delete '%1'?\n\nThis operation cannot be undone.").arg(vdiName),
        QMessageBox::Yes | QMessageBox::No);

    if (confirm != QMessageBox::Yes)
    {
        return;
    }

    bool allowRunningDelete = false;
    QVariantList vbdRefs = vdiData.value("VBDs").toList();
    for (const QVariant& vbdVar : vbdRefs)
    {
        QString vbdRef = vbdVar.toString();
        QVariantMap vbdData = this->m_xenLib->getCache()->ResolveObjectData("vbd", vbdRef);
        if (vbdData.value("currently_attached", false).toBool())
        {
            QMessageBox::StandardButton attachedConfirm = QMessageBox::question(
                this,
                tr("Disk Currently Attached"),
                tr("'%1' is currently attached to one or more VMs.\n\nDo you want to detach it and delete it anyway?").arg(vdiName),
                QMessageBox::Yes | QMessageBox::No);

            if (attachedConfirm != QMessageBox::Yes)
            {
                return;
            }

            allowRunningDelete = true;
            break;
        }
    }

    DestroyDiskAction* action = new DestroyDiskAction(vdiRef, this->m_xenLib->getConnection(), allowRunningDelete, this);
    OperationProgressDialog* dialog = new OperationProgressDialog(action, this);
    dialog->exec();
    delete dialog;

    this->requestSrRefresh();
}

void SrStorageTabPage::onEditButtonClicked()
{
    QString vdiRef = this->getSelectedVDIRef();
    if (vdiRef.isEmpty() || !this->m_xenLib)
    {
        return;
    }

    VdiPropertiesDialog dialog(this->m_xenLib->getConnection(), vdiRef, this);
    if (dialog.exec() != QDialog::Accepted || !dialog.hasChanges())
    {
        return;
    }

    XenConnection* connection = this->m_xenLib->getConnection();
    if (!connection || !connection->GetSession())
        return;

    bool hasErrors = false;
    QVariantMap vdiData = this->m_xenLib->getCache()->ResolveObjectData("vdi", vdiRef);

    QString newName = dialog.getVdiName();
    QString oldName = vdiData.value("name_label", "").toString();
    if (newName != oldName)
    {
        try
        {
            XenAPI::VDI::set_name_label(connection->GetSession(), vdiRef, newName);
        }
        catch (const std::exception&)
        {
            QMessageBox::warning(this, tr("Warning"), tr("Failed to update virtual disk name."));
            hasErrors = true;
        }
    }

    QString newDescription = dialog.getVdiDescription();
    QString oldDescription = vdiData.value("name_description", "").toString();
    if (newDescription != oldDescription)
    {
        try
        {
            XenAPI::VDI::set_name_description(connection->GetSession(), vdiRef, newDescription);
        }
        catch (const std::exception&)
        {
            QMessageBox::warning(this, tr("Warning"), tr("Failed to update virtual disk description."));
            hasErrors = true;
        }
    }

    qint64 newSize = dialog.getNewSize();
    qint64 oldSize = vdiData.value("virtual_size", 0).toLongLong();
    if (newSize > oldSize + (10 * 1024 * 1024))
    {
        try
        {
            XenAPI::VDI::resize(connection->GetSession(), vdiRef, newSize);
        }
        catch (const std::exception&)
        {
            QMessageBox::warning(this, tr("Warning"), tr("Failed to resize virtual disk."));
            hasErrors = true;
        }
    }

    if (!hasErrors)
    {
        QMessageBox::information(this, tr("Success"), tr("Virtual disk properties updated successfully."));
    }

    this->requestSrRefresh();
}

void SrStorageTabPage::onStorageTableSelectionChanged()
{
    this->updateButtonStates();
}

void SrStorageTabPage::onStorageTableDoubleClicked(const QModelIndex& index)
{
    Q_UNUSED(index);

    if (this->ui->editButton->isEnabled())
    {
        this->onEditButtonClicked();
    }
}

void SrStorageTabPage::onStorageTableCustomContextMenuRequested(const QPoint& pos)
{
    QMenu menu(this);

    QAction* rescanAction = menu.addAction(tr("Rescan"));
    rescanAction->setEnabled(this->ui->rescanButton->isEnabled());
    connect(rescanAction, &QAction::triggered, this, &SrStorageTabPage::onRescanButtonClicked);

    QAction* addAction = menu.addAction(tr("Add Virtual Disk..."));
    addAction->setEnabled(this->ui->addButton->isEnabled());
    connect(addAction, &QAction::triggered, this, &SrStorageTabPage::onAddButtonClicked);

    QAction* moveAction = menu.addAction(tr("Move Virtual Disk..."));
    moveAction->setEnabled(this->ui->moveButton->isEnabled());
    connect(moveAction, &QAction::triggered, this, &SrStorageTabPage::onMoveButtonClicked);

    QAction* deleteAction = menu.addAction(tr("Delete Virtual Disk..."));
    deleteAction->setEnabled(this->ui->deleteButton->isEnabled());
    connect(deleteAction, &QAction::triggered, this, &SrStorageTabPage::onDeleteButtonClicked);

    menu.addSeparator();

    QAction* editAction = menu.addAction(tr("Properties..."));
    editAction->setEnabled(this->ui->editButton->isEnabled());
    connect(editAction, &QAction::triggered, this, &SrStorageTabPage::onEditButtonClicked);

    menu.exec(this->ui->storageTable->mapToGlobal(pos));
}

void SrStorageTabPage::requestSrRefresh(int delayMs)
{
    if (!this->m_xenLib || this->m_objectRef.isEmpty())
    {
        return;
    }

    auto request = [this]() {
        if (this->m_xenLib && !this->m_objectRef.isEmpty())
        {
            this->m_xenLib->requestObjectData("sr", this->m_objectRef);
        }
    };

    if (delayMs <= 0)
    {
        request();
    }
    else
    {
        QTimer::singleShot(delayMs, this, request);
    }
}
