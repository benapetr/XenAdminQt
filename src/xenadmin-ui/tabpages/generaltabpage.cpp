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
#include "xen/pbd.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/hostmetrics.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/network.h"
#include "xenlib/xen/pif.h"
#include "xenlib/xen/dockercontainer.h"
#include "xenlib/xen/vmappliance.h"
#include "xenlib/utils/misc.h"
#include "dialogs/vmpropertiesdialog.h"
#include "dialogs/hostpropertiesdialog.h"
#include "dialogs/poolpropertiesdialog.h"
#include "dialogs/storagepropertiesdialog.h"
#include "dialogs/networkpropertiesdialog.h"
#include <QDateTime>

GeneralTabPage::GeneralTabPage(QWidget* parent) : BaseTabPage(parent), ui(new Ui::GeneralTabPage)
{
    this->ui->setupUi(this);

    this->ui->pdSectionGeneral->SetSectionTitle(tr("General"));
    this->ui->pdSectionBios->SetSectionTitle(tr("BIOS Information"));
    this->ui->pdSectionCustomFields->SetSectionTitle(tr("Custom Fields"));
    this->ui->pdSectionManagementInterfaces->SetSectionTitle(tr("Management Interfaces"));
    this->ui->pdSectionMemory->SetSectionTitle(tr("Memory"));
    this->ui->pdSectionCpu->SetSectionTitle(tr("Processor"));
    this->ui->pdSectionVersion->SetSectionTitle(tr("Software Version"));
    this->ui->pdSectionBootOptions->SetSectionTitle(tr("Boot Options"));
    this->ui->pdSectionHighAvailability->SetSectionTitle(tr("High Availability"));
    this->ui->pdSectionStatus->SetSectionTitle(tr("Status"));
    this->ui->pdSectionMultipathing->SetSectionTitle(tr("Multipathing"));
    this->ui->pdSectionMultipathBoot->SetSectionTitle(tr("Multipath Boot"));
    this->ui->pdSectionVcpus->SetSectionTitle(tr("vCPUs"));
    this->ui->pdSectionDockerInfo->SetSectionTitle(tr("Docker Info"));
    this->ui->pdSectionReadCaching->SetSectionTitle(tr("Read Caching"));
    this->ui->pdSectionDeviceSecurity->SetSectionTitle(tr("Device Security"));

    //this->ui->pdSectionGeneral->Expand();

    QFont linkFont = this->ui->expandAllButton->font();
    linkFont.setUnderline(true);
    this->ui->expandAllButton->setFont(linkFont);
    this->ui->collapseAllButton->setFont(linkFont);
    this->ui->expandAllButton->setAutoRaise(true);
    this->ui->collapseAllButton->setAutoRaise(true);
    this->ui->expandAllButton->setCursor(Qt::PointingHandCursor);
    this->ui->collapseAllButton->setCursor(Qt::PointingHandCursor);

    this->propertiesAction_ = new QAction(tr("Properties"), this);
    this->connect(this->propertiesAction_, &QAction::triggered, this, &GeneralTabPage::openPropertiesDialog);

    this->sections_ = {
        this->ui->pdSectionGeneral,
        this->ui->pdSectionBios,
        this->ui->pdSectionCustomFields,
        this->ui->pdSectionManagementInterfaces,
        this->ui->pdSectionMemory,
        this->ui->pdSectionVersion,
        this->ui->pdSectionCpu,
        this->ui->pdSectionBootOptions,
        this->ui->pdSectionHighAvailability,
        this->ui->pdSectionStatus,
        this->ui->pdSectionMultipathing,
        this->ui->pdSectionMultipathBoot,
        this->ui->pdSectionVcpus,
        this->ui->pdSectionDockerInfo,
        this->ui->pdSectionReadCaching,
        this->ui->pdSectionDeviceSecurity
    };

    for (PDSection* section : this->sections_)
    {
        section->Expand();
        connect(section, &PDSection::ExpandedChanged, this, &GeneralTabPage::onSectionExpandedChanged);
    }

    connect(this->ui->expandAllButton, &QToolButton::clicked, this, &GeneralTabPage::onExpandAllClicked);
    connect(this->ui->collapseAllButton, &QToolButton::clicked, this, &GeneralTabPage::onCollapseAllClicked);
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
    if (!this->m_object)
    {
        this->clearProperties();
        return;
    }

    // Clear previous properties
    this->clearProperties();

    XenObjectType object_type = this->m_object->GetObjectType();

    QList<QAction*> propertiesMenu;
    propertiesMenu.append(this->propertiesAction_);

    const QString objectName = this->m_object->GetName().isEmpty() ? "N/A" : this->m_object->GetName();
    const QString objectDescription = this->m_object->GetDescription().isEmpty() ? "N/A" : this->m_object->GetDescription();
    this->addPropertyByKey(this->ui->pdSectionGeneral, "host.name_label",
                           objectName,
                           propertiesMenu);
    this->addPropertyByKey(this->ui->pdSectionGeneral, "host.name_description",
                           objectDescription,
                           propertiesMenu);

    QStringList tags = this->m_object->GetTags();
    QString tagsValue = tags.isEmpty() ? tr("None") : tags.join(", ");
    this->addProperty(this->ui->pdSectionGeneral, tr("Tags"), tagsValue);

    QString folderValue = this->m_object->GetFolderPath();
    if (folderValue.isEmpty())
        folderValue = tr("None");
    this->addProperty(this->ui->pdSectionGeneral, tr("Folder"), folderValue);

    // TODO: Add "View tag" and "View folder" context menu actions once search helpers are ported.
    this->addPropertyByKey(this->ui->pdSectionGeneral, "host.uuid",
                           this->m_object->GetUUID().isEmpty() ? "N/A" : this->m_object->GetUUID());

    this->populateCustomFieldsSection();

    // Add type-specific properties
    if (object_type == XenObjectType::VM)
    {
        this->populateVMProperties();
    } else if (object_type == XenObjectType::Host)
    {
        this->populateHostProperties();
    } else if (object_type == XenObjectType::Pool)
    {
        this->populatePoolProperties();
    } else if (object_type == XenObjectType::SR)
    {
        this->populateSRProperties();
    } else if (object_type == XenObjectType::Network)
    {
        this->populateNetworkProperties();
    } else if (object_type == XenObjectType::DockerContainer)
    {
        this->populateDockerInfoSection();
    }

    this->showSectionIfNotEmpty(this->ui->pdSectionGeneral);
    this->showSectionIfNotEmpty(this->ui->pdSectionBios);
    this->showSectionIfNotEmpty(this->ui->pdSectionCustomFields);
    this->showSectionIfNotEmpty(this->ui->pdSectionManagementInterfaces);
    this->showSectionIfNotEmpty(this->ui->pdSectionMemory);
    this->showSectionIfNotEmpty(this->ui->pdSectionVersion);
    this->showSectionIfNotEmpty(this->ui->pdSectionCpu);
    this->showSectionIfNotEmpty(this->ui->pdSectionBootOptions);
    this->showSectionIfNotEmpty(this->ui->pdSectionHighAvailability);
    this->showSectionIfNotEmpty(this->ui->pdSectionStatus);
    this->showSectionIfNotEmpty(this->ui->pdSectionMultipathing);
    this->showSectionIfNotEmpty(this->ui->pdSectionMultipathBoot);
    this->showSectionIfNotEmpty(this->ui->pdSectionVcpus);
    this->showSectionIfNotEmpty(this->ui->pdSectionDockerInfo);
    this->showSectionIfNotEmpty(this->ui->pdSectionReadCaching);
    this->showSectionIfNotEmpty(this->ui->pdSectionDeviceSecurity);

    this->applyExpandedState();
    this->updateExpandCollapseButtons();
}

