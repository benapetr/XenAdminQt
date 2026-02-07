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

#include "vmstoragetabpage.h"
#include "ui_vmstoragetabpage.h"
#include "xen/session.h"
#include "xencache.h"
#include "xen/api.h"
#include "xen/network/connection.h"
#include "xen/vm.h"
#include "xen/vbd.h"
#include "xen/vdi.h"
#include "xen/sr.h"
#include "xen/xenapi/xenapi_VBD.h"
#include "xen/xenapi/xenapi_VDI.h"
#include "xen/xenapi/xenapi_VM.h"
#include "xen/actions/vm/changevmisoaction.h"
#include "xen/actions/vdi/creatediskaction.h"
#include "xen/actions/vdi/detachvirtualdiskaction.h"
#include "xen/actions/vdi/destroydiskaction.h"
#include "xen/actions/vbd/vbdcreateandplugaction.h"
#include "xen/actions/delegatedasyncoperation.h"
#include "dialogs/newvirtualdiskdialog.h"
#include "dialogs/attachvirtualdiskdialog.h"
#include "dialogs/movevirtualdiskdialog.h"
#include "dialogs/vdipropertiesdialog.h"
#include "dialogs/actionprogressdialog.h"
#include "settingsmanager.h"
#include "../widgets/isodropdownbox.h"
#include "operations/multipleaction.h"
#include <QTableWidgetItem>
#include <QSignalBlocker>
#include <QDebug>
#include <QMessageBox>
#include <QMenu>
#include <QAction>
#include <QKeyEvent>
#include <QItemSelectionModel>

namespace
{
    void refreshVmRecord(XenConnection* connection, const QString& vmRef)
    {
        if (!connection || !connection->GetCache() || vmRef.isEmpty())
            return;

        XenAPI::Session* session = connection->GetSession();
        if (!session || !session->IsLoggedIn())
            return;

        try
        {
            QVariantMap record = XenAPI::VM::get_record(session, vmRef);
            record["ref"] = vmRef;
            connection->GetCache()->Update(XenObjectType::VM, vmRef, record);
        }
        catch (const std::exception& ex)
        {
            qWarning() << "VMStorageTabPage: Failed to refresh VM record:" << ex.what();
        }
    }
}

namespace
{
    class DevicePositionItem : public QTableWidgetItem
    {
        public:
            explicit DevicePositionItem(const QString& text) : QTableWidgetItem(text)
            {
            }

            bool operator<(const QTableWidgetItem& other) const override
            {
                bool ok1 = false;
                bool ok2 = false;
                int a = text().toInt(&ok1);
                int b = other.text().toInt(&ok2);

                if (ok1 && ok2)
                {
                    return a < b;
                }

                return text() < other.text();
            }
    };
}

VMStorageTabPage::VMStorageTabPage(QWidget* parent) : BaseTabPage(parent), ui(new Ui::VMStorageTabPage)
{
    this->ui->setupUi(this);
    this->ui->storageTable->horizontalHeader()->setStretchLastSection(true);
    this->ui->storageTable->setSortingEnabled(true);

    // Make table read-only (C# SrStoragePage.Designer.cs line 210: dataGridViewVDIs.ReadOnly = true)
    this->ui->storageTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->ui->storageTable->installEventFilter(this);

    // Connect CD/DVD drive signals
    connect(this->ui->driveComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VMStorageTabPage::onDriveComboBoxChanged);
    connect(this->ui->isoComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VMStorageTabPage::onIsoComboBoxChanged);
    connect(this->ui->ejectButton, &QPushButton::clicked, this, &VMStorageTabPage::onEjectButtonClicked);
    connect(this->ui->noDrivesLabel, &QLabel::linkActivated, this, &VMStorageTabPage::onNewCDDriveLinkClicked);

    // Connect storage table signals
    connect(this->ui->storageTable, &QTableWidget::customContextMenuRequested, this, &VMStorageTabPage::onStorageTableCustomContextMenuRequested);
    connect(this->ui->storageTable, &QTableWidget::itemSelectionChanged, this, &VMStorageTabPage::onStorageTableSelectionChanged);
    connect(this->ui->storageTable, &QTableWidget::doubleClicked, this, &VMStorageTabPage::onStorageTableDoubleClicked);

    // Connect button signals
    connect(this->ui->addButton, &QPushButton::clicked, this, &VMStorageTabPage::onAddButtonClicked);
    connect(this->ui->attachButton, &QPushButton::clicked, this, &VMStorageTabPage::onAttachButtonClicked);
    connect(this->ui->activateButton, &QPushButton::clicked, this, &VMStorageTabPage::onActivateButtonClicked);
    connect(this->ui->deactivateButton, &QPushButton::clicked, this, &VMStorageTabPage::onDeactivateButtonClicked);
    connect(this->ui->moveButton, &QPushButton::clicked, this, &VMStorageTabPage::onMoveButtonClicked);
    connect(this->ui->detachButton, &QPushButton::clicked, this, &VMStorageTabPage::onDetachButtonClicked);
    connect(this->ui->deleteButton, &QPushButton::clicked, this, &VMStorageTabPage::onDeleteButtonClicked);
    connect(this->ui->editButton, &QPushButton::clicked, this, &VMStorageTabPage::onEditButtonClicked);

    // Initially hide CD/DVD section
    this->ui->cdDvdGroupBox->setVisible(false);

    // Update button states
    this->updateStorageButtons();
}

VMStorageTabPage::~VMStorageTabPage()
{
    delete this->ui;
}

bool VMStorageTabPage::IsApplicableForObjectType(const QString& objectType) const
{
    // VM storage tab is only applicable to VMs (matches C# VMStoragePage)
    return objectType == "vm";
}

void VMStorageTabPage::SetObject(QSharedPointer<XenObject> object)
{
    // Disconnect previous object updates
    if (this->m_connection && this->m_connection->GetCache())
    {
        disconnect(this->m_connection->GetCache(), &XenCache::objectChanged, this, &VMStorageTabPage::onCacheObjectChanged);
    }

    // Connect to object updates for real-time CD/DVD changes
    if (object->GetObjectType() == XenObjectType::VM)
    {
        this->m_vm = qSharedPointerDynamicCast<VM>(object);
        connect(object->GetCache(), &XenCache::objectChanged, this, &VMStorageTabPage::onCacheObjectChanged, Qt::UniqueConnection);
    }

    // Call base implementation
    BaseTabPage::SetObject(object);
}

