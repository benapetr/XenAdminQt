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

#include "networktabpage.h"
#include "ui_networktabpage.h"
#include "xenlib/xen/session.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/vif.h"
#include "xenlib/xen/network.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/pifmetrics.h"
#include "xenlib/xen/tunnel.h"
#include "xenlib/xen/vlan.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/xenapi/xenapi_Network.h"
#include "../settingsmanager.h"
#include "../dialogs/newnetworkwizard.h"
#include "../dialogs/networkpropertiesdialog.h"
#include "../dialogs/networkingpropertiesdialog.h"
#include "../dialogs/vifdialog.h"
#include "../dialogs/operationprogressdialog.h"
#include "xenlib/xen/actions/vif/deletevifaction.h"
#include "xenlib/xen/actions/vif/plugvifaction.h"
#include "xenlib/xen/actions/vif/unplugvifaction.h"
#include "xenlib/xen/actions/vif/createvifaction.h"
#include "xenlib/xen/actions/vif/updatevifaction.h"
#include "xen/pif.h"
#include <QTableWidgetItem>
#include <QMessageBox>
#include <QDebug>
#include <QMenu>
#include <QClipboard>
#include <QApplication>

NetworkTabPage::NetworkTabPage(QWidget* parent) : BaseTabPage(parent), ui(new Ui::NetworkTabPage)
{
    this->ui->setupUi(this);

    // Set up table properties
    this->ui->networksTable->horizontalHeader()->setStretchLastSection(true);
    this->ui->ipConfigTable->horizontalHeader()->setStretchLastSection(true);

    // Disable editing
    this->ui->networksTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->ui->ipConfigTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Set icon column width to minimum
    this->ui->ipConfigTable->setColumnWidth(1, 20);

    // Enable context menus
    this->ui->networksTable->setContextMenuPolicy(Qt::CustomContextMenu);
    this->ui->ipConfigTable->setContextMenuPolicy(Qt::CustomContextMenu);

    // Connect button signals (matches C# flowLayoutPanel1 buttons)
    connect(this->ui->addButton, &QPushButton::clicked, this, &NetworkTabPage::onAddNetwork);
    connect(this->ui->propertiesButton, &QPushButton::clicked, this, &NetworkTabPage::onEditNetwork);
    connect(this->ui->removeButton, &QPushButton::clicked, this, &NetworkTabPage::onRemoveNetwork);
    connect(this->ui->activateButton, &QPushButton::clicked, this, &NetworkTabPage::onActivateToggle);
    connect(this->ui->configureButton, &QPushButton::clicked, this, &NetworkTabPage::onConfigureClicked);
    connect(this->ui->networksTable, &QTableWidget::customContextMenuRequested, this, &NetworkTabPage::showNetworksContextMenu);
    connect(this->ui->ipConfigTable, &QTableWidget::customContextMenuRequested, this, &NetworkTabPage::showIPConfigContextMenu);
    connect(this->ui->networksTable, &QTableWidget::itemSelectionChanged, this, &NetworkTabPage::onNetworksTableSelectionChanged);
    connect(this->ui->ipConfigTable, &QTableWidget::itemSelectionChanged, this, &NetworkTabPage::onIPConfigTableSelectionChanged);
    connect(this->ui->networksTable, &QTableWidget::itemDoubleClicked, this, &NetworkTabPage::onNetworksTableDoubleClicked);
    connect(this->ui->ipConfigTable, &QTableWidget::itemDoubleClicked, this, &NetworkTabPage::onIpConfigTableDoubleClicked);
}

NetworkTabPage::~NetworkTabPage()
{
    delete this->ui;
}

bool NetworkTabPage::IsApplicableForObjectType(const QString& objectType) const
{
    // Network tab is applicable to VMs, Hosts, and Pools
    // For VMs: shows network interfaces (VIFs)
    // For Hosts/Pools: shows network infrastructure
    return objectType == "vm" || objectType == "host" || objectType == "pool";
}

void NetworkTabPage::refreshContent()
{
    if (this->m_objectData.isEmpty())
    {
        this->ui->networksTable->setRowCount(0);
        this->ui->ipConfigTable->setRowCount(0);
        return;
    }

    if (this->m_objectType == "vm")
    {
        // Show only networks section for VMs (VIFs)
        // Hide IP configuration (that's for hosts/pools)
        this->ui->networksGroup->setVisible(true);
        this->ui->ipConfigurationGroup->setVisible(false);

        // Set up VIF columns for VMs
        this->setupVifColumns();
        this->populateVIFsForVM();
    } else if (this->m_objectType == "host")
    {
        // Show both sections for hosts
        this->ui->networksGroup->setVisible(true);
        this->ui->ipConfigurationGroup->setVisible(true);

        // Set up network infrastructure columns
        this->setupNetworkColumns();
        this->populateNetworksForHost();
        this->populateIPConfigForHost();
    } else if (this->m_objectType == "pool")
    {
        // Show both sections for pools
        this->ui->networksGroup->setVisible(true);
        this->ui->ipConfigurationGroup->setVisible(true);

        // Set up network infrastructure columns
        this->setupNetworkColumns();
        this->populateNetworksForPool();
        this->populateIPConfigForPool();
    }
}

void NetworkTabPage::removeObject()
{
    if (!this->m_connection)
        return;

    XenCache* cache = this->m_connection->GetCache();
    disconnect(cache, &XenCache::objectChanged, this, &NetworkTabPage::onCacheObjectChanged);
}

void NetworkTabPage::updateObject()
{
    XenCache* cache = this->m_connection ? this->m_connection->GetCache() : nullptr;
    connect(cache, &XenCache::objectChanged, this, &NetworkTabPage::onCacheObjectChanged, Qt::UniqueConnection);
}

void NetworkTabPage::setupVifColumns()
{
    // Set up columns for VM VIFs (Virtual Interfaces)
    // Matches C# NetworkList.AddVifColumns()
    this->ui->networksTable->clear();
    this->ui->networksTable->setColumnCount(6);

    QStringList headers;
    headers << "Device" << "MAC" << "Limit" << "Network" << "IP Address" << "Active";
    this->ui->networksTable->setHorizontalHeaderLabels(headers);

    // Set column widths
    this->ui->networksTable->setColumnWidth(0, 80);  // Device
    this->ui->networksTable->setColumnWidth(1, 140); // MAC
    this->ui->networksTable->setColumnWidth(2, 100); // Limit
    this->ui->networksTable->setColumnWidth(3, 150); // Network
    this->ui->networksTable->setColumnWidth(4, 150); // IP Address
    this->ui->networksTable->setColumnWidth(5, 80);  // Active

    // Last column should stretch
    this->ui->networksTable->horizontalHeader()->setStretchLastSection(false);
    this->ui->networksTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
}