void GeneralTabPage::clearProperties()
{
    this->ui->pdSectionGeneral->ClearData();
    this->ui->pdSectionBios->ClearData();
    this->ui->pdSectionCustomFields->ClearData();
    this->ui->pdSectionManagementInterfaces->ClearData();
    this->ui->pdSectionMemory->ClearData();
    this->ui->pdSectionCpu->ClearData();
    this->ui->pdSectionVersion->ClearData();
    this->ui->pdSectionBootOptions->ClearData();
    this->ui->pdSectionHighAvailability->ClearData();
    this->ui->pdSectionStatus->ClearData();
    this->ui->pdSectionMultipathing->ClearData();
    this->ui->pdSectionMultipathBoot->ClearData();
    this->ui->pdSectionVcpus->ClearData();
    this->ui->pdSectionDockerInfo->ClearData();
    this->ui->pdSectionReadCaching->ClearData();
    this->ui->pdSectionDeviceSecurity->ClearData();

    this->ui->pdSectionGeneral->setVisible(false);
    this->ui->pdSectionBios->setVisible(false);
    this->ui->pdSectionCustomFields->setVisible(false);
    this->ui->pdSectionManagementInterfaces->setVisible(false);
    this->ui->pdSectionMemory->setVisible(false);
    this->ui->pdSectionCpu->setVisible(false);
    this->ui->pdSectionVersion->setVisible(false);
    this->ui->pdSectionBootOptions->setVisible(false);
    this->ui->pdSectionHighAvailability->setVisible(false);
    this->ui->pdSectionStatus->setVisible(false);
    this->ui->pdSectionMultipathing->setVisible(false);
    this->ui->pdSectionMultipathBoot->setVisible(false);
    this->ui->pdSectionVcpus->setVisible(false);
    this->ui->pdSectionDockerInfo->setVisible(false);
    this->ui->pdSectionReadCaching->setVisible(false);
    this->ui->pdSectionDeviceSecurity->setVisible(false);
}

void GeneralTabPage::addProperty(PDSection* section, const QString& label, const QString& value, const QList<QAction*>& contextMenuItems)
{
    if (!section)
        return;

    section->AddEntry(label, value, contextMenuItems);
}

void GeneralTabPage::addPropertyByKey(PDSection* section, const QString& key, const QString& value, const QList<QAction*>& contextMenuItems)
{
    this->addProperty(section, this->friendlyName(key), value, contextMenuItems);
}

void GeneralTabPage::showSectionIfNotEmpty(PDSection* section)
{
    if (!section)
        return;

    section->setVisible(!section->IsEmpty());
}

QString GeneralTabPage::friendlyName(const QString& key) const
{
    static QHash<QString, QString> labels;
    if (labels.isEmpty())
    {
        labels.insert("host.name_label", tr("Name"));
        labels.insert("host.name_description", tr("Description"));
        labels.insert("host.uuid", tr("UUID"));
        labels.insert("host.address", tr("Address"));
        labels.insert("host.hostname", tr("Hostname"));
        labels.insert("host.enabled", tr("Enabled"));
        labels.insert("host.iscsi_iqn", tr("iSCSI IQN"));
        labels.insert("host.log_destination", tr("Log destination"));
        labels.insert("host.uptime", tr("Server Uptime"));
        labels.insert("host.agentUptime", tr("Toolstack Uptime"));
        labels.insert("host.external_auth_service_name", tr("External Auth Service"));
        labels.insert("host.ServerMemory", tr("Server"));
        labels.insert("host.VMMemory", tr("VMs"));
        labels.insert("host.XenMemory", tr("XCP-ng"));
        labels.insert("pool.master", tr("Master"));
        labels.insert("pool.default_SR", tr("Default SR"));
        labels.insert("pool.ha_enabled", tr("HA Enabled"));
        labels.insert("VM.OSName", tr("Operating system"));
        labels.insert("VM.OperatingMode", tr("Operating mode"));
        labels.insert("VM.Appliance", tr("vApp"));
        labels.insert("VM.snapshot_of", tr("Snapshot of"));
        labels.insert("VM.snapshot_time", tr("Creation time"));
        labels.insert("VM.uptime", tr("Uptime"));
        labels.insert("VM.memory", tr("Memory"));
        labels.insert("VM.auto_boot", tr("Auto boot"));
        labels.insert("VM.BootOrder", tr("Boot order"));
        labels.insert("VM.BootMode", tr("Boot mode"));
        labels.insert("VM.PV_args", tr("Boot parameters"));
        labels.insert("VM.ha_restart_priority", tr("HA restart priority"));
        labels.insert("VM.P2V_SourceMachine", tr("P2V source machine"));
        labels.insert("VM.P2V_ImportDate", tr("P2V import date"));
        labels.insert("VM.affinity", tr("Home server"));
        labels.insert("VM.VCPUs", tr("vCPUs at startup"));
        labels.insert("VM.MaxVCPUs", tr("vCPUs maximum"));
        labels.insert("VM.Topology", tr("Topology"));
        labels.insert("VM.VirtualizationState", tr("Virtualization state"));
        labels.insert("VM.read_caching_status", tr("Read caching status"));
        labels.insert("VM.read_caching_disks", tr("Read caching disks"));
        labels.insert("VM.read_caching_reason", tr("Read caching reason"));
        labels.insert("VM.pvs_read_caching_status", tr("PVS read caching status"));
        labels.insert("host.pool_master", tr("Pool master"));
        labels.insert("host.auto_poweron", tr("Autoboot of VMs enabled"));
        labels.insert("host.bios_vendor", tr("Vendor"));
        labels.insert("host.bios_version", tr("Version"));
        labels.insert("host.system_manufacturer", tr("Manufacturer"));
        labels.insert("host.system_product", tr("Product"));
        labels.insert("host.cpu_count", tr("Count"));
        labels.insert("host.cpu_model", tr("Model"));
        labels.insert("host.cpu_speed", tr("Speed"));
        labels.insert("host.cpu_vendor", tr("Vendor"));
        labels.insert("host.product_version", tr("Product Version"));
        labels.insert("host.build_date", tr("Build Date"));
        labels.insert("host.build_number", tr("Build Number"));
        labels.insert("host.dbv", tr("DBV"));
        labels.insert("SR.type", tr("Type"));
        labels.insert("SR.size", tr("Total Size"));
        labels.insert("SR.utilisation", tr("Used Space"));
        labels.insert("SR.shared", tr("Shared"));
        labels.insert("network.bridge", tr("Bridge"));
        labels.insert("network.MTU", tr("MTU"));
        labels.insert("network.managed", tr("Managed"));
        labels.insert("SR.state", tr("State"));
        labels.insert("multipath.capable", tr("Multipath capable"));
    }

    return labels.value(key, key);
}