void VMStorageTabPage::onObjectDataReceived(QString type, QString ref, QVariantMap data)
{
    // Check if this update is for our VM
    if (type == "vm" && ref == this->m_object->OpaqueRef())
    {
        // Update our object data
        this->m_objectData = data;

        // Refresh CD/DVD drives if VBDs changed
        this->refreshCDDVDDrives();
        this->populateVMStorage();
        this->updateStorageButtons();
    }
    // Also monitor VBD updates for the current drive
    else if (type == "vbd" && ref == this->m_currentVBDRef)
    {
        // Refresh ISO list if current VBD changed (e.g., ISO mounted/ejected)
        this->refreshISOList();
    }
    else if (type == "vbd" && this->m_storageVbdRefs.contains(ref))
    {
        this->populateVMStorage();
        this->updateStorageButtons();
    }
    else if (type == "vdi" && this->m_storageVdiRefs.contains(ref))
    {
        this->populateVMStorage();
        this->updateStorageButtons();
    }
}

void VMStorageTabPage::onCacheObjectChanged(XenConnection* connection, const QString& type, const QString& ref)
{
    Q_ASSERT(this->m_connection == connection);

    if (this->m_connection != connection)
        return;

    QVariantMap data = connection->GetCache()->ResolveObjectData(type, ref);
    this->onObjectDataReceived(type, ref, data);
}

void VMStorageTabPage::refreshContent()
{
    this->ui->storageTable->setRowCount(0);

    if (!this->m_object || this->m_object->GetObjectType() != XenObjectType::VM)
    {
        this->ui->cdDvdGroupBox->setVisible(false);
        this->updateStorageButtons();
        return;
    }

    this->populateVMStorage();
    this->refreshCDDVDDrives();

    // Update button states after populating table
    this->updateStorageButtons();
}

bool VMStorageTabPage::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == this->ui->storageTable && event->type() == QEvent::KeyPress)
    {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Menu)
        {
            QPoint pos;
            QList<QTableWidgetItem*> selectedItems = this->ui->storageTable->selectedItems();
            if (selectedItems.isEmpty())
            {
                pos = QPoint(3, this->ui->storageTable->horizontalHeader()->height() + 3);
            } else
            {
                int row = selectedItems.first()->row();
                pos = QPoint(3, this->ui->storageTable->rowViewportPosition(row) + this->ui->storageTable->rowHeight(row) / 2);
            }

            onStorageTableCustomContextMenuRequested(pos);
            return true;
        }
    }

    return BaseTabPage::eventFilter(watched, event);
}

void VMStorageTabPage::populateVMStorage()
{
    this->ui->titleLabel->setText("Virtual Disks");

    if (!this->m_vm)
        return;

    const int kColumnPosition = 0;
    const int kColumnName = 1;
    const int kColumnDescription = 2;
    const int kColumnSr = 3;
    const int kColumnSrVolume = 4;
    const int kColumnSize = 5;
    const int kColumnReadOnly = 6;
    const int kColumnPriority = 7;
    const int kColumnActive = 8;
    const int kColumnDevicePath = 9;

    QStringList selectedVbdRefs = this->getSelectedVBDRefs();
    QSet<QString> selectedVbdSet;
    for (const QString& ref : selectedVbdRefs)
    {
        selectedVbdSet.insert(ref);
    }

    this->ui->storageTable->setSortingEnabled(false);
    this->ui->storageTable->setRowCount(0);
    this->m_storageVbdRefs.clear();
    this->m_storageVdiRefs.clear();

    bool showHidden = SettingsManager::instance().getShowHiddenObjects();
    bool storageLinkColumnVisible = false;
    const QList<QSharedPointer<VBD>> vbds = this->m_vm->GetVBDs();
    for (const QSharedPointer<VBD>& vbd : vbds)
    {
        if (!vbd || !vbd->IsValid())
            continue;

        QString type = vbd->GetType();

        // Skip CD/DVD and Floppy drives - they're shown in the CD/DVD section
        if (type == "CD" || type == "Floppy")
        {
            continue;
        }

        QSharedPointer<VDI> vdi = vbd->GetVDI();
        if (!vdi || !vdi->IsValid())
            continue;

        if (!showHidden && vdi->IsHidden())
            continue;

        QSharedPointer<SR> sr = vdi->GetSR();
        if (!sr || !sr->IsValid())
            continue;

        // Skip tools SRs
        QString srType = sr->GetType();
        if (srType == "udev")
        {
            continue;
        }

        // Build row data matching C# VBDRow
        QString position = vbd->GetUserdevice();
        QString vdiName = vdi->GetName();
        QString vdiDescription = vdi->GetDescription();
        QString srName = sr->GetName();
        QVariantMap smConfig = vdi->SMConfig();
        QString srVolume = smConfig.value("displayname", "").toString();
        if (smConfig.contains("SVID"))
        {
            storageLinkColumnVisible = true;
        }

        // Get size in human-readable format
        qint64 virtualSize = vdi->VirtualSize();
        QString size = "N/A";
        if (virtualSize > 0)
        {
            double sizeGB = virtualSize / (1024.0 * 1024.0 * 1024.0);
            size = QString::number(sizeGB, 'f', 2) + " GB";
        }

        // Check if read-only
        QString readOnly = vbd->IsReadOnly() ? "Yes" : "No";

        // Get QoS priority (IO nice value)
        int ioPriority = vbd->GetIoNice();
        QString priority;
        if (ioPriority == 0)
        {
            priority = "Lowest";
        } else if (ioPriority == 7)
        {
            priority = "Highest";
        } else
        {
            priority = QString::number(ioPriority);
        }

        // Check if currently attached
        bool currentlyAttached = vbd->CurrentlyAttached();
        QString active = currentlyAttached ? "Yes" : "No";

        // Get device path
        QString device = vbd->GetDevice();
        QString devicePath = device.isEmpty() ? "Unknown" : QString("/dev/%1").arg(device);

        // Add row to table
        int row = this->ui->storageTable->rowCount();
        this->ui->storageTable->insertRow(row);

        // Store VBD and VDI refs for context menu
        QTableWidgetItem* positionItem = new DevicePositionItem(position);
        positionItem->setData(Qt::UserRole, vbd->OpaqueRef());
        positionItem->setData(Qt::UserRole + 1, vdi->OpaqueRef());
        this->ui->storageTable->setItem(row, kColumnPosition, positionItem);

        this->ui->storageTable->setItem(row, kColumnName, new QTableWidgetItem(vdiName));
        this->ui->storageTable->setItem(row, kColumnDescription, new QTableWidgetItem(vdiDescription));
        this->ui->storageTable->setItem(row, kColumnSr, new QTableWidgetItem(srName));
        this->ui->storageTable->setItem(row, kColumnSrVolume, new QTableWidgetItem(srVolume));
        this->ui->storageTable->setItem(row, kColumnSize, new QTableWidgetItem(size));
        this->ui->storageTable->setItem(row, kColumnReadOnly, new QTableWidgetItem(readOnly));
        this->ui->storageTable->setItem(row, kColumnPriority, new QTableWidgetItem(priority));
        this->ui->storageTable->setItem(row, kColumnActive, new QTableWidgetItem(active));
        this->ui->storageTable->setItem(row, kColumnDevicePath, new QTableWidgetItem(devicePath));

        this->m_storageVbdRefs.insert(vbd->OpaqueRef());
        this->m_storageVdiRefs.insert(vdi->OpaqueRef());
    }

    this->ui->storageTable->setColumnHidden(kColumnSrVolume, !storageLinkColumnVisible);

    // Resize columns to content
    for (int i = 0; i < this->ui->storageTable->columnCount(); ++i)
    {
        this->ui->storageTable->resizeColumnToContents(i);
    }

    this->ui->storageTable->setSortingEnabled(true);
    this->ui->storageTable->sortItems(kColumnPosition, Qt::AscendingOrder);

    QItemSelectionModel* selectionModel = this->ui->storageTable->selectionModel();
    if (selectionModel)
    {
        selectionModel->clearSelection();
        for (int row = 0; row < this->ui->storageTable->rowCount(); ++row)
        {
            QTableWidgetItem* item = this->ui->storageTable->item(row, kColumnPosition);
            if (item && selectedVbdSet.contains(item->data(Qt::UserRole).toString()))
            {
                selectionModel->select(this->ui->storageTable->model()->index(row, kColumnPosition), QItemSelectionModel::Select | QItemSelectionModel::Rows);
            }
        }
    }
}