void NetworkTabPage::setupNetworkColumns()
{
    // Set up columns for Host/Pool networks
    // Matches C# NetworkList.AddNetworkColumns()
    this->ui->networksTable->clear();
    this->ui->networksTable->setColumnCount(9);

    QStringList headers;
    headers << "Name" << "Description" << "NIC" << "VLAN" << "Auto"
            << "Link Status" << "MAC" << "MTU" << "SR-IOV";
    this->ui->networksTable->setHorizontalHeaderLabels(headers);

    // Set column widths
    this->ui->networksTable->setColumnWidth(0, 150); // Name
    this->ui->networksTable->setColumnWidth(2, 80);  // NIC
    this->ui->networksTable->setColumnWidth(3, 60);  // VLAN
    this->ui->networksTable->setColumnWidth(4, 60);  // Auto
    this->ui->networksTable->setColumnWidth(5, 100); // Link Status
    this->ui->networksTable->setColumnWidth(6, 140); // MAC
    this->ui->networksTable->setColumnWidth(7, 60);  // MTU
    this->ui->networksTable->setColumnWidth(8, 80);  // SR-IOV

    // Description column should stretch
    this->ui->networksTable->horizontalHeader()->setStretchLastSection(false);
    this->ui->networksTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

void NetworkTabPage::populateVIFsForVM()
{
    // Clear the table
    this->ui->networksTable->setRowCount(0);

    QSharedPointer<VM> vm = qSharedPointerCast<VM>(this->m_object);

    if (!vm)
    {
        qDebug() << "NetworkTabPage::populateVIFsForVM - No object";
        return;
    }

    if (!vm->GetConnection())
    {
        qDebug() << "NetworkTabPage::populateVIFsForVM - No connection";
        return;
    }

    // Match C# logic: Resolve VIF objects from VM
    // C#: List<VIF> vifs = vm.Connection.ResolveAll(vm.VIFs);
    QList<QSharedPointer<VIF>> vifs = vm->GetVIFs();
    if (vifs.isEmpty())
    {
        qDebug() << "NetworkTabPage::populateVIFsForVM - No VIFs found for VM";
        return;
    }

    // Get guest_metrics reference for IP addresses
    // C#: VM_guest_metrics vmGuestMetrics = vm.Connection.Resolve(vm.guest_metrics);
    QString guestMetricsRef = vm->GetGuestMetricsRef();
    QVariantMap guestMetrics;
    QVariantMap networks;

    XenCache* cache = vm->GetCache();

    if (!guestMetricsRef.isEmpty() && guestMetricsRef != "OpaqueRef:NULL")
    {
        // Resolve guest_metrics from cache to get network info (IP addresses)
        guestMetrics = cache->ResolveObjectData("vm_guest_metrics", guestMetricsRef);
        if (!guestMetrics.isEmpty())
        {
            networks = guestMetrics.value("networks").toMap();
            //qDebug() << "NetworkTabPage::populateVIFsForVM - Guest metrics networks:" << networks.keys();
        }
    }

    QList<QSharedPointer<VIF>> visibleVifs;
    visibleVifs.reserve(vifs.size());
    for (const QSharedPointer<VIF>& vif : vifs)
    {
        if (!vif || !vif->IsValid())
        {
            continue;
        }

        // C#: Check for guest installer network (CA-73056)
        // var network = vif.Connection.Resolve(vif.network);
        // if (network != null && network.IsGuestInstallerNetwork() && !ShowHiddenVMs) continue;
        QSharedPointer<Network> network = vif->GetNetwork();
        if (network && network->IsValid())
        {
            bool isGuestInstallerNetwork = network->IsGuestInstallerNetwork();
            // TODO: Check ShowHiddenVMs setting when implemented
            if (isGuestInstallerNetwork)
            {
                qDebug() << "NetworkTabPage::populateVIFsForVM - Skipping guest installer network VIF";
                continue;
            }
        }

        visibleVifs.append(vif);
    }

    // Sort VIFs by device number (matches C# vifs.Sort())
    std::sort(visibleVifs.begin(), visibleVifs.end(),
              [](const QSharedPointer<VIF>& a, const QSharedPointer<VIF>& b) {
                  return a->GetDevice().toInt() < b->GetDevice().toInt();
              });

    //qDebug() << "NetworkTabPage::populateVIFsForVM - Displaying" << vifs.size() << "VIFs";

    // Populate table with VIF information (matches C# VifRow structure)
    for (const QSharedPointer<VIF>& vif : visibleVifs)
    {
        int row = this->ui->networksTable->rowCount();
        this->ui->networksTable->insertRow(row);

        // Store VIF ref for later retrieval (used by getSelectedVifRef)
        QString vifRef = vif->OpaqueRef();

        // Column 0: GetDevice (e.g., "0", "1", "2")
        // C#: DeviceCell.Value = Vif.device;
        QString device = vif->GetDevice();
        QTableWidgetItem* deviceItem = new QTableWidgetItem(device);
        deviceItem->setData(Qt::UserRole, vifRef); // Store ref as hidden data
        this->ui->networksTable->setItem(row, 0, deviceItem);

        // Column 1: MAC address
        // C#: MacCell.Value = Helpers.GetMacString(Vif.MAC);
        QString mac = vif->GetMAC();
        // Format MAC address like C# Helpers.GetMacString() - insert colons
        if (mac.length() == 12 && !mac.contains(":"))
        {
            QString formattedMac;
            for (int i = 0; i < mac.length(); i += 2)
            {
                if (i > 0)
                    formattedMac += ":";
                formattedMac += mac.mid(i, 2);
            }
            mac = formattedMac;
        }
        this->ui->networksTable->setItem(row, 1, new QTableWidgetItem(mac));

        // Column 2: Limit (QoS bandwidth limit)
        // C#: LimitCell.Value = Vif.qos_algorithm_type != "" ? Vif.LimitString() : "";
        QString limit;
        QString qosAlgorithm = vif->QosAlgorithmType();
        if (!qosAlgorithm.isEmpty())
        {
            QVariantMap qosParams = vif->QosAlgorithmParams();
            if (qosParams.contains("kbps"))
            {
                // Format as "<value> kbps" like C# VIF.LimitString()
                QString kbps = qosParams.value("kbps").toString();
                limit = kbps + " kbps";
            }
        }
        this->ui->networksTable->setItem(row, 2, new QTableWidgetItem(limit));

        // Column 3: Network name
        // C#: NetworkCell.Value = Vif.NetworkName();
        QString networkName = "-";
        QSharedPointer<Network> network = vif->GetNetwork();
        if (network && network->IsValid())
            networkName = network->GetName();
        this->ui->networksTable->setItem(row, 3, new QTableWidgetItem(networkName));

        // Column 4: IP Address(es) from guest_metrics
        // C#: IpCell.Value = Vif.IPAddressesAsString();
        QString ipAddress;
        if (!networks.isEmpty())
        {
            QStringList ipAddresses;
            // Look for keys like "0/ip", "0/ipv4/0", "0/ipv6/0", etc.
            // C# VIF.IPAddressesAsString() searches for "<device>/ip*" keys
            QString devicePrefix = device + "/";
            for (auto it = networks.constBegin(); it != networks.constEnd(); ++it)
            {
                QString key = it.key();
                if (key.startsWith(devicePrefix) && key.contains("/ip"))
                {
                    QString ip = it.value().toString();
                    if (!ip.isEmpty())
                        ipAddresses.append(ip);
                }
            }

            if (!ipAddresses.isEmpty())
            {
                // Join multiple IPs with comma+space
                ipAddress = ipAddresses.join(", ");
            }
        }
        this->ui->networksTable->setItem(row, 4, new QTableWidgetItem(ipAddress));

        // Column 5: Active status (currently_attached)
        // C#: AttachedCell.Value = Vif.currently_attached ? Messages.YES : Messages.NO;
        bool attached = vif->CurrentlyAttached();
        QString activeText = attached ? tr("Yes") : tr("No");
        this->ui->networksTable->setItem(row, 5, new QTableWidgetItem(activeText));
    }

    // Update button states after populating (matches C# UpdateEnablement call)
    updateButtonStates();
}

void NetworkTabPage::populateNetworksForHost()
{
    this->ui->networksTable->setRowCount(0);

    if (!this->m_connection || !this->m_connection->GetCache())
    {
        qDebug() << "NetworkTabPage::populateNetworksForHost - No connection/cache";
        return;
    }

    XenCache* cache = this->m_connection->GetCache();
    QList<QSharedPointer<Network>> networks = cache->GetAll<Network>("network");

    for (const QSharedPointer<Network>& network : networks)
    {
        if (!network || !network->IsValid())
            continue;

        if (!shouldShowNetwork(network))
        {
            qDebug() << "Skipping network:" << network->GetName();
            continue;
        }

        this->addNetworkRow(network);
    }

    //qDebug() << "NetworkTabPage::populateNetworksForHost - Added" << this->ui->networksTable->rowCount() << "rows";
}

bool NetworkTabPage::shouldShowNetwork(QSharedPointer<Network> network)
{
    // Matching C# Network.Show() logic

    QVariantMap otherConfig = network->GetOtherConfig();

    // 1. Check IsGuestInstallerNetwork - don't show guest installer networks
    if (otherConfig.value("is_guest_installer_network", "false").toString() == "true")
    {
        return false;
    }

    // 2. Check IsHidden - don't show if HideFromXenCenter is set
    if (otherConfig.value("HideFromXenCenter", "false").toString() == "true")
    {
        return false;
    }

    // 3. Check if network has name - networks without names are usually internal
    QString name = network->GetName();
    if (name.isEmpty())
    {
        return false;
    }

    const bool showHiddenObjects = SettingsManager::instance().getShowHiddenObjects();
    return network->Show(showHiddenObjects);
}

void NetworkTabPage::populateNetworksForPool()
{
    // For pools, show the same as hosts
    this->populateNetworksForHost();
}

void NetworkTabPage::addNetworkRow(QSharedPointer<Network> network)
{
    if (!network)
        return;

    XenCache* cache = network->GetCache();
    if (!network->IsValid() || !cache)
        return;

    int row = this->ui->networksTable->rowCount();
    this->ui->networksTable->insertRow(row);

    QString name = network->GetName();
    QString description = network->GetDescription();

    QSharedPointer<PIF> pif;
    QList<QSharedPointer<PIF>> pifs = network->GetPIFs();

    if (this->m_objectType == "host")
    {
        QSharedPointer<Host> host = qSharedPointerCast<Host>(this->m_object);
        if (!host && this->m_connection)
            host = this->m_connection->GetCache()->ResolveObject<Host>("host", this->m_objectRef);

        QString hostRef = host ? host->OpaqueRef() : QString();
        for (const QSharedPointer<PIF>& currentPif : pifs)
        {
            if (!currentPif || !currentPif->IsValid())
                continue;
            if (!hostRef.isEmpty() && currentPif->GetHostRef() == hostRef)
            {
                pif = currentPif;
                break;
            }
        }
    } else if (this->m_objectType == "pool" && !pifs.isEmpty())
    {
        pif = pifs.first();
    }

    QString nicInfo = "-";
    QString vlanInfo = "-";
    QString autoInfo = network->IsAutomatic() == true ? "Yes" : "No";
    QString linkStatus = "-";
    QString macInfo = "-";
    QString mtuInfo = "-";
    QString sriovInfo = "No";

    // C# NetworkRow.UpdateDetails() logic - build NIC name, VLAN, Link Status, etc.
    if (pif && pif->IsValid())
    {
        // NIC name: Use C# PIF.Name() formatting ("NIC 0", "Bond 0+1").
        nicInfo = pif->GetName();

        // VLAN: Check if this is a VLAN interface
        // C#: VlanCell.Value = Helpers.VlanString(Pif);
        qint64 vlan = pif->GetVLAN();
        if (vlan >= 0)
        {
            vlanInfo = QString::number(vlan);
        } else
        {
            vlanInfo = "-";
        }

        // Link Status: Must check PIF_metrics.carrier, NOT currently_attached
        // C# NetworkRow.UpdateDetails(): LinkStatusCell.Value = Xmo is Pool ? Network.LinkStatusString() : Pif == null ? Messages.NONE : Pif.LinkStatusString();
        if (this->m_objectType == "pool")
        {
            // For pools, aggregate link status across all PIFs (C# Network.LinkStatusString())
            linkStatus = network->GetLinkStatusString();
        } else
        {
            // For hosts, use PIF.LinkStatusString() - check PIF_metrics.carrier
            linkStatus = pif->GetLinkStatusString();
        }
        
        //qDebug() << "NetworkTabPage: Network" << name << "linkStatus:" << linkStatus;

        // MAC: Only show for physical NICs, not VLANs or tunnels
        // C#: MacCell.Value = Pif != null && Pif.IsPhysical() ? Pif.MAC : Messages.SPACED_HYPHEN;
        if (pif->IsPhysical())
        {
            macInfo = pif->GetMAC();
        } else
        {
            macInfo = "-";
        }

        // MTU: Network-level property
        // C#: MtuCell.Value = Network.CanUseJumboFrames() ? Network.MTU.ToString() : Messages.SPACED_HYPHEN;
        bool canUseJumboFrames = network->CanUseJumboFrames();
        if (canUseJumboFrames)
        {
            qint64 mtu = network->GetMTU();
            if (mtu > 0)
                mtuInfo = QString::number(mtu);
        }

        // SR-IOV: Check if PIF has network_sriov
        // C# NetworkRow.UpdateDetails() checks PIF.NetworkSriov()
        QString networkSriovRef = this->getPifNetworkSriov(pif);
        if (!networkSriovRef.isEmpty())
        {
            QVariantMap sriovData = cache->ResolveObjectData("network_sriov", networkSriovRef);
            bool requiresReboot = sriovData.value("requires_reboot", false).toBool();
            if (requiresReboot)
            {
                sriovInfo = "Reboot Required";
            } else
            {
                sriovInfo = "Yes";
            }
        } else
        {
            sriovInfo = "No";
        }
    }

    // Create items and store network ref in first column as user data
    QTableWidgetItem* nameItem = new QTableWidgetItem(name);
    nameItem->setData(Qt::UserRole, network->OpaqueRef()); // Store network ref for later use

    this->ui->networksTable->setItem(row, 0, nameItem);
    this->ui->networksTable->setItem(row, 1, new QTableWidgetItem(description));
    this->ui->networksTable->setItem(row, 2, new QTableWidgetItem(nicInfo));
    this->ui->networksTable->setItem(row, 3, new QTableWidgetItem(vlanInfo));
    this->ui->networksTable->setItem(row, 4, new QTableWidgetItem(autoInfo));
    this->ui->networksTable->setItem(row, 5, new QTableWidgetItem(linkStatus));
    this->ui->networksTable->setItem(row, 6, new QTableWidgetItem(macInfo));
    this->ui->networksTable->setItem(row, 7, new QTableWidgetItem(mtuInfo));
    this->ui->networksTable->setItem(row, 8, new QTableWidgetItem(sriovInfo));
}

void NetworkTabPage::populateIPConfigForHost()
{
    this->ui->ipConfigTable->setRowCount(0);

    QSharedPointer<Host> host = qSharedPointerCast<Host>(this->m_object);
    if (!this->m_connection || !this->m_connection->GetCache() || !host)
        return;

    // Get all PIFs for this host
    QList<QSharedPointer<PIF>> pifs = host->GetPIFs();
    QList<QSharedPointer<PIF>> managementPIFs;

    for (const QSharedPointer<PIF>& pif : pifs)
    {
        if (!pif || !pif->IsValid())
            continue;

        // Only show management interfaces
        bool isManagement = pif->Management();

        // Also check other_config for secondary management interfaces
        QVariantMap otherConfig = pif->GetOtherConfig();
        bool hasManagementPurpose = otherConfig.contains("management_purpose");

        if (isManagement || hasManagementPurpose)
        {
            managementPIFs.append(pif);
        }
    }

    // Sort PIFs - primary management first
    std::sort(managementPIFs.begin(), managementPIFs.end(),
              [](const QSharedPointer<PIF>& a, const QSharedPointer<PIF>& b) {
                  bool aIsPrimary = a->Management();
                  bool bIsPrimary = b->Management();
                  if (aIsPrimary != bIsPrimary)
                      return aIsPrimary;
                  return a->GetDevice() < b->GetDevice();
              });

    for (const QSharedPointer<PIF>& pif : managementPIFs)
    {
        this->addIPConfigRow(pif, host);
    }
}

void NetworkTabPage::populateIPConfigForPool()
{
    this->ui->ipConfigTable->setRowCount(0);

    QSharedPointer<Pool> pool = qSharedPointerCast<Pool>(this->m_object);
    if (!this->m_connection || !this->m_connection->GetCache() || !pool)
        return;

    // For pools, show management interfaces from all hosts
    QList<QSharedPointer<Host>> hosts = pool->GetHosts();
    for (const QSharedPointer<Host>& host : hosts)
    {
        if (!host || !host->IsValid())
            continue;

        QList<QSharedPointer<PIF>> pifs = host->GetPIFs();
        for (const QSharedPointer<PIF>& pif : pifs)
        {
            if (!pif || !pif->IsValid())
                continue;

            // Only show management interfaces
            bool isManagement = pif->Management();
            QVariantMap otherConfig = pif->GetOtherConfig();
            bool hasManagementPurpose = otherConfig.contains("management_purpose");

            if (isManagement || hasManagementPurpose)
            {
                this->addIPConfigRow(pif, host);
            }
        }
    }
}

void NetworkTabPage::addIPConfigRow(const QSharedPointer<PIF>& pif, const QSharedPointer<Host>& host)
{
    if (!this->m_connection || !this->m_connection->GetCache())
        return;

    if (!pif || !pif->IsValid())
        return;

    int row = this->ui->ipConfigTable->rowCount();
    this->ui->ipConfigTable->insertRow(row);

    QString pifRef = pif->OpaqueRef();

    // Server name
    QString hostName = "Unknown";
    if (host && host->IsValid())
        hostName = host->GetName();
    else
    {
        QSharedPointer<Host> resolvedHost = pif->GetHost();
        if (resolvedHost && resolvedHost->IsValid())
            hostName = resolvedHost->GetName();
    }

    // Icon column - TODO: Add proper icon

    // Interface (Management or other purpose)
    QString interfaceType;
    bool isManagement = pif->Management();
    if (isManagement)
    {
        interfaceType = "Management";
    } else
    {
        QVariantMap otherConfig = pif->GetOtherConfig();
        interfaceType = otherConfig.value("management_purpose", "Unknown").toString();
    }

    // Network name
    QString networkName = "-";
    QSharedPointer<Network> network = pif->GetNetwork();
    if (network && network->IsValid())
        networkName = network->GetName();

    // NIC (C# PIF.Name() formatting)
    QString nicName = pif->GetName();

    // IP Setup (DHCP or Static)
    QString ipMode = pif->IpConfigurationMode();
    QString ipSetup = ipMode;
    if (ipMode.compare("DHCP", Qt::CaseInsensitive) == 0)
        ipSetup = "DHCP";
    else if (ipMode.compare("Static", Qt::CaseInsensitive) == 0)
        ipSetup = "Static";
    else if (ipMode.compare("None", Qt::CaseInsensitive) == 0)
        ipSetup = "None";

    // IP Address
    QString ipAddress = pif->IP();

    // Subnet mask
    QString netmask = pif->Netmask();

    // Gateway
    QString gateway = pif->Gateway();

    // DNS
    QString dns = pif->DNS();

    // Create items and store PIF ref in first column as user data
    QTableWidgetItem* hostNameItem = new QTableWidgetItem(hostName);
    hostNameItem->setData(Qt::UserRole, pifRef); // Store PIF ref for later use

    this->ui->ipConfigTable->setItem(row, 0, hostNameItem);
    // Column 1 is icon - skip for now
    this->ui->ipConfigTable->setItem(row, 2, new QTableWidgetItem(interfaceType));
    this->ui->ipConfigTable->setItem(row, 3, new QTableWidgetItem(networkName));
    this->ui->ipConfigTable->setItem(row, 4, new QTableWidgetItem(nicName));
    this->ui->ipConfigTable->setItem(row, 5, new QTableWidgetItem(ipSetup));
    this->ui->ipConfigTable->setItem(row, 6, new QTableWidgetItem(ipAddress));
    this->ui->ipConfigTable->setItem(row, 7, new QTableWidgetItem(netmask));
    this->ui->ipConfigTable->setItem(row, 8, new QTableWidgetItem(gateway));
    this->ui->ipConfigTable->setItem(row, 9, new QTableWidgetItem(dns));
}

void NetworkTabPage::onConfigureClicked()
{
    // Get selected PIF from IP Config table
    QString selectedPifRef = this->getSelectedPifRef();

    if (selectedPifRef.isEmpty())
    {
        QMessageBox::information(this, tr("Configure IP Addresses"),
                                 tr("Please select a management interface to configure."));
        return;
    }

    QSharedPointer<PIF> pif = this->m_connection && this->m_connection->GetCache()
        ? this->m_connection->GetCache()->ResolveObject<PIF>("pif", selectedPifRef)
        : QSharedPointer<PIF>();
    if (!pif || !pif->IsValid())
        return;

    // Open NetworkingProperties dialog with selected PIF
    NetworkingPropertiesDialog dialog(pif, this);
    if (dialog.exec() == QDialog::Accepted)
    {
        // Refresh the IP configuration display after changes
        this->populateIPConfigForHost();
    }
}

void NetworkTabPage::onNetworksTableDoubleClicked(QTableWidgetItem* item)
{
    if (!item || !this->canEnterPropertiesWindow)
        return;
    this->ui->networksTable->setCurrentItem(item);
    this->onEditNetwork();
}

void NetworkTabPage::onIpConfigTableDoubleClicked(QTableWidgetItem* item)
{
    if (!item)
        return;
    this->onConfigureClicked();
}

QString NetworkTabPage::getSelectedNetworkRef() const
{
    QList<QTableWidgetItem*> selectedItems = this->ui->networksTable->selectedItems();
    if (selectedItems.isEmpty())
        return QString();

    int row = this->ui->networksTable->currentRow();
    if (row < 0 || row >= this->ui->networksTable->rowCount())
        return QString();

    // Network ref is stored as user data in the first column
    QTableWidgetItem* item = this->ui->networksTable->item(row, 0);
    if (!item)
        return QString();

    return item->data(Qt::UserRole).toString();
}

QString NetworkTabPage::getSelectedPifRef() const
{
    QList<QTableWidgetItem*> selectedItems = this->ui->ipConfigTable->selectedItems();
    if (selectedItems.isEmpty())
        return QString();

    int row = this->ui->ipConfigTable->currentRow();
    if (row < 0 || row >= this->ui->ipConfigTable->rowCount())
        return QString();

    // PIF ref is stored as user data in the first column
    QTableWidgetItem* item = this->ui->ipConfigTable->item(row, 0);
    if (!item)
        return QString();

    return item->data(Qt::UserRole).toString();
}

void NetworkTabPage::showNetworksContextMenu(const QPoint& pos)
{
    QPoint globalPos = this->ui->networksTable->mapToGlobal(pos);

    // Get item at position
    QTableWidgetItem* item = this->ui->networksTable->itemAt(pos);

    QMenu contextMenu;

    // Always add "Copy" if there's an item
    QAction* copyAction = nullptr;
    if (item && !item->text().isEmpty())
    {
        copyAction = contextMenu.addAction(tr("&Copy"));
    }

    // Add separator
    if (copyAction)
        contextMenu.addSeparator();

    // For VMs: Add/Edit/Remove VIF actions
    // For Host/Pool: Add/Edit/Remove Network actions
    QAction* addAction = nullptr;
    QAction* propertiesAction = nullptr;
    QAction* removeAction = nullptr;

    if (this->m_objectType == "vm")
    {
        // VM-specific actions (VIF management)
        addAction = contextMenu.addAction(tr("Add &Interface..."));

        // Only enable edit/remove if an interface is selected
        if (item)
        {
            propertiesAction = contextMenu.addAction(tr("&Properties..."));
            removeAction = contextMenu.addAction(tr("&Remove Interface"));
        }
    } else if (this->m_objectType == "host" || this->m_objectType == "pool")
    {
        // Host/Pool-specific actions (Network management)
        addAction = contextMenu.addAction(tr("&Add Network..."));

        QString selectedNetworkRef = this->getSelectedNetworkRef();
        if (!selectedNetworkRef.isEmpty() && this->m_connection && this->m_connection->GetCache())
        {
            QVariantMap networkData = this->m_connection->GetCache()->ResolveObjectData("network", selectedNetworkRef);

            // Enable Properties and Remove for editable networks
            // Check if network is not a bond member, not guest installer, etc.
            QVariantMap otherConfig = networkData.value("other_config", QVariantMap()).toMap();
            bool isGuestInstaller = otherConfig.value("is_guest_installer_network", "false").toString() == "true";

            if (!isGuestInstaller)
            {
                propertiesAction = contextMenu.addAction(tr("&Properties..."));
                removeAction = contextMenu.addAction(tr("&Remove Network"));
            }
        }
    }

    // Show menu and handle selection
    QAction* selectedAction = contextMenu.exec(globalPos);

    if (selectedAction == copyAction)
    {
        this->copyToClipboard();
    } else if (selectedAction == addAction)
    {
        // Launch New Network Wizard
        this->onAddNetwork();
    } else if (selectedAction == propertiesAction)
    {
        // Open network properties dialog
        this->onEditNetwork();
    } else if (selectedAction == removeAction)
    {
        // Remove network
        this->onRemoveNetwork();
    }
}

void NetworkTabPage::showIPConfigContextMenu(const QPoint& pos)
{
    QPoint globalPos = this->ui->ipConfigTable->mapToGlobal(pos);

    // Get item at position
    QTableWidgetItem* item = this->ui->ipConfigTable->itemAt(pos);

    QMenu contextMenu;

    // Always add "Copy" if there's an item
    QAction* copyAction = nullptr;
    if (item && !item->text().isEmpty())
    {
        copyAction = contextMenu.addAction(tr("&Copy"));
    }

    // Add separator
    if (copyAction)
        contextMenu.addSeparator();

    // Add "Configure" action
    QAction* configureAction = contextMenu.addAction(tr("C&onfigure..."));

    QString selectedPifRef = this->getSelectedPifRef();
    if (selectedPifRef.isEmpty())
    {
        configureAction->setEnabled(false);
    }

    // Show menu and handle selection
    QAction* selectedAction = contextMenu.exec(globalPos);

    if (selectedAction == copyAction)
    {
        this->copyToClipboard();
    } else if (selectedAction == configureAction)
    {
        this->onConfigureClicked();
    }
}

void NetworkTabPage::copyToClipboard()
{
    // Determine which table has focus
    QTableWidget* activeTable = nullptr;

    if (this->ui->networksTable->hasFocus() || !this->ui->networksTable->selectedItems().isEmpty())
    {
        activeTable = this->ui->networksTable;
    } else if (this->ui->ipConfigTable->hasFocus() || !this->ui->ipConfigTable->selectedItems().isEmpty())
    {
        activeTable = this->ui->ipConfigTable;
    }

    if (!activeTable)
        return;

    QList<QTableWidgetItem*> selectedItems = activeTable->selectedItems();
    if (selectedItems.isEmpty())
        return;

    // Get the first selected item's text
    QString text = selectedItems.first()->text();

    if (!text.isEmpty())
    {
        QApplication::clipboard()->setText(text);
    }
}

void NetworkTabPage::onNetworksTableSelectionChanged()
{
    // Match C# UpdateEnablement() - update button states based on selection
    this->updateButtonStates();
}

void NetworkTabPage::onIPConfigTableSelectionChanged()
{
    // Update Configure button state based on selection
    QString selectedPifRef = this->getSelectedPifRef();
    bool hasSelection = !selectedPifRef.isEmpty();

    this->ui->configureButton->setEnabled(hasSelection);

    if (hasSelection)
    {
        qDebug() << "NetworkTabPage: Selected PIF:" << selectedPifRef;
    }
}

void NetworkTabPage::onAddNetwork()
{
    // Match C# NetworkList::AddNetworkButton_Click

    if (!this->m_connection || !this->m_connection->IsConnected())
    {
        QMessageBox::warning(this, tr("Not Connected"),
                             tr("Not connected to XenServer."));
        return;
    }

    if (this->m_objectType == "vm")
    {
        // C#: For VMs, check MaxVIFsAllowed() then show VIFDialog
        int currentVifCount = this->ui->networksTable->rowCount();
        // TODO: Get actual MaxVIFsAllowed from VM - for now use 7 as default
        int maxVifs = 7;

        if (currentVifCount >= maxVifs)
        {
            QMessageBox::critical(this, tr("Maximum VIFs Reached"),
                                  tr("The maximum number of network interfaces (%1) has been reached for this VM.")
                                      .arg(maxVifs));
            return;
        }

        // Find next available device ID
        int deviceId = 0;
        QSet<int> usedDevices;
        for (int row = 0; row < this->ui->networksTable->rowCount(); ++row)
        {
            QTableWidgetItem* item = this->ui->networksTable->item(row, 0);
            if (item)
            {
                int device = item->text().toInt();
                usedDevices.insert(device);
            }
        }

        while (usedDevices.contains(deviceId))
            deviceId++;

        QSharedPointer<VM> vm = this->m_connection && this->m_connection->GetCache()
            ? this->m_connection->GetCache()->ResolveObject<VM>("vm", this->m_objectRef)
            : QSharedPointer<VM>();
        if (!vm || !vm->IsValid())
            return;

        // Show VIFDialog
        VIFDialog dialog(vm, deviceId, this);
        if (dialog.exec() == QDialog::Accepted)
        {
            QVariantMap vifSettings = dialog.getVifSettings();

            // Create VIF using CreateVIFAction
            CreateVIFAction* action = new CreateVIFAction(this->m_connection,
                                                          this->m_objectRef, // VM ref
                                                          vifSettings,
                                                          this);

            // Matches C# NetworkList.cs createVIFAction_Completed (line 539)
            connect(action, &CreateVIFAction::completed, this, [this, action]() {
                if (action->GetState() == AsyncOperation::OperationState::Completed)
                {
                    // Check if reboot is required for hot-plug (matches C# line 544)
                    if (action->rebootRequired())
                    {
                        QMessageBox::information(this,
                                                 tr("Virtual Network Device Changes"),
                                                 tr("The virtual network device changes will take effect when you shut down and then restart the VM."));
                    }

                    //qDebug() << "VIF created successfully";
                    // Refresh the tab to show new VIF
                    this->refreshContent();
                }
                action->deleteLater();
            });

            connect(action, &CreateVIFAction::failed, this, [this, action](const QString& error) {
                QMessageBox::critical(this, tr("Create VIF Failed"),
                                      tr("Failed to create network interface.\n\nError: %1").arg(error));
                action->deleteLater();
            });

            // Show progress dialog
            OperationProgressDialog* progressDialog = new OperationProgressDialog(action, this);
            progressDialog->setAttribute(Qt::WA_DeleteOnClose);
            progressDialog->show();

            // Start the action
            action->RunAsync();
        }
    } else
    {
        // C#: For hosts/pools, show NewNetworkWizard
        // Launch New Network Wizard
        NewNetworkWizard wizard(this);

        // Set connection context
        wizard.setWindowTitle(tr("New Network"));

        if (wizard.exec() == QDialog::Accepted)
        {
            // Get network configuration from wizard
            QString networkName = wizard.networkName();
            QString networkDescription = wizard.networkDescription();
            NewNetworkWizard::NetworkType networkType = wizard.networkType();
            int vlanId = wizard.vlanId();
            int mtu = wizard.mtu();
            bool autoAddToVMs = wizard.autoAddToVMs();
            bool autoPlug = wizard.autoPlug();

            qDebug() << "Creating new network:" << networkName << "type:" << static_cast<int>(networkType);

            // Build other_config based on wizard settings
            QVariantMap otherConfig;

            // Add network type information
            if (networkType == NewNetworkWizard::NetworkType::External)
            {
                otherConfig["network_type"] = "external";
            } else if (networkType == NewNetworkWizard::NetworkType::Internal)
            {
                otherConfig["network_type"] = "internal";
            } else if (networkType == NewNetworkWizard::NetworkType::Bonded)
            {
                otherConfig["network_type"] = "bonded";
            } else if (networkType == NewNetworkWizard::NetworkType::CrossHost)
            {
                otherConfig["network_type"] = "crosshost";
            } else if (networkType == NewNetworkWizard::NetworkType::SRIOV)
            {
                otherConfig["network_type"] = "sriov";
            }

            // Add VLAN tag if specified
            if (vlanId > 0)
            {
                otherConfig["vlan"] = vlanId;
            }

            // Add auto-configuration settings
            if (autoAddToVMs)
            {
                otherConfig["automatic"] = "true";
            }

            if (autoPlug)
            {
                otherConfig["auto_plug"] = "true";
            }

            // Create network using XenAPI
            XenAPI::Session* session = this->m_connection ? this->m_connection->GetSession() : nullptr;
            if (!session || !session->IsLoggedIn())
            {
                QMessageBox::critical(this, tr("Failed to Create Network"),
                                      tr("No active session to create network."));
                return;
            }

            QVariantMap networkRecord;
            networkRecord["name_label"] = networkName;
            networkRecord["name_description"] = networkDescription;
            networkRecord["other_config"] = otherConfig;
            networkRecord["MTU"] = 1500;
            networkRecord["tags"] = QVariantList();

            QString networkRef;
            try
            {
                networkRef = XenAPI::Network::create(session, networkRecord);
            }
            catch (const std::exception& ex)
            {
                QMessageBox::critical(this, tr("Failed to Create Network"),
                                      tr("Failed to create network '%1'.\n\nError: %2")
                                          .arg(networkName)
                                          .arg(ex.what()));
                return;
            }

            if (!networkRef.isEmpty())
            {
                qDebug() << "Network created successfully:" << networkRef;

                // Set MTU if specified
                if (mtu > 0 && mtu != 1500) // 1500 is the default
                {
                    try
                    {
                        XenAPI::Network::set_MTU(session, networkRef, mtu);
                    }
                    catch (const std::exception& ex)
                    {
                        qWarning() << "NetworkTabPage::onAddNetwork - Failed to set MTU:" << ex.what();
                    }
                }

                // Refresh network cache after creation
                try
                {
                    QVariantMap allRecords;
                    QVariantList refs = XenAPI::Network::get_all(session);
                    for (const QVariant& refVar : refs)
                    {
                        QString ref = refVar.toString();
                        if (ref.isEmpty())
                            continue;
                        QVariantMap record = XenAPI::Network::get_record(session, ref);
                        record["ref"] = ref;
                        allRecords[ref] = record;
                    }
                    if (this->m_connection && this->m_connection->GetCache())
                        this->m_connection->GetCache()->UpdateBulk("network", allRecords);
                }
                catch (const std::exception& ex)
                {
                    qWarning() << "NetworkTabPage::onAddNetwork - Failed to refresh networks:" << ex.what();
                }
            } else
            {
                // Show error message
                QMessageBox::critical(this, tr("Failed to Create Network"), tr("Failed to create network '%1'.").arg(networkName));
            }
        }
    }
}

void NetworkTabPage::onEditNetwork()
{
    // Match C# NetworkList::EditNetworkButton_Click

    if (this->m_objectType == "vm")
    {
        // C#: launchVmNetworkSettingsDialog() - opens VIFDialog
        QString vifRef = getSelectedVifRef();
        if (vifRef.isEmpty())
            return;

        QSharedPointer<VIF> vif = this->m_connection && this->m_connection->GetCache()
            ? this->m_connection->GetCache()->ResolveObject<VIF>("VIF", vifRef)
            : QSharedPointer<VIF>();
        if (!vif || !vif->IsValid())
            return;

        // Show VIFDialog for editing
        VIFDialog dialog(vif, this);
        if (dialog.exec() == QDialog::Accepted && dialog.hasChanges())
        {
            QVariantMap newSettings = dialog.getVifSettings();

            // Update VIF using UpdateVIFAction
            UpdateVIFAction* action = new UpdateVIFAction(this->m_connection,
                                                          this->m_objectRef, // VM ref
                                                          vifRef,            // old VIF ref
                                                          newSettings,       // new settings
                                                          this);

            connect(action, &UpdateVIFAction::completed, this, [this, action]() {
                if (action->GetState() == AsyncOperation::OperationState::Completed)
                {
                    qDebug() << "VIF updated successfully";
                    // Refresh the tab to show updated VIF
                    this->refreshContent();
                }
                action->deleteLater();
            });

            connect(action, &UpdateVIFAction::failed, this, [this, action](const QString& error)
            {
                QMessageBox::critical(this, tr("Update VIF Failed"), tr("Failed to update network interface.\n\nError: %1").arg(error));
                action->deleteLater();
            });

            // Show progress dialog
            OperationProgressDialog* progressDialog = new OperationProgressDialog(action, this);
            progressDialog->setAttribute(Qt::WA_DeleteOnClose);
            progressDialog->show();

            // Start the action
            action->RunAsync();
        }
    } else
    {
        // C#: launchHostOrPoolNetworkSettingsDialog()
        if (!this->m_connection)
            return;

        QSharedPointer<Network> network = this->m_connection->GetCache()->ResolveObject<Network>("network", this->getSelectedNetworkRef());

        if (!network)
            return;

        // Launch network properties dialog
        NetworkPropertiesDialog dialog(network, this);

        if (dialog.exec() == QDialog::Accepted)
        {
            // Network properties were updated
            qDebug() << "Network properties updated for:" << network->GetName();

            // Refresh the network list
            this->refreshContent();
        }
    }
}

void NetworkTabPage::onRemoveNetwork()
{
    // Match C# NetworkList::RemoveNetworkButton_Click

    if (this->m_objectType == "vm")
    {
        // C#: Use DeleteVIFAction for VMs
        QSharedPointer<VIF> vif = getSelectedVif();
        if (!vif || !vif->IsValid())
            return;

        QString vifRef = vif->OpaqueRef();
        QString device = vif->GetDevice();
        QString networkName = "-";

        // Get network name
        QSharedPointer<Network> network = vif->GetNetwork();
        if (network && network->IsValid())
            networkName = network->GetName();

        // C#: Show confirmation dialog, then use DeleteVIFAction
        int ret = QMessageBox::question(this, tr("Remove Network Interface"),
                                        tr("Are you sure you want to remove network interface %1 (%2)?")
                                            .arg(device, networkName),
                                        QMessageBox::Yes | QMessageBox::No);

        if (ret == QMessageBox::Yes)
        {
            // Use DeleteVIFAction (matches C# DeleteVIFAction)
            DeleteVIFAction* action = new DeleteVIFAction(this->m_connection, vifRef, this);

            connect(action, &DeleteVIFAction::completed, this, [this, action]() {
                if (action->GetState() == AsyncOperation::OperationState::Completed)
                {
                    qDebug() << "VIF deleted successfully";
                    // Refresh the tab to show updated VIF list
                    this->refreshContent();
                }
                action->deleteLater();
            });

            connect(action, &DeleteVIFAction::failed, this, [this, action](const QString& error) {
                QMessageBox::critical(this, tr("Delete VIF Failed"),
                                      tr("Failed to delete network interface.\n\nError: %1").arg(error));
                action->deleteLater();
            });

            // Show progress dialog
            OperationProgressDialog* progressDialog = new OperationProgressDialog(action, this);
            progressDialog->setAttribute(Qt::WA_DeleteOnClose);
            progressDialog->show();

            // Start the action
            action->RunAsync();
        }
    } else
    {
        // C#: Use NetworkAction for hosts/pools
        QString selectedNetworkRef = this->getSelectedNetworkRef();
        if (selectedNetworkRef.isEmpty() || !this->m_connection || !this->m_connection->GetCache())
        {
            return;
        }

        QSharedPointer<Network> network = this->m_connection->GetCache()->ResolveObject<Network>("network", selectedNetworkRef);
        QString networkName = network ? network->GetName() : tr("Unknown");

        // Confirm removal
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            tr("Remove Network"),
            tr("Are you sure you want to remove the network '%1'?\n\n"
               "This action cannot be undone.")
                .arg(networkName),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes)
        {
            // Remove the network using XenAPI
            this->removeNetwork(selectedNetworkRef);
        }
    }
}