void GeneralTabPage::openPropertiesDialog()
{
    if (!this->m_object)
        return;

    XenObjectType object_type = this->m_object->GetObjectType();

    if (object_type == XenObjectType::VM)
    {
        QSharedPointer<VM> vm = qSharedPointerDynamicCast<VM>(this->m_object);
        if (!vm)
            return;
        VMPropertiesDialog dialog(vm, this);
        dialog.exec();
    } else if (object_type == XenObjectType::Host)
    {
        QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(this->m_object);
        if (!host)
            return;
        HostPropertiesDialog dialog(host, this);
        dialog.exec();
    } else if (object_type == XenObjectType::Pool)
    {
        QSharedPointer<Pool> pool = qSharedPointerDynamicCast<Pool>(this->m_object);
        if (!pool)
            return;
        PoolPropertiesDialog dialog(pool, this);
        dialog.exec();
    } else if (object_type == XenObjectType::SR)
    {
        QSharedPointer<SR> sr = qSharedPointerDynamicCast<SR>(this->m_object);
        if (!sr)
            return;
        StoragePropertiesDialog dialog(sr, this);
        dialog.exec();
    } else if (object_type == XenObjectType::Network)
    {
        QSharedPointer<Network> network = qSharedPointerDynamicCast<Network>(this->m_object);
        if (!network)
            return;
        NetworkPropertiesDialog dialog(network, this);
        dialog.exec();
    }
}

void GeneralTabPage::updateExpandCollapseButtons()
{
    bool canExpand = false;
    bool canCollapse = false;

    for (PDSection* section : this->sections_)
    {
        if (!section || section->IsEmpty())
            continue;

        if (section->IsExpanded())
            canCollapse = true;
        else
            canExpand = true;
    }

    this->ui->expandAllButton->setEnabled(canExpand);
    this->ui->collapseAllButton->setEnabled(canCollapse);
}

void GeneralTabPage::toggleExpandedState(bool expandAll)
{
    for (PDSection* section : this->sections_)
    {
        if (!section || !section->isVisible())
            continue;

        section->SetDisableFocusEvent(true);
        if (expandAll)
            section->Expand();
        else
            section->Collapse();
        section->SetDisableFocusEvent(false);
    }
}

void GeneralTabPage::applyExpandedState()
{
    //TODO inspect this probably can be changed to proper type
    const QString key = XenObject::TypeToString(this->m_objectType);
    if (key.isEmpty())
        return;

    QList<PDSection*> expanded = expandedSections_.value(key);
    if (expanded.isEmpty())
        expanded = { this->ui->pdSectionGeneral };

    for (PDSection* section : this->sections_)
    {
        if (!section || !section->isVisible())
            continue;

        section->SetDisableFocusEvent(true);
        if (expanded.contains(section))
            section->Expand();
        else
            section->Collapse();
        section->SetDisableFocusEvent(false);
    }
}

void GeneralTabPage::onExpandAllClicked()
{
    this->toggleExpandedState(true);
    this->updateExpandCollapseButtons();
}

void GeneralTabPage::onCollapseAllClicked()
{
    this->toggleExpandedState(false);
    this->updateExpandCollapseButtons();
}

void GeneralTabPage::onSectionExpandedChanged(PDSection* section)
{
    Q_UNUSED(section);

    if (!this->m_object)
        return;

    const QString key = XenObject::TypeToString(this->m_objectType);
    if (!key.isEmpty())
    {
        QList<PDSection*> expanded;
        for (PDSection* s : this->sections_)
        {
            if (s && s->isVisible() && s->IsExpanded())
                expanded.append(s);
        }
        expandedSections_.insert(key, expanded);
    }

    this->updateExpandCollapseButtons();
}