void VMStorageTabPage::refreshCDDVDDrives()
{
    // Clear previous data
    this->m_vbdRefs.clear();
    this->ui->driveComboBox->clear();

    // Only show for VMs (not control domain)
    bool isControlDomain = this->m_vm && this->m_vm->IsControlDomain();
    if (isControlDomain)
    {
        this->ui->cdDvdGroupBox->setVisible(false);
        return;
    }

    this->ui->cdDvdGroupBox->setVisible(true);

    // Get VBDs from VM object data
    int dvdCount = 0;
    int floppyCount = 0;

    // Iterate through VBDs and find CD/DVD drives
    const QList<QSharedPointer<VBD>> vbds = this->m_vm ? this->m_vm->GetVBDs() : QList<QSharedPointer<VBD>>();
    for (const QSharedPointer<VBD>& vbd : vbds)
    {
        if (!vbd || !vbd->IsValid())
            continue;

        if (vbd->IsCD())
        {
            dvdCount++;
            this->m_vbdRefs.append(vbd->OpaqueRef());
            this->ui->driveComboBox->addItem(QString("DVD Drive %1").arg(dvdCount), vbd->OpaqueRef());
        } else if (vbd->IsFloppyDrive())
        {
            floppyCount++;
            this->m_vbdRefs.append(vbd->OpaqueRef());
            this->ui->driveComboBox->addItem(QString("Floppy Drive %1").arg(floppyCount), vbd->OpaqueRef());
        }
    }

    // Update visibility based on drive count
    this->updateCDDVDVisibility();

    // Select first drive if available
    if (this->ui->driveComboBox->count() > 0)
    {
        this->ui->driveComboBox->setCurrentIndex(0);
        this->onDriveComboBoxChanged(0);
    }
}

void VMStorageTabPage::updateCDDVDVisibility()
{
    int driveCount = this->ui->driveComboBox->count();

    // Show single drive label or dropdown
    this->ui->singleDriveLabel->setVisible(driveCount == 1);
    this->ui->driveLabel->setVisible(driveCount > 1);
    this->ui->driveComboBox->setVisible(driveCount > 1);

    if (driveCount == 1)
    {
        this->ui->singleDriveLabel->setText(this->ui->driveComboBox->itemText(0));
    }

    // Show ISO selector and eject button only if drives exist
    this->ui->isoContainer->setVisible(driveCount > 0);

    // Show "New CD/DVD Drive" link if no drives
    this->ui->noDrivesLabel->setVisible(driveCount == 0);
}

void VMStorageTabPage::refreshISOList()
{
    QSignalBlocker blocker(this->ui->isoComboBox);
    this->ui->isoComboBox->clear();

    if (this->m_currentVBDRef.isEmpty() || !this->m_connection || !this->m_connection->GetCache())
        return;

    IsoDropDownBox* isoBox = qobject_cast<IsoDropDownBox*>(this->ui->isoComboBox);
    if (!isoBox)
        return;

    isoBox->SetConnection(this->m_connection);
    isoBox->SetVMRef(this->m_objectRef);
    isoBox->Refresh();

    QSharedPointer<VBD> vbd = this->m_connection->GetCache()->ResolveObject<VBD>(this->m_currentVBDRef);
    QString currentVdiRef = vbd ? vbd->GetVDIRef() : QString();
    bool empty = vbd ? vbd->Empty() : true;

    if (!empty && !currentVdiRef.isEmpty())
    {
        isoBox->SetSelectedVdiRef(currentVdiRef);
        if (isoBox->SelectedVdiRef() != currentVdiRef)
        {
            QSharedPointer<VDI> vdi = this->m_connection->GetCache()->ResolveObject<VDI>(currentVdiRef);
            if (vdi && vdi->IsValid())
            {
                QString isoName = vdi->GetName();
                this->ui->isoComboBox->addItem(isoName + " (mounted)", currentVdiRef);
                this->ui->isoComboBox->setCurrentIndex(this->ui->isoComboBox->count() - 1);
            }
        }
    } else
    {
        isoBox->SetSelectedVdiRef(QString());
    }
}

void VMStorageTabPage::onDriveComboBoxChanged(int index)
{
    if (index < 0)
    {
        this->m_currentVBDRef.clear();
        return;
    }

    this->m_currentVBDRef = this->ui->driveComboBox->itemData(index).toString();
    this->refreshISOList();
}

