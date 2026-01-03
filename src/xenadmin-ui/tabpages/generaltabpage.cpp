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

#include "generaltabpage.h"
#include "ui_generaltabpage.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/vm.h"
#include <QLabel>
#include <QDebug>
#include <QDateTime>

GeneralTabPage::GeneralTabPage(QWidget* parent) : BaseTabPage(parent), ui(new Ui::GeneralTabPage)
{
    this->ui->setupUi(this);

    // Initialize layout pointers from UI file
    this->m_generalLayout = this->ui->generalGroup->findChild<QFormLayout*>("generalLayout");
    this->m_biosLayout = this->ui->biosGroup->findChild<QFormLayout*>("biosLayout");
    this->m_managementInterfacesLayout = this->ui->managementInterfacesGroup->findChild<QFormLayout*>("managementInterfacesLayout");
    this->m_memoryLayout = this->ui->memoryGroup->findChild<QFormLayout*>("memoryLayout");
    this->m_cpuLayout = this->ui->cpuGroup->findChild<QFormLayout*>("cpuLayout");
    this->m_versionLayout = this->ui->versionGroup->findChild<QFormLayout*>("versionLayout");
    this->m_statusLayout = this->ui->statusGroup->findChild<QFormLayout*>("statusLayout");
    this->m_multipathingLayout = this->ui->multipathingGroup->findChild<QFormLayout*>("multipathingLayout");

    // Initially hide all sections (will be shown when populated)
    this->ui->generalGroup->setVisible(false);
    this->ui->biosGroup->setVisible(false);
    this->ui->managementInterfacesGroup->setVisible(false);
    this->ui->memoryGroup->setVisible(false);
    this->ui->cpuGroup->setVisible(false);
    this->ui->versionGroup->setVisible(false);
    this->ui->statusGroup->setVisible(false);
    this->ui->multipathingGroup->setVisible(false);
}

GeneralTabPage::~GeneralTabPage()
{
    delete this->ui;
}

bool GeneralTabPage::IsApplicableForObjectType(const QString& objectType) const
{
    // General tab is applicable to all object types
    Q_UNUSED(objectType);
    return true;
}

void GeneralTabPage::refreshContent()
{
    if (this->m_objectData.isEmpty())
    {
        this->ui->nameValue->setText("N/A");
        this->ui->descriptionValue->setText("N/A");
        this->ui->uuidValue->setText("N/A");
        this->clearProperties();
        return;
    }

    // Set basic information
    this->ui->nameValue->setText(this->m_objectData.value("name_label", "N/A").toString());
    this->ui->descriptionValue->setText(this->m_objectData.value("name_description", "N/A").toString());
    this->ui->uuidValue->setText(this->m_objectData.value("uuid", "N/A").toString());

    // Clear previous properties
    this->clearProperties();

    // Add type-specific properties
    if (this->m_objectType == "vm")
    {
        this->populateVMProperties();
    } else if (this->m_objectType == "host")
    {
        this->populateHostProperties();
    } else if (this->m_objectType == "pool")
    {
        this->populatePoolProperties();
    } else if (this->m_objectType == "sr")
    {
        this->populateSRProperties();
    } else if (this->m_objectType == "network")
    {
        this->populateNetworkProperties();
    }
}

void GeneralTabPage::clearProperties()
{
    // Clear all section layouts
    QList<QFormLayout*> layouts = {
        m_generalLayout,
        m_biosLayout,
        m_managementInterfacesLayout,
        m_memoryLayout,
        m_cpuLayout,
        m_versionLayout,
        m_statusLayout,
        m_multipathingLayout};

    for (QFormLayout* layout : layouts)
    {
        if (!layout)
            continue;

        // Remove all dynamically added widgets
        while (layout->count() > 0)
        {
            QLayoutItem* item = layout->takeAt(0);
            if (item->widget())
            {
                delete item->widget();
            }
            delete item;
        }
    }

    // Hide all sections initially
    this->ui->generalGroup->setVisible(false);
    this->ui->biosGroup->setVisible(false);
    this->ui->managementInterfacesGroup->setVisible(false);
    this->ui->memoryGroup->setVisible(false);
    this->ui->cpuGroup->setVisible(false);
    this->ui->versionGroup->setVisible(false);
    this->ui->statusGroup->setVisible(false);
    this->ui->multipathingGroup->setVisible(false);
}

void GeneralTabPage::addProperty(const QString& label, const QString& value)
{
    // Legacy method - kept for VM/Pool/SR/Network types that don't use sections yet
    if (!this->m_generalLayout)
        return;

    QLabel* labelWidget = new QLabel(label + ":");
    QFont labelFont = labelWidget->font();
    labelFont.setBold(true);
    labelWidget->setFont(labelFont);

    QLabel* valueWidget = new QLabel(value);
    valueWidget->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextSelectableByMouse);
    valueWidget->setWordWrap(true);

    this->m_generalLayout->addRow(labelWidget, valueWidget);
    this->ui->generalGroup->setVisible(true);
}

void GeneralTabPage::addPropertyToLayout(QFormLayout* layout, const QString& label, const QString& value)
{
    if (!layout)
        return;

    QLabel* labelWidget = new QLabel(label + ":");
    QFont labelFont = labelWidget->font();
    labelFont.setBold(true);
    labelWidget->setFont(labelFont);

    QLabel* valueWidget = new QLabel(value);
    valueWidget->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextSelectableByMouse);
    valueWidget->setWordWrap(true);

    layout->addRow(labelWidget, valueWidget);
}

