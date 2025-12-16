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

#include "movevirtualdiskdialog.h"
#include "ui_movevirtualdiskdialog.h"
#include "xenlib.h"
#include "xencache.h"
#include "xen/connection.h"
#include "xen/api.h"
#include "xen/actions/vdi/movevirtualdiskaction.h"
#include "../operations/operationmanager.h"
#include <QTableWidgetItem>
#include <QMessageBox>
#include <QHeaderView>
#include <QTimer>

MoveVirtualDiskDialog::MoveVirtualDiskDialog(XenLib* xenLib, const QString& vdiRef,
                                             QWidget* parent)
    : QDialog(parent), ui(new Ui::MoveVirtualDiskDialog), m_xenLib(xenLib)
{
    m_vdiRefs << vdiRef;
    setupUI();
}

MoveVirtualDiskDialog::MoveVirtualDiskDialog(XenLib* xenLib, const QStringList& vdiRefs,
                                             QWidget* parent)
    : QDialog(parent), ui(new Ui::MoveVirtualDiskDialog), m_xenLib(xenLib), m_vdiRefs(vdiRefs)
{
    setupUI();
}

MoveVirtualDiskDialog::~MoveVirtualDiskDialog()
{
    delete ui;
}

void MoveVirtualDiskDialog::setupUI()
{
    ui->setupUi(this);

    // Connect signals
    connect(ui->srTable, &QTableWidget::itemSelectionChanged,
            this, &MoveVirtualDiskDialog::onSRSelectionChanged);
    connect(ui->srTable, &QTableWidget::cellDoubleClicked,
            this, &MoveVirtualDiskDialog::onSRDoubleClicked);
    connect(ui->rescanButton, &QPushButton::clicked,
            this, &MoveVirtualDiskDialog::onRescanButtonClicked);
    connect(ui->moveButton, &QPushButton::clicked,
            this, &MoveVirtualDiskDialog::onMoveButtonClicked);

    // Configure table
    ui->srTable->horizontalHeader()->setStretchLastSection(true);
    ui->srTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->srTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    // Update window title for multiple VDIs
    if (m_vdiRefs.count() > 1)
    {
        setWindowTitle(QString("Move %1 Virtual Disks").arg(m_vdiRefs.count()));
    }

    // Get source SR(s) for filtering
    for (const QString& vdiRef : m_vdiRefs)
    {
        QVariantMap vdiData = m_xenLib->getCache()->resolve("vdi", vdiRef);
        QString srRef = vdiData.value("SR").toString();
        if (!srRef.isEmpty() && !m_sourceSRs.contains(srRef))
        {
            m_sourceSRs << srRef;
        }
    }

    // Populate SR list
    populateSRTable();

    // Update button state
    updateMoveButton();
}