void NetworkTabPage::onActivateToggle()
{
    // Match C# NetworkList::buttonActivateToggle_Click

    if (this->m_objectType != "vm")
        return;

    QSharedPointer<VIF> vif = getSelectedVif();
    if (!vif || !vif->IsValid())
        return;

    QString vifRef = vif->OpaqueRef();
    bool currentlyAttached = vif->CurrentlyAttached();

    if (currentlyAttached)
    {
        // C#: Use UnplugVIFAction to deactivate (unplug) VIF
        UnplugVIFAction* action = new UnplugVIFAction(this->m_connection, vifRef, this);

        connect(action, &UnplugVIFAction::completed, this, [this, action]() {
            if (action->GetState() == AsyncOperation::OperationState::Completed)
            {
                qDebug() << "VIF unplugged successfully";
                // Refresh the tab to show updated VIF status
                this->refreshContent();
            }
            action->deleteLater();
        });

        connect(action, &UnplugVIFAction::failed, this, [this, action](const QString& error) {
            QMessageBox::critical(this, tr("Unplug VIF Failed"),
                                  tr("Failed to deactivate network interface.\n\nError: %1").arg(error));
            action->deleteLater();
        });

        // Show progress dialog
        OperationProgressDialog* progressDialog = new OperationProgressDialog(action, this);
        progressDialog->setAttribute(Qt::WA_DeleteOnClose);
        progressDialog->show();

        // Start the action
        action->RunAsync();
    } else
    {
        // C#: Use PlugVIFAction to activate (plug) VIF
        PlugVIFAction* action = new PlugVIFAction(this->m_connection, vifRef, this);

        connect(action, &PlugVIFAction::completed, this, [this, action]() {
            if (action->GetState() == AsyncOperation::OperationState::Completed)
            {
                qDebug() << "VIF plugged successfully";
                // Refresh the tab to show updated VIF status
                this->refreshContent();
            }
            action->deleteLater();
        });

        connect(action, &PlugVIFAction::failed, this, [this, action](const QString& error) {
            QMessageBox::critical(this, tr("Plug VIF Failed"),
                                  tr("Failed to activate network interface.\n\nError: %1").arg(error));
            action->deleteLater();
        });

        // Show progress dialog
        OperationProgressDialog* progressDialog = new OperationProgressDialog(action, this);
        progressDialog->setAttribute(Qt::WA_DeleteOnClose);
        progressDialog->show();

        // Start the action
        action->RunAsync();
    }
}