void GeneralTabPage::populateVMProperties()
{
    // VM-specific properties - comprehensive implementation matching C# GenerateGeneralBox
    // C# Reference: xenadmin/XenAdmin/TabPages/GeneralTabPage.cs lines 943-1095

    bool isTemplate = this->m_objectData.value("is_a_template", false).toBool();
    bool isSnapshot = this->m_objectData.value("is_a_snapshot", false).toBool();
    //bool isControlDomain = this->m_objectData.value("is_control_domain", false).toBool();

    // OS Name / Guest Operating System
    if (this->m_objectData.contains("guest_metrics") && this->m_connection)
    {
        QString guestMetricsRef = this->m_objectData.value("guest_metrics").toString();
        if (!guestMetricsRef.isEmpty() && guestMetricsRef != "OpaqueRef:NULL")
        {
            QVariantMap guestMetrics = this->m_connection->GetCache()->ResolveObjectData("vm_guest_metrics", guestMetricsRef);
            if (!guestMetrics.isEmpty())
            {
                QVariantMap osVersion = guestMetrics.value("os_version").toMap();
                QString osName = osVersion.value("name").toString();
                if (osName.isEmpty())
                    osName = "Unknown";
                this->addProperty("Operating system", osName);
            }
        }
    }

    // Operating Mode (HVM vs PV)
    if (this->m_objectData.contains("HVM_boot_policy"))
    {
        QString bootPolicy = this->m_objectData.value("HVM_boot_policy").toString();
        bool isHVM = !bootPolicy.isEmpty();
        this->addProperty("Operating mode", isHVM ? "HVM" : "Paravirtualized");
    }

    // BIOS strings copied (for templates) - C# line 1052
    if (isTemplate && this->m_objectData.contains("bios_strings"))
    {
        QVariantMap biosStrings = this->m_objectData.value("bios_strings").toMap();
        bool biosStringsCopied = !biosStrings.isEmpty() && biosStrings.contains("bios-vendor");
        this->addProperty("BIOS strings copied", biosStringsCopied ? "Yes" : "No");
    }

    // vApp / VM Appliance - C# lines 1056-1065
    if (this->m_objectData.contains("appliance") && this->m_connection)
    {
        QString applianceRef = this->m_objectData.value("appliance").toString();
        if (!applianceRef.isEmpty() && applianceRef != "OpaqueRef:NULL")
        {
            QVariantMap appliance = this->m_connection->GetCache()->ResolveObjectData("vm_appliance", applianceRef);
            if (!appliance.isEmpty())
            {
                QString applianceName = appliance.value("name_label", "Unknown").toString();
                this->addProperty("vApp", applianceName);
            }
        }
    }

    // Snapshot information - C# lines 1067-1070
    if (isSnapshot)
    {
        if (this->m_objectData.contains("snapshot_of") && this->m_connection)
        {
            QString snapshotOfRef = this->m_objectData.value("snapshot_of").toString();
            if (!snapshotOfRef.isEmpty())
            {
                QVariantMap snapshotOfVM = this->m_connection->GetCache()->ResolveObjectData("vm", snapshotOfRef);
                if (!snapshotOfVM.isEmpty())
                {
                    QString vmName = snapshotOfVM.value("name_label", "Unknown").toString();
                    this->addProperty("Snapshot of", vmName);
                }
            }
        }

        if (this->m_objectData.contains("snapshot_time"))
        {
            QString snapshotTimeStr = this->m_objectData.value("snapshot_time").toString();
            QDateTime snapshotTime = QDateTime::fromString(snapshotTimeStr, Qt::ISODate);
            if (snapshotTime.isValid())
            {
                this->addProperty("Creation time", snapshotTime.toLocalTime().toString("dd/MM/yyyy HH:mm:ss"));
            }
        }
    }

    // Properties for running VMs (not templates)
    if (!isTemplate)
    {
        // Virtualization Status - C# lines 1066 GenerateVirtualisationStatusForGeneralBox
        QString powerState = this->m_objectData.value("power_state", "unknown").toString();
        if (powerState == "Running" && this->m_objectData.contains("guest_metrics") && this->m_connection)
        {
            QString guestMetricsRef = this->m_objectData.value("guest_metrics").toString();
            if (!guestMetricsRef.isEmpty() && guestMetricsRef != "OpaqueRef:NULL")
            {
                QVariantMap guestMetrics = this->m_connection->GetCache()->ResolveObjectData("vm_guest_metrics", guestMetricsRef);
                if (!guestMetrics.isEmpty())
                {
                    QVariantMap pvDriversVersion = guestMetrics.value("PV_drivers_version").toMap();
                    QVariantMap osVersion = guestMetrics.value("os_version").toMap();

                    QString virtStatus;
                    bool hasIODrivers = pvDriversVersion.contains("major");
                    bool hasManagementAgent = osVersion.contains("major");
                    bool hasVendorDevice = this->m_objectData.value("has_vendor_device", false).toBool();

                    QStringList statusLines;

                    // I/O Drivers
                    if (hasIODrivers)
                        statusLines << "I/O drivers: optimized";
                    else
                        statusLines << "I/O drivers: not optimized";

                    // Management Agent
                    if (hasManagementAgent)
                        statusLines << "Management agent: installed";
                    else
                        statusLines << "Management agent: not installed";

                    // Windows Update / vendor device
                    if (hasVendorDevice)
                        statusLines << "Receiving Windows Update";
                    else
                        statusLines << "Not receiving Windows Update";

                    virtStatus = statusLines.join("\n");
                    this->addProperty("Virtualization state", virtStatus);
                }
            }
        }

        // VM Uptime - C# line 1069 vm.RunningTime()
        if (powerState == "Running" && this->m_objectData.contains("metrics") && this->m_connection)
        {
            QString metricsRef = this->m_objectData.value("metrics").toString();
            if (!metricsRef.isEmpty())
            {
                QVariantMap metrics = this->m_connection->GetCache()->ResolveObjectData("vm_metrics", metricsRef);
                if (!metrics.isEmpty() && metrics.contains("start_time"))
                {
                    QString startTimeStr = metrics.value("start_time").toString();
                    QDateTime startTime = QDateTime::fromString(startTimeStr, Qt::ISODate);
                    if (startTime.isValid())
                    {
                        qint64 uptimeSeconds = startTime.secsTo(QDateTime::currentDateTimeUtc());
                        if (uptimeSeconds > 0)
                        {
                            int days = uptimeSeconds / 86400;
                            int hours = (uptimeSeconds % 86400) / 3600;
                            int minutes = (uptimeSeconds % 3600) / 60;

                            QString uptimeStr;
                            if (days > 0)
                                uptimeStr = QString("%1 days, %2 hours, %3 minutes").arg(days).arg(hours).arg(minutes);
                            else if (hours > 0)
                                uptimeStr = QString("%1 hours, %2 minutes").arg(hours).arg(minutes);
                            else
                                uptimeStr = QString("%1 minutes").arg(minutes);

                            this->addProperty("Uptime", uptimeStr);
                        }
                    }
                }
            }
        }

        // P2V Source information - C# lines 1072-1075
        if (this->m_objectData.contains("other_config"))
        {
            QVariantMap otherConfig = this->m_objectData.value("other_config").toMap();

            // P2V source machine
            if (otherConfig.contains("p2v_source_machine"))
            {
                QString sourceMachine = otherConfig.value("p2v_source_machine").toString();
                this->addProperty("P2V source machine", sourceMachine);
            }

            // P2V import date
            if (otherConfig.contains("p2v_import_date"))
            {
                QString importDate = otherConfig.value("p2v_import_date").toString();
                QDateTime dt = QDateTime::fromString(importDate, Qt::ISODate);
                if (dt.isValid())
                {
                    this->addProperty("P2V import date", dt.toLocalTime().toString("dd/MM/yyyy HH:mm:ss"));
                }
            }
        }

        // Home Server / Affinity - C# lines 1078-1083 (if WLB not enabled)
        // Show if VM can choose home server (has affinity field and is not managed by WLB)
        if (this->m_objectData.contains("affinity") && this->m_connection)
        {
            QString affinityRef = this->m_objectData.value("affinity").toString();
            QString affinityDisplay = "None";

            if (!affinityRef.isEmpty() && affinityRef != "OpaqueRef:NULL")
            {
                QVariantMap affinityHost = this->m_connection->GetCache()->ResolveObjectData("host", affinityRef);
                if (!affinityHost.isEmpty())
                {
                    affinityDisplay = affinityHost.value("name_label", "Unknown").toString();
                }
            }

            this->addProperty("Home server", affinityDisplay);
        }
    }

    // Memory (for all VM types)
    if (this->m_objectData.contains("memory_dynamic_max"))
    {
        qint64 memoryBytes = this->m_objectData.value("memory_dynamic_max").toLongLong();
        double memoryMB = memoryBytes / (1024.0 * 1024.0);
        double memoryGB = memoryBytes / (1024.0 * 1024.0 * 1024.0);

        QString memoryStr;
        if (memoryGB >= 1.0)
            memoryStr = QString("%1 GB").arg(memoryGB, 0, 'f', 2);
        else
            memoryStr = QString("%1 MB").arg(memoryMB, 0, 'f', 0);

        this->addProperty("Memory", memoryStr);
    }

    // vCPUs - C# GenerateVCPUsBox lines 851-861
    if (this->m_objectData.contains("VCPUs_at_startup"))
    {
        int vcpusAtStartup = this->m_objectData.value("VCPUs_at_startup").toInt();
        qint64 vcpusMax = this->m_objectData.value("VCPUs_max", vcpusAtStartup).toLongLong();

        this->addProperty("vCPUs at startup", QString::number(vcpusAtStartup));

        // Show max vCPUs if different from startup or if hotplug is supported
        // For now, show if different (hotplug detection would require more context)
        if (vcpusMax != vcpusAtStartup)
        {
            this->addProperty("vCPUs maximum", QString::number(vcpusMax));
        }

        // Topology (matches C# VM.Topology())
        qint64 coresPerSocket = 1;
        QVariantMap platform = this->m_objectData.value("platform").toMap();
        if (platform.contains("cores-per-socket"))
        {
            bool ok = false;
            qint64 parsed = platform.value("cores-per-socket").toString().toLongLong(&ok);
            if (ok && parsed > 0)
                coresPerSocket = parsed;
        }

        QString topologyWarning = VM::ValidVCPUConfiguration(vcpusMax, coresPerSocket);
        qint64 sockets = topologyWarning.isEmpty() && coresPerSocket > 0
            ? (vcpusMax / coresPerSocket)
            : 0;
        this->addProperty("Topology", VM::GetTopology(sockets, coresPerSocket));
    }
}

