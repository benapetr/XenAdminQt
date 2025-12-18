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
#include "xenlib.h"
#include "xencache.h"
#include "xen/api.h"
#include "xen/connection.h"
#include "xen/vm.h"
#include "xen/vdi.h"
#include "xen/vbd.h"
#include "xen/xenapi/VBD.h"
#include "xen/actions/vm/changevmisoaction.h"
#include "xen/actions/vdi/creatediskaction.h"
#include "xen/actions/vdi/detachvirtualdiskaction.h"
#include "xen/actions/vdi/destroydiskaction.h"
#include "xen/actions/vbd/vbdcreateandplugaction.h"
#include "xen/actions/delegatedasyncoperation.h"
#include "dialogs/newvirtualdiskdialog.h"
#include "dialogs/attachvirtualdiskdialog.h"
#include "dialogs/vdipropertiesdialog.h"
#include "dialogs/operationprogressdialog.h"
#include "../operations/operationmanager.h"
#include <QTableWidgetItem>
#include <QDebug>
#include <QMessageBox>
#include <QMenu>
#include <QAction>

VMStorageTabPage::VMStorageTabPage(QWidget* parent) : BaseTabPage(parent), ui(new Ui::VMStorageTabPage)
{
    this->ui->setupUi(this);
    this->ui->storageTable->horizontalHeader()->setStretchLastSection(true);

    // Make table read-only (C# SrStoragePage.Designer.cs line 210: dataGridViewVDIs.ReadOnly = true)
    this->ui->storageTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

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

bool VMStorageTabPage::isApplicableForObjectType(const QString& objectType) const
{
    // VM storage tab is only applicable to VMs (matches C# VMStoragePage)
    return objectType == "vm";
}

void VMStorageTabPage::setXenObject(const QString& objectType, const QString& objectRef, const QVariantMap& objectData)
{
    // Disconnect previous object updates
    if (this->m_xenLib)
    {
        disconnect(this->m_xenLib, &XenLib::objectDataReceived, this, &VMStorageTabPage::onObjectDataReceived);
    }

    // Call base implementation
    BaseTabPage::setXenObject(objectType, objectRef, objectData);

    // Connect to object updates for real-time CD/DVD changes
    if (this->m_xenLib && objectType == "vm")
    {
        connect(this->m_xenLib, &XenLib::objectDataReceived, this, &VMStorageTabPage::onObjectDataReceived);
    }
}

void VMStorageTabPage::onObjectDataReceived(QString type, QString ref, QVariantMap data)
{
    // Check if this update is for our VM
    if (type == "vm" && ref == this->m_objectRef)
    {
        // Update our object data
        this->m_objectData = data;

        // Refresh CD/DVD drives if VBDs changed
        this->refreshCDDVDDrives();
    }
    // Also monitor VBD updates for the current drive
    else if (type == "vbd" && ref == this->m_currentVBDRef)
    {
        // Refresh ISO list if current VBD changed (e.g., ISO mounted/ejected)
        this->refreshISOList();
    }
}

void VMStorageTabPage::refreshContent()
{
    this->ui->storageTable->setRowCount(0);

    if (this->m_objectData.isEmpty() || this->m_objectType != "vm")
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

void VMStorageTabPage::populateVMStorage()
{
    this->ui->titleLabel->setText("Virtual Disks");

    if (!this->m_xenLib || this->m_objectRef.isEmpty())
    {
        return;
    }

    // Get VBDs for this VM from cache (already populated by XenLib)
    QVariantList vbdRefs = this->m_objectData.value("VBDs").toList();

    for (const QVariant& vbdVar : vbdRefs)
    {
        QString vbdRef = vbdVar.toString();
        QVariantMap vbdRecord = this->m_xenLib->getCache()->resolve("vbd", vbdRef);

        if (vbdRecord.isEmpty())
        {
            continue;
        }

        QString type = vbdRecord.value("type", "").toString();

        // Skip CD/DVD and Floppy drives - they're shown in the CD/DVD section
        if (type == "CD" || type == "Floppy")
        {
            continue;
        }

        // Get VDI information
        QString vdiRef = vbdRecord.value("VDI", "").toString();

        // Skip if no VDI attached (empty VBD)
        if (vdiRef.isEmpty() || vdiRef == "OpaqueRef:NULL")
        {
            continue;
        }

        QVariantMap vdiRecord = this->m_xenLib->getCache()->resolve("vdi", vdiRef);

        if (vdiRecord.isEmpty())
        {
            continue;
        }

        // Get SR information
        QString srRef = vdiRecord.value("SR", "").toString();
        QVariantMap srRecord = this->m_xenLib->getCache()->resolve("sr", srRef);

        // Skip tools ISOs
        if (!srRecord.isEmpty() && srRecord.value("content_type", "").toString() == "iso")
        {
            continue;
        }

        // Build row data matching C# VBDRow
        QString position = vbdRecord.value("userdevice", "").toString();
        QString vdiName = vdiRecord.value("name_label", "").toString();
        QString vdiDescription = vdiRecord.value("name_description", "").toString();
        QString srName = srRecord.value("name_label", "Unknown").toString();

        // Get size in human-readable format
        qint64 virtualSize = vdiRecord.value("virtual_size", 0).toLongLong();
        QString size = "N/A";
        if (virtualSize > 0)
        {
            double sizeGB = virtualSize / (1024.0 * 1024.0 * 1024.0);
            size = QString::number(sizeGB, 'f', 2) + " GB";
        }

        // Check if read-only
        QString mode = vbdRecord.value("mode", "").toString();
        QString readOnly = (mode == "RO") ? "Yes" : "No";

        // Get QoS priority (IO nice value)
        QVariantMap qosAlgorithmParams = vbdRecord.value("qos_algorithm_params").toMap();
        int ioPriority = qosAlgorithmParams.value("class", 4).toInt();
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
        bool currentlyAttached = vbdRecord.value("currently_attached", false).toBool();
        QString active = currentlyAttached ? "Yes" : "No";

        // Get device path
        QString device = vbdRecord.value("device", "").toString();
        QString devicePath = device.isEmpty() ? "Unknown" : QString("/dev/%1").arg(device);

        // Add row to table
        int row = this->ui->storageTable->rowCount();
        this->ui->storageTable->insertRow(row);

        // Store VBD and VDI refs for context menu
        QTableWidgetItem* positionItem = new QTableWidgetItem(position);
        positionItem->setData(Qt::UserRole, vbdRef);
        positionItem->setData(Qt::UserRole + 1, vdiRef);
        this->ui->storageTable->setItem(row, 0, positionItem);

        this->ui->storageTable->setItem(row, 1, new QTableWidgetItem(vdiName));
        this->ui->storageTable->setItem(row, 2, new QTableWidgetItem(vdiDescription));
        this->ui->storageTable->setItem(row, 3, new QTableWidgetItem(srName));
        this->ui->storageTable->setItem(row, 4, new QTableWidgetItem(size));
        this->ui->storageTable->setItem(row, 5, new QTableWidgetItem(readOnly));
        this->ui->storageTable->setItem(row, 6, new QTableWidgetItem(priority));
        this->ui->storageTable->setItem(row, 7, new QTableWidgetItem(active));
        this->ui->storageTable->setItem(row, 8, new QTableWidgetItem(devicePath));
    }

    // Resize columns to content
    for (int i = 0; i < this->ui->storageTable->columnCount(); ++i)
    {
        this->ui->storageTable->resizeColumnToContents(i);
    }
}

void VMStorageTabPage::refreshCDDVDDrives()
{
    // Clear previous data
    this->m_vbdRefs.clear();
    this->ui->driveComboBox->clear();

    // Only show for VMs (not control domain)
    bool isControlDomain = this->m_objectData.value("is_control_domain", false).toBool();
    if (isControlDomain)
    {
        this->ui->cdDvdGroupBox->setVisible(false);
        return;
    }

    this->ui->cdDvdGroupBox->setVisible(true);

    // Get VBDs from VM object data
    QVariantList vbdRefs = this->m_objectData.value("VBDs").toList();

    int dvdCount = 0;
    int floppyCount = 0;

    // Iterate through VBDs and find CD/DVD drives
    for (const QVariant& vbdRefVar : vbdRefs)
    {
        QString vbdRef = vbdRefVar.toString();

        // Get VBD record from cache
        QVariantMap vbdData = this->m_xenLib->getCache()->resolve("vbd", vbdRef);

        if (vbdData.isEmpty())
            continue;

        QString type = vbdData.value("type").toString();

        // Check if this is a CD or Floppy drive
        if (type == "CD")
        {
            dvdCount++;
            this->m_vbdRefs.append(vbdRef);
            this->ui->driveComboBox->addItem(QString("DVD Drive %1").arg(dvdCount), vbdRef);
        } else if (type == "Floppy")
        {
            floppyCount++;
            this->m_vbdRefs.append(vbdRef);
            this->ui->driveComboBox->addItem(QString("Floppy Drive %1").arg(floppyCount), vbdRef);
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
    this->ui->isoComboBox->clear();

    if (this->m_currentVBDRef.isEmpty() || !this->m_xenLib)
        return;

    // Add "Empty" option
    this->ui->isoComboBox->addItem("[Empty]", QString());

    // Get current VBD data to see what's mounted
    QVariantMap vbdData = this->m_xenLib->getCache()->resolve("vbd", this->m_currentVBDRef);

    QString currentVdiRef;
    bool empty = true;

    if (!vbdData.isEmpty())
    {
        currentVdiRef = vbdData.value("VDI").toString();
        empty = vbdData.value("empty").toBool();
    }

    // Get all ISOs from pool's SRs
    // 1. Get VM's connection pool
    QString poolRef;
    if (this->m_objectData.contains("resident_on"))
    {
        QString hostRef = this->m_objectData.value("resident_on").toString();
        if (!hostRef.isEmpty())
        {
            QVariantMap hostData = this->m_xenLib->getCache()->resolve("host", hostRef);
            if (!hostData.isEmpty())
            {
                poolRef = hostData.value("pool").toString();
            }
        }
    }

    // 2. Get all SRs in the pool
    if (!poolRef.isEmpty())
    {
        QVariantMap poolData = this->m_xenLib->getCache()->resolve("pool", poolRef);
        if (!poolData.isEmpty())
        {
            QVariantList srRefs = poolData.value("SRs").toList();

            // 3. For each SR, get VDIs where content_type == "iso"
            for (const QVariant& srRefVar : srRefs)
            {
                QString srRef = srRefVar.toString();
                QVariantMap srData = this->m_xenLib->getCache()->resolve("sr", srRef);

                if (srData.isEmpty())
                    continue;

                // Check if SR contains ISOs
                QString srType = srData.value("type").toString();
                QVariantList vdiRefs = srData.value("VDIs").toList();

                for (const QVariant& vdiRefVar : vdiRefs)
                {
                    QString vdiRef = vdiRefVar.toString();
                    QVariantMap vdiData = this->m_xenLib->getCache()->resolve("vdi", vdiRef);

                    if (vdiData.isEmpty())
                        continue;

                    // Check if this is an ISO
                    QString contentType = vdiData.value("type").toString();
                    if (contentType == "user")
                    {
                        // Check if it's actually an ISO by looking at SR type or VDI properties
                        QString isoName = vdiData.value("name_label").toString();

                        // Add to list if it looks like an ISO
                        if (srType == "iso" || isoName.toLower().endsWith(".iso"))
                        {
                            this->ui->isoComboBox->addItem(isoName, vdiRef);
                        }
                    }
                }
            }
        }
    }

    // Select currently mounted ISO or [Empty]
    if (!empty && !currentVdiRef.isEmpty())
    {
        // Find and select the mounted ISO
        for (int i = 1; i < this->ui->isoComboBox->count(); ++i)
        {
            if (this->ui->isoComboBox->itemData(i).toString() == currentVdiRef)
            {
                this->ui->isoComboBox->setCurrentIndex(i);
                return;
            }
        }

        // If mounted ISO not found in list, add it manually
        QVariantMap vdiData = this->m_xenLib->getCache()->resolve("vdi", currentVdiRef);
        if (!vdiData.isEmpty())
        {
            QString isoName = vdiData.value("name_label").toString();
            this->ui->isoComboBox->addItem(isoName + " (mounted)", currentVdiRef);
            this->ui->isoComboBox->setCurrentIndex(this->ui->isoComboBox->count() - 1);
        }
    } else
    {
        this->ui->isoComboBox->setCurrentIndex(0); // Select [Empty]
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
    return;
    if (index < 0 || !this->m_xenLib)
        return;

    QString vdiRef = this->ui->isoComboBox->itemData(index).toString();

    if (this->m_currentVBDRef.isEmpty())
        return;

    // Get connection
    XenConnection* conn = this->m_xenLib->getConnection();

    if (!conn)
        return;

    // Disable UI during operation
    this->ui->isoComboBox->setEnabled(false);
    this->ui->ejectButton->setEnabled(false);

    // Create and run the AsyncOperation
    ChangeVMISOAction* action = new ChangeVMISOAction(
        conn,
        this->m_objectRef,     // VM ref
        vdiRef,                // VDI ref (empty for eject)
        this->m_currentVBDRef, // VBD ref
        this);

    OperationManager::instance()->registerOperation(action);

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

    connect(action, &AsyncOperation::failed, this, [this, action](QString error) {
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
    action->runAsync();
}

void VMStorageTabPage::onEjectButtonClicked()
{
    // Set to [Empty]
    this->ui->isoComboBox->setCurrentIndex(0);
}

void VMStorageTabPage::onNewCDDriveLinkClicked(const QString& link)
{
    Q_UNUSED(link);

    if (!this->m_xenLib || this->m_objectRef.isEmpty())
        return;

    XenRpcAPI* api = this->m_xenLib->getAPI();
    if (!api)
        return;

    qDebug() << "Creating new CD/DVD drive for VM:" << this->m_objectRef;

    // Disable the link during operation
    this->ui->noDrivesLabel->setEnabled(false);

    // Find next available device position
    QVariantList vbdRefs = this->m_objectData.value("VBDs").toList();
    int maxDeviceNum = -1;

    for (const QVariant& vbdRefVar : vbdRefs)
    {
        QString vbdRef = vbdRefVar.toString();
        QVariantMap vbdData = this->m_xenLib->getCache()->resolve("vbd", vbdRef);

        if (!vbdData.isEmpty())
        {
            QString userdevice = vbdData.value("userdevice").toString();
            bool ok;
            int deviceNum = userdevice.toInt(&ok);
            if (ok && deviceNum > maxDeviceNum)
            {
                maxDeviceNum = deviceNum;
            }
        }
    }

    QString nextDevice = QString::number(maxDeviceNum + 1);

    // Create the new CD/DVD drive using VbdCreateAndPlugAction
    XenConnection* connection = m_xenLib->getConnection();
    if (!connection)
    {
        QMessageBox::warning(this, tr("Error"), tr("No active connection."));
        return;
    }

    VM* vm = new VM(connection, m_objectRef, this);

    // Build VBD record for CD/DVD drive (empty VDI)
    QVariantMap vbdRecord;
    vbdRecord["VM"] = m_objectRef;
    vbdRecord["VDI"] = "OpaqueRef:NULL"; // Empty for CD/DVD
    vbdRecord["userdevice"] = nextDevice;
    vbdRecord["bootable"] = false;
    vbdRecord["mode"] = "RO";
    vbdRecord["type"] = "CD";
    vbdRecord["unpluggable"] = true;
    vbdRecord["empty"] = true;
    vbdRecord["other_config"] = QVariantMap();
    vbdRecord["qos_algorithm_type"] = "";
    vbdRecord["qos_algorithm_params"] = QVariantMap();

    VbdCreateAndPlugAction* createAction = new VbdCreateAndPlugAction(
        vm, vbdRecord, tr("CD/DVD Drive"), false, this);

    OperationProgressDialog* dialog = new OperationProgressDialog(createAction, this);
    if (dialog->exec() != QDialog::Accepted)
    {
        QMessageBox::warning(this, tr("Failed"), tr("Failed to create CD/DVD drive."));
        delete dialog;
        return;
    }

    QString newVbdRef = createAction->result();
    delete dialog;

    this->ui->noDrivesLabel->setEnabled(true);

    if (!newVbdRef.isEmpty())
    {
        qDebug() << "CD/DVD drive created successfully:" << newVbdRef;

        // Refresh the drives list to show new drive
        // We need to update the VM's VBD list first
        if (this->m_xenLib)
        {
            this->m_xenLib->requestObjectData("vm", this->m_objectRef);
        }
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

    if (this->m_objectType == "vm")
    {
        // VM View: Show VM-specific buttons
        // Hide SR-specific buttons: Rescan, Move
        this->ui->rescanButton->setVisible(false);
        this->ui->moveButton->setVisible(false);

        this->ui->attachButton->setVisible(true);
        this->ui->activateButton->setVisible(true);
        this->ui->deactivateButton->setVisible(true);
        this->ui->detachButton->setVisible(true);
        this->ui->addButton->setVisible(true);
        this->ui->editButton->setVisible(true);
        this->ui->deleteButton->setVisible(true);

        // Original VM button logic
        QString vbdRef = getSelectedVBDRef();
        QString vdiRef = getSelectedVDIRef();

        bool hasSelection = !vbdRef.isEmpty();
        bool hasVDI = !vdiRef.isEmpty();

        // Check VM power state
        bool vmRunning = false;
        if (!this->m_objectData.isEmpty())
        {
            QString powerState = this->m_objectData.value("power_state", "").toString();
            vmRunning = (powerState == "Running");
        }

        // Get VBD data if selected
        bool currentlyAttached = false;
        bool isLocked = false;
        if (hasSelection && this->m_xenLib)
        {
            QVariantMap vbdData = this->m_xenLib->getCache()->resolve("vbd", vbdRef);
            currentlyAttached = vbdData.value("currently_attached", false).toBool();

            // Check if VBD or VDI is locked
            QVariantList allowedOps = vbdData.value("allowed_operations", QVariantList()).toList();
            isLocked = allowedOps.isEmpty();

            if (hasVDI)
            {
                QVariantMap vdiData = this->m_xenLib->getCache()->resolve("vdi", vdiRef);
                QVariantList vdiAllowedOps = vdiData.value("allowed_operations", QVariantList()).toList();
                isLocked = isLocked || vdiAllowedOps.isEmpty();
            }
        }

        // Add: Always enabled for VMs
        this->ui->addButton->setEnabled(true);

        // Attach: Always enabled for VMs
        this->ui->attachButton->setEnabled(true);

        // Activate: Only for detached VBDs
        this->ui->activateButton->setEnabled(hasSelection && !currentlyAttached && !isLocked);
        this->ui->activateButton->setVisible(!currentlyAttached || !hasSelection);

        // Deactivate: Only for attached VBDs
        this->ui->deactivateButton->setEnabled(hasSelection && currentlyAttached && !isLocked && vmRunning);
        this->ui->deactivateButton->setVisible(currentlyAttached || !hasSelection);

        // Detach: Enabled for selected, detached VBDs
        this->ui->detachButton->setEnabled(hasSelection && hasVDI && !currentlyAttached && !isLocked);

        // Delete: Enabled for selected VDIs
        this->ui->deleteButton->setEnabled(hasSelection && hasVDI && !isLocked);

        // Properties/Edit: Enabled for single selection
        this->ui->editButton->setEnabled(hasSelection && hasVDI && !isLocked);
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
    QList<QTableWidgetItem*> selectedItems = this->ui->storageTable->selectedItems();
    if (selectedItems.isEmpty())
        return QString();

    int row = selectedItems.first()->row();
    QTableWidgetItem* item = this->ui->storageTable->item(row, 0);
    if (item)
        return item->data(Qt::UserRole).toString();

    return QString();
}

QString VMStorageTabPage::getSelectedVDIRef() const
{
    QList<QTableWidgetItem*> selectedItems = this->ui->storageTable->selectedItems();
    if (selectedItems.isEmpty())
        return QString();

    int row = selectedItems.first()->row();
    QTableWidgetItem* item = this->ui->storageTable->item(row, 0);
    if (!item)
        return QString();

    // VM view stores the VDI ref in Qt::UserRole + 1 (Qt::UserRole stores VBD ref)
    return item->data(Qt::UserRole + 1).toString();
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
    QMenu contextMenu(this);

    // Build context menu based on object type and button visibility
    // C# Reference: SrStoragePage.cs contextMenuStrip_Opening() lines 379-399

    if (this->m_objectType == "vm")
    {
        // VM View: Add, Attach, Activate/Deactivate, Detach, Delete, Properties
        if (this->ui->addButton->isVisible())
        {
            QAction* addAction = contextMenu.addAction("Add Virtual Disk...");
            addAction->setEnabled(this->ui->addButton->isEnabled());
            connect(addAction, &QAction::triggered, this, &VMStorageTabPage::onAddButtonClicked);
        }

        if (this->ui->attachButton->isVisible())
        {
            QAction* attachAction = contextMenu.addAction("Attach Virtual Disk...");
            attachAction->setEnabled(this->ui->attachButton->isEnabled());
            connect(attachAction, &QAction::triggered, this, &VMStorageTabPage::onAttachButtonClicked);
        }

        if (this->ui->activateButton->isVisible())
        {
            QAction* activateAction = contextMenu.addAction("Activate");
            activateAction->setEnabled(this->ui->activateButton->isEnabled());
            connect(activateAction, &QAction::triggered, this, &VMStorageTabPage::onActivateButtonClicked);
        }

        if (this->ui->deactivateButton->isVisible())
        {
            QAction* deactivateAction = contextMenu.addAction("Deactivate");
            deactivateAction->setEnabled(this->ui->deactivateButton->isEnabled());
            connect(deactivateAction, &QAction::triggered, this, &VMStorageTabPage::onDeactivateButtonClicked);
        }

        if (this->ui->detachButton->isVisible())
        {
            QAction* detachAction = contextMenu.addAction("Detach Virtual Disk");
            detachAction->setEnabled(this->ui->detachButton->isEnabled());
            connect(detachAction, &QAction::triggered, this, &VMStorageTabPage::onDetachButtonClicked);
        }

        if (this->ui->deleteButton->isVisible())
        {
            QAction* deleteAction = contextMenu.addAction("Delete Virtual Disk...");
            deleteAction->setEnabled(this->ui->deleteButton->isEnabled());
            connect(deleteAction, &QAction::triggered, this, &VMStorageTabPage::onDeleteButtonClicked);
        }

        contextMenu.addSeparator();

        if (this->ui->editButton->isVisible())
        {
            QAction* editAction = contextMenu.addAction("Properties...");
            editAction->setEnabled(this->ui->editButton->isEnabled());
            connect(editAction, &QAction::triggered, this, &VMStorageTabPage::onEditButtonClicked);
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
    if (!this->m_xenLib || this->m_objectRef.isEmpty())
        return;

    XenConnection* connection = this->m_xenLib->getConnection();
    if (!connection)
    {
        QMessageBox::warning(this, "Error", "No connection available.");
        return;
    }

    // Open New Virtual Disk Dialog
    NewVirtualDiskDialog dialog(this->m_xenLib, this->m_objectRef, this);
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

    CreateDiskAction* createAction = new CreateDiskAction(vdiRecord, connection, this);

    OperationProgressDialog* createDialog = new OperationProgressDialog(createAction, this);
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
    QString vdiRef = createAction->result();
    delete createDialog;

    if (vdiRef.isEmpty())
    {
        qWarning() << "[StorageTabPage] VDI ref is empty!";
        QMessageBox::warning(this, "Failed", "Failed to create virtual disk.");
        return;
    }

    qDebug() << "VDI created:" << vdiRef << "Now attaching to VM...";

    // Create VBD and attach to VM using VbdCreateAndPlugAction
    VM* vm = new VM(connection, this->m_objectRef, this);

    QVariantMap vbdRecord;
    vbdRecord["VM"] = this->m_objectRef;
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

    VbdCreateAndPlugAction* attachAction = new VbdCreateAndPlugAction(
        vm, vbdRecord, name, false, this);

    OperationProgressDialog* attachDialog = new OperationProgressDialog(attachAction, this);
    if (attachDialog->exec() != QDialog::Accepted)
    {
        QMessageBox::warning(this, "Warning",
                             "Virtual disk created but failed to attach to VM.\n"
                             "You can attach it manually from the Attach menu.");
        delete attachDialog;
        // VDI was created, so refresh anyway
        if (this->m_xenLib)
            this->m_xenLib->requestObjectData("vm", this->m_objectRef);
        return;
    }

    delete attachDialog;

    qDebug() << "VBD created and attached successfully";
    QMessageBox::information(this, "Success", "Virtual disk created and attached successfully.");

    // Refresh to show new disk
    if (this->m_xenLib)
        this->m_xenLib->requestObjectData("vm", this->m_objectRef);
}

void VMStorageTabPage::onAttachButtonClicked()
{
    if (!this->m_xenLib || this->m_objectRef.isEmpty())
        return;

    XenRpcAPI* api = this->m_xenLib->getAPI();
    if (!api)
        return;

    // Open Attach Virtual Disk Dialog
    AttachVirtualDiskDialog dialog(this->m_xenLib, this->m_objectRef, this);
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

    XenConnection* connection = m_xenLib->getConnection();
    if (!connection)
    {
        QMessageBox::warning(this, tr("Error"), tr("No active connection."));
        return;
    }

    VM* vm = new VM(connection, m_objectRef, this);

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
    QVariantMap vdiData = m_xenLib->getCache()->resolve("vdi", vdiRef);
    if (vdiData.contains("name_label"))
    {
        vdiName = vdiData["name_label"].toString();
    }

    VbdCreateAndPlugAction* attachAction = new VbdCreateAndPlugAction(vm, vbdRecord, vdiName, false, this);
    OperationProgressDialog* attachDialog = new OperationProgressDialog(attachAction, this);

    if (attachDialog->exec() != QDialog::Accepted)
    {
        QMessageBox::warning(this, tr("Failed"), tr("Failed to attach virtual disk."));
        delete attachDialog;
        return;
    }

    QString vbdRef = attachAction->result();
    delete attachDialog;

    if (vbdRef.isEmpty())
    {
        QMessageBox::warning(this, tr("Failed"), tr("Failed to attach virtual disk."));
        return;
    }

    qDebug() << "VBD created:" << vbdRef;
    QMessageBox::information(this, tr("Success"), tr("Virtual disk attached successfully."));

    // Refresh to show attached disk
    if (this->m_xenLib)
        this->m_xenLib->requestObjectData("vm", this->m_objectRef);
}

void VMStorageTabPage::onActivateButtonClicked()
{
    QString vbdRef = getSelectedVBDRef();
    if (vbdRef.isEmpty() || !this->m_xenLib)
        return;

    XenConnection* connection = m_xenLib->getConnection();
    if (!connection || !connection->getSession())
    {
        QMessageBox::warning(this, tr("Error"), tr("No active connection."));
        return;
    }

    // Get VBD and VDI names for display
    QVariantMap vbdData = m_xenLib->getCache()->resolve("vbd", vbdRef);
    QString vdiRef = vbdData.value("VDI").toString();
    QString vdiName = tr("Virtual Disk");
    if (!vdiRef.isEmpty() && vdiRef != "OpaqueRef:NULL")
    {
        QVariantMap vdiData = m_xenLib->getCache()->resolve("vdi", vdiRef);
        vdiName = vdiData.value("name_label", tr("Virtual Disk")).toString();
    }

    QString vmName = m_xenLib->getCache()->resolve("vm", m_objectRef).value("name_label", tr("VM")).toString();

    // Create DelegatedAsyncOperation to plug the VBD (mimics C# DelegatedAsyncAction)
    auto* activateAction = new DelegatedAsyncOperation(
        connection,
        tr("Activating disk '%1' on '%2'").arg(vdiName, vmName),
        tr("Activating disk..."),
        [vbdRef](DelegatedAsyncOperation* op) {
            // Exceptions are caught by AsyncOperation and converted to errors automatically
            XenAPI::VBD::plug(op->session(), vbdRef);
            op->setPercentComplete(100);
        },
        this);

    // Show progress dialog (mimics C# .RunAsync())
    OperationProgressDialog* dialog = new OperationProgressDialog(activateAction, this);
    if (dialog->exec() == QDialog::Accepted)
    {
        QMessageBox::information(this, tr("Success"), tr("Virtual disk activated successfully."));

        // Refresh to show updated state
        if (this->m_xenLib)
            this->m_xenLib->requestObjectData("vm", this->m_objectRef);
    } else
    {
        QMessageBox::warning(this, tr("Failed"), tr("Failed to activate virtual disk."));
    }

    delete dialog;
}

void VMStorageTabPage::onDeactivateButtonClicked()
{
    QString vbdRef = getSelectedVBDRef();
    if (vbdRef.isEmpty() || !this->m_xenLib)
        return;

    XenConnection* connection = m_xenLib->getConnection();
    if (!connection || !connection->getSession())
    {
        QMessageBox::warning(this, tr("Error"), tr("No active connection."));
        return;
    }

    // Get VBD and VDI names for display
    QVariantMap vbdData = m_xenLib->getCache()->resolve("vbd", vbdRef);
    QString vdiRef = vbdData.value("VDI").toString();
    QString vdiName = tr("Virtual Disk");
    if (!vdiRef.isEmpty() && vdiRef != "OpaqueRef:NULL")
    {
        QVariantMap vdiData = m_xenLib->getCache()->resolve("vdi", vdiRef);
        vdiName = vdiData.value("name_label", tr("Virtual Disk")).toString();
    }

    QString vmName = m_xenLib->getCache()->resolve("vm", m_objectRef).value("name_label", tr("VM")).toString();

    // Create DelegatedAsyncOperation to unplug the VBD (mimics C# DelegatedAsyncAction)
    auto* deactivateAction = new DelegatedAsyncOperation(
        connection,
        tr("Deactivating disk '%1' on '%2'").arg(vdiName, vmName),
        tr("Deactivating disk..."),
        [vbdRef](DelegatedAsyncOperation* op) {
            // Exceptions are caught by AsyncOperation and converted to errors automatically
            XenAPI::VBD::unplug(op->session(), vbdRef);
            op->setPercentComplete(100);
        },
        this);

    // Show progress dialog (mimics C# .RunAsync())
    OperationProgressDialog* dialog = new OperationProgressDialog(deactivateAction, this);
    if (dialog->exec() == QDialog::Accepted)
    {
        QMessageBox::information(this, tr("Success"), tr("Virtual disk deactivated successfully."));

        // Refresh to show updated state
        if (this->m_xenLib)
            this->m_xenLib->requestObjectData("vm", this->m_objectRef);
    } else
    {
        QMessageBox::warning(this, tr("Failed"), tr("Failed to deactivate virtual disk."));
    }

    delete dialog;
}

void VMStorageTabPage::onDetachButtonClicked()
{
    QString vbdRef = getSelectedVBDRef();
    QString vdiRef = getSelectedVDIRef();

    if (vbdRef.isEmpty() || vdiRef.isEmpty() || !this->m_xenLib)
        return;

    // Get VDI name for confirmation
    QString vdiName = "this virtual disk";
    QVariantMap vdiData = this->m_xenLib->getCache()->resolve("vdi", vdiRef);
    if (!vdiData.isEmpty())
    {
        vdiName = vdiData.value("name_label", "Unknown").toString();
    }

    // Confirm detachment
    QMessageBox::StandardButton reply = QMessageBox::question(this,
                                                              "Detach Virtual Disk",
                                                              QString("Are you sure you want to detach '%1' from this VM?\n\n"
                                                                      "The disk will not be deleted and can be attached again later.")
                                                                  .arg(vdiName),
                                                              QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    // Get VM object
    VM* vm = new VM(this->m_xenLib->getConnection(), this->m_objectRef);

    // Create and run detach action
    DetachVirtualDiskAction* action = new DetachVirtualDiskAction(vdiRef, vm, this);

    OperationProgressDialog* dialog = new OperationProgressDialog(action, this);
    dialog->exec();

    // Refresh to show updated state
    if (this->m_xenLib)
        this->m_xenLib->requestObjectData("vm", this->m_objectRef);

    delete dialog;
}

void VMStorageTabPage::onDeleteButtonClicked()
{
    QString vbdRef = getSelectedVBDRef();
    QString vdiRef = getSelectedVDIRef();

    if (vdiRef.isEmpty() || !this->m_xenLib)
        return;

    // Get VDI name for confirmation
    QString vdiName = "this virtual disk";
    QVariantMap vdiData = this->m_xenLib->getCache()->resolve("vdi", vdiRef);
    if (!vdiData.isEmpty())
    {
        vdiName = vdiData.value("name_label", "Unknown").toString();
    }

    // Confirm deletion
    QMessageBox::StandardButton reply = QMessageBox::question(this,
                                                              "Delete Virtual Disk",
                                                              QString("Are you sure you want to permanently delete '%1'?\n\n"
                                                                      "This operation cannot be undone.")
                                                                  .arg(vdiName),
                                                              QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    // Check if VDI is currently attached - if so, warn user
    bool allowRunningVMDelete = false;
    if (!vbdRef.isEmpty())
    {
        QVariantMap vbdData = this->m_xenLib->getCache()->resolve("vbd", vbdRef);
        bool currentlyAttached = vbdData.value("currently_attached", false).toBool();

        if (currentlyAttached)
        {
            QMessageBox::StandardButton reply2 = QMessageBox::question(this,
                                                                       "Disk Currently Attached",
                                                                       QString("'%1' is currently attached to this running VM.\n\n"
                                                                               "Do you want to detach it and delete it anyway?")
                                                                           .arg(vdiName),
                                                                       QMessageBox::Yes | QMessageBox::No);

            if (reply2 != QMessageBox::Yes)
                return;

            allowRunningVMDelete = true;
        }
    }

    // Create and run destroy action
    DestroyDiskAction* action = new DestroyDiskAction(vdiRef,
                                                      this->m_xenLib->getConnection(),
                                                      allowRunningVMDelete,
                                                      this);

    OperationProgressDialog* dialog = new OperationProgressDialog(action, this);
    dialog->exec();

    // Refresh to show updated state
    if (this->m_xenLib)
        this->m_xenLib->requestObjectData("vm", this->m_objectRef);

    delete dialog;
}

void VMStorageTabPage::onEditButtonClicked()
{
    QString vdiRef = getSelectedVDIRef();
    if (vdiRef.isEmpty() || !this->m_xenLib)
        return;

    XenRpcAPI* api = this->m_xenLib->getAPI();
    if (!api)
        return;

    // Open VDI Properties Dialog
    VdiPropertiesDialog dialog(this->m_xenLib, vdiRef, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    // Check if there are any changes
    if (!dialog.hasChanges())
    {
        qDebug() << "No changes to VDI properties";
        return;
    }

    // Apply changes
    bool hasErrors = false;

    // Update name if changed
    QString newName = dialog.getVdiName();
    QVariantMap vdiData = this->m_xenLib->getCache()->resolve("vdi", vdiRef);
    QString oldName = vdiData.value("name_label", "").toString();

    if (newName != oldName)
    {
        qDebug() << "Updating VDI name:" << oldName << "->" << newName;
        if (!api->setVDINameLabel(vdiRef, newName))
        {
            QMessageBox::warning(this, "Warning", "Failed to update virtual disk name.");
            hasErrors = true;
        }
    }

    // Update description if changed
    QString newDescription = dialog.getVdiDescription();
    QString oldDescription = vdiData.value("name_description", "").toString();

    if (newDescription != oldDescription)
    {
        qDebug() << "Updating VDI description";
        if (!api->setVDINameDescription(vdiRef, newDescription))
        {
            QMessageBox::warning(this, "Warning", "Failed to update virtual disk description.");
            hasErrors = true;
        }
    }

    // Update size if changed (and increase only)
    qint64 newSize = dialog.getNewSize();
    qint64 oldSize = vdiData.value("virtual_size", 0).toLongLong();
    qint64 sizeDiff = newSize - oldSize;

    if (sizeDiff > (10 * 1024 * 1024)) // More than 10 MB increase
    {
        qDebug() << "Resizing VDI from" << oldSize << "to" << newSize << "bytes";
        if (!api->resizeVDI(vdiRef, newSize))
        {
            QMessageBox::warning(this, "Warning", "Failed to resize virtual disk.");
            hasErrors = true;
        }
    }

    if (!hasErrors)
    {
        QMessageBox::information(this, "Success", "Virtual disk properties updated successfully.");
    }

    // Refresh to show updated properties
    if (this->m_xenLib)
        this->m_xenLib->requestObjectData("vm", this->m_objectRef);
}