void NetworkTabPage::removeNetwork(const QString& networkRef)
{
    if (!this->m_connection || !this->m_connection->GetCache())
    {
        qWarning() << "NetworkTabPage::removeNetwork - No connection/cache available";
        return;
    }

    QSharedPointer<Network> network = this->m_connection->GetCache()->ResolveObject<Network>("network", networkRef);
    if (!network || !network->IsValid())
        return;

    QString networkName = network->GetName();

    qDebug() << "Removing network:" << networkName << "ref:" << networkRef;

    // Check if network has any PIFs attached
    QList<QSharedPointer<PIF>> pifs = network->GetPIFs();

    if (!pifs.isEmpty())
    {
        // Network has PIFs - warn user
        QMessageBox::StandardButton reply = QMessageBox::warning(
            this,
            tr("Network In Use"),
            tr("Network '%1' has %2 network interface(s) attached.\n\n"
               "Removing this network will disconnect these interfaces.\n\n"
               "Continue?")
                .arg(networkName)
                .arg(pifs.count()),
            QMessageBox::Yes | QMessageBox::No);

        if (reply != QMessageBox::Yes)
        {
            return;
        }
    }

    // Remove network using XenAPI
    XenAPI::Session* session = this->m_connection->GetSession();
    if (!session || !session->IsLoggedIn())
    {
        QMessageBox::critical(this, tr("Failed to Remove Network"),
                              tr("No active session to remove network '%1'.").arg(networkName));
        return;
    }

    try
    {
        XenAPI::Network::destroy(session, networkRef);
        qDebug() << "Network removed successfully:" << networkRef;

        QVariantMap allRecords;
        QVariantList refs = XenAPI::Network::get_all(session);
        for (const QVariant& refVar : refs)
        {
            QString ref = refVar.toString();
            if (ref.isEmpty())
                continue;
            QVariantMap record = XenAPI::Network::get_record(session, ref);
            record["ref"] = ref;
            allRecords[ref] = record;
        }
        if (this->m_connection->GetCache())
            this->m_connection->GetCache()->UpdateBulk("network", allRecords);
    }
    catch (const std::exception& ex)
    {
        // Show error message
        QMessageBox::critical(this, tr("Failed to Remove Network"),
                              tr("Failed to remove network '%1'.\n\nError: %2")
                                  .arg(networkName)
                                  .arg(ex.what()));
    }
}