void GeneralTabPage::populateHostProperties()
{
    // Host-specific properties organized into sections
    // C# Reference: xenadmin/XenAdmin/TabPages/GeneralTabPage.cs lines 953-1032

    populateGeneralSection();
    populateBIOSSection();
    populateManagementInterfacesSection();
    populateMemorySection();
    populateCPUSection();
    populateVersionSection();
}

void GeneralTabPage::populatePoolProperties()
{
    // Pool-specific properties
    if (this->m_objectData.contains("master"))
    {
        this->addProperty("Master", this->m_objectData.value("master").toString());
    }

    if (this->m_objectData.contains("default_SR"))
    {
        this->addProperty("Default SR", this->m_objectData.value("default_SR").toString());
    }

    if (this->m_objectData.contains("ha_enabled"))
    {
        bool haEnabled = this->m_objectData.value("ha_enabled").toBool();
        this->addProperty("HA Enabled", haEnabled ? "Yes" : "No");
    }
}

void GeneralTabPage::populateSRProperties()
{
    // Storage Repository specific properties
    // C# Reference: GeneralTabPage.cs lines 360-390
    // Calls GenerateStatusBox() and GenerateMultipathBox() for SR

    if (this->m_objectData.contains("type"))
    {
        this->addProperty("Type", this->m_objectData.value("type").toString());
    }

    if (this->m_objectData.contains("physical_size"))
    {
        qint64 sizeBytes = this->m_objectData.value("physical_size").toLongLong();
        double sizeGB = sizeBytes / (1024.0 * 1024.0 * 1024.0);
        this->addProperty("Total Size", QString("%1 GB").arg(sizeGB, 0, 'f', 2));
    }

    if (this->m_objectData.contains("physical_utilisation"))
    {
        qint64 usedBytes = this->m_objectData.value("physical_utilisation").toLongLong();
        double usedGB = usedBytes / (1024.0 * 1024.0 * 1024.0);
        this->addProperty("Used Space", QString("%1 GB").arg(usedGB, 0, 'f', 2));
    }

    if (this->m_objectData.contains("shared"))
    {
        bool shared = this->m_objectData.value("shared").toBool();
        this->addProperty("Shared", shared ? "Yes" : "No");
    }

    // Populate SR-specific sections (Status and Multipathing)
    this->populateStatusSection();
    this->populateMultipathingSection();
}