void MoveVirtualDiskDialog::populateSRTable()
{
    ui->srTable->setRowCount(0);

    if (!m_xenLib)
        return;

    // Get default SR from pool (for auto-selection)
    QString defaultSRRef;
    XenConnection* connection = m_xenLib->getConnection();
    if (connection)
    {
        // Get pool data
        QList<QVariantMap> pools = m_xenLib->getCache()->getAll("pool");
        if (!pools.isEmpty())
        {
            defaultSRRef = pools.first().value("default_SR").toString();
        }
    }

    // Get all SRs
    QList<QVariantMap> allSRs = m_xenLib->getCache()->getAll("sr");

    int row = 0;
    int rowToSelect = -1; // Track which row to auto-select
    for (const QVariantMap& srData : allSRs)
    {
        QString srRef = srData.value("ref").toString();

        // Filter out invalid destinations
        if (!isValidDestination(srRef, srData))
            continue;

        // Add SR to table
        ui->srTable->insertRow(row);

        // Name
        QString name = srData.value("name_label", "").toString();
        QTableWidgetItem* nameItem = new QTableWidgetItem(name);
        nameItem->setData(Qt::UserRole, srRef);
        ui->srTable->setItem(row, 0, nameItem);

        // Description
        QString description = srData.value("name_description", "").toString();
        ui->srTable->setItem(row, 1, new QTableWidgetItem(description));

        // Type
        QString type = srData.value("type", "").toString();
        ui->srTable->setItem(row, 2, new QTableWidgetItem(type));

        // Shared
        bool shared = srData.value("shared", false).toBool();
        ui->srTable->setItem(row, 3, new QTableWidgetItem(shared ? "Yes" : "No"));

        // Size
        qint64 size = srData.value("physical_size", 0).toLongLong();
        ui->srTable->setItem(row, 4, new QTableWidgetItem(formatSize(size)));

        // Free space
        qint64 utilisation = srData.value("physical_utilisation", 0).toLongLong();
        qint64 freeSpace = size - utilisation;
        QTableWidgetItem* freeItem = new QTableWidgetItem(formatSize(freeSpace));

        // Highlight if insufficient space
        qint64 requiredSpace = getTotalVDISize();
        if (requiredSpace > 0 && freeSpace < requiredSpace)
        {
            freeItem->setForeground(Qt::red);
        }

        ui->srTable->setItem(row, 5, freeItem);

        // Check if this SR should be auto-selected (matches C# SrPicker logic)
        if (rowToSelect == -1) // If nothing selected yet
        {
            // Priority 1: Default SR from pool
            if (!defaultSRRef.isEmpty() && srRef == defaultSRRef)
            {
                rowToSelect = row;
            }
            // Priority 2: First valid SR (fallback)
            else if (rowToSelect == -1)
            {
                rowToSelect = row;
            }
        }

        row++;
    }

    // Resize columns to content
    ui->srTable->resizeColumnsToContents();

    // Auto-select first valid SR (or default SR) - matches C# SrPicker behavior
    if (rowToSelect >= 0 && rowToSelect < ui->srTable->rowCount())
    {
        ui->srTable->selectRow(rowToSelect);
    }

    // Update info label
    if (ui->srTable->rowCount() == 0)
    {
        ui->labelInfo->setText("<b>No compatible storage repositories found.</b>");
    } else
    {
        ui->labelInfo->setText("");
    }

    // Update button states after populating and selecting
    updateMoveButton();
}

bool MoveVirtualDiskDialog::isValidDestination(const QString& srRef, const QVariantMap& srData)
{
    // Exclude source SRs
    if (m_sourceSRs.contains(srRef))
        return false;

    // Exclude ISO SRs
    QString contentType = srData.value("content_type", "").toString();
    if (contentType == "iso")
        return false;

    // Exclude detached/broken SRs
    QVariantList pbdRefs = srData.value("PBDs").toList();
    if (pbdRefs.isEmpty())
        return false;

    // Check if at least one PBD is attached
    bool hasAttachedPBD = false;
    for (const QVariant& pbdRefVar : pbdRefs)
    {
        QString pbdRef = pbdRefVar.toString();
        QVariantMap pbdData = m_xenLib->getCache()->resolve("pbd", pbdRef);
        if (pbdData.value("currently_attached", false).toBool())
        {
            hasAttachedPBD = true;
            break;
        }
    }

    if (!hasAttachedPBD)
        return false;

    // Exclude read-only SRs (check for allowed_operations containing "vdi_create")
    QVariantList allowedOps = srData.value("allowed_operations").toList();
    bool canCreateVDI = false;
    for (const QVariant& opVar : allowedOps)
    {
        if (opVar.toString() == "vdi_create")
        {
            canCreateVDI = true;
            break;
        }
    }

    if (!canCreateVDI)
        return false;

    return true;
}

qint64 MoveVirtualDiskDialog::getTotalVDISize() const
{
    qint64 totalSize = 0;

    for (const QString& vdiRef : m_vdiRefs)
    {
        QVariantMap vdiData = m_xenLib->getCache()->resolve("vdi", vdiRef);
        totalSize += vdiData.value("virtual_size", 0).toLongLong();
    }

    return totalSize;
}

QString MoveVirtualDiskDialog::formatSize(qint64 bytes) const
{
    if (bytes < 0)
        return "Unknown";

    const qint64 KB = 1024;
    const qint64 MB = 1024 * KB;
    const qint64 GB = 1024 * MB;
    const qint64 TB = 1024 * GB;

    if (bytes >= TB)
        return QString("%1 TB").arg(bytes / (double) TB, 0, 'f', 2);
    else if (bytes >= GB)
        return QString("%1 GB").arg(bytes / (double) GB, 0, 'f', 2);
    else if (bytes >= MB)
        return QString("%1 MB").arg(bytes / (double) MB, 0, 'f', 2);
    else if (bytes >= KB)
        return QString("%1 KB").arg(bytes / (double) KB, 0, 'f', 2);
    else
        return QString("%1 bytes").arg(bytes);
}