void NetworkTabPage::onNetworksDataUpdated(const QVariantList& networks)
{
    Q_UNUSED(networks);

    // Networks data has been updated - refresh the UI
    //qDebug() << "NetworkTabPage::onNetworksDataUpdated - Refreshing UI with" << networks.size() << "networks";
    this->refreshContent();
}

void NetworkTabPage::onCacheObjectChanged(XenConnection* connection, const QString& type, const QString& ref)
{
    Q_ASSERT(this->m_connection == connection);

    if (this->m_connection != connection)
        return;

    Q_UNUSED(ref);

    if (type == "network" || type == "pif" || type == "vif" || type == "bond" ||
        type == "network_sriov" || type == "pif_metrics")
    {
        this->refreshContent();
    }
}

// ===== Button Handlers (matches C# NetworkList button handlers) =====

QString NetworkTabPage::getSelectedVifRef() const
{
    QList<QTableWidgetItem*> items = this->ui->networksTable->selectedItems();
    if (items.isEmpty())
        return QString();

    int row = items.first()->row();
    // Store VIF ref as hidden data in first column
    QTableWidgetItem* item = this->ui->networksTable->item(row, 0);
    if (item)
        return item->data(Qt::UserRole).toString();

    return QString();
}

QSharedPointer<VIF> NetworkTabPage::getSelectedVif() const
{
    if (!this->m_connection)
        return QSharedPointer<VIF>();

    return this->m_connection->GetCache()->ResolveObject<VIF>("vif", this->getSelectedVifRef());
}