void GeneralTabPage::populateNetworkProperties()
{
    // Network-specific properties
    if (this->m_objectData.contains("bridge"))
    {
        this->addProperty("Bridge", this->m_objectData.value("bridge").toString());
    }

    if (this->m_objectData.contains("MTU"))
    {
        int mtu = this->m_objectData.value("MTU").toInt();
        this->addProperty("MTU", QString::number(mtu));
    }

    if (this->m_objectData.contains("managed"))
    {
        bool managed = this->m_objectData.value("managed").toBool();
        this->addProperty("Managed", managed ? "Yes" : "No");
    }
}

// === Host Section Population Methods ===

void GeneralTabPage::populateGeneralSection()
{
    // General Section: Address, Hostname, Pool Coordinator, Enabled, iSCSI IQN, Uptime
    // C# Reference: GenerateGeneralBox lines 953-1032

    if (this->m_objectData.contains("address"))
    {
        this->addPropertyToLayout(m_generalLayout, "Address", this->m_objectData.value("address").toString());
    }

    if (this->m_objectData.contains("hostname"))
    {
        this->addPropertyToLayout(m_generalLayout, "Hostname", this->m_objectData.value("hostname").toString());
    }

    // Pool master status (shown as "Pool Coordinator" in C# but displays as "Pool master: Yes/No" in UI)
    // C# Reference: line 956-958 - only shown if not standalone host
    // C# logic: isStandAloneHost = Helpers.GetPool(connection) == null
    // GetPool returns null if Pool.IsVisible() is false
    // Pool.IsVisible() returns false when name_label == "" AND HostCount <= 1
    // So we show "Pool master" if the pool has a name OR has multiple hosts
    if (this->m_objectData.contains("pool") && this->m_connection && this->m_objectData.contains("is_pool_coordinator"))
    {
        QString poolRef = this->m_objectData.value("pool").toString();
        if (!poolRef.isEmpty())
        {
            QVariantMap pool = this->m_connection->GetCache()->ResolveObjectData("pool", poolRef);
            if (!pool.isEmpty())
            {
                // Check if pool is "visible" (not a standalone host pool)
                QString poolName = pool.value("name_label").toString();
                bool hasPoolName = !poolName.isEmpty();

                // Count hosts in pool (check if there are multiple hosts)
                bool hasMultipleHosts = false;
                if (pool.contains("HostCount"))
                {
                    int hostCount = pool.value("HostCount").toInt();
                    hasMultipleHosts = (hostCount > 1);
                }

                // Show "Pool master" if pool is visible (has name or multiple hosts)
                if (hasPoolName || hasMultipleHosts)
                {
                    bool isCoordinator = this->m_objectData.value("is_pool_coordinator").toBool();
                    this->addPropertyToLayout(m_generalLayout, "Pool master", isCoordinator ? "Yes" : "No");
                }
            }
        }
    }

    // Enabled status with maintenance mode detection
    if (this->m_objectData.contains("enabled"))
    {
        bool enabled = this->m_objectData.value("enabled").toBool();
        bool isLive = this->m_objectData.value("is_live", true).toBool();

        QString enabledStr;
        if (!isLive)
            enabledStr = "Host not live";
        else if (!enabled)
            enabledStr = "Disabled (Maintenance Mode)";
        else
            enabledStr = "Yes";

        this->addPropertyToLayout(m_generalLayout, "Enabled", enabledStr);
    }

    // Autoboot of VMs enabled
    // C# Reference: line 980 - host.GetVmAutostartEnabled()
    if (this->m_objectData.contains("other_config"))
    {
        QVariantMap otherConfig = this->m_objectData.value("other_config").toMap();
        // GetVmAutostartEnabled checks other_config["auto_poweron"] == "true"
        bool autoPowerOn = (otherConfig.value("auto_poweron").toString() == "true");
        this->addPropertyToLayout(m_generalLayout, "Autoboot of VMs enabled", autoPowerOn ? "Yes" : "No");
    }

    // Log destination
    // C# Reference: lines 1011-1017 - host.GetSysLogDestination() from logging["syslog_destination"]
    if (this->m_objectData.contains("logging"))
    {
        QVariantMap logging = this->m_objectData.value("logging").toMap();
        QString syslogDest = logging.value("syslog_destination").toString();

        QString logDisplay;
        if (syslogDest.isEmpty())
            logDisplay = "Local";
        else
            logDisplay = QString("Local and %1").arg(syslogDest);

        this->addPropertyToLayout(m_generalLayout, "Log destination", logDisplay);
    }

    // Server Uptime (calculated from boot time in host_metrics)
    // C# Reference: Host.Uptime() lines 853-859
    if (this->m_objectData.contains("metrics") && this->m_connection)
    {
        QString metricsRef = this->m_objectData.value("metrics").toString();
        if (!metricsRef.isEmpty())
        {
            QVariantMap metrics = this->m_connection->GetCache()->ResolveObjectData("host_metrics", metricsRef);
            if (!metrics.isEmpty() && metrics.contains("start_time"))
            {
                QString startTimeStr = metrics.value("start_time").toString();
                QDateTime startTime = QDateTime::fromString(startTimeStr, Qt::ISODate);
                if (startTime.isValid())
                {
                    qint64 uptimeSeconds = startTime.secsTo(QDateTime::currentDateTimeUtc());
                    if (uptimeSeconds > 0)
                    {
                        this->addPropertyToLayout(m_generalLayout, "Server Uptime", formatUptime(uptimeSeconds));
                    }
                }
            }
        }
    }

    // Toolstack Uptime (xapi agent uptime from other_config.agent_start_time)
    // C# Reference: Host.AgentUptime() lines 885-891
    if (this->m_objectData.contains("other_config"))
    {
        QVariantMap otherConfig = this->m_objectData.value("other_config").toMap();
        if (otherConfig.contains("agent_start_time"))
        {
            bool ok = false;
            double agentStartTime = otherConfig.value("agent_start_time").toDouble(&ok);
            if (ok && agentStartTime > 0)
            {
                QDateTime startTime = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(agentStartTime), Qt::UTC);
                qint64 uptimeSeconds = startTime.secsTo(QDateTime::currentDateTimeUtc());
                if (uptimeSeconds > 0)
                {
                    this->addPropertyToLayout(m_generalLayout, "Toolstack Uptime", formatUptime(uptimeSeconds));
                }
            }
        }
    }

    // iSCSI IQN
    if (this->m_objectData.contains("iscsi_iqn"))
    {
        QString iscsiIqn = this->m_objectData.value("iscsi_iqn").toString();
        if (!iscsiIqn.isEmpty())
            this->addPropertyToLayout(m_generalLayout, "iSCSI IQN", iscsiIqn);
    }

    // Show section if it has content
    if (m_generalLayout && m_generalLayout->count() > 0)
        this->ui->generalGroup->setVisible(true);
}