void VMStorageTabPage::onIsoComboBoxChanged(int index)
{
    if (index < 0 || !this->m_connection || !this->m_connection->GetCache())
        return;

    QString vdiRef = this->ui->isoComboBox->itemData(index).toString();

    if (this->m_currentVBDRef.isEmpty())
        return;

    QSharedPointer<VBD> vbd = this->m_connection->GetCache()->ResolveObject<VBD>(this->m_currentVBDRef);
    if (!vbd || !vbd->IsValid())
        return;

    QString currentVdiRef = vbd->GetVDIRef();
    bool empty = vbd->Empty();

    if ((vdiRef.isEmpty() && empty) || (!vdiRef.isEmpty() && vdiRef == currentVdiRef && !empty))
    {
        return;
    }

    // Get connection
    XenConnection* conn = this->m_connection;

    if (!conn)
        return;

    // Disable UI during operation
    this->ui->isoComboBox->setEnabled(false);
    this->ui->ejectButton->setEnabled(false);

    // Get VM object from cache
    QSharedPointer<VM> vm = conn->GetCache()->ResolveObject<VM>(XenObjectType::VM, this->m_objectRef);
    if (!vm || !vm->IsValid())
    {
        this->ui->isoComboBox->setEnabled(true);
        this->ui->ejectButton->setEnabled(true);
        return;
    }

    // Create and run the AsyncOperation
    ChangeVMISOAction* action = new ChangeVMISOAction(
        vm,                    // VM object
        vdiRef,                // VDI ref (empty for eject)
        this->m_currentVBDRef, // VBD ref
        this);


    // Connect to completion signals
    connect(action, &AsyncOperation::completed, this, [this, action]() {
        // Re-enable UI
        this->ui->isoComboBox->setEnabled(true);
        this->ui->ejectButton->setEnabled(true);

        // Refresh the CD/DVD drives to show updated state
        this->refreshCDDVDDrives();

        // Clean up
        action->deleteLater();
    });

    connect(action, &AsyncOperation::failed, this, [this, action](QString error)
    {
        // Re-enable UI
        this->ui->isoComboBox->setEnabled(true);
        this->ui->ejectButton->setEnabled(true);

        // Show error message
        QMessageBox::warning(this, "Failed", error);

        // Revert combo box selection
        this->refreshISOList();

        // Clean up
        action->deleteLater();
    });

    // Start the operation (runs on QThreadPool)
    action->RunAsync();
}

void VMStorageTabPage::onEjectButtonClicked()
{
    // Set to [Empty]
    this->ui->isoComboBox->setCurrentIndex(0);
}

void VMStorageTabPage::onNewCDDriveLinkClicked(const QString& link)
{
    Q_UNUSED(link);

    if (!this->m_connection || !this->m_connection->GetCache() || this->m_objectRef.isEmpty())
        return;

    qDebug() << "Creating new CD/DVD drive for VM:" << this->m_objectRef;

    // Disable the link during operation
    this->ui->noDrivesLabel->setEnabled(false);

    // Find next available device position
    int maxDeviceNum = -1;

    const QList<QSharedPointer<VBD>> vbds = this->m_vm ? this->m_vm->GetVBDs() : QList<QSharedPointer<VBD>>();
    for (const QSharedPointer<VBD>& vbd : vbds)
    {
        if (!vbd || !vbd->IsValid())
            continue;

        bool ok = false;
        int deviceNum = vbd->GetUserdevice().toInt(&ok);
        if (ok && deviceNum > maxDeviceNum)
        {
            maxDeviceNum = deviceNum;
        }
    }

    QString nextDevice = QString::number(maxDeviceNum + 1);

    // Create the new CD/DVD drive using VbdCreateAndPlugAction
    XenConnection* connection = this->m_connection;
    if (!connection)
    {
        QMessageBox::warning(this, tr("Error"), tr("No active connection."));
        return;
    }

    QSharedPointer<VM> vm(new VM(connection, m_objectRef));

    // Build VBD record for CD/DVD drive (empty VDI)
    QVariantMap vbdRecord;
    vbdRecord["VM"] = m_objectRef;
    vbdRecord["VDI"] = XENOBJECT_NULL; // Empty for CD/DVD
    vbdRecord["userdevice"] = nextDevice;
    vbdRecord["bootable"] = false;
    vbdRecord["mode"] = "RO";
    vbdRecord["type"] = "CD";
    vbdRecord["unpluggable"] = true;
    vbdRecord["empty"] = true;
    vbdRecord["other_config"] = QVariantMap();
    vbdRecord["qos_algorithm_type"] = "";
    vbdRecord["qos_algorithm_params"] = QVariantMap();

    VbdCreateAndPlugAction* createAction = new VbdCreateAndPlugAction(vm, vbdRecord, tr("CD/DVD Drive"), false, this);

    ActionProgressDialog* dialog = new ActionProgressDialog(createAction, this);
    if (dialog->exec() != QDialog::Accepted)
    {
        QMessageBox::warning(this, tr("Failed"), tr("Failed to create CD/DVD drive."));
        delete dialog;
        return;
    }

    QString newVbdRef = createAction->GetResult();
    delete dialog;

    this->ui->noDrivesLabel->setEnabled(true);

    if (!newVbdRef.isEmpty())
    {
        qDebug() << "CD/DVD drive created successfully:" << newVbdRef;

        // Refresh the drives list to show new drive
        // We need to update the VM's VBD list first
        refreshVmRecord(this->m_connection, this->m_objectRef);
    } else
    {
        qDebug() << "Failed to create CD/DVD drive";
        QMessageBox::warning(this, "Create Drive Failed", "Failed to create CD/DVD drive. Check the error log for details.");
    }
}

// Storage table button and context menu implementation