void NetworkTabPage::updateButtonStates()
{
    // Match C# NetworkList::UpdateEnablement()

    if (this->m_objectType == "vm")
    {
        QSharedPointer<VIF> vif = this->getSelectedVif();
        bool hasSelection = !vif.isNull() && vif->IsValid();
        bool locked = hasSelection && vif->IsLocked();

        this->ui->addButton->setEnabled(!locked);

        if (hasSelection)
        {
            bool currentlyAttached = vif->CurrentlyAttached();
            QStringList allowedOps = vif->AllowedOperations();

            // Check if unplug or plug is allowed
            bool canUnplug = false;
            bool canPlug = false;
            for (const QString& opStr : allowedOps)
            {
                if (opStr == "unplug")
                    canUnplug = true;
                if (opStr == "plug")
                    canPlug = true;
            }

            // C#: RemoveNetworkButton.Enabled = !locked && (vif.allowed_operations.Contains(vif_operations.unplug) || !vif.currently_attached);
            this->ui->removeButton->setEnabled(!locked && (canUnplug || !currentlyAttached));
            this->canEnterPropertiesWindow = !locked && (canUnplug || !currentlyAttached);
            this->ui->propertiesButton->setEnabled(this->canEnterPropertiesWindow);

            // C#: buttonActivateToggle.Enabled = !locked && (currently_attached && canUnplug || !currently_attached && canPlug)
            this->ui->activateButton->setEnabled(!locked && ((currentlyAttached && canUnplug) || (!currentlyAttached && canPlug)));

            // Update button text based on state
            this->ui->activateButton->setText(currentlyAttached ? tr("Deacti&vate") : tr("Acti&vate"));
        } else
        {
            this->ui->removeButton->setEnabled(false);
            this->ui->propertiesButton->setEnabled(false);
            this->ui->activateButton->setEnabled(false);
        }

        // Show/hide activate button for VMs only
        this->ui->activateButton->setVisible(true);
        this->ui->separator->setVisible(true);
    } else
    {
        // For hosts/pools - hide activate button
        this->ui->activateButton->setVisible(false);
        this->ui->separator->setVisible(false);

        QString networkRef = getSelectedNetworkRef();
        bool hasSelection = !networkRef.isEmpty();
        bool locked = this->m_objectData.value("Locked", false).toBool();

        this->ui->addButton->setEnabled(!locked);
        this->ui->removeButton->setEnabled(hasSelection && !locked);
        this->canEnterPropertiesWindow = hasSelection && !locked;
        this->ui->propertiesButton->setEnabled(hasSelection && !locked);
    }
}