void GeneralTabPage::populateBIOSSection()
{
    // BIOS Information Section
    // C# Reference: bios_strings field

    if (!this->m_objectData.contains("bios_strings"))
        return;

    QVariantMap biosStrings = this->m_objectData.value("bios_strings").toMap();

    if (biosStrings.contains("bios-vendor"))
    {
        QString biosVendor = biosStrings.value("bios-vendor").toString();
        if (!biosVendor.isEmpty())
            this->addPropertyToLayout(m_biosLayout, "Vendor", biosVendor);
    }

    if (biosStrings.contains("bios-version"))
    {
        QString biosVersion = biosStrings.value("bios-version").toString();
        if (!biosVersion.isEmpty())
            this->addPropertyToLayout(m_biosLayout, "Version", biosVersion);
    }

    if (biosStrings.contains("system-manufacturer"))
    {
        QString sysManufacturer = biosStrings.value("system-manufacturer").toString();
        if (!sysManufacturer.isEmpty())
            this->addPropertyToLayout(m_biosLayout, "Manufacturer", sysManufacturer);
    }

    if (biosStrings.contains("system-product-name"))
    {
        QString sysProduct = biosStrings.value("system-product-name").toString();
        if (!sysProduct.isEmpty())
            this->addPropertyToLayout(m_biosLayout, "Product", sysProduct);
    }

    // Show section if it has content
    if (m_biosLayout && m_biosLayout->count() > 0)
        this->ui->biosGroup->setVisible(true);
}

void GeneralTabPage::populateManagementInterfacesSection()
{
    // Management Interfaces Section
    // C# Reference: GenerateInterfaceBox lines 396-461 (fillInterfacesForHost)

    if (!this->m_objectData.contains("PIFs") || !this->m_connection)
        return;

    QVariantList pifRefs = this->m_objectData.value("PIFs").toList();

    // Resolve each PIF reference from cache
    for (const QVariant& pifRefVar : pifRefs)
    {
        QString pifRef = pifRefVar.toString();
        if (pifRef.isEmpty())
            continue;

        QVariantMap pif = this->m_connection->GetCache()->ResolveObjectData("PIF", pifRef);
        if (pif.isEmpty())
            continue;

        // Check if this is a management interface
        bool isManagement = pif.value("management", false).toBool();
        if (isManagement)
        {
            QString ipAddress = pif.value("IP", "").toString();
            QString device = pif.value("device", "").toString();

            if (!ipAddress.isEmpty())
            {
                QString label = device.isEmpty() ? "IP Address" : device;
                this->addPropertyToLayout(m_managementInterfacesLayout, label, ipAddress);
            }
        }
    }

    // Show section if it has content
    if (m_managementInterfacesLayout && m_managementInterfacesLayout->count() > 0)
        this->ui->managementInterfacesGroup->setVisible(true);
}