void MoveVirtualDiskDialog::onSRSelectionChanged()
{
    updateMoveButton();
}

void MoveVirtualDiskDialog::onSRDoubleClicked(int row, int column)
{
    Q_UNUSED(row);
    Q_UNUSED(column);

    if (ui->moveButton->isEnabled())
    {
        onMoveButtonClicked();
    }
}

void MoveVirtualDiskDialog::onRescanButtonClicked()
{
    // Rescan all SRs
    if (!m_xenLib)
        return;

    XenConnection* connection = m_xenLib->getConnection();
    if (!connection || !connection->isConnected())
        return;

    // Get all SRs and scan them
    QList<QVariantMap> allSRs = m_xenLib->getCache()->getAll("sr");
    for (const QVariantMap& srData : allSRs)
    {
        QString srRef = srData.value("ref").toString();

        // Build JSON-RPC request for SR.scan
        QVariantList params;
        params << connection->getSessionId() << srRef;

        QString jsonRequest = m_xenLib->getAPI()->buildJsonRpcCall("SR.scan", params);
        QByteArray requestData = jsonRequest.toUtf8();
        connection->sendRequestAsync(requestData);
    }

    ui->labelInfo->setText("Scanning storage repositories...");

    // Refresh SR list after a delay
    QTimer::singleShot(3000, this, [this]() {
        populateSRTable();
        updateMoveButton();
    });
}

void MoveVirtualDiskDialog::updateMoveButton()
{
    // Enable Move button only if an SR is selected
    bool hasSelection = !ui->srTable->selectedItems().isEmpty();
    ui->moveButton->setEnabled(hasSelection);
}

void MoveVirtualDiskDialog::onMoveButtonClicked()
{
    // Get selected SR
    QList<QTableWidgetItem*> selectedItems = ui->srTable->selectedItems();
    if (selectedItems.isEmpty())
        return;

    QTableWidgetItem* firstItem = ui->srTable->item(selectedItems.first()->row(), 0);
    QString targetSRRef = firstItem->data(Qt::UserRole).toString();

    if (targetSRRef.isEmpty())
        return;

    // Get target SR name
    QVariantMap targetSRData = m_xenLib->getCache()->resolve("sr", targetSRRef);
    QString targetSRName = targetSRData.value("name_label", "").toString();

    // Close dialog
    accept();

    // Create actions (virtual method - can be overridden)
    createAndRunActions(targetSRRef, targetSRName);
}

void MoveVirtualDiskDialog::createAndRunActions(const QString& targetSRRef, const QString& targetSRName)
{
    // Create move operations
    OperationManager* opManager = OperationManager::instance();

    if (m_vdiRefs.count() == 1)
    {
        // Single VDI move
        QVariantMap vdiData = m_xenLib->getCache()->resolve("vdi", m_vdiRefs.first());
        QString vdiName = vdiData.value("name_label", "Virtual Disk").toString();

        MoveVirtualDiskAction* action = new MoveVirtualDiskAction(
            m_xenLib->getConnection(),
            m_vdiRefs.first(),
            targetSRRef);

        action->setTitle(QString("Moving virtual disk '%1' to '%2'")
                             .arg(vdiName)
                             .arg(targetSRName));
        action->setDescription(QString("Moving '%1'...").arg(vdiName));

        opManager->registerOperation(action);
        action->runAsync();
    } else
    {
        // Multiple VDI move - batch in parallel (max 3 at a time)
        // C# uses ParallelAction with BATCH_SIZE=3
        for (const QString& vdiRef : m_vdiRefs)
        {
            QVariantMap vdiData = m_xenLib->getCache()->resolve("vdi", vdiRef);
            QString vdiName = vdiData.value("name_label", "Virtual Disk").toString();

            MoveVirtualDiskAction* action = new MoveVirtualDiskAction(
                m_xenLib->getConnection(),
                vdiRef,
                targetSRRef);

            action->setTitle(QString("Moving virtual disk '%1' to '%2'")
                                 .arg(vdiName)
                                 .arg(targetSRName));
            action->setDescription(QString("Moving '%1'...").arg(vdiName));

            opManager->registerOperation(action);
            action->runAsync();
        }
    }
}
