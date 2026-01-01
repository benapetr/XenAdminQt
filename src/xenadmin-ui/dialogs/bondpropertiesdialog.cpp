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

#include "bondpropertiesdialog.h"
#include "ui_bondpropertiesdialog.h"
#include "xen/network/connection.h"
#include "xencache.h"
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QPushButton>

BondPropertiesDialog::BondPropertiesDialog(XenConnection* connection, const QString& hostRef, const QString& networkRef, QWidget* parent)
    : QDialog(parent), ui(new Ui::BondPropertiesDialog), m_connection(connection), m_hostRef(hostRef), m_networkRef(networkRef)
{
    ui->setupUi(this);

    // Set up table
    ui->tableWidgetNICs->setSelectionMode(QAbstractItemView::MultiSelection);
    ui->tableWidgetNICs->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidgetNICs->horizontalHeader()->setStretchLastSection(true);

    // Set default bond mode to Active-Backup (safest option)
    ui->comboBoxBondMode->setCurrentIndex(0);

    // Connect signals
    connect(ui->tableWidgetNICs, &QTableWidget::itemSelectionChanged,
            this, &BondPropertiesDialog::onSelectionChanged);

    // Load available PIFs
    loadAvailablePIFs();

    // Update OK button state
    updateOkButtonState();
}

BondPropertiesDialog::~BondPropertiesDialog()
{
    delete ui;
}

QString BondPropertiesDialog::getBondMode() const
{
    int index = ui->comboBoxBondMode->currentIndex();
    switch (index)
    {
    case 0:
        return "active-backup";
    case 1:
        return "balance-slb";
    case 2:
        return "lacp";
    default:
        return "active-backup";
    }
}

QString BondPropertiesDialog::getMACAddress() const
{
    return ui->lineEditMAC->text().trimmed();
}

QStringList BondPropertiesDialog::getSelectedPIFRefs() const
{
    QStringList pifRefs;
    QList<QTableWidgetItem*> selectedItems = ui->tableWidgetNICs->selectedItems();
    QSet<int> selectedRows;

    // Get unique rows
    for (QTableWidgetItem* item : selectedItems)
    {
        selectedRows.insert(item->row());
    }

    // Get PIF refs for selected rows
    for (int row : selectedRows)
    {
        if (m_rowToPIFRef.contains(row))
        {
            pifRefs.append(m_rowToPIFRef[row]);
        }
    }

    return pifRefs;
}

void BondPropertiesDialog::loadAvailablePIFs()
{
    if (!m_connection || !m_connection->GetCache())
        return;

    // Get all PIFs from cache
    QList<QVariantMap> allPIFs = m_connection->GetCache()->GetAllData("pif");

    // Filter PIFs that belong to this host and are physical (not VLANs or bonds)
    QList<QVariantMap> availablePIFs;
    for (const QVariantMap& pifData : allPIFs)
    {
        QString pifHost = pifData.value("host").toString();
        bool isPhysical = pifData.value("physical").toBool();
        QString bondMasterOf = pifData.value("bond_master_of").toString();
        bool currentlyAttached = pifData.value("currently_attached").toBool();

        // Only show physical PIFs on this host that aren't already in a bond
        if (pifHost == m_hostRef && isPhysical && bondMasterOf.isEmpty())
        {
            availablePIFs.append(pifData);
        }
    }

    // Populate table
    ui->tableWidgetNICs->setRowCount(availablePIFs.size());
    m_rowToPIFRef.clear();

    for (int i = 0; i < availablePIFs.size(); ++i)
    {
        const QVariantMap& pifData = availablePIFs[i];
        QString pifRef = pifData.value("ref").toString();
        m_rowToPIFRef[i] = pifRef;

        // Device name
        QString device = pifData.value("device").toString();
        QTableWidgetItem* deviceItem = new QTableWidgetItem(device);
        deviceItem->setFlags(deviceItem->flags() & ~Qt::ItemIsEditable);
        ui->tableWidgetNICs->setItem(i, 0, deviceItem);

        // MAC address
        QString mac = pifData.value("MAC").toString();
        QTableWidgetItem* macItem = new QTableWidgetItem(mac);
        macItem->setFlags(macItem->flags() & ~Qt::ItemIsEditable);
        ui->tableWidgetNICs->setItem(i, 1, macItem);

        // Speed
        qint64 speed = pifData.value("speed").toLongLong();
        QString speedStr = speed > 0 ? QString("%1 Mbps").arg(speed) : "Unknown";
        QTableWidgetItem* speedItem = new QTableWidgetItem(speedStr);
        speedItem->setFlags(speedItem->flags() & ~Qt::ItemIsEditable);
        ui->tableWidgetNICs->setItem(i, 2, speedItem);

        // Link status
        bool currentlyAttached = pifData.value("currently_attached").toBool();
        QString status = currentlyAttached ? "Connected" : "Disconnected";
        QTableWidgetItem* statusItem = new QTableWidgetItem(status);
        statusItem->setFlags(statusItem->flags() & ~Qt::ItemIsEditable);
        if (currentlyAttached)
        {
            statusItem->setForeground(QColor(0, 128, 0)); // Green
        } else
        {
            statusItem->setForeground(QColor(128, 0, 0)); // Red
        }
        ui->tableWidgetNICs->setItem(i, 3, statusItem);
    }

    // Resize columns to content
    ui->tableWidgetNICs->resizeColumnsToContents();
}

void BondPropertiesDialog::onSelectionChanged()
{
    updateOkButtonState();
}

void BondPropertiesDialog::updateOkButtonState()
{
    // Bond requires at least 2 PIFs
    int selectedCount = getSelectedPIFRefs().size();
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(selectedCount >= 2);

    // Update label to show selection count
    if (selectedCount < 2)
    {
        ui->groupBoxNICs->setTitle(QString("Network Interfaces (Select at least 2) - %1 selected").arg(selectedCount));
    } else
    {
        ui->groupBoxNICs->setTitle(QString("Network Interfaces - %1 selected").arg(selectedCount));
    }
}