void GeneralTabPage::populateMemorySection()
{
    // Memory Section: Server, VMs, XCP-ng/Xen
    // C# Reference: GenerateMemoryBox lines 1380-1392
    // Labels use FriendlyName("host.ServerMemory"), FriendlyName("host.VMMemory"), FriendlyName("host.XenMemory")
    // which resolve to "Server", "VMs", and "XCP-ng" (or "Xen")

    // Server Memory: Shows "X GB free of Y GB total"
    // C# Reference: Host.HostMemoryString() lines 1269-1281
    if (this->m_objectData.contains("memory_total") && this->m_connection)
    {
        qint64 memTotal = this->m_objectData.value("memory_total").toLongLong();

        // Try to get memory_free from host_metrics
        QString metricsRef = this->m_objectData.value("metrics").toString();
        if (!metricsRef.isEmpty())
        {
            QVariantMap metrics = this->m_connection->GetCache()->ResolveObjectData("host_metrics", metricsRef);
            if (!metrics.isEmpty() && metrics.contains("memory_free"))
            {
                qint64 memFree = metrics.value("memory_free").toLongLong();
                double memFreeGB = memFree / (1024.0 * 1024.0 * 1024.0);
                double memTotalGB = memTotal / (1024.0 * 1024.0 * 1024.0);

                QString serverMemStr = QString("%1 GB free of %2 GB total")
                                           .arg(memFreeGB, 0, 'f', 2)
                                           .arg(memTotalGB, 0, 'f', 2);
                this->addPropertyToLayout(m_memoryLayout, "Server", serverMemStr);
            }
        }
    }

    // VMs: Shows memory used by VMs
    // C# Reference: Host.ResidentVMMemoryUsageString() shows each VM's memory on separate line
    // Simplified version: Just show total VM memory used
    if (this->m_objectData.contains("memory_free"))
    {
        qint64 memTotal = this->m_objectData.value("memory_total", 0).toLongLong();
        qint64 memFree = this->m_objectData.value("memory_free").toLongLong();
        qint64 memUsed = memTotal - memFree;
        double memUsedGB = memUsed / (1024.0 * 1024.0 * 1024.0);
        this->addPropertyToLayout(m_memoryLayout, "VMs", QString("%1 GB").arg(memUsedGB, 0, 'f', 2));
    }

    // XCP-ng/Xen Memory overhead
    // C# Reference: Host.XenMemoryString() lines 1286-1294
    if (this->m_objectData.contains("memory_overhead"))
    {
        qint64 memOverhead = this->m_objectData.value("memory_overhead").toLongLong();
        double memOverheadMB = memOverhead / (1024.0 * 1024.0);
        // Use "XCP-ng" as label (C# uses product brand name)
        this->addPropertyToLayout(m_memoryLayout, "XCP-ng", QString("%1 MB").arg(memOverheadMB, 0, 'f', 0));
    }

    // Show section if it has content
    if (m_memoryLayout && m_memoryLayout->count() > 0)
        this->ui->memoryGroup->setVisible(true);
}

void GeneralTabPage::populateCPUSection()
{
    // CPU Section: CPU Count, Model, Speed, Vendor
    // C# Reference: GenerateCPUBox lines 826-857

    if (!this->m_objectData.contains("cpu_info"))
        return;

    QVariantMap cpuInfo = this->m_objectData.value("cpu_info").toMap();

    if (cpuInfo.contains("cpu_count"))
    {
        int cpuCount = cpuInfo.value("cpu_count").toInt();
        this->addPropertyToLayout(m_cpuLayout, "Count", QString::number(cpuCount));
    }

    if (cpuInfo.contains("modelname"))
    {
        QString cpuModel = cpuInfo.value("modelname").toString();
        if (!cpuModel.isEmpty())
            this->addPropertyToLayout(m_cpuLayout, "Model", cpuModel);
    }

    if (cpuInfo.contains("speed"))
    {
        QString cpuSpeed = cpuInfo.value("speed").toString();
        if (!cpuSpeed.isEmpty())
            this->addPropertyToLayout(m_cpuLayout, "Speed", cpuSpeed + " MHz");
    }

    if (cpuInfo.contains("vendor"))
    {
        QString cpuVendor = cpuInfo.value("vendor").toString();
        if (!cpuVendor.isEmpty())
            this->addPropertyToLayout(m_cpuLayout, "Vendor", cpuVendor);
    }

    // Show section if it has content
    if (m_cpuLayout && m_cpuLayout->count() > 0)
        this->ui->cpuGroup->setVisible(true);
}

void GeneralTabPage::populateVersionSection()
{
    // Software Version Section: Product Version, Build Date, Build Number, DBV
    // C# Reference: GenerateVersionBox lines 784-825

    if (!this->m_objectData.contains("software_version"))
        return;

    QVariantMap swVersion = this->m_objectData.value("software_version").toMap();

    // Product version (most important)
    if (swVersion.contains("product_version"))
    {
        QString productVersion = swVersion.value("product_version").toString();
        QString productBrand = swVersion.value("product_brand", "XenServer").toString();
        this->addPropertyToLayout(m_versionLayout, "Product Version", QString("%1 %2").arg(productBrand, productVersion));
    }

    // Build date
    if (swVersion.contains("date"))
    {
        QString buildDate = swVersion.value("date").toString();
        this->addPropertyToLayout(m_versionLayout, "Build Date", buildDate);
    }

    // Build number
    if (swVersion.contains("build_number"))
    {
        this->addPropertyToLayout(m_versionLayout, "Build Number", swVersion.value("build_number").toString());
    }

    // DBV (Database Version)
    if (swVersion.contains("dbv"))
    {
        this->addPropertyToLayout(m_versionLayout, "DBV", swVersion.value("dbv").toString());
    }

    // Show section if it has content
    if (m_versionLayout && m_versionLayout->count() > 0)
        this->ui->versionGroup->setVisible(true);
}