void VMStorageTabPage::updateStorageButtons()
{
    // Different button visibility for VM vs SR view
    // C# Reference: SrStoragePage.cs RefreshButtons() lines 400-445

    if (!this->m_object)
        return;

    if (this->m_object->GetObjectType() == XenObjectType::VM)
    {
        // VM View: Show VM-specific buttons
        // Hide SR-specific buttons: Rescan, Move
        this->ui->rescanButton->setVisible(false);
        this->ui->moveButton->setVisible(true);

        this->ui->attachButton->setVisible(true);
        this->ui->activateButton->setVisible(true);
        this->ui->deactivateButton->setVisible(true);
        this->ui->detachButton->setVisible(true);
        this->ui->addButton->setVisible(true);
        this->ui->editButton->setVisible(true);
        this->ui->deleteButton->setVisible(true);

        // Original VM button logic
        QStringList vbdRefs = getSelectedVBDRefs();
        QStringList vdiRefs = getSelectedVDIRefs();

        bool hasSelection = !vbdRefs.isEmpty();
        bool hasVDI = !vdiRefs.isEmpty();

        bool vmRunning = this->m_vm && this->m_vm->GetPowerState() == "Running";

        bool anyAttached = false;
        bool anyDetached = false;
        bool anyLocked = false;
        bool anyActivateEligible = false;
        bool anyDeactivateEligible = false;
        bool anyDetachEligible = false;
        bool anyDeleteEligible = false;
        bool anyMoveEligible = false;

        if (hasSelection && this->m_connection && this->m_connection->GetCache() && this->m_vm)
        {
            for (const QString& vbdRef : vbdRefs)
            {
                QSharedPointer<VBD> vbd = this->m_connection->GetCache()->ResolveObject<VBD>(vbdRef);
                if (!vbd || !vbd->IsValid())
                    continue;

                QSharedPointer<VDI> vdi = vbd->GetVDI();

                bool currentlyAttached = vbd->CurrentlyAttached();
                anyAttached = anyAttached || currentlyAttached;
                anyDetached = anyDetached || !currentlyAttached;

                bool vbdLocked = vbd->IsLocked() || vbd->AllowedOperations().isEmpty();
                bool vdiLocked = vdi ? (vdi->IsLocked() || vdi->AllowedOperations().isEmpty()) : true;

                bool isLocked = vbdLocked || vdiLocked;
                anyLocked = anyLocked || isLocked;

                if (this->canActivateVBD(vbd, vdi, this->m_vm))
                {
                    anyActivateEligible = true;
                }

                if (this->canDeactivateVBD(vbd, vdi, this->m_vm))
                {
                    anyDeactivateEligible = true;
                }

                if (vdi && vdi->IsValid() && !isLocked)
                {
                    anyDetachEligible = true;
                    anyDeleteEligible = true;
                    anyMoveEligible = true;
                }
            }
        }

        // Add: Always enabled for VMs
        this->ui->addButton->setEnabled(true);

        // Attach: Always enabled for VMs
        this->ui->attachButton->setEnabled(true);

        bool showActivate = anyDetached;
        if (!hasSelection)
        {
            showActivate = false;
        }

        this->ui->activateButton->setVisible(showActivate);
        this->ui->deactivateButton->setVisible(!showActivate);

        // Activate: when at least one selected VBD can be activated
        this->ui->activateButton->setEnabled(hasSelection && anyActivateEligible);

        // Deactivate: when at least one selected VBD can be deactivated
        this->ui->deactivateButton->setEnabled(hasSelection && anyDeactivateEligible && vmRunning);

        // Detach/Delete/Move: enable if at least one selected VDI is eligible
        this->ui->detachButton->setEnabled(hasSelection && hasVDI && anyDetachEligible);
        this->ui->deleteButton->setEnabled(hasSelection && hasVDI && anyDeleteEligible);
        this->ui->moveButton->setEnabled(hasSelection && hasVDI && anyMoveEligible);

        // Properties/Edit: Enabled for single selection
        bool singleSelection = (vbdRefs.size() == 1);
        bool canEdit = false;
        if (singleSelection && this->m_connection && this->m_connection->GetCache())
        {
            QString vbdRef = vbdRefs.first();
            QSharedPointer<VBD> vbd = this->m_connection->GetCache()->ResolveObject<VBD>(vbdRef);
            QSharedPointer<VDI> vdi = vbd ? vbd->GetVDI() : QSharedPointer<VDI>();

            bool vbdLocked = !vbd || vbd->IsLocked() || vbd->AllowedOperations().isEmpty();
            bool vdiLocked = !vdi || vdi->IsLocked() || vdi->AllowedOperations().isEmpty();

            canEdit = vdi && vdi->IsValid() && !vbdLocked && !vdiLocked;
        }

        this->ui->editButton->setEnabled(singleSelection && canEdit);
        return;
    }

    // Non-VM fallback - hide controls defensively
    this->ui->addButton->setVisible(false);
    this->ui->attachButton->setVisible(false);
    this->ui->rescanButton->setVisible(false);
    this->ui->activateButton->setVisible(false);
    this->ui->deactivateButton->setVisible(false);
    this->ui->moveButton->setVisible(false);
    this->ui->detachButton->setVisible(false);
    this->ui->deleteButton->setVisible(false);
    this->ui->editButton->setVisible(false);
}

QString VMStorageTabPage::getSelectedVBDRef() const
{
    QStringList refs = this->getSelectedVBDRefs();
    return refs.isEmpty() ? QString() : refs.first();
}

QString VMStorageTabPage::getSelectedVDIRef() const
{
    QStringList refs = this->getSelectedVDIRefs();
    return refs.isEmpty() ? QString() : refs.first();
}

QStringList VMStorageTabPage::getSelectedVBDRefs() const
{
    QStringList refs;
    QList<QTableWidgetItem*> selectedItems = this->ui->storageTable->selectedItems();
    QSet<int> rows;

    for (QTableWidgetItem* item : selectedItems)
    {
        if (item)
        {
            rows.insert(item->row());
        }
    }

    for (int row : rows)
    {
        QTableWidgetItem* item = this->ui->storageTable->item(row, 0);
        if (item)
        {
            QString vbdRef = item->data(Qt::UserRole).toString();
            if (!vbdRef.isEmpty())
            {
                refs.append(vbdRef);
            }
        }
    }

    return refs;
}

QStringList VMStorageTabPage::getSelectedVDIRefs() const
{
    QStringList refs;
    QList<QTableWidgetItem*> selectedItems = this->ui->storageTable->selectedItems();
    QSet<int> rows;

    for (QTableWidgetItem* item : selectedItems)
    {
        if (item)
        {
            rows.insert(item->row());
        }
    }

    for (int row : rows)
    {
        QTableWidgetItem* item = this->ui->storageTable->item(row, 0);
        if (item)
        {
            QString vdiRef = item->data(Qt::UserRole + 1).toString();
            if (!vdiRef.isEmpty() && vdiRef != XENOBJECT_NULL)
            {
                refs.append(vdiRef);
            }
        }
    }

    return refs;
}

bool VMStorageTabPage::canActivateVBD(const QSharedPointer<VBD>& vbd, const QSharedPointer<VDI>& vdi, const QSharedPointer<VM>& vm) const
{
    if (!vbd || !vdi || !vm || !vbd->IsValid() || !vdi->IsValid() || !vm->IsValid())
        return false;

    if (vm->IsTemplate())
        return false;

    if (vm->GetPowerState() != "Running")
        return false;

    if (vdi->GetType() == "system")
        return false;

    if (vbd->CurrentlyAttached())
        return false;

    if (!vbd->AllowedOperations().contains("plug"))
        return false;

    return true;
}

bool VMStorageTabPage::canDeactivateVBD(const QSharedPointer<VBD>& vbd, const QSharedPointer<VDI>& vdi, const QSharedPointer<VM>& vm) const
{
    if (!vbd || !vdi || !vm || !vbd->IsValid() || !vdi->IsValid() || !vm->IsValid())
        return false;

    if (vm->IsTemplate())
        return false;

    if (vm->GetPowerState() != "Running")
        return false;

    if (vdi->GetType() == "system")
    {
        bool isOwner = vbd->IsOwner() || vbd->GetUserdevice() == "0" || vbd->IsBootable();
        if (isOwner)
            return false;
    }

    if (!vbd->CurrentlyAttached())
        return false;

    if (!vbd->AllowedOperations().contains("unplug"))
        return false;

    return true;
}

