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

#include "nicstabpage.h"
#include "ui_nicstabpage.h"
#include "xenlib.h"
#include "xencache.h"
#include "xen/connection.h"
#include "xen/actions/network/createbondaction.h"
#include "xen/actions/network/destroybondaction.h"
#include "operations/operationmanager.h"
#include "../dialogs/bondpropertiesdialog.h"
#include <QTableWidgetItem>
#include <QMessageBox>
#include <QDebug>

NICsTabPage::NICsTabPage(QWidget* parent)
    : BaseTabPage(parent), ui(new Ui::NICsTabPage)
{
    this->ui->setupUi(this);

    // Set up table properties
    this->ui->nicsTable->horizontalHeader()->setStretchLastSection(true);

    // Connect signals
    connect(this->ui->nicsTable, &QTableWidget::itemSelectionChanged,
            this, &NICsTabPage::onSelectionChanged);
    connect(this->ui->createBondButton, &QPushButton::clicked,
            this, &NICsTabPage::onCreateBondClicked);
    connect(this->ui->deleteBondButton, &QPushButton::clicked,
            this, &NICsTabPage::onDeleteBondClicked);
    connect(this->ui->rescanButton, &QPushButton::clicked,
            this, &NICsTabPage::onRescanClicked);

    // Disable editing
    this->ui->nicsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

NICsTabPage::~NICsTabPage()
{
    delete this->ui;
}

bool NICsTabPage::isApplicableForObjectType(const QString& objectType) const
{
    // NICs tab is only applicable to Hosts
    return objectType == "host";
}

void NICsTabPage::refreshContent()
{
    this->ui->nicsTable->setRowCount(0);

    if (this->m_objectData.isEmpty() || this->m_objectType != "host")
    {
        return;
    }

    this->populateNICs();
    this->updateButtonStates();
}

void NICsTabPage::populateNICs()
{
    if (!this->m_xenLib)
    {
        qDebug() << "NICsTabPage::populateNICs - No XenLib";
        return;
    }

    // Get all PIFs for this host
    QVariantList pifRefs = this->m_objectData.value("PIFs", QVariantList()).toList();
    qDebug() << "NICsTabPage::populateNICs - Host has" << pifRefs.size() << "PIFs";

    QList<QVariantMap> physicalPIFs;

    for (const QVariant& pifRefVar : pifRefs)
    {
        QString pifRef = pifRefVar.toString();
        QVariantMap pifData = this->m_xenLib->getCache()->ResolveObjectData("PIF", pifRef);

        // Debug: print all keys in pifData
        if (pifData.isEmpty())
        {
            qDebug() << "  PIF" << pifRef << "- EMPTY DATA from cache!";
            continue;
        }

        qDebug() << "  PIF" << pifRef << "has keys:" << pifData.keys();
        qDebug() << "    physical:" << pifData.value("physical", false).toBool()
                 << "device:" << pifData.value("device", "");

        // Implement C# PIF.IsPhysical() logic:
        // IsPhysical() = VLAN == -1 && !IsTunnelAccessPIF() && !IsSriovLogicalPIF()
        // This includes bonds (which have physical=false but are shown in NICs tab)
        qint64 vlan = pifData.value("VLAN", -1).toLongLong();
        QVariantList tunnelAccessPifOf = pifData.value("tunnel_access_PIF_of", QVariantList()).toList();
        QVariantList sriovLogicalPifOf = pifData.value("sriov_logical_PIF_of", QVariantList()).toList();
        
        bool isPhysical = (vlan == -1) && tunnelAccessPifOf.isEmpty() && sriovLogicalPifOf.isEmpty();

        if (isPhysical)
        {
            physicalPIFs.append(pifData);
        }
    }

    qDebug() << "Found" << physicalPIFs.size() << "physical/bond PIFs";

    // Sort by device name
    std::sort(physicalPIFs.begin(), physicalPIFs.end(),
              [](const QVariantMap& a, const QVariantMap& b) {
                  return a.value("device", "").toString() < b.value("device", "").toString();
              });

    for (const QVariantMap& pifData : physicalPIFs)
    {
        this->addNICRow(pifData);
    }

    qDebug() << "NICsTabPage::populateNICs - Added" << this->ui->nicsTable->rowCount() << "rows";
}

void NICsTabPage::addNICRow(const QVariantMap& pifData)
{
    if (!this->m_xenLib)
        return;

    int row = this->ui->nicsTable->rowCount();
    this->ui->nicsTable->insertRow(row);

    // NIC name - Implement C# PIF.Name() logic
    // C# Name(): For physical PIFs, returns "NIC {number}" or "Bond {slaves}"
    // C# NICIdentifier(): strips "eth" from device name, for bonds joins slave numbers with "+"
    QString nicName;
    QVariantList bondMasterOfRefs = pifData.value("bond_master_of", QVariantList()).toList();
    
    if (bondMasterOfRefs.isEmpty())
    {
        // Regular NIC: "eth0" -> "NIC 0"
        QString device = pifData.value("device", "").toString();
        QString number = device;
        number.remove("eth"); // Remove "eth" prefix
        nicName = QString("NIC %1").arg(number);
    }
    else
    {
        // Bond: Get all slave PIFs and format as "Bond 1+2+3"
        QString bondRef = bondMasterOfRefs.first().toString();
        QVariantMap bondData = this->m_xenLib->getCache()->ResolveObjectData("bond", bondRef);
        QVariantList slaveRefs = bondData.value("slaves", QVariantList()).toList();
        
        QStringList slaveNumbers;
        for (const QVariant& slaveRefVar : slaveRefs)
        {
            QString slaveRef = slaveRefVar.toString();
            QVariantMap slavePif = this->m_xenLib->getCache()->ResolveObjectData("pif", slaveRef);
            QString slaveDevice = slavePif.value("device", "").toString();
            QString number = slaveDevice;
            number.remove("eth");
            if (!number.isEmpty())
                slaveNumbers.append(number);
        }
        
        // Sort numbers
        slaveNumbers.sort();
        
        // Format as "Bond 0+1"
        nicName = QString("Bond %1").arg(slaveNumbers.join("+"));
    }

    // MAC Address
    QString mac = pifData.value("MAC", "").toString();

    // Link Status - Must check PIF_metrics.carrier, NOT pifData.carrier
    // C# PIFRow.Update(): _cellConnected.Value = Pif.Carrier() ? Messages.CONNECTED : Messages.DISCONNECTED;
    // C# PIF.Carrier() resolves PIF_metrics and checks carrier field
    QString linkStatus = "Unknown";
    QString pifMetricsRef = pifData.value("metrics", "").toString();
    if (!pifMetricsRef.isEmpty() && pifMetricsRef != "OpaqueRef:NULL")
    {
        QVariantMap metricsData = this->m_xenLib->getCache()->ResolveObjectData("pif_metrics", pifMetricsRef);
        if (!metricsData.isEmpty())
        {
            bool carrier = metricsData.value("carrier", false).toBool();
            linkStatus = carrier ? "Connected" : "Disconnected";
        }
    }

    // Speed (only if connected) - Also from PIF_metrics
    // C# PIFRow.Update(): _cellSpeed.Value = Pif.Carrier() ? Pif.Speed() : Messages.HYPHEN;
    QString speed = "-";
    QString duplex = "-";
    
    if (linkStatus == "Connected")
    {
        // Re-ResolveObjectData metrics for speed/duplex (we already have it above but let's be consistent)
        QVariantMap metricsData = this->m_xenLib->getCache()->ResolveObjectData("pif_metrics", pifMetricsRef);
        if (!metricsData.isEmpty())
        {
            qint64 speedValue = metricsData.value("speed", -1).toLongLong();
            if (speedValue > 0)
            {
                speed = QString::number(speedValue) + " Mbit/s";
            }
            
            // Duplex from metrics
            bool duplexFull = metricsData.value("duplex", false).toBool();
            duplex = duplexFull ? "Full" : "Half";
        }
    }

    // Get PIF_metrics for vendor/device/bus info (reuse the same metricsRef and resolve again)
    QVariantMap metricsData;
    if (!pifMetricsRef.isEmpty())
    {
        metricsData = this->m_xenLib->getCache()->ResolveObjectData("pif_metrics", pifMetricsRef);
    }

    QString vendor = metricsData.value("vendor_name", "-").toString();
    QString device = metricsData.value("device_name", "-").toString();
    QString busPath = metricsData.value("pci_bus_path", "-").toString();

    // FCoE Capable
    QVariantMap capabilities = pifData.value("capabilities", QVariantMap()).toMap();
    bool fcoeCapable = capabilities.contains("fcoe");
    QString fcoeText = fcoeCapable ? "Yes" : "No";

    // SR-IOV
    QString sriovText = "No";
    QVariantList sriovPhysicalPIFOf = pifData.value("sriov_physical_PIF_of", QVariantList()).toList();

    if (!sriovPhysicalPIFOf.isEmpty())
    {
        // This PIF has SR-IOV capability
        QString networkSriovRef = sriovPhysicalPIFOf.first().toString();
        QVariantMap networkSriov = this->m_xenLib->getCache()->ResolveObjectData("network_sriov", networkSriovRef);

        if (!networkSriov.isEmpty())
        {
            bool requiresReboot = networkSriov.value("requires_reboot", false).toBool();
            if (requiresReboot)
            {
                sriovText = "Host needs reboot to enable SR-IOV";
            } else
            {
                // Check logical PIF
                QString logicalPifRef = networkSriov.value("logical_PIF", "").toString();
                if (!logicalPifRef.isEmpty())
                {
                    QVariantMap logicalPif = this->m_xenLib->getCache()->ResolveObjectData("pif", logicalPifRef);
                    bool currentlyAttached = logicalPif.value("currently_attached", false).toBool();

                    if (currentlyAttached)
                    {
                        sriovText = "Yes";
                        // Could query remaining capacity here via API call
                    } else
                    {
                        sriovText = "SR-IOV logical PIF unplugged";
                    }
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
    this->ui->nicsTable->item(row, 0)->setData(Qt::UserRole, pifData.value("ref", "").toString());
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

    if (pifRef.isEmpty() || !this->m_xenLib)
    {
        this->ui->deleteBondButton->setEnabled(false);
        return;
    }

    QVariantMap pifData = this->m_xenLib->getCache()->ResolveObjectData("pif", pifRef);

    // Check if this PIF is a bond interface
    QVariantList bondInterfaceOf = pifData.value("bond_slave_of", QVariantList()).toList();

    // Enable delete bond button only if this is part of a bond
    this->ui->deleteBondButton->setEnabled(!bondInterfaceOf.isEmpty());
}

void NICsTabPage::onSelectionChanged()
{
    this->updateButtonStates();
}

void NICsTabPage::onCreateBondClicked()
{
    if (!this->m_xenLib || this->m_objectType != "host")
    {
        return;
    }

    // Get the network ref - use the first available network or create a bond network
    QString networkRef;
    QList<QVariantMap> networks = this->m_xenLib->getCache()->GetAll("network");
    if (!networks.isEmpty())
    {
        // Use the first network (typically the management network)
        networkRef = networks.first().value("ref").toString();
    } else
    {
        QMessageBox::warning(this, "Create Bond",
                             "No networks available. Please create a network first.");
        return;
    }

    // Open bond creation dialog
    BondPropertiesDialog dialog(this->m_xenLib, this->m_objectRef, networkRef, this);
    if (dialog.exec() == QDialog::Accepted)
    {
        QString bondMode = dialog.getBondMode();
        QStringList pifRefs = dialog.getSelectedPIFRefs();

        if (pifRefs.size() < 2)
        {
            QMessageBox::warning(this, "Create Bond",
                                 "At least 2 network interfaces are required to create a bond.");
            return;
        }

        XenConnection* connection = this->m_xenLib->getConnection();
        if (!connection)
        {
            QMessageBox::critical(this, "Error",
                                  "No active connection.");
            return;
        }

        QVariantMap networkData = this->m_xenLib->getCache()->ResolveObjectData("network", networkRef);
        QString networkName = networkData.value("name_label").toString();
        if (networkName.isEmpty())
            networkName = "Bond Network";

        qint64 mtu = networkData.value("MTU").toLongLong();
        if (mtu <= 0)
            mtu = 1500;

        QString hashingAlgorithm = (bondMode == "lacp") ? "src_mac" : QString();

        CreateBondAction* action = new CreateBondAction(
            connection,
            networkName,
            pifRefs,
            true,
            mtu,
            bondMode,
            hashingAlgorithm,
            this);

        OperationManager::instance()->registerOperation(action);

        connect(action, &AsyncOperation::completed, [this, bondMode, action]() {
            this->refreshContent();
            QMessageBox::information(this, "Bond Created",
                                     QString("Bond created successfully with mode: %1").arg(bondMode));
            action->deleteLater();
        });

        connect(action, &AsyncOperation::failed, [this, action](const QString& error) {
            QMessageBox::critical(this, "Error",
                                  QString("Failed to create bond: %1").arg(error));
            action->deleteLater();
        });

        action->runAsync();
    }
}

void NICsTabPage::onDeleteBondClicked()
{
    QList<QTableWidgetItem*> selectedItems = this->ui->nicsTable->selectedItems();
    if (selectedItems.isEmpty())
    {
        QMessageBox::information(this, "Delete Bond",
                                 "Please select a bonded interface to delete.");
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
    QVariant pifDataVar = this->m_xenLib->getCache()->ResolveObjectData("pif", pifRef);
    if (pifDataVar.isNull())
        return;

    QVariantMap pifData = pifDataVar.value<QVariantMap>();
    QString bondSlaveOf = pifData.value("bond_slave_of").toString();

    if (bondSlaveOf.isEmpty())
    {
        QMessageBox::information(this, "Delete Bond",
                                 "Selected interface is not a bonded interface.");
        return;
    }

    // Confirm deletion
    QString device = pifData.value("device").toString();
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Delete Bond",
                                                              QString("Are you sure you want to delete the bond on %1?\n\n"
                                                                      "This will separate the bonded interfaces.")
                                                                  .arg(device),
                                                              QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes)
    {
        XenConnection* connection = this->m_xenLib->getConnection();
        if (!connection)
        {
            QMessageBox::critical(this, "Error",
                                  "No active connection.");
            return;
        }

        DestroyBondAction* action = new DestroyBondAction(connection, bondSlaveOf, this);
        OperationManager::instance()->registerOperation(action);

        connect(action, &AsyncOperation::completed, [this, action]() {
            this->refreshContent();
            QMessageBox::information(this, "Bond Deleted",
                                     "Bond deleted successfully.");
            action->deleteLater();
        });

        connect(action, &AsyncOperation::failed, [this, action](const QString& error) {
            QMessageBox::critical(this, "Error",
                                  QString("Failed to delete bond: %1").arg(error));
            action->deleteLater();
        });

        action->runAsync();
    }
}

void NICsTabPage::onRescanClicked()
{
    // Refresh the PIF data
    if (this->m_xenLib)
    {
        this->refreshContent();
        QMessageBox::information(this, "Rescan",
                                 "Network interfaces rescanned.");
    }
}