// Helper method to format uptime in human-readable format
// C# Reference: XenCenterLib/PrettyTimeSpan.cs
QString GeneralTabPage::formatUptime(qint64 seconds) const
{
    if (seconds < 0)
        return QString();

    qint64 days = seconds / 86400;
    qint64 hours = (seconds % 86400) / 3600;
    qint64 minutes = (seconds % 3600) / 60;
    qint64 secs = seconds % 60;

    QStringList parts;

    if (days > 0)
        parts << QString("%1 day%2").arg(days).arg(days > 1 ? "s" : "");
    if (hours > 0)
        parts << QString("%1 hour%2").arg(hours).arg(hours > 1 ? "s" : "");
    if (minutes > 0)
        parts << QString("%1 minute%2").arg(minutes).arg(minutes > 1 ? "s" : "");
    if (secs > 0 || parts.isEmpty())
        parts << QString("%1 second%2").arg(secs).arg(secs != 1 ? "s" : "");

    return parts.join(", ");
}

// ============================================================================
// SR Section Population Methods (Status and Multipathing)
// C# Reference: GeneralTabPage.cs GenerateStatusBox() and GenerateMultipathBox()
// Lines 505-717
// ============================================================================

void GeneralTabPage::populateStatusSection()
{
    // SR Status Section
    // C# Reference: GeneralTabPage.cs GenerateStatusBox() lines 505-588

    if (!this->m_connection || this->m_objectRef.isEmpty() || this->m_objectType != "sr")
        return;

    // Determine SR status (OK, Detached, Broken, Multipath Failure)
    bool broken = false;
    QString statusString = "OK";

    // Check if SR is detached (no PBDs currently attached)
    // C# SR.IsDetached() checks if any PBD is currently_attached
    bool hasAttachedPBD = false;
    QVariantList pbdRefs = this->m_objectData.value("PBDs").toList();

    for (const QVariant& pbdRefVar : pbdRefs)
    {
        QString pbdRef = pbdRefVar.toString();
        QVariantMap pbdData = this->m_connection->GetCache()->ResolveObjectData("pbd", pbdRef);
        if (!pbdData.isEmpty() && pbdData.value("currently_attached").toBool())
        {
            hasAttachedPBD = true;
            break;
        }
    }

    if (pbdRefs.isEmpty())
    {
        broken = true;
        statusString = "Detached (No PBDs)";
    } else if (!hasAttachedPBD)
    {
        broken = true;
        statusString = "Detached";
    } else
    {
        // Check if SR is broken (wrong number of PBDs or not all attached)
        // C# SR.IsBroken() checks: standalone = 1 PBD, pooled = PBD per host
        bool isShared = this->m_objectData.value("shared", false).toBool();
        int pbdCount = pbdRefs.size();

        // Get host count to check if we have correct PBD count for pooled SR
        int expectedPBDCount = 1; // standalone default
        if (isShared)
        {
            // For shared SR, should have PBD for each host in pool
            QList<QVariantMap> allHosts = this->m_connection->GetCache()->GetAllData("host");
            expectedPBDCount = allHosts.size();
        }

        if (pbdCount != expectedPBDCount)
        {
            broken = true;
            statusString = "Broken (Wrong PBD count)";
        } else
        {
            // Check if all PBDs are attached
            for (const QVariant& pbdRefVar : pbdRefs)
            {
                QString pbdRef = pbdRefVar.toString();
                QVariantMap pbdData = this->m_connection->GetCache()->ResolveObjectData("pbd", pbdRef);
                if (pbdData.isEmpty() || !pbdData.value("currently_attached").toBool())
                {
                    broken = true;
                    statusString = "Broken (PBD not attached)";
                    break;
                }
            }
        }

        // TODO: Check multipath status (requires sm_config parsing)
        // C# checks SR.MultipathAOK() which looks at SM config for multipath health
    }

    // Show overall status
    QLabel* stateLabel = new QLabel("State:");
    QFont labelFont = stateLabel->font();
    labelFont.setBold(true);
    stateLabel->setFont(labelFont);

    QLabel* stateValue = new QLabel(statusString);
    if (broken)
    {
        QPalette palette = stateValue->palette();
        palette.setColor(QPalette::WindowText, Qt::red);
        stateValue->setPalette(palette);
    }
    stateValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_statusLayout->addRow(stateLabel, stateValue);

    // Show per-host PBD status
    // C# iterates through all hosts and shows their PBD connection status
    bool isShared = this->m_objectData.value("shared", false).toBool();
    QList<QVariantMap> allHosts = this->m_connection->GetCache()->GetAllData("host");

    for (const QVariantMap& hostData : allHosts)
    {
        QString hostRef = hostData.value("ref").toString();
        QString hostName = hostData.value("name_label", "Unknown").toString();

        // Find PBD for this host
        QString pbdStatus = "Connected";
        QColor statusColor = Qt::black;

        bool foundPBD = false;
        for (const QVariant& pbdRefVar : pbdRefs)
        {
            QString pbdRef = pbdRefVar.toString();
            QVariantMap pbdData = this->m_connection->GetCache()->ResolveObjectData("pbd", pbdRef);

            if (pbdData.isEmpty())
                continue;

            QString pbdHost = pbdData.value("host").toString();
            if (pbdHost == hostRef)
            {
                foundPBD = true;
                bool attached = pbdData.value("currently_attached", false).toBool();

                if (attached)
                {
                    pbdStatus = "Connected";
                    statusColor = Qt::black;
                } else
                {
                    pbdStatus = "Disconnected";
                    statusColor = Qt::red;
                }
                break;
            }
        }

        if (!foundPBD)
        {
            // Shared SR missing PBD for this host
            if (isShared)
            {
                pbdStatus = "Connection missing";
                statusColor = Qt::red;
            } else
            {
                // Non-shared SR doesn't need PBD on every host
                continue;
            }
        }

        // Ellipsize host name if too long (C# uses .Ellipsise(30))
        QString displayName = hostName;
        if (displayName.length() > 30)
        {
            displayName = displayName.left(27) + "...";
        }

        QLabel* hostLabel = new QLabel(displayName + ":");
        labelFont = hostLabel->font();
        labelFont.setBold(true);
        hostLabel->setFont(labelFont);

        QLabel* hostValue = new QLabel(pbdStatus);
        if (statusColor != Qt::black)
        {
            QPalette palette = hostValue->palette();
            palette.setColor(QPalette::WindowText, statusColor);
            hostValue->setPalette(palette);
        }
        hostValue->setTextInteractionFlags(Qt::TextSelectableByMouse);

        m_statusLayout->addRow(hostLabel, hostValue);
    }

    // Show section if it has content
    if (m_statusLayout && m_statusLayout->count() > 0)
        this->ui->statusGroup->setVisible(true);
}