void GeneralTabPage::populateVMProperties()
{
    // VM-specific properties - comprehensive implementation matching C# GenerateGeneralBox
    // C# Reference: xenadmin/XenAdmin/TabPages/GeneralTabPage.cs lines 943-1095

    if (!this->m_object || this->m_object->GetObjectType() != XenObjectType::VM)
        return;

    QSharedPointer<VM> vm = qSharedPointerDynamicCast<VM>(this->m_object);
    if (!vm)
        return;

    bool isTemplate = vm->IsTemplate();
    bool isSnapshot = vm->IsSnapshot();

    // OS Name / Guest Operating System
    QString osName = vm->GetOSName();
    if (osName.isEmpty())
        osName = "Unknown";
    this->addPropertyByKey(this->ui->pdSectionGeneral, "VM.OSName", osName);

    // Operating Mode (HVM vs PV)
    bool isHVM = vm->IsHVM();
    this->addPropertyByKey(this->ui->pdSectionGeneral, "VM.OperatingMode", isHVM ? tr("HVM") : tr("Paravirtualized"));

    // BIOS strings copied (for templates) - C# line 1052
    if (isTemplate)
    {
        QVariantMap biosStrings = vm->BIOSStrings();
        bool biosStringsCopied = !biosStrings.isEmpty() && biosStrings.contains("bios-vendor");
        this->addProperty(this->ui->pdSectionGeneral, tr("BIOS strings copied"), biosStringsCopied ? tr("Yes") : tr("No"));
    }

    // vApp / VM Appliance - C# lines 1056-1065
    if (this->m_connection)
    {
        QString applianceRef = vm->ApplianceRef();
        if (!applianceRef.isEmpty() && applianceRef != XENOBJECT_NULL)
        {
            QSharedPointer<VMAppliance> appliance = this->m_connection->GetCache()->ResolveObject<VMAppliance>(XenObjectType::VMAppliance, applianceRef);
            QString applianceName = appliance ? appliance->GetName() : "Unknown";
            this->addPropertyByKey(this->ui->pdSectionGeneral, "VM.Appliance", applianceName);
        }
    }

    // Snapshot information - C# lines 1067-1070
    if (isSnapshot)
    {
        QSharedPointer<VM> snapshotOf = vm->SnapshotOf();
        if (snapshotOf)
            this->addPropertyByKey(this->ui->pdSectionGeneral, "VM.snapshot_of", snapshotOf->GetName());

        QDateTime snapshotTime = vm->SnapshotTime();
        if (snapshotTime.isValid())
            this->addPropertyByKey(this->ui->pdSectionGeneral, "VM.snapshot_time", snapshotTime.toLocalTime().toString("dd/MM/yyyy HH:mm:ss"));
    }

    // Properties for running VMs (not templates)
    if (!isTemplate)
    {
        // Virtualization Status - C# lines 1066 GenerateVirtualisationStatusForGeneralBox
        QString powerState = vm->GetPowerState();
        if (powerState == "Running")
        {
            int status = vm->GetVirtualizationStatus();
            bool hasIODrivers = (status & 4) != 0;
            bool hasManagementAgent = (status & 8) != 0;
            bool hasVendorDevice = vm->HasVendorDeviceState();

            QStringList statusLines;

            if (hasIODrivers)
                statusLines << "I/O drivers: optimized";
            else
                statusLines << "I/O drivers: not optimized";

            if (hasManagementAgent)
                statusLines << "Management agent: installed";
            else
                statusLines << "Management agent: not installed";

            if (hasVendorDevice)
                statusLines << "Receiving Windows Update";
            else
                statusLines << "Not receiving Windows Update";

            this->addPropertyByKey(this->ui->pdSectionGeneral, "VM.VirtualizationState", statusLines.join("\n"));
        }

        // VM Uptime - C# line 1069 vm.RunningTime()
        if (powerState == "Running")
        {
            qint64 startTime = vm->GetStartTime();
            if (startTime > 0)
            {
                qint64 uptimeSeconds = QDateTime::fromSecsSinceEpoch(startTime, Qt::UTC)
                                           .secsTo(QDateTime::currentDateTimeUtc());
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

                    this->addPropertyByKey(this->ui->pdSectionGeneral, "VM.uptime", uptimeStr);
                }
            }
        }

        // P2V Source information - C# lines 1072-1075
        QVariantMap otherConfig = vm->GetOtherConfig();

        if (otherConfig.contains("p2v_source_machine"))
        {
            QString sourceMachine = otherConfig.value("p2v_source_machine").toString();
            this->addPropertyByKey(this->ui->pdSectionGeneral, "VM.P2V_SourceMachine", sourceMachine);
        }

        if (otherConfig.contains("p2v_import_date"))
        {
            QString importDate = otherConfig.value("p2v_import_date").toString();
            QDateTime dt = QDateTime::fromString(importDate, Qt::ISODate);
            if (dt.isValid())
                this->addPropertyByKey(this->ui->pdSectionGeneral, "VM.P2V_ImportDate", dt.toLocalTime().toString("dd/MM/yyyy HH:mm:ss"));
        }

        // Home Server / Affinity - C# lines 1078-1083 (if WLB not enabled)
        // Show if VM can choose home server (has affinity field and is not managed by WLB)
        QSharedPointer<Host> affinityHost = vm->GetAffinityHost();
        QString affinityDisplay = affinityHost ? affinityHost->GetName() : "None";
        this->addPropertyByKey(this->ui->pdSectionGeneral, "VM.affinity", affinityDisplay);
    }

    // Memory (for all VM types)
    qint64 memoryBytes = vm->GetMemoryDynamicMax();
    if (memoryBytes > 0)
    {
        QString memoryStr = Misc::FormatSize(memoryBytes);
        this->addPropertyByKey(this->ui->pdSectionGeneral, "VM.memory", memoryStr);
    }

    this->populateBootOptionsSection();
    this->populateHighAvailabilitySection();
    this->populateVcpusSection();
    this->populateReadCachingSection();
    this->populateDeviceSecuritySection();
}

void GeneralTabPage::populateHostProperties()
{
    // Host-specific properties organized into sections
    // C# Reference: xenadmin/XenAdmin/TabPages/GeneralTabPage.cs lines 953-1032

    this->populateGeneralSection();
    this->populateBIOSSection();
    this->populateManagementInterfacesSection();
    this->populateMemorySection();
    this->populateCPUSection();
    this->populateVersionSection();
    this->populateMultipathBootSection();
}

void GeneralTabPage::populatePoolProperties()
{
    // Pool-specific properties
    QSharedPointer<Pool> pool = qSharedPointerDynamicCast<Pool>(this->m_object);
    if (!pool)
        return;

    const QString masterRef = pool->GetMasterHostRef();
    if (!masterRef.isEmpty())
        this->addPropertyByKey(this->ui->pdSectionGeneral, "pool.master", masterRef);

    const QString defaultSrRef = pool->GetDefaultSRRef();
    if (!defaultSrRef.isEmpty())
        this->addPropertyByKey(this->ui->pdSectionGeneral, "pool.default_SR", defaultSrRef);

    this->addPropertyByKey(this->ui->pdSectionGeneral, "pool.ha_enabled",
                           pool->HAEnabled() ? tr("Yes") : tr("No"));
}

void GeneralTabPage::populateSRProperties()
{
    // Storage Repository specific properties
    // C# Reference: GeneralTabPage.cs lines 360-390
    // Calls GenerateStatusBox() and GenerateMultipathBox() for SR

    if (!this->m_object || this->m_object->GetObjectType() != XenObjectType::SR)
        return;

    QSharedPointer<SR> sr = qSharedPointerDynamicCast<SR>(this->m_object);
    if (!sr)
        return;

    QString type = sr->GetType();
    if (!type.isEmpty())
        this->addPropertyByKey(this->ui->pdSectionGeneral, "SR.type", type);

    qint64 sizeBytes = sr->PhysicalSize();
    if (sizeBytes > 0)
    {
        QString sizeValue = Misc::FormatSize(sizeBytes);
        this->addPropertyByKey(this->ui->pdSectionGeneral, "SR.size", sizeValue);
    }

    qint64 usedBytes = sr->PhysicalUtilisation();
    if (usedBytes > 0)
    {
        QString usedValue = Misc::FormatSize(usedBytes);
        this->addPropertyByKey(this->ui->pdSectionGeneral, "SR.utilisation", usedValue);
    }

    this->addPropertyByKey(this->ui->pdSectionGeneral, "SR.shared", sr->IsShared() ? tr("Yes") : tr("No"));

    // Populate SR-specific sections (Status and Multipathing)
    this->populateStatusSection();
    this->populateMultipathingSection();
}

void GeneralTabPage::populateNetworkProperties()
{
    // Network-specific properties
    QSharedPointer<Network> network = qSharedPointerDynamicCast<Network>(this->m_object);
    if (!network)
        return;

    const QString bridge = network->GetBridge();
    if (!bridge.isEmpty())
        this->addPropertyByKey(this->ui->pdSectionGeneral, "network.bridge", bridge);

    qint64 mtu = network->GetMTU();
    if (mtu > 0)
        this->addPropertyByKey(this->ui->pdSectionGeneral, "network.MTU", QString::number(mtu));

    this->addPropertyByKey(this->ui->pdSectionGeneral, "network.managed",
                           network->IsManaged() ? tr("Yes") : tr("No"));
}