void VMStorageTabPage::runVbdPlugOperations(const QStringList& vbdRefs, bool plug)
{
    if (vbdRefs.isEmpty() || !this->m_connection || !this->m_connection->GetCache())
        return;

    XenConnection* connection = this->m_connection;
    if (!connection || !connection->GetSession())
    {
        QMessageBox::warning(this, tr("Error"), tr("No active connection."));
        return;
    }

    QList<AsyncOperation*> operations;
    XenCache* cache = this->m_connection->GetCache();

    for (const QString& vbdRef : vbdRefs)
    {
        if (vbdRef.isEmpty())
        {
            continue;
        }

        QString vdiName = tr("Virtual Disk");
        QString vmName = tr("VM");

        QSharedPointer<VBD> vbd = cache ? cache->ResolveObject<VBD>(vbdRef) : QSharedPointer<VBD>();
        if (vbd && vbd->IsValid())
        {
            QSharedPointer<VDI> vdi = vbd->GetVDI();
            if (vdi && vdi->IsValid())
                vdiName = vdi->GetName();
        }

        if (this->m_vm && this->m_vm->IsValid())
            vmName = this->m_vm->GetName();

        QString opTitle = plug
                              ? tr("Activating disk '%1' on '%2'").arg(vdiName, vmName)
                              : tr("Deactivating disk '%1' on '%2'").arg(vdiName, vmName);
        QString opDesc = plug ? tr("Activating disk...") : tr("Deactivating disk...");

        auto* op = new DelegatedAsyncOperation(
            connection,
            opTitle,
            opDesc,
            [vbdRef, plug](DelegatedAsyncOperation* operation) {
                if (plug)
                {
                    XenAPI::VBD::plug(operation->GetSession(), vbdRef);
                }
                else
                {
                    XenAPI::VBD::unplug(operation->GetSession(), vbdRef);
                }
                operation->SetPercentComplete(100);
            },
            this);

        operations.append(op);
    }

    if (operations.isEmpty())
    {
        return;
    }

    QString title = plug ? tr("Activating Virtual Disks") : tr("Deactivating Virtual Disks");
    QString startDesc = plug ? tr("Activating disks...") : tr("Deactivating disks...");
    QString endDesc = tr("Completed");

    MultipleAction* multi = new MultipleAction(
        connection,
        title,
        startDesc,
        endDesc,
        operations,
        true,
        true,
        false,
        this);

    ActionProgressDialog* dialog = new ActionProgressDialog(multi, this);
    int result = dialog->exec();
    delete dialog;

    if (result == QDialog::Accepted)
    {
        // C# shows no success dialog here; status updates are handled elsewhere.
    } else
    {
        QString failText = plug ? tr("Failed to activate virtual disk(s).")
                                : tr("Failed to deactivate virtual disk(s).");
        QMessageBox::warning(this, tr("Failed"), failText);
    }

    refreshVmRecord(this->m_connection, this->m_objectRef);

    delete multi;
}

