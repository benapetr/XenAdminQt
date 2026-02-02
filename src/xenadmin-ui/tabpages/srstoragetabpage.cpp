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
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>
#include "xenlib/xencache.h"
#include "xenlib/utils/misc.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/pbd.h"
#include "xenlib/xen/vbd.h"
#include "xenlib/xen/vdi.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/xenapi/xenapi_VDI.h"
#include "xenlib/xen/actions/sr/srrefreshaction.h"
#include "xenlib/xen/actions/vdi/destroydiskaction.h"
#include "xenlib/xen/sr.h"
#include "srstoragetabpage.h"
#include "ui_srstoragetabpage.h"
#include "dialogs/movevirtualdiskdialog.h"
#include "dialogs/vdipropertiesdialog.h"
#include "dialogs/actionprogressdialog.h"
#include "../operations/operationmanager.h"

namespace
{
class SizeTableWidgetItem : public QTableWidgetItem
{
    public:
        explicit SizeTableWidgetItem(const QString& text, qint64 sizeBytes) : QTableWidgetItem(text)
        {
            this->setData(Qt::UserRole, sizeBytes);
        }

        bool operator<(const QTableWidgetItem& other) const override
        {
            return this->data(Qt::UserRole).toLongLong() < other.data(Qt::UserRole).toLongLong();
        }
    };
}