// ===== PIF/Network Helper Methods (matching C# XenAPI extension methods) =====

QString NetworkTabPage::getPifNetworkSriov(const QSharedPointer<PIF>& pif) const
{
    // Matches C# PIF.NetworkSriov()
    // Returns XenRef<Network_sriov> if this PIF has SR-IOV

    if (!pif || !this->m_connection || !this->m_connection->GetCache())
        return QString();

    // Check if this is an SR-IOV logical PIF
    QStringList sriovLogicalPifOf = pif->SriovLogicalPIFOfRefs();
    if (!sriovLogicalPifOf.isEmpty())
        return sriovLogicalPifOf.first();

    // Check if this is a VLAN on an SR-IOV network
    if (!pif->IsVLAN())
        return QString(); // Not a VLAN

    // Resolve VLAN to get tagged_PIF
    QString vlanMasterOf = pif->VLANMasterOfRef();
    if (vlanMasterOf.isEmpty())
        return QString();

    QSharedPointer<VLAN> vlan = this->m_connection->GetCache()->ResolveObject<VLAN>("vlan", vlanMasterOf);
    QSharedPointer<PIF> taggedPif = vlan ? vlan->GetTaggedPIF() : QSharedPointer<PIF>();

    if (!taggedPif || !taggedPif->IsValid())
        return QString();

    // Check if tagged PIF is SR-IOV logical PIF
    QStringList taggedSriovLogicalPifOf = taggedPif->SriovLogicalPIFOfRefs();
    if (!taggedSriovLogicalPifOf.isEmpty())
        return taggedSriovLogicalPifOf.first();

    return QString();
}