void VMStorageTabPage::runDetachOperations(const QStringList& vdiRefs)
{
    if (vdiRefs.isEmpty() || !this->m_vm || !this->m_vm->GetConnection())
        return;

    QString confirmText;
    if (vdiRefs.size() == 1)
    {
        QSharedPointer<VDI> vdi = this->m_vm->GetCache()->ResolveObject<VDI>(vdiRefs.first());
        QString vdiName = vdi ? vdi->GetName() : tr("this virtual disk");
        confirmText = tr("Are you sure you want to detach '%1' from this VM?\n\n"
                         "The disk will not be deleted and can be attached again later.")
                          .arg(vdiName);
    } else
    {
        confirmText = tr("Are you sure you want to detach the selected virtual disks from this VM?\n\n"
                         "The disks will not be deleted and can be attached again later.");
    }

    if (QMessageBox::question(this, tr("Detach Virtual Disk"), confirmText, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }

    QList<AsyncOperation*> operations;

    for (const QString& vdiRef : vdiRefs)
    {
        if (vdiRef.isEmpty())
        {
            continue;
        }
        operations.append(new DetachVirtualDiskAction(vdiRef, this->m_vm.data(), this));
    }

    if (operations.isEmpty())
    {
        return;
    }

    MultipleAction* multi = new MultipleAction(
        this->m_vm->GetConnection(),
        tr("Detaching Virtual Disks"),
        tr("Detaching disks..."),
        tr("Completed"),
        operations,
        true,
        true,
        false,
        this);

    ActionProgressDialog* dialog = new ActionProgressDialog(multi, this);
    dialog->exec();
    delete dialog;

    refreshVmRecord(this->m_vm->GetConnection(), this->m_vm->OpaqueRef());

    delete multi;
}

void VMStorageTabPage::runDeleteOperations(const QStringList& vdiRefs)
{
    if (vdiRefs.isEmpty() || !this->m_vm || !this->m_vm->GetConnection())
        return;

    QString confirmText;
    if (vdiRefs.size() == 1)
    {
        QSharedPointer<VDI> vdi = this->m_vm->GetCache()->ResolveObject<VDI>(vdiRefs.first());
        QString vdiName = vdi ? vdi->GetName() : tr("this virtual disk");
        confirmText = tr("Are you sure you want to permanently delete '%1'?\n\nThis operation cannot be undone.").arg(vdiName);
    } else
    {
        confirmText = tr("Are you sure you want to permanently delete the selected virtual disks?\n\nThis operation cannot be undone.");
    }

    if (QMessageBox::question(this, tr("Delete Virtual Disk"), confirmText, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    bool allowRunningVMDelete = false;
    for (const QString& vdiRef : vdiRefs)
    {
        QSharedPointer<VDI> vdi = this->m_vm->GetCache()->ResolveObject<VDI>(vdiRef);
        if (!vdi || !vdi->IsValid())
            continue;

        const QList<QSharedPointer<VBD>> vbds = vdi->GetVBDs();
        for (const QSharedPointer<VBD>& vbd : vbds)
        {
            if (vbd && vbd->CurrentlyAttached())
            {
                allowRunningVMDelete = true;
                break;
            }
        }
        if (allowRunningVMDelete)
        {
            break;
        }
    }

    if (allowRunningVMDelete)
    {
        QString attachedText = tr("One or more disks are currently attached to a running VM.\n\n"
                                  "Do you want to detach and delete them anyway?");
        if (QMessageBox::question(this, tr("Disk Currently Attached"), attachedText, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        {
            return;
        }
    }

    QList<AsyncOperation*> operations;
    for (const QString& vdiRef : vdiRefs)
    {
        if (vdiRef.isEmpty())
        {
            continue;
        }

        operations.append(new DestroyDiskAction(vdiRef, this->m_vm->GetConnection(), allowRunningVMDelete, this));
    }

    if (operations.isEmpty())
    {
        return;
    }

    MultipleAction* multi = new MultipleAction(
        this->m_vm->GetConnection(),
        tr("Deleting Virtual Disks"),
        tr("Deleting disks..."),
        tr("Completed"),
        operations,
        true,
        true,
        false,
        this);

    ActionProgressDialog* dialog = new ActionProgressDialog(multi, this);
    dialog->exec();
    delete dialog;

    refreshVmRecord(this->m_connection, this->m_objectRef);

    delete multi;
}

void VMStorageTabPage::onStorageTableSelectionChanged()
{
    this->updateStorageButtons();
}

void VMStorageTabPage::onStorageTableDoubleClicked(const QModelIndex& index)
{
    Q_UNUSED(index);

    if (this->ui->editButton->isEnabled())
    {
        this->onEditButtonClicked();
    }
}

void VMStorageTabPage::onStorageTableCustomContextMenuRequested(const QPoint& pos)
{
    QTableWidgetItem* clickedItem = this->ui->storageTable->itemAt(pos);
    if (clickedItem)
    {
        int row = clickedItem->row();
        QTableWidgetItem* rowItem = this->ui->storageTable->item(row, 0);
        if (rowItem && !rowItem->isSelected())
        {
            QItemSelectionModel* selectionModel = this->ui->storageTable->selectionModel();
            if (selectionModel)
            {
                selectionModel->select(this->ui->storageTable->model()->index(row, 0),
                                       QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            }
        }
    } else
    {
        this->ui->storageTable->clearSelection();
    }

    QMenu contextMenu(this);

    // Build context menu based on object type and button visibility
    // C# Reference: SrStoragePage.cs contextMenuStrip_Opening() lines 379-399

    if (this->m_object && this->m_object->GetObjectType() == XenObjectType::VM)
    {
        // VM View: Add, Attach, Activate/Deactivate, Detach, Delete, Properties
        bool hasVisibleAction = false;
        bool hasPrimaryAction = false;

        if (this->ui->addButton->isVisible())
        {
            QAction* addAction = contextMenu.addAction("Add Virtual Disk...");
            addAction->setEnabled(this->ui->addButton->isEnabled());
            connect(addAction, &QAction::triggered, this, &VMStorageTabPage::onAddButtonClicked);
            hasVisibleAction = true;
            hasPrimaryAction = true;
        }

        if (this->ui->attachButton->isVisible())
        {
            QAction* attachAction = contextMenu.addAction("Attach Virtual Disk...");
            attachAction->setEnabled(this->ui->attachButton->isEnabled());
            connect(attachAction, &QAction::triggered, this, &VMStorageTabPage::onAttachButtonClicked);
            hasVisibleAction = true;
            hasPrimaryAction = true;
        }

        if (this->ui->activateButton->isVisible())
        {
            QAction* activateAction = contextMenu.addAction("Activate");
            activateAction->setEnabled(this->ui->activateButton->isEnabled());
            connect(activateAction, &QAction::triggered, this, &VMStorageTabPage::onActivateButtonClicked);
            hasVisibleAction = true;
            hasPrimaryAction = true;
        }

        if (this->ui->deactivateButton->isVisible())
        {
            QAction* deactivateAction = contextMenu.addAction("Deactivate");
            deactivateAction->setEnabled(this->ui->deactivateButton->isEnabled());
            connect(deactivateAction, &QAction::triggered, this, &VMStorageTabPage::onDeactivateButtonClicked);
            hasVisibleAction = true;
            hasPrimaryAction = true;
        }

        if (this->ui->moveButton->isVisible())
        {
            QAction* moveAction = contextMenu.addAction("Move Virtual Disk...");
            moveAction->setEnabled(this->ui->moveButton->isEnabled());
            connect(moveAction, &QAction::triggered, this, &VMStorageTabPage::onMoveButtonClicked);
            hasVisibleAction = true;
            hasPrimaryAction = true;
        }

        if (this->ui->detachButton->isVisible())
        {
            QAction* detachAction = contextMenu.addAction("Detach Virtual Disk");
            detachAction->setEnabled(this->ui->detachButton->isEnabled());
            connect(detachAction, &QAction::triggered, this, &VMStorageTabPage::onDetachButtonClicked);
            hasVisibleAction = true;
            hasPrimaryAction = true;
        }

        if (this->ui->deleteButton->isVisible())
        {
            QAction* deleteAction = contextMenu.addAction("Delete Virtual Disk...");
            deleteAction->setEnabled(this->ui->deleteButton->isEnabled());
            connect(deleteAction, &QAction::triggered, this, &VMStorageTabPage::onDeleteButtonClicked);
            hasVisibleAction = true;
            hasPrimaryAction = true;
        }

        if (this->ui->editButton->isVisible())
        {
            if (hasPrimaryAction)
            {
                contextMenu.addSeparator();
            }

            QAction* editAction = contextMenu.addAction("Properties...");
            editAction->setEnabled(this->ui->editButton->isEnabled());
            connect(editAction, &QAction::triggered, this, &VMStorageTabPage::onEditButtonClicked);
            hasVisibleAction = true;
        }

        if (!hasVisibleAction)
        {
            return;
        }
    } else
    {
        return;
    }

    // Show context menu at the requested position
    contextMenu.exec(this->ui->storageTable->mapToGlobal(pos));
}

void VMStorageTabPage::onAddButtonClicked()
{
    if (!this->m_vm)
        return;

    if (!this->m_vm->GetConnection())
    {
        QMessageBox::warning(this, "Error", "No connection available.");
        return;
    }

    // Open New Virtual Disk Dialog
    NewVirtualDiskDialog dialog(this->m_vm, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    // Get parameters from dialog
    QString srRef = dialog.getSelectedSR();
    QString name = dialog.getVDIName();
    QString description = dialog.getVDIDescription();
    qint64 size = dialog.getSize();
    QString devicePosition = dialog.getDevicePosition();
    QString mode = dialog.getMode();
    bool bootable = dialog.isBootable();

    // Build VDI record
    QVariantMap vdiRecord;
    vdiRecord["name_label"] = name;
    vdiRecord["name_description"] = description;
    vdiRecord["SR"] = srRef;
    vdiRecord["virtual_size"] = QString::number(size);
    vdiRecord["type"] = "user";
    vdiRecord["sharable"] = false;
    vdiRecord["read_only"] = false;
    vdiRecord["other_config"] = QVariantMap();

    // Create VDI using CreateDiskAction
    qDebug() << "Creating VDI:" << name << "size:" << size << "in SR:" << srRef;

    CreateDiskAction* createAction = new CreateDiskAction(vdiRecord, this->m_vm->GetConnection(), this);

    ActionProgressDialog* createDialog = new ActionProgressDialog(createAction, this);
    qDebug() << "[StorageTabPage] Executing create dialog for VDI...";
    int dialogResult = createDialog->exec();
    qDebug() << "[StorageTabPage] Create dialog exec() returned:" << dialogResult
             << "QDialog::Accepted=" << QDialog::Accepted
             << "QDialog::Rejected=" << QDialog::Rejected;

    if (dialogResult != QDialog::Accepted)
    {
        qWarning() << "[StorageTabPage] Dialog was rejected!";
        QMessageBox::warning(this, "Failed", "Failed to create virtual disk.");
        delete createDialog;
        return;
    }

    qDebug() << "[StorageTabPage] Dialog was accepted, getting VDI ref...";
    QString vdiRef = createAction->GetResult();
    delete createDialog;

    if (vdiRef.isEmpty())
    {
        qWarning() << "[StorageTabPage] VDI ref is empty!";
        QMessageBox::warning(this, "Failed", "Failed to create virtual disk.");
        return;
    }

    qDebug() << "VDI created:" << vdiRef << "Now attaching to VM...";

    // Create VBD and attach to VM using VbdCreateAndPlugAction

    QVariantMap vbdRecord;
    vbdRecord["VM"] = this->m_vm->OpaqueRef();
    vbdRecord["VDI"] = vdiRef;
    vbdRecord["userdevice"] = devicePosition;
    vbdRecord["bootable"] = bootable;
    vbdRecord["mode"] = mode;
    vbdRecord["type"] = "Disk";
    vbdRecord["unpluggable"] = true;
    vbdRecord["empty"] = false;
    vbdRecord["other_config"] = QVariantMap();
    vbdRecord["qos_algorithm_type"] = "";
    vbdRecord["qos_algorithm_params"] = QVariantMap();

    VbdCreateAndPlugAction* attachAction = new VbdCreateAndPlugAction(this->m_vm, vbdRecord, name, false, this);

    ActionProgressDialog* attachDialog = new ActionProgressDialog(attachAction, this);
    if (attachDialog->exec() != QDialog::Accepted)
    {
        QMessageBox::warning(this, "Warning", "Virtual disk created but failed to attach to VM.\nYou can attach it manually from the Attach menu.");
        delete attachDialog;
        // VDI was created, so refresh anyway
        refreshVmRecord(this->m_connection, this->m_objectRef);
        return;
    }

    delete attachDialog;

    qDebug() << "VBD created and attached successfully";
    QMessageBox::information(this, "Success", "Virtual disk created and attached successfully.");

    // Refresh to show new disk
    refreshVmRecord(this->m_connection, this->m_objectRef);
}

void VMStorageTabPage::onAttachButtonClicked()
{
    if (!this->m_connection || !this->m_vm)
        return;

    // Open Attach Virtual Disk Dialog
    AttachVirtualDiskDialog dialog(this->m_vm, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    // Get parameters from dialog
    QString vdiRef = dialog.getSelectedVDIRef();
    QString devicePosition = dialog.getDevicePosition();
    QString mode = dialog.getMode();
    bool bootable = dialog.isBootable();

    if (vdiRef.isEmpty())
    {
        QMessageBox::warning(this, "Error", "No virtual disk selected.");
        return;
    }

    // Attach VDI to VM using VbdCreateAndPlugAction
    qDebug() << "Attaching VDI:" << vdiRef << "to VM:" << this->m_objectRef;

    XenConnection* connection = this->m_connection;
    if (!connection)
    {
        QMessageBox::warning(this, tr("Error"), tr("No active connection."));
        return;
    }

    // Build VBD record
    QVariantMap vbdRecord;
    vbdRecord["VM"] = m_objectRef;
    vbdRecord["VDI"] = vdiRef;
    vbdRecord["userdevice"] = devicePosition;
    vbdRecord["bootable"] = bootable;
    vbdRecord["mode"] = mode;
    vbdRecord["type"] = "Disk";
    vbdRecord["unpluggable"] = true;
    vbdRecord["empty"] = false;
    vbdRecord["other_config"] = QVariantMap();
    vbdRecord["qos_algorithm_type"] = "";
    vbdRecord["qos_algorithm_params"] = QVariantMap();

    // Get VDI name for display
    QString vdiName = "Virtual Disk";
    QSharedPointer<VDI> vdi = this->m_connection->GetCache()->ResolveObject<VDI>(vdiRef);
    if (vdi && vdi->IsValid())
        vdiName = vdi->GetName();

    VbdCreateAndPlugAction* attachAction = new VbdCreateAndPlugAction(this->m_vm, vbdRecord, vdiName, false, this);
    ActionProgressDialog* attachDialog = new ActionProgressDialog(attachAction, this);

    if (attachDialog->exec() != QDialog::Accepted)
    {
        QMessageBox::warning(this, tr("Failed"), tr("Failed to attach virtual disk."));
        delete attachDialog;
        return;
    }

    QString vbdRef = attachAction->GetResult();
    delete attachDialog;

    if (vbdRef.isEmpty())
    {
        QMessageBox::warning(this, tr("Failed"), tr("Failed to attach virtual disk."));
        return;
    }

    qDebug() << "VBD created:" << vbdRef;

    // Refresh to show attached disk
    refreshVmRecord(this->m_connection, this->m_objectRef);
}

void VMStorageTabPage::onActivateButtonClicked()
{
    QStringList vbdRefs = getSelectedVBDRefs();
    this->runVbdPlugOperations(vbdRefs, true);
}

void VMStorageTabPage::onDeactivateButtonClicked()
{
    QStringList vbdRefs = getSelectedVBDRefs();
    this->runVbdPlugOperations(vbdRefs, false);
}

void VMStorageTabPage::onMoveButtonClicked()
{
    QStringList vdiRefs = getSelectedVDIRefs();
    if (vdiRefs.isEmpty() || !this->m_connection)
    {
        return;
    }

    QList<QSharedPointer<VDI>> vdis;
    vdis.reserve(vdiRefs.size());
    for (const QString& vdiRef : vdiRefs)
    {
        QSharedPointer<VDI> vdi = this->m_connection->GetCache()->ResolveObject<VDI>(vdiRef);
        if (vdi && vdi->IsValid())
            vdis.append(vdi);
    }

    if (vdis.isEmpty())
        return;

    MoveVirtualDiskDialog dialog(vdis, this);
    if (dialog.exec() == QDialog::Accepted)
    {
        refreshVmRecord(this->m_connection, this->m_objectRef);
    }
}

void VMStorageTabPage::onDetachButtonClicked()
{
    QStringList vdiRefs = getSelectedVDIRefs();
    this->runDetachOperations(vdiRefs);
}

void VMStorageTabPage::onDeleteButtonClicked()
{
    QStringList vdiRefs = getSelectedVDIRefs();
    this->runDeleteOperations(vdiRefs);
}

void VMStorageTabPage::onEditButtonClicked()
{
    QString vdiRef = getSelectedVDIRef();
    if (vdiRef.isEmpty() || !this->m_connection)
        return;

    QSharedPointer<VDI> vdi = this->m_connection->GetCache()->ResolveObject<VDI>(vdiRef);
    if (!vdi || !vdi->IsValid())
        return;

    // Open VDI Properties Dialog
    VdiPropertiesDialog dialog(vdi, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    // Refresh to show updated properties
    refreshVmRecord(this->m_connection, this->m_objectRef);
}