void GeneralTabPage::populateMultipathingSection()
{
    // SR Multipathing Section
    // C# Reference: GeneralTabPage.cs GenerateMultipathBox() lines 589-717

    if (!this->m_connection || this->m_objectRef.isEmpty() || this->m_objectType != "sr")
        return;

    // Check if SR is multipath capable
    // C# SR.MultipathCapable() checks sm_config["multipathable"] == "true"
    QVariantMap smConfig = this->m_objectData.value("sm_config").toMap();
    QString multipathable = smConfig.value("multipathable", "false").toString();
    bool isMultipathCapable = (multipathable == "true");

    QLabel* capableLabel = new QLabel("Multipath capable:");
    QFont labelFont = capableLabel->font();
    labelFont.setBold(true);
    capableLabel->setFont(labelFont);

    QLabel* capableValue = new QLabel(isMultipathCapable ? "Yes" : "No");
    capableValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_multipathingLayout->addRow(capableLabel, capableValue);

    if (!isMultipathCapable)
    {
        this->ui->multipathingGroup->setVisible(true);
        return;
    }

    // Check each host for multipath status
    // C# iterates through hosts and checks PBD multipath active status
    QVariantList pbdRefs = this->m_objectData.value("PBDs").toList();
    QList<QVariantMap> allHosts = this->m_connection->GetCache()->GetAllData("host");

    for (const QVariantMap& hostData : allHosts)
    {
        QString hostRef = hostData.value("ref").toString();
        QString hostName = hostData.value("name_label", "Unknown").toString();

        // Find PBD for this host
        QString multipathStatus = "Not active";
        QColor statusColor = Qt::black;

        for (const QVariant& pbdRefVar : pbdRefs)
        {
            QString pbdRef = pbdRefVar.toString();
            QVariantMap pbdData = this->m_connection->GetCache()->ResolveObjectData("pbd", pbdRef);

            if (pbdData.isEmpty())
                continue;

            QString pbdHost = pbdData.value("host").toString();
            if (pbdHost == hostRef)
            {
                // Check if multipath is active on this PBD
                // C# PBD.MultipathActive() checks device_config["multipathed"] == "true"
                QVariantMap deviceConfig = pbdData.value("device_config").toMap();
                QString multipathed = deviceConfig.value("multipathed", "false").toString();
                bool multipathActive = (multipathed == "true");

                if (multipathActive)
                {
                    // Parse path counts from other_config
                    // C# PBD.ParsePathCounts() extracts "multipath-current-paths" and "multipath-maximum-paths"
                    QVariantMap otherConfig = pbdData.value("other_config").toMap();
                    int currentPaths = otherConfig.value("multipath-current-paths", "0").toInt();
                    int maxPaths = otherConfig.value("multipath-maximum-paths", "0").toInt();

                    // Get iSCSI session count if applicable
                    int iscsiSessions = otherConfig.value("iscsi_sessions", "-1").toInt();

                    // Format status string: "X of Y paths active"
                    multipathStatus = QString("%1 of %2 paths active").arg(currentPaths).arg(maxPaths);

                    if (iscsiSessions > 0)
                    {
                        multipathStatus += QString(" (%1 iSCSI sessions)").arg(iscsiSessions);
                    }

                    // Red if degraded (current < max or max < iscsi sessions)
                    bool degraded = (currentPaths < maxPaths) || (iscsiSessions > 0 && maxPaths < iscsiSessions);
                    statusColor = degraded ? Qt::red : Qt::black;
                } else
                {
                    multipathStatus = "Not active";
                    statusColor = Qt::black;
                }

                break;
            }
        }

        QLabel* hostLabel = new QLabel(hostName + ":");
        labelFont = hostLabel->font();
        labelFont.setBold(true);
        hostLabel->setFont(labelFont);

        QLabel* hostValue = new QLabel(multipathStatus);
        if (statusColor != Qt::black)
        {
            QPalette palette = hostValue->palette();
            palette.setColor(QPalette::WindowText, statusColor);
            hostValue->setPalette(palette);
        }
        hostValue->setTextInteractionFlags(Qt::TextSelectableByMouse);

        m_multipathingLayout->addRow(hostLabel, hostValue);
    }

    // Show section if it has content
    if (m_multipathingLayout && m_multipathingLayout->count() > 0)
        this->ui->multipathingGroup->setVisible(true);
}
