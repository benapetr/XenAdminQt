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
#include <QMessageBox>
#include <QDebug>
#include "mainwindow.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/network.h"
#include "xenlib/xen/pif.h"
#include "xenlib/xen/pifmetrics.h"
#include "xenlib/xen/network_sriov.h"
#include "xenlib/xen/bond.h"
#include "xenlib/xen/actions/network/createbondaction.h"
#include "xenlib/xen/actions/network/destroybondaction.h"
#include "nicstabpage.h"
#include "ui_nicstabpage.h"
#include "commands/host/rescanpifscommand.h"
#include "../dialogs/bondpropertiesdialog.h"

NICsTabPage::NICsTabPage(QWidget* parent) : BaseTabPage(parent), ui(new Ui::NICsTabPage)
{
    this->ui->setupUi(this);

    // Set up table properties
    this->ui->nicsTable->horizontalHeader()->setStretchLastSection(true);

    // Connect signals
    connect(this->ui->nicsTable, &QTableWidget::itemSelectionChanged, this, &NICsTabPage::onSelectionChanged);
    connect(this->ui->createBondButton, &QPushButton::clicked, this, &NICsTabPage::onCreateBondClicked);
    connect(this->ui->deleteBondButton, &QPushButton::clicked, this, &NICsTabPage::onDeleteBondClicked);
    connect(this->ui->rescanButton, &QPushButton::clicked, this, &NICsTabPage::onRescanClicked);

    // Disable editing
    this->ui->nicsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

NICsTabPage::~NICsTabPage()
{
    delete this->ui;
}

bool NICsTabPage::IsApplicableForObjectType(const QString& objectType) const
{
    // NICs tab is only applicable to Hosts
    return objectType == "host";
}

void NICsTabPage::refreshContent()
{
    this->ui->nicsTable->setRowCount(0);

    if (this->m_objectData.isEmpty() || this->m_objectType != XenObjectType::Host)
    {
        return;
    }

    this->populateNICs();
    this->updateButtonStates();
}

void NICsTabPage::populateNICs()
{
    if (!this->m_connection || !this->m_connection->GetCache())
    {
        qDebug() << "NICsTabPage::populateNICs - No connection/cache";
        return;
    }

    QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(this->m_object);
    if (!host)
    {
        qDebug() << "NICsTabPage::populateNICs - Host object missing";
        return;
    }

    QList<QSharedPointer<PIF>> pifs = host->GetPIFs();
    QList<QSharedPointer<PIF>> physicalPifs;

    for (const QSharedPointer<PIF>& pif : pifs)
    {
        if (!pif || !pif->IsValid())
            continue;

        if (!pif->IsPhysical())
            continue;

        physicalPifs.append(pif);
    }

    // Sort by device name
    std::sort(physicalPifs.begin(), physicalPifs.end(),
              [](const QSharedPointer<PIF>& a, const QSharedPointer<PIF>& b) {
                  QString left = a ? a->GetDevice() : QString();
                  QString right = b ? b->GetDevice() : QString();
                  return left < right;
              });

    for (const QSharedPointer<PIF>& pif : physicalPifs)
    {
        this->addNICRow(pif);
    }

    //qDebug() << "NICsTabPage::populateNICs - Added" << this->ui->nicsTable->rowCount() << "rows";
}

void NICsTabPage::addNICRow(const QSharedPointer<PIF>& pif)
{
    if (!this->m_connection || !this->m_connection->GetCache())
        return;

    if (!pif || !pif->IsValid())
        return;

    int row = this->ui->nicsTable->rowCount();
    this->ui->nicsTable->insertRow(row);

    // NIC name - Use PIF.GetName() logic (matches C# PIF.Name()).
    QString nicName = pif->GetName();

    // MAC Address
    QString mac = pif->GetMAC();

    // Link Status - Must check PIF_metrics.carrier, NOT pifData.carrier
    // C# PIFRow.Update(): _cellConnected.Value = Pif.Carrier() ? Messages.CONNECTED : Messages.DISCONNECTED;
    // C# PIF.Carrier() resolves PIF_metrics and checks carrier field
    QString linkStatus = pif->GetLinkStatusString();

    // Speed (only if connected) - Also from PIF_metrics
    // C# PIFRow.Update(): _cellSpeed.Value = Pif.Carrier() ? Pif.Speed() : Messages.HYPHEN;
    QString speed = "-";
    QString duplex = "-";
    
    if (linkStatus == "Connected")
    {
        QSharedPointer<PIFMetrics> metrics = this->m_connection->GetCache()->ResolveObject<PIFMetrics>(pif->MetricsRef());
        if (metrics && metrics->IsValid())
        {
            qint64 speedValue = metrics->Speed();
            if (speedValue > 0)
                speed = QString::number(speedValue) + " Mbit/s";

            duplex = metrics->Duplex() ? "Full" : "Half";
        }
    }

    // Get PIF_metrics for vendor/device/bus info (reuse the same metricsRef and resolve again)
    QString vendor = "-";
    QString device = "-";
    QString busPath = "-";
    QSharedPointer<PIFMetrics> metrics = this->m_connection->GetCache()->ResolveObject<PIFMetrics>(pif->MetricsRef());
    if (metrics && metrics->IsValid())
    {
        vendor = metrics->VendorName();
        device = metrics->DeviceName();
        busPath = metrics->PciBusPath();
    }

    // FCoE Capable
    QStringList capabilities = pif->Capabilities();
    bool fcoeCapable = capabilities.contains("fcoe");
    QString fcoeText = fcoeCapable ? "Yes" : "No";

    // SR-IOV
    QString sriovText = "No";
    QStringList sriovPhysicalPIFOf = pif->SriovPhysicalPIFOfRefs();

    if (!sriovPhysicalPIFOf.isEmpty())
    {
        // This PIF has SR-IOV capability
        QString networkSriovRef = sriovPhysicalPIFOf.first();
        QSharedPointer<NetworkSriov> networkSriov = this->m_connection->GetCache()->ResolveObject<NetworkSriov>(networkSriovRef);

        if (networkSriov && networkSriov->IsValid())
        {
            bool requiresReboot = networkSriov->RequiresReboot();
            if (requiresReboot)
            {
                sriovText = "Host needs reboot to enable SR-IOV";
            } else
            {
                // Check logical PIF
                QSharedPointer<PIF> logicalPif = networkSriov->GetLogicalPIF();
                if (logicalPif && logicalPif->IsValid())
                {
                    if (logicalPif->IsCurrentlyAttached())
                        sriovText = "Yes";
                    else
                        sriovText = "SR-IOV logical PIF unplugged";
                }
            }
        }
    } else
    {
        // Check if SR-IOV is capable but network not created
        bool sriovCapable = capabilities.contains("sriov");
        if (sriovCapable)
        {
            sriovText = "SR-IOV network should be created";
        }
    }

    // Set all cell values
    this->ui->nicsTable->setItem(row, 0, new QTableWidgetItem(nicName));
    this->ui->nicsTable->setItem(row, 1, new QTableWidgetItem(mac));
    this->ui->nicsTable->setItem(row, 2, new QTableWidgetItem(linkStatus));
    this->ui->nicsTable->setItem(row, 3, new QTableWidgetItem(speed));
    this->ui->nicsTable->setItem(row, 4, new QTableWidgetItem(duplex));
    this->ui->nicsTable->setItem(row, 5, new QTableWidgetItem(vendor));
    this->ui->nicsTable->setItem(row, 6, new QTableWidgetItem(device));
    this->ui->nicsTable->setItem(row, 7, new QTableWidgetItem(busPath));
    this->ui->nicsTable->setItem(row, 8, new QTableWidgetItem(fcoeText));
    this->ui->nicsTable->setItem(row, 9, new QTableWidgetItem(sriovText));

    // Store PIF ref in first column for later retrieval
    this->ui->nicsTable->item(row, 0)->setData(Qt::UserRole, pif->OpaqueRef());
}

void NICsTabPage::updateButtonStates()
{
    bool hasSelection = this->ui->nicsTable->currentRow() >= 0;

    if (!hasSelection)
    {
        this->ui->deleteBondButton->setEnabled(false);
        return;
    }

    // Get selected PIF
    int row = this->ui->nicsTable->currentRow();
    QString pifRef = this->ui->nicsTable->item(row, 0)->data(Qt::UserRole).toString();

    if (pifRef.isEmpty() || !this->m_connection || !this->m_connection->GetCache())
    {
        this->ui->deleteBondButton->setEnabled(false);
        return;
    }

    QSharedPointer<PIF> pif = this->m_connection->GetCache()->ResolveObject<PIF>(pifRef);
    if (!pif || !pif->IsValid())
    {
        this->ui->deleteBondButton->setEnabled(false);
        return;
    }

    if (!pif->IsBondMaster())
    {
        this->ui->deleteBondButton->setEnabled(false);
        return;
    }

    const QStringList bondRefs = pif->BondMasterOfRefs();
    if (bondRefs.isEmpty())
    {
        this->ui->deleteBondButton->setEnabled(false);
        return;
    }

    QSharedPointer<Bond> bond = this->m_connection->GetCache()->ResolveObject<Bond>(bondRefs.first());
    bool bondLocked = bond && bond->IsLocked();
    this->ui->deleteBondButton->setEnabled(!bondLocked);
}

void NICsTabPage::onSelectionChanged()
{
    this->updateButtonStates();
}

void NICsTabPage::onCreateBondClicked()
{
    if (!this->m_connection || !this->m_connection->GetCache() || this->m_objectType != XenObjectType::Host)
    {
        return;
    }

    // Get the network ref - use the first available network or create a bond network
    QString networkRef;
    QList<QSharedPointer<Network>> networks = this->m_connection->GetCache()->GetAll<Network>();
    if (!networks.isEmpty())
    {
        // Use the first network (typically the management network)
        QSharedPointer<Network> network = networks.first();
        networkRef = network ? network->OpaqueRef() : QString();
    } else
    {
        QMessageBox::warning(this, "Create Bond", "No networks available. Please create a network first.");
        return;
    }

    // Open bond creation dialog
    XenCache* cache = this->m_connection->GetCache();
    QSharedPointer<Host> host = cache->ResolveObject<Host>(XenObjectType::Host, this->m_objectRef);
    QSharedPointer<Network> network = cache->ResolveObject<Network>(XenObjectType::Network, networkRef);
    BondPropertiesDialog dialog(host, network, this);
    if (dialog.exec() == QDialog::Accepted)
    {
        QString bondMode = dialog.getBondMode();
        if (bondMode.isEmpty())
            bondMode = "active-backup";
        QStringList pifRefs = dialog.getSelectedPIFRefs();

        if (pifRefs.size() < 2)
        {
            QMessageBox::warning(this, "Create Bond", "At least 2 network interfaces are required to create a bond.");
            return;
        }

        XenConnection* connection = this->m_connection;
        if (!connection)
        {
            QMessageBox::critical(this, "Error", "No active connection.");
            return;
        }

        QSharedPointer<Network> network = this->m_connection->GetCache()->ResolveObject<Network>(networkRef);
        QString bondName = dialog.getBondName();
        if (bondName.isEmpty())
            bondName = "Bond";

        qint64 mtu = dialog.getMTU();
        if (mtu <= 0)
            mtu = 1500;

        const bool autoPlug = dialog.getAutoPlug();
        const QString hashingAlgorithm = dialog.getHashingAlgorithm();

        CreateBondAction* action = new CreateBondAction(
            connection,
            bondName,
            pifRefs,
            autoPlug,
            mtu,
            bondMode,
            hashingAlgorithm,
            this);


        connect(action, &AsyncOperation::completed, this, [this, bondMode, action]()
        {
            this->refreshContent();
            QMessageBox::information(this, "Bond Created", QString("Bond created successfully with mode: %1").arg(bondMode));
            action->deleteLater();
        }, Qt::QueuedConnection);

        connect(action, &AsyncOperation::failed, this, [this, action](const QString& error)
        {
            QMessageBox::critical(this, "Error", QString("Failed to create bond: %1").arg(error));
            action->deleteLater();
        }, Qt::QueuedConnection);

        action->RunAsync();
    }
}

void NICsTabPage::onDeleteBondClicked()
{
    QList<QTableWidgetItem*> selectedItems = this->ui->nicsTable->selectedItems();
    if (selectedItems.isEmpty())
    {
        QMessageBox::information(this, "Delete Bond", "Please select a bonded interface to delete.");
        return;
    }

    int selectedRow = selectedItems.first()->row();
    QTableWidgetItem* bondRefItem = this->ui->nicsTable->item(selectedRow, 0);
    if (!bondRefItem)
        return;

    QString pifRef = bondRefItem->data(Qt::UserRole).toString();
    if (pifRef.isEmpty())
        return;

    // Get PIF data to check if it's a bond
    QSharedPointer<PIF> pif = this->m_connection->GetCache()->ResolveObject<PIF>(pifRef);
    if (!pif || !pif->IsValid())
        return;

    if (!pif->IsBondMaster())
    {
        QMessageBox::information(this, "Delete Bond", "Please select a bonded interface to delete.");
        return;
    }

    const QStringList bondRefs = pif->BondMasterOfRefs();
    if (bondRefs.isEmpty())
    {
        QMessageBox::information(this, "Delete Bond", "No bond found for the selected interface.");
        return;
    }

    const QString bondRef = bondRefs.first();

    // Confirm deletion
    QString device = pif->GetDevice();
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Delete Bond",
                                                              QString("Are you sure you want to delete the bond on %1?\n\n"
                                                                      "This will separate the bonded interfaces.")
                                                                  .arg(device),
                                                              QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes)
    {
        XenConnection* connection = this->m_connection;
        if (!connection)
        {
            QMessageBox::critical(this, "Error", "No active connection.");
            return;
        }

        try
        {
            DestroyBondAction* action = new DestroyBondAction(connection, bondRef, this);

            connect(action, &AsyncOperation::completed, this, [this, action]()
            {
                this->refreshContent();
                action->deleteLater();
            }, Qt::QueuedConnection);

            connect(action, &AsyncOperation::failed, this, [this, action](const QString& error)
            {
                QMessageBox::critical(this, "Error", QString("Failed to delete bond: %1").arg(error));
                action->deleteLater();
            }, Qt::QueuedConnection);

            action->RunAsync();
        } catch (const std::exception& e)
        {
            QMessageBox::critical(this, "Error", QString("Failed to delete bond: %1").arg(e.what()));
        } catch (...)
        {
            QMessageBox::critical(this, "Error", "Failed to delete bond.");
        }
    }
}

void NICsTabPage::onRescanClicked()
{
    QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(this->m_object);
    if (!host || !host->IsValid())
        return;

    RescanPIFsCommand cmd(MainWindow::instance(), this);
    if (cmd.CanRun())
        cmd.Run();
}