void GeneralTabPage::populateCustomFieldsSection()
{
    if (!this->m_object)
        return;

    QVariantMap otherConfig = this->m_object->GetOtherConfig();
    if (otherConfig.isEmpty())
        return;

    // TODO: Use CustomFieldsManager definitions once gui_config parsing is implemented.
    QStringList keys = otherConfig.keys();
    keys.sort();

    const QString prefix = "XenCenter.CustomFields.";
    for (const QString& key : keys)
    {
        if (!key.startsWith(prefix))
            continue;

        QString fieldName = key.mid(prefix.length());
        QString value = otherConfig.value(key).toString();
        if (value.isEmpty())
            value = tr("None");

        this->addProperty(this->ui->pdSectionCustomFields, fieldName, value);
    }
}

void GeneralTabPage::populateBootOptionsSection()
{
    if (this->m_objectType != XenObjectType::VM)
        return;

    QSharedPointer<VM> vm = qSharedPointerDynamicCast<VM>(this->m_object);
    if (!vm)
        return;

    // TODO: Add Boot Mode/UEFI/Secure Boot display once VM helpers are ported.
    QVariantMap otherConfig = vm->GetOtherConfig();
    bool autoPowerOn = (otherConfig.value("auto_poweron", "false").toString() == "true");
    this->addPropertyByKey(this->ui->pdSectionBootOptions, "VM.auto_boot", autoPowerOn ? tr("Yes") : tr("No"));

    bool isHvm = !vm->HVMBootPolicy().isEmpty();
    if (isHvm)
    {
        QVariantMap bootParams = vm->HVMBootParams();
        QString order = bootParams.value("order", "cd").toString().toUpper();

        QStringList devices;
        for (int i = 0; i < order.length(); ++i)
        {
            const QString device = order.mid(i, 1);
            if (device == "C")
                devices.append(tr("Hard Disk"));
            else if (device == "D")
                devices.append(tr("DVD Drive"));
            else if (device == "N")
                devices.append(tr("Network"));
        }

        QString orderDisplay = devices.isEmpty() ? tr("None") : devices.join(", ");
        this->addPropertyByKey(this->ui->pdSectionBootOptions, "VM.BootOrder", orderDisplay);
    } else
    {
        QString pvArgs = vm->PVArgs();
        if (pvArgs.isEmpty())
            pvArgs = tr("None");
        this->addPropertyByKey(this->ui->pdSectionBootOptions, "VM.PV_args", pvArgs);
    }
}

void GeneralTabPage::populateHighAvailabilitySection()
{
    if (!this->m_object || this->m_object->GetObjectType() != XenObjectType::VM)
        return;

    QSharedPointer<VM> vm = qSharedPointerDynamicCast<VM>(this->m_object);
    if (!vm)
        return;

    XenCache* cache = vm->GetCache();

    QSharedPointer<Pool> pool = cache->GetPool();
    if (!pool || !pool->HAEnabled())
        return;

    QString restartPriority = vm->HARestartPriority();
    if (restartPriority.isEmpty())
        return;

    // TODO: Map restart priority values to friendly display strings (C# Helpers.RestartPriorityI18n).
    this->addPropertyByKey(this->ui->pdSectionHighAvailability, "VM.ha_restart_priority", restartPriority);
}

void GeneralTabPage::populateMultipathBootSection()
{
    if (this->m_objectType != XenObjectType::Host)
        return;

    // Boot path counts are not currently exposed in the Qt port.
    // TODO: Add Host::GetBootPathCounts() and populate Multipath Boot status.
}

void GeneralTabPage::populateVcpusSection()
{
    if (!this->m_object || this->m_object->GetObjectType() != XenObjectType::VM)
        return;

    QSharedPointer<VM> vm = qSharedPointerDynamicCast<VM>(this->m_object);
    if (!vm)
        return;

    int vcpusAtStartup = vm->VCPUsAtStartup();
    qint64 vcpusMax = vm->VCPUsMax();

    this->addPropertyByKey(this->ui->pdSectionVcpus, "VM.VCPUs", QString::number(vcpusAtStartup));

    if (vcpusMax != vcpusAtStartup)
        this->addPropertyByKey(this->ui->pdSectionVcpus, "VM.MaxVCPUs", QString::number(vcpusMax));

    qint64 coresPerSocket = 1;
    QVariantMap platform = vm->Platform();
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
    this->addPropertyByKey(this->ui->pdSectionVcpus, "VM.Topology", VM::GetTopology(sockets, coresPerSocket));
}

void GeneralTabPage::populateDockerInfoSection()
{
    if (this->m_objectType != XenObjectType::DockerContainer)
        return;

    QSharedPointer<DockerContainer> container = qSharedPointerDynamicCast<DockerContainer>(this->m_object);
    if (!container)
        return;

    QString name = container->GetName();
    if (name.isEmpty())
        name = tr("None");
    this->addProperty(this->ui->pdSectionDockerInfo, tr("Name"), name);

    QString status = container->Status();
    this->addProperty(this->ui->pdSectionDockerInfo, tr("Status"), status.isEmpty() ? tr("None") : status);

    QString created = container->Created();
    if (!created.isEmpty())
    {
        bool ok = false;
        double createdSeconds = created.toDouble(&ok);
        if (ok)
        {
            QDateTime dt = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(createdSeconds)).toLocalTime();
            created = dt.toString("dd/MM/yyyy HH:mm:ss");
        }
    }
    this->addProperty(this->ui->pdSectionDockerInfo, tr("Created"), created.isEmpty() ? tr("None") : created);

    QString image = container->Image();
    this->addProperty(this->ui->pdSectionDockerInfo, tr("Image"), image.isEmpty() ? tr("None") : image);

    QString containerId = container->Container();
    this->addProperty(this->ui->pdSectionDockerInfo, tr("Container"), containerId.isEmpty() ? tr("None") : containerId);

    QString command = container->Command();
    this->addProperty(this->ui->pdSectionDockerInfo, tr("Command"), command.isEmpty() ? tr("None") : command);

    QString ports;
    QList<DockerContainer::DockerContainerPort> portList = container->PortList();
    if (!portList.isEmpty())
    {
        QStringList portDescriptions;
        for (const DockerContainer::DockerContainerPort& port : portList)
            portDescriptions.append(port.description());
        ports = portDescriptions.join("\n");
    }
    this->addProperty(this->ui->pdSectionDockerInfo, tr("Ports"), ports.isEmpty() ? tr("None") : ports);

    QString uuid = container->GetUUID();
    this->addProperty(this->ui->pdSectionDockerInfo, tr("UUID"), uuid.isEmpty() ? tr("None") : uuid);
}

void GeneralTabPage::populateReadCachingSection()
{
    if (!this->m_object || this->m_object->GetObjectType() != XenObjectType::VM)
        return;

    QSharedPointer<VM> vm = qSharedPointerDynamicCast<VM>(this->m_object);
    if (!vm)
        return;

    if (vm->GetPowerState() != "Running")
        return;

    bool enabled = vm->ReadCachingEnabled();
    this->addPropertyByKey(this->ui->pdSectionReadCaching, "VM.read_caching_status",
                           enabled ? tr("Enabled") : tr("Disabled"));
    // TODO: Add read caching disk list and disabled reason (VM::ReadCachingVDIs/ReadCachingDisabledReason).
}