SrStorageTabPage::SrStorageTabPage(QWidget* parent) : BaseTabPage(parent), ui(new Ui::SrStorageTabPage)
{
    this->ui->setupUi(this);
    this->ui->storageTable->horizontalHeader()->setStretchLastSection(true);
    this->ui->storageTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->ui->storageTable->setSortingEnabled(true);
    this->ui->storageTable->horizontalHeader()->setSortIndicatorShown(true);

    connect(this->ui->storageTable, &QTableWidget::customContextMenuRequested, this, &SrStorageTabPage::onStorageTableCustomContextMenuRequested);
    connect(this->ui->storageTable, &QTableWidget::itemSelectionChanged, this, &SrStorageTabPage::onStorageTableSelectionChanged);
    connect(this->ui->storageTable, &QTableWidget::doubleClicked, this, &SrStorageTabPage::onStorageTableDoubleClicked);

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

bool SrStorageTabPage::IsApplicableForObjectType(const QString& objectType) const
{
    return objectType == "sr";
}

void SrStorageTabPage::SetObject(QSharedPointer<XenObject> object)
{
    BaseTabPage::SetObject(object);
}

QSharedPointer<SR> SrStorageTabPage::GetSR()
{
    return qSharedPointerDynamicCast<SR>(this->m_object);
}

void SrStorageTabPage::refreshContent()
{
    this->ui->storageTable->setRowCount(0);

    if (!this->GetSR())
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

    QSharedPointer<SR> sr = this->GetSR();
    if (!sr)
        return;

    const QList<QSharedPointer<VDI>> vdis = sr->GetVDIs();

    this->ui->storageTable->setColumnCount(5);
    this->ui->storageTable->setHorizontalHeaderLabels(QStringList() << "Name" << "Description" << "Size" << "VM" << "CBT");
    this->ui->storageTable->setSortingEnabled(false);

    for (const QSharedPointer<VDI>& vdi : vdis)
    {
        if (!vdi || !vdi->IsValid())
            continue;

        bool isSnapshot = vdi->IsSnapshot();
        QVariantMap smConfig = vdi->SMConfig();
        if (isSnapshot || smConfig.contains("base_mirror"))
        {
            continue;
        }

        QString vdiName = vdi->GetName();
        QString vdiDescription = vdi->GetDescription();

        qint64 virtualSize = vdi->VirtualSize();
        QString sizeText = Misc::FormatSize(virtualSize);

        QStringList vmNames;
        const QList<QSharedPointer<VBD>> vbds = vdi->GetVBDs();
        for (const QSharedPointer<VBD>& vbd : vbds)
        {
            if (!vbd)
                continue;

            QSharedPointer<VM> vm = vbd->GetVM();
            if (vm && vm->IsValid())
            {
                QString vmName = vm->GetName();
                if (!vmNames.contains(vmName))
                {
                    vmNames.append(vmName);
                }
            }
        }

        QString vmString = vmNames.isEmpty() ? "-" : vmNames.join(", ");

        bool cbtEnabled = vdi->IsCBTEnabled();
        QString cbtStatus = cbtEnabled ? "Enabled" : "-";

        int row = this->ui->storageTable->rowCount();
        this->ui->storageTable->insertRow(row);

        QTableWidgetItem* nameItem = new QTableWidgetItem(vdiName);
        nameItem->setData(Qt::UserRole, vdi->OpaqueRef());
        this->ui->storageTable->setItem(row, 0, nameItem);
        this->ui->storageTable->setItem(row, 1, new QTableWidgetItem(vdiDescription));
        this->ui->storageTable->setItem(row, 2, new SizeTableWidgetItem(sizeText, virtualSize));
        this->ui->storageTable->setItem(row, 3, new QTableWidgetItem(vmString));
        this->ui->storageTable->setItem(row, 4, new QTableWidgetItem(cbtStatus));
    }

    this->ui->storageTable->setSortingEnabled(true);

    for (int i = 0; i < this->ui->storageTable->columnCount(); ++i)
    {
        this->ui->storageTable->resizeColumnToContents(i);
    }
}

QString SrStorageTabPage::getSelectedVDIRef() const
{
    QList<QTableWidgetItem*> selectedItems = this->ui->storageTable->selectedItems();
    if (selectedItems.isEmpty())
        return QString();

    int row = selectedItems.first()->row();

    QTableWidgetItem* item = this->ui->storageTable->item(row, 0);
    if (!item)
        return QString();

    return item->data(Qt::UserRole).toString();
}

QSharedPointer<VDI> SrStorageTabPage::getSelectedVDI() const
{
    if (!this->m_connection)
        return QSharedPointer<VDI>();

    const QString vdiRef = this->getSelectedVDIRef();
    if (vdiRef.isEmpty())
        return QSharedPointer<VDI>();

    return this->m_connection->GetCache()->ResolveObject<VDI>(vdiRef);
}

void SrStorageTabPage::updateButtonStates()
{
    QSharedPointer<SR> sr = this->GetSR();
    bool hasSelection = !this->getSelectedVDIRef().isEmpty();
    bool srAvailable = sr && sr->IsValid();

    const QStringList srAllowedOps = sr ? sr->AllowedOperations() : QStringList();
    bool srLocked = srAllowedOps.isEmpty();

    bool srDetached = true;
    if (sr)
    {
        const QList<QSharedPointer<PBD>> pbds = sr->GetPBDs();
        for (const QSharedPointer<PBD>& pbd : pbds)
        {
            if (pbd && pbd->IsCurrentlyAttached())
            {
                srDetached = false;
                break;
            }
        }
    }

    this->ui->rescanButton->setEnabled(srAvailable && !srLocked && !srDetached);
    this->ui->addButton->setEnabled(srAvailable && !srLocked);
    this->ui->moveButton->setEnabled(hasSelection);

    if (hasSelection && this->m_connection)
    {
        QSharedPointer<VDI> vdi = this->getSelectedVDI();
        bool isSnapshot = vdi && vdi->IsSnapshot();
        QStringList vdiAllowed = vdi ? vdi->AllowedOperations() : QStringList();
        bool vdiLocked = vdiAllowed.isEmpty();
        this->ui->editButton->setEnabled(!isSnapshot && !vdiLocked && !srLocked);
        this->ui->deleteButton->setEnabled(!srLocked);
    } else
    {
        this->ui->editButton->setEnabled(false);
        this->ui->deleteButton->setEnabled(false);
    }
}

void SrStorageTabPage::onRescanButtonClicked()
{
    if (!this->m_connection || this->m_objectRef.isEmpty())
        return;

    SrRefreshAction* action = new SrRefreshAction(this->m_connection, this->m_objectRef);
    OperationManager::instance()->RegisterOperation(action);
    action->RunAsync();

    this->requestSrRefresh(2000);
}

void SrStorageTabPage::onAddButtonClicked()
{
    QMessageBox::information(this, tr("New Virtual Disk"), tr("New disk wizard should be triggered by MainWindow."));
}

void SrStorageTabPage::onMoveButtonClicked()
{
    QSharedPointer<VDI> vdi = this->getSelectedVDI();
    if (!vdi || !vdi->IsValid())
        return;

    MoveVirtualDiskDialog dialog(vdi, this);
    if (dialog.exec() == QDialog::Accepted)
    {
        this->requestSrRefresh(1000);
    }
}

void SrStorageTabPage::onDeleteButtonClicked()
{
    QSharedPointer<VDI> vdi = this->getSelectedVDI();
    if (!vdi)
        return;

    QString vdiName = vdi->GetName();
    if (vdiName.isEmpty())
        vdiName = tr("Virtual Disk");

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
    const QList<QSharedPointer<VBD>> vbds = vdi->GetVBDs();
    for (const QSharedPointer<VBD>& vbd : vbds)
    {
        if (!vbd)
            continue;

        if (vbd->CurrentlyAttached())
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

    DestroyDiskAction* action = new DestroyDiskAction(vdi->OpaqueRef(), vdi->GetConnection(), allowRunningDelete, this);
    ActionProgressDialog* dialog = new ActionProgressDialog(action, this);
    dialog->exec();
    delete dialog;

    this->requestSrRefresh();
}

void SrStorageTabPage::onEditButtonClicked()
{
    QSharedPointer<VDI> vdi = this->getSelectedVDI();
    if (!vdi || !vdi->IsValid())
        return;

    VdiPropertiesDialog dialog(vdi, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

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
    if (!this->m_connection || this->m_objectRef.isEmpty())
        return;

    auto request = [this]()
    {
        SrRefreshAction* action = new SrRefreshAction(this->m_connection, this->m_objectRef);
        OperationManager::instance()->RegisterOperation(action);
        action->RunAsync();
    };

    if (delayMs <= 0)
        request();
    else
        QTimer::singleShot(delayMs, this, request);
}
