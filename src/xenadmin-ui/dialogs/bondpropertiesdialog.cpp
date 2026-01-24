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
#include "xen/host.h"
#include "xen/network.h"
#include "xen/pif.h"
#include "xencache.h"
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QPushButton>

BondPropertiesDialog::BondPropertiesDialog(QSharedPointer<Host> host, QSharedPointer<Network> network, QWidget* parent) : QDialog(parent), ui(new Ui::BondPropertiesDialog), m_host(host), m_network(network)
{
    this->ui->setupUi(this);

    // Set up table
    this->ui->tableWidgetNICs->setSelectionMode(QAbstractItemView::MultiSelection);
    this->ui->tableWidgetNICs->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->ui->tableWidgetNICs->horizontalHeader()->setStretchLastSection(true);

    // Set default bond mode to Active-Backup (safest option)
    this->ui->comboBoxBondMode->setCurrentIndex(0);

    // Connect signals
    connect(this->ui->tableWidgetNICs, &QTableWidget::itemSelectionChanged, this, &BondPropertiesDialog::onSelectionChanged);

    // Load available PIFs
    this->loadAvailablePIFs();

    // Update OK button state
    this->updateOkButtonState();
}

BondPropertiesDialog::~BondPropertiesDialog()
{
    delete this->ui;
}

QString BondPropertiesDialog::getBondMode() const
{
    int index = this->ui->comboBoxBondMode->currentIndex();
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
    return this->ui->lineEditMAC->text().trimmed();
}

QStringList BondPropertiesDialog::getSelectedPIFRefs() const
{
    QStringList pifRefs;
    QList<QTableWidgetItem*> selectedItems = this->ui->tableWidgetNICs->selectedItems();
    QSet<int> selectedRows;

    // Get unique rows
    for (QTableWidgetItem* item : selectedItems)
    {
        selectedRows.insert(item->row());
    }

    // Get PIF refs for selected rows
    for (int row : selectedRows)
    {
        if (this->m_rowToPIFRef.contains(row))
        {
            pifRefs.append(this->m_rowToPIFRef[row]);
        }
    }

    return pifRefs;
}

void BondPropertiesDialog::loadAvailablePIFs()
{
    if (!this->m_host || !this->m_host->GetCache())
        return;

    // Get all PIFs from cache
    QList<QSharedPointer<PIF>> allPIFs = this->m_host->GetCache()->GetAll<PIF>("pif");

    // Filter PIFs that belong to this host and are physical (not VLANs or bonds)
    QList<QSharedPointer<PIF>> availablePIFs;
    for (const QSharedPointer<PIF>& pif : allPIFs)
    {
        if (!pif || !pif->IsValid())
            continue;

        QString pifHost = pif->GetHostRef();
        bool isPhysical = pif->IsPhysical();
        bool isBondMaster = !pif->BondMasterOfRefs().isEmpty();
        //bool currentlyAttached = pifData.value("currently_attached").toBool();

        // Only show physical PIFs on this host that aren't already in a bond
        if (pifHost == this->m_host->OpaqueRef() && isPhysical && !isBondMaster)
        {
            availablePIFs.append(pif);
        }
    }

    // Populate table
    this->ui->tableWidgetNICs->setRowCount(availablePIFs.size());
    this->m_rowToPIFRef.clear();

    for (int i = 0; i < availablePIFs.size(); ++i)
    {
        QSharedPointer<PIF> pif = availablePIFs[i];
        QString pifRef = pif ? pif->OpaqueRef() : QString();
        this->m_rowToPIFRef[i] = pifRef;

        // Device name
        QString device = pif ? pif->GetDevice() : QString();
        QTableWidgetItem* deviceItem = new QTableWidgetItem(device);
        deviceItem->setFlags(deviceItem->flags() & ~Qt::ItemIsEditable);
        this->ui->tableWidgetNICs->setItem(i, 0, deviceItem);

        // MAC address
        QString mac = pif ? pif->GetMAC() : QString();
        QTableWidgetItem* macItem = new QTableWidgetItem(mac);
        macItem->setFlags(macItem->flags() & ~Qt::ItemIsEditable);
        this->ui->tableWidgetNICs->setItem(i, 1, macItem);

        // Speed
        qint64 speed = 0;
        if (pif)
            speed = pif->GetData().value("speed").toLongLong();
        QString speedStr = speed > 0 ? QString("%1 Mbps").arg(speed) : "Unknown";
        QTableWidgetItem* speedItem = new QTableWidgetItem(speedStr);
        speedItem->setFlags(speedItem->flags() & ~Qt::ItemIsEditable);
        this->ui->tableWidgetNICs->setItem(i, 2, speedItem);

        // Link status
        bool currentlyAttached = pif && pif->IsCurrentlyAttached();
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
        this->ui->tableWidgetNICs->setItem(i, 3, statusItem);
    }

    // Resize columns to content
    this->ui->tableWidgetNICs->resizeColumnsToContents();
}

void BondPropertiesDialog::onSelectionChanged()
{
    this->updateOkButtonState();
}

void BondPropertiesDialog::updateOkButtonState()
{
    // Bond requires at least 2 PIFs
    int selectedCount = this->getSelectedPIFRefs().size();
    this->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(selectedCount >= 2);

    // Update label to show selection count
    if (selectedCount < 2)
    {
        this->ui->groupBoxNICs->setTitle(QString("Network Interfaces (Select at least 2) - %1 selected").arg(selectedCount));
    } else
    {
        this->ui->groupBoxNICs->setTitle(QString("Network Interfaces - %1 selected").arg(selectedCount));
    }
}