void GeneralTabPage::populateDeviceSecuritySection()
{
    if (!this->m_object || this->m_object->GetObjectType() != XenObjectType::VM)
        return;

    QSharedPointer<VM> vm = qSharedPointerDynamicCast<VM>(this->m_object);
    if (!vm)
        return;

    QList<QSharedPointer<VTPM>> vtpms = vm->GetVTPMs();
    if (vtpms.isEmpty())
        return;

    QString value = vtpms.count() == 1
        ? tr("1 attached")
        : tr("%1 attached").arg(vtpms.count());
    this->addProperty(this->ui->pdSectionDeviceSecurity, tr("vTPM"), value);
    // TODO: Add VTPM command actions and feature gating once Helpers.FeatureForbidden is ported.
}

// === Host Section Population Methods ===

void GeneralTabPage::populateGeneralSection()
{
    // General Section: Address, Hostname, Pool Coordinator, Enabled, iSCSI IQN, Uptime
    // C# Reference: GenerateGeneralBox lines 953-1032

    QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(this->m_object);
    if (!host)
        return;

    QString address = host->GetAddress();
    if (!address.isEmpty())
        this->addPropertyByKey(this->ui->pdSectionGeneral, "host.address", address);

    QString hostname = host->GetHostname();
    if (!hostname.isEmpty())
        this->addPropertyByKey(this->ui->pdSectionGeneral, "host.hostname", hostname);

    // Pool master status (shown as "Pool Coordinator" in C# but displays as "Pool master: Yes/No" in UI)
    // C# Reference: line 956-958 - only shown if not standalone host
    // C# logic: isStandAloneHost = Helpers.GetPool(connection) == null
    // GetPool returns null if Pool.IsVisible() is false
    // Pool.IsVisible() returns false when name_label == "" AND HostCount <= 1
    // So we show "Pool master" if the pool has a name OR has multiple hosts
    if (this->m_connection)
    {
        QSharedPointer<Pool> pool = host->GetPool();
        if (pool && pool->IsValid())
        {
            const QString poolName = pool->GetName();
            const bool hasPoolName = !poolName.isEmpty();
            const int hostCount = pool->GetHosts().size();
            const bool hasMultipleHosts = hostCount > 1;

            if (hasPoolName || hasMultipleHosts)
                this->addPropertyByKey(this->ui->pdSectionGeneral, "host.pool_master",
                                       host->IsMaster() ? tr("Yes") : tr("No"));
        }
    }

    // Enabled status with maintenance mode detection
    {
        bool enabled = host->IsEnabled();
        bool isLive = host->IsLive();

        QString enabledStr;
        if (!isLive)
            enabledStr = "Host not live";
        else if (!enabled)
            enabledStr = "Disabled (Maintenance Mode)";
        else
            enabledStr = "Yes";

        this->addPropertyByKey(this->ui->pdSectionGeneral, "host.enabled", enabledStr);
    }

    // Autoboot of VMs enabled
    // C# Reference: line 980 - host.GetVmAutostartEnabled()
    {
        QVariantMap otherConfig = host->GetOtherConfig();
        // GetVmAutostartEnabled checks other_config["auto_poweron"] == "true"
        bool autoPowerOn = (otherConfig.value("auto_poweron").toString() == "true");
        this->addPropertyByKey(this->ui->pdSectionGeneral, "host.auto_poweron", autoPowerOn ? tr("Yes") : tr("No"));
    }

    // Log destination
    // C# Reference: lines 1011-1017 - host.GetSysLogDestination() from logging["syslog_destination"]
    {
        QVariantMap logging = host->Logging();
        QString syslogDest = logging.value("syslog_destination").toString();

        QString logDisplay;
        if (syslogDest.isEmpty())
            logDisplay = "Local";
        else
            logDisplay = QString("Local and %1").arg(syslogDest);

        this->addPropertyByKey(this->ui->pdSectionGeneral, "host.log_destination", logDisplay);
    }

    // Server Uptime (calculated from boot time in host_metrics)
    // C# Reference: Host.Uptime() lines 853-859
    if (this->m_connection)
    {
        double bootTime = host->BootTime();
        if (bootTime > 0)
        {
            QDateTime startTime = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(bootTime), Qt::UTC);
            qint64 uptimeSeconds = startTime.secsTo(QDateTime::currentDateTimeUtc());
            if (uptimeSeconds > 0)
                this->addPropertyByKey(this->ui->pdSectionGeneral, "host.uptime", Misc::FormatUptime(uptimeSeconds));
        }
    }

    // Toolstack Uptime (xapi agent uptime from other_config.agent_start_time)
    // C# Reference: Host.AgentUptime() lines 885-891
    {
        QVariantMap otherConfig = host->GetOtherConfig();
        if (otherConfig.contains("agent_start_time"))
        {
            bool ok = false;
            double agentStartTime = otherConfig.value("agent_start_time").toDouble(&ok);
            if (ok && agentStartTime > 0)
            {
                QDateTime startTime = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(agentStartTime), Qt::UTC);
                qint64 uptimeSeconds = startTime.secsTo(QDateTime::currentDateTimeUtc());
                if (uptimeSeconds > 0)
                    this->addPropertyByKey(this->ui->pdSectionGeneral, "host.agentUptime", Misc::FormatUptime(uptimeSeconds));
            }
        }
    }

    // iSCSI IQN
    QString iscsiIqn = host->IscsiIQN();
    if (!iscsiIqn.isEmpty())
        this->addPropertyByKey(this->ui->pdSectionGeneral, "host.iscsi_iqn", iscsiIqn);

    // Show section if it has content
    this->showSectionIfNotEmpty(this->ui->pdSectionGeneral);
}

void GeneralTabPage::populateBIOSSection()
{
    // BIOS Information Section
    // C# Reference: bios_strings field

    QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(this->m_object);
    if (!host)
        return;

    QVariantMap biosStrings = host->BIOSStrings();
    if (biosStrings.isEmpty())
        return;

    if (biosStrings.contains("bios-vendor"))
    {
        QString biosVendor = biosStrings.value("bios-vendor").toString();
        if (!biosVendor.isEmpty())
            this->addPropertyByKey(this->ui->pdSectionBios, "host.bios_vendor", biosVendor);
    }

    if (biosStrings.contains("bios-version"))
    {
        QString biosVersion = biosStrings.value("bios-version").toString();
        if (!biosVersion.isEmpty())
            this->addPropertyByKey(this->ui->pdSectionBios, "host.bios_version", biosVersion);
    }

    if (biosStrings.contains("system-manufacturer"))
    {
        QString sysManufacturer = biosStrings.value("system-manufacturer").toString();
        if (!sysManufacturer.isEmpty())
            this->addPropertyByKey(this->ui->pdSectionBios, "host.system_manufacturer", sysManufacturer);
    }

    if (biosStrings.contains("system-product-name"))
    {
        QString sysProduct = biosStrings.value("system-product-name").toString();
        if (!sysProduct.isEmpty())
            this->addPropertyByKey(this->ui->pdSectionBios, "host.system_product", sysProduct);
    }

    // Show section if it has content
    this->showSectionIfNotEmpty(this->ui->pdSectionBios);
}

void GeneralTabPage::populateManagementInterfacesSection()
{
    // Management Interfaces Section
    // C# Reference: GenerateInterfaceBox lines 396-461 (fillInterfacesForHost)

    if (!this->m_connection)
        return;

    XenCache* cache = this->m_connection->GetCache();
    if (!cache)
        return;

    auto addInterfacesForHost = [&](const QSharedPointer<Host>& host, bool includeHostName)
    {
        if (!host)
            return;

        QList<QSharedPointer<PIF>> pifs = host->GetPIFs();
        for (const QSharedPointer<PIF>& pif : pifs)
        {
            if (!pif || !pif->IsValid())
                continue;

            if (!pif->IsManagementInterface())
                continue;

            QString ipAddress = pif->IP();
            if (ipAddress.isEmpty())
                continue;

            QString label = tr("Management interface");
            if (includeHostName)
            {
                QString hostName = host->GetName();
                if (!hostName.isEmpty())
                    label = tr("%1 (%2)").arg(label, hostName);
            }

            this->addProperty(this->ui->pdSectionManagementInterfaces, label, ipAddress);
        }
    };

    if (this->m_objectType == XenObjectType::Host)
    {
        QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(this->m_object);
        addInterfacesForHost(host, false);
    } else if (this->m_objectType == XenObjectType::Pool)
    {
        QSharedPointer<Pool> pool = qSharedPointerDynamicCast<Pool>(this->m_object);
        if (!pool)
            return;

        QList<QSharedPointer<Host>> hosts = pool->GetHosts();
        for (const QSharedPointer<Host>& host : hosts)
            addInterfacesForHost(host, true);
    }

    // Show section if it has content
    this->showSectionIfNotEmpty(this->ui->pdSectionManagementInterfaces);
}

void GeneralTabPage::populateMemorySection()
{
    // Memory Section: Server, VMs, XCP-ng/Xen
    // C# Reference: GenerateMemoryBox lines 1380-1392
    // Labels use FriendlyName("host.ServerMemory"), FriendlyName("host.VMMemory"), FriendlyName("host.XenMemory")
    // which resolve to "Server", "VMs", and "XCP-ng" (or "Xen")

    // Server Memory: Shows "X GB free of Y GB total"
    // C# Reference: Host.HostMemoryString() lines 1269-1281
    QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(this->m_object);
    if (host && this->m_connection)
    {
        QSharedPointer<HostMetrics> metrics = host->GetMetrics();
        if (metrics && metrics->IsValid())
        {
            qint64 memTotal = metrics->GetMemoryTotal();
            qint64 memFree = metrics->GetMemoryFree();
            if (memTotal > 0)
            {
                double memFreeGB = memFree / (1024.0 * 1024.0 * 1024.0);
                double memTotalGB = memTotal / (1024.0 * 1024.0 * 1024.0);

                QString serverMemStr = QString("%1 GB free of %2 GB total")
                                           .arg(memFreeGB, 0, 'f', 2)
                                           .arg(memTotalGB, 0, 'f', 2);
                this->addPropertyByKey(this->ui->pdSectionMemory, "host.ServerMemory", serverMemStr);
            }
        }
    }

    // VMs: Shows memory used by VMs
    // C# Reference: Host.ResidentVMMemoryUsageString() shows each VM's memory on separate line
    // Simplified version: Just show total VM memory used
    if (host)
    {
        QSharedPointer<HostMetrics> metrics = host->GetMetrics();
        if (metrics && metrics->IsValid())
        {
            qint64 memTotal = metrics->GetMemoryTotal();
            qint64 memFree = metrics->GetMemoryFree();
            if (memTotal > 0)
            {
                qint64 memUsed = memTotal - memFree;
                double memUsedGB = memUsed / (1024.0 * 1024.0 * 1024.0);
                this->addPropertyByKey(this->ui->pdSectionMemory, "host.VMMemory", QString("%1 GB").arg(memUsedGB, 0, 'f', 2));
            }
        }
    }

    // XCP-ng/Xen Memory overhead
    // C# Reference: Host.XenMemoryString() lines 1286-1294
    if (host)
    {
        qint64 memOverhead = host->MemoryOverhead();
        if (memOverhead > 0)
        {
            double memOverheadMB = memOverhead / (1024.0 * 1024.0);
            // Use "XCP-ng" as label (C# uses product brand name)
            this->addPropertyByKey(this->ui->pdSectionMemory, "host.XenMemory", QString("%1 MB").arg(memOverheadMB, 0, 'f', 0));
        }
    }

    // Show section if it has content
    this->showSectionIfNotEmpty(this->ui->pdSectionMemory);
}

void GeneralTabPage::populateCPUSection()
{
    // CPU Section: CPU Count, Model, Speed, Vendor
    // C# Reference: GenerateCPUBox lines 826-857

    QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(this->m_object);
    if (!host)
        return;

    QVariantMap cpuInfo = host->GetCPUInfo();
    if (cpuInfo.isEmpty())
        return;

    if (cpuInfo.contains("cpu_count"))
    {
        int cpuCount = cpuInfo.value("cpu_count").toInt();
        this->addPropertyByKey(this->ui->pdSectionCpu, "host.cpu_count", QString::number(cpuCount));
    }

    if (cpuInfo.contains("modelname"))
    {
        QString cpuModel = cpuInfo.value("modelname").toString();
        if (!cpuModel.isEmpty())
            this->addPropertyByKey(this->ui->pdSectionCpu, "host.cpu_model", cpuModel);
    }

    if (cpuInfo.contains("speed"))
    {
        QString cpuSpeed = cpuInfo.value("speed").toString();
        if (!cpuSpeed.isEmpty())
            this->addPropertyByKey(this->ui->pdSectionCpu, "host.cpu_speed", cpuSpeed + " MHz");
    }

    if (cpuInfo.contains("vendor"))
    {
        QString cpuVendor = cpuInfo.value("vendor").toString();
        if (!cpuVendor.isEmpty())
            this->addPropertyByKey(this->ui->pdSectionCpu, "host.cpu_vendor", cpuVendor);
    }

    // Show section if it has content
    this->showSectionIfNotEmpty(this->ui->pdSectionCpu);
}

void GeneralTabPage::populateVersionSection()
{
    // Software Version Section: Product Version, Build Date, Build Number, DBV
    // C# Reference: GenerateVersionBox lines 784-825

    QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(this->m_object);
    if (!host)
        return;

    QVariantMap swVersion = host->SoftwareVersion();
    if (swVersion.isEmpty())
        return;

    // Product version (most important)
    if (swVersion.contains("product_version"))
    {
        QString productVersion = swVersion.value("product_version").toString();
        QString productBrand = swVersion.value("product_brand", "XenServer").toString();
        this->addPropertyByKey(this->ui->pdSectionVersion, "host.product_version", QString("%1 %2").arg(productBrand, productVersion));
    }

    // Build date
    if (swVersion.contains("date"))
    {
        QString buildDate = swVersion.value("date").toString();
        this->addPropertyByKey(this->ui->pdSectionVersion, "host.build_date", buildDate);
    }

    // Build number
    if (swVersion.contains("build_number"))
    {
        this->addPropertyByKey(this->ui->pdSectionVersion, "host.build_number", swVersion.value("build_number").toString());
    }

    // DBV (Database Version)
    if (swVersion.contains("dbv"))
    {
        this->addPropertyByKey(this->ui->pdSectionVersion, "host.dbv", swVersion.value("dbv").toString());
    }

    // Show section if it has content
    this->showSectionIfNotEmpty(this->ui->pdSectionVersion);
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

    if (!this->m_object || this->m_object->GetObjectType() != XenObjectType::SR)
        return;

    QSharedPointer<SR> sr = qSharedPointerDynamicCast<SR>(this->m_object);
    if (!sr)
        return;

    // Determine SR status (OK, Detached, Broken, Multipath Failure)
    bool broken = false;
    QString statusString = "OK";

    // Check if SR is detached (no PBDs currently attached)
    // C# SR.IsDetached() checks if any PBD is currently_attached
    bool hasAttachedPBD = false;
    QList<QSharedPointer<PBD>> pbds = sr->GetPBDs();
    foreach (QSharedPointer<PBD> pbd, pbds)
    {
        if (pbd->IsCurrentlyAttached())
        {
            hasAttachedPBD = true;
            break;
        }
    }

    if (pbds.isEmpty())
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
        bool isShared = sr->IsShared();
        int pbdCount = pbds.size();

        // Get host count to check if we have correct PBD count for pooled SR
        int expectedPBDCount = 1; // standalone default
        if (isShared)
        {
            // For shared SR, should have PBD for each host in pool
            QList<QSharedPointer<Host>> allHosts = this->m_connection->GetCache()->GetAll<Host>();
            expectedPBDCount = allHosts.size();
        }

        if (pbdCount != expectedPBDCount)
        {
            broken = true;
            statusString = "Broken (Wrong PBD count)";
        } else
        {
            // Check if all PBDs are attached
            foreach (QSharedPointer<PBD> pbd, pbds)
            {
                if (!pbd->IsCurrentlyAttached())
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
    this->ui->pdSectionStatus->AddEntry(this->friendlyName("SR.state"), statusString, broken ? QColor(Qt::red) : QColor());

    // Show per-host PBD status
    // C# iterates through all hosts and shows their PBD connection status
    bool isShared = sr->IsShared();

    QList<QSharedPointer<Host>> allHosts = this->m_connection->GetCache()->GetAll<Host>();

    for (const QSharedPointer<Host>& host : allHosts)
    {
        if (!host || !host->IsValid())
            continue;

        QString hostRef = host->OpaqueRef();
        QString hostName = host->GetName().isEmpty() ? "Unknown" : host->GetName();

        // Find PBD for this host
        QString pbdStatus = "Connected";
        QColor statusColor;

        bool foundPBD = false;
        foreach (QSharedPointer<PBD> pbd, pbds)
        {
            QString pbdHost = pbd->GetHostRef();
            if (pbdHost == hostRef)
            {
                foundPBD = true;
                if (pbd->IsCurrentlyAttached())
                {
                    pbdStatus = "Connected";
                    statusColor = QColor();
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

        this->ui->pdSectionStatus->AddEntry(displayName, pbdStatus, statusColor);
    }

    this->showSectionIfNotEmpty(this->ui->pdSectionStatus);
}

void GeneralTabPage::populateMultipathingSection()
{
    // SR Multipathing Section
    // C# Reference: GeneralTabPage.cs GenerateMultipathBox() lines 589-717

    if (!this->m_object || this->m_object->GetObjectType() != XenObjectType::SR)
        return;

    QSharedPointer<SR> sr = qSharedPointerDynamicCast<SR>(this->m_object);
    if (!sr)
        return;

    // Check if SR is multipath capable
    // C# SR.MultipathCapable() checks sm_config["multipathable"] == "true"
    QVariantMap smConfig = sr->SMConfig();
    QString multipathable = smConfig.value("multipathable", "false").toString();
    bool isMultipathCapable = (multipathable == "true");

    this->addPropertyByKey(this->ui->pdSectionMultipathing, "multipath.capable", isMultipathCapable ? tr("Yes") : tr("No"));

    if (!isMultipathCapable)
    {
        this->showSectionIfNotEmpty(this->ui->pdSectionMultipathing);
        return;
    }

    if (!this->m_connection || !this->m_connection->GetCache())
        return;

    QList<QSharedPointer<PBD>> pbds = sr->GetPBDs();
    QList<QSharedPointer<Host>> allHosts = this->m_connection->GetCache()->GetAll<Host>();

    for (const QSharedPointer<Host>& host : allHosts)
    {
        if (!host || host->IsEvicted())
            continue;

        QString hostRef = host->OpaqueRef();
        QString hostName = host->GetName();

        // Find PBD for this host
        QString multipathStatus = "Not active";
        QColor statusColor;

        for (const QSharedPointer<PBD>& pbd : pbds)
        {
            if (!pbd || pbd->IsEvicted())
                continue;

            if (pbd->GetHostRef() == hostRef)
            {
                // Check if multipath is active on this PBD
                // C# PBD.MultipathActive() checks device_config["multipathed"] == "true"
                QString multipathed = pbd->GetDeviceConfigValue("multipathed");
                bool multipathActive = (multipathed == "true");

                if (multipathActive)
                {
                    // Parse path counts from other_config
                    // C# PBD.ParsePathCounts() extracts "multipath-current-paths" and "multipath-maximum-paths"
                    int currentPaths = pbd->GetOtherConfigValue("multipath-current-paths").toInt();
                    int maxPaths = pbd->GetOtherConfigValue("multipath-maximum-paths").toInt();

                    // Get iSCSI session count if applicable
                    int iscsiSessions = pbd->GetOtherConfigValue("iscsi_sessions").toInt();

                    // Format status string: "X of Y paths active"
                    multipathStatus = QString("%1 of %2 paths active").arg(currentPaths).arg(maxPaths);

                    if (iscsiSessions > 0)
                    {
                        multipathStatus += QString(" (%1 iSCSI sessions)").arg(iscsiSessions);
                    }

                    // Red if degraded (current < max or max < iscsi sessions)
                    bool degraded = (currentPaths < maxPaths) || (iscsiSessions > 0 && maxPaths < iscsiSessions);
                    statusColor = degraded ? Qt::red : QColor();
                } else
                {
                    multipathStatus = "Not active";
                    statusColor = QColor();
                }

                break;
            }
        }

        this->ui->pdSectionMultipathing->AddEntry(hostName, multipathStatus, statusColor);
    }

    this->showSectionIfNotEmpty(this->ui->pdSectionMultipathing);
}
