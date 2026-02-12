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

#include <QDebug>
#include <QtWidgets>
#include <QHeaderView>
#include <QDomDocument>
#include <algorithm>

#include "newvmwizard.h"
#include "ui_newvmwizard.h"
#include "newvirtualdiskdialog.h"
#include "actionprogressdialog.h"
#include "vifdialog.h"
#include "newsrwizard.h"
#include "mainwindow.h"
#include "../widgets/wizardnavigationpane.h"
#include "../widgets/isodropdownbox.h"
#include "../settingsmanager.h"

#include "xenlib/xencache.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/actions/vm/createvmaction.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/sr.h"
#include "xenlib/vmhelpers.h"

NewVMWizard::NewVMWizard(XenConnection* connection, QWidget* parent) : QWizard(parent), m_connection(connection), ui(new Ui::NewVMWizard)
{
    this->ui->setupUi(this);
    this->setWindowTitle(tr("New Virtual Machine Wizard"));
    this->setWindowIcon(QIcon(":/icons/vm-create-32.png"));

    this->setupUiPages();

    connect(this, &QWizard::currentIdChanged, this, &NewVMWizard::onCurrentIdChanged);
    connect(this->ui->templateSearchEdit, &QLineEdit::textChanged, this, &NewVMWizard::filterTemplates);
    connect(this->ui->templateTree, &QTreeWidget::itemSelectionChanged, this, &NewVMWizard::handleTemplateSelectionChanged);
    connect(this->ui->vmNameEdit, &QLineEdit::textChanged, this, &NewVMWizard::onVmNameChanged);

    connect(this->ui->autoHomeServerRadio, &QRadioButton::toggled, this, &NewVMWizard::onAutoHomeServerToggled);
    connect(this->ui->specificHomeServerRadio, &QRadioButton::toggled, this, &NewVMWizard::onSpecificHomeServerToggled);
    connect(this->ui->copyBiosStringsCheckBox, &QCheckBox::toggled, this, &NewVMWizard::onCopyBiosStringsToggled);
    connect(this->ui->vcpusMaxSpin, qOverload<int>(&QSpinBox::valueChanged), this, &NewVMWizard::onVcpusMaxChanged);
    connect(this->ui->memoryStaticMaxSpin, qOverload<int>(&QSpinBox::valueChanged), this, &NewVMWizard::onMemoryStaticMaxChanged);
    connect(this->ui->memoryDynamicMaxSpin, qOverload<int>(&QSpinBox::valueChanged), this, &NewVMWizard::onMemoryDynamicMaxChanged);
    connect(this->ui->coresPerSocketCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &NewVMWizard::onCoresPerSocketChanged);
    connect(this->ui->isoRadioButton, &QRadioButton::toggled, this, &NewVMWizard::onIsoRadioToggled);
    connect(this->ui->urlRadioButton, &QRadioButton::toggled, this, &NewVMWizard::onUrlRadioToggled);
    connect(this->ui->defaultSrCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &NewVMWizard::onDefaultSrChanged);
    connect(this->ui->diskTable, &QTableWidget::itemSelectionChanged, this, &NewVMWizard::onDiskTableSelectionChanged);
    connect(this->ui->addDiskButton, &QPushButton::clicked, this, &NewVMWizard::onAddDiskClicked);
    connect(this->ui->editDiskButton, &QPushButton::clicked, this, &NewVMWizard::onEditDiskClicked);
    connect(this->ui->removeDiskButton, &QPushButton::clicked, this, &NewVMWizard::onRemoveDiskClicked);
    connect(this->ui->disklessCheckBox, &QCheckBox::toggled, this, &NewVMWizard::onDisklessToggled);
    connect(this->ui->networkTable, &QTableWidget::itemSelectionChanged, this, &NewVMWizard::onNetworkTableSelectionChanged);
    connect(this->ui->addNetworkButton, &QPushButton::clicked, this, &NewVMWizard::onAddNetworkClicked);
    connect(this->ui->editNetworkButton, &QPushButton::clicked, this, &NewVMWizard::onEditNetworkClicked);
    connect(this->ui->removeNetworkButton, &QPushButton::clicked, this, &NewVMWizard::onRemoveNetworkClicked);
    connect(this->ui->attachIsoButton, &QPushButton::clicked, this, &NewVMWizard::onAttachIsoLibraryClicked);

    this->ui->networkTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this->ui->networkTable, &QTableWidget::customContextMenuRequested, this, &NewVMWizard::onNetworkContextMenuRequested);
    this->ui->diskTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this->ui->diskTable, &QTableWidget::customContextMenuRequested, this, &NewVMWizard::onDiskContextMenuRequested);

    this->updateIsoControls();
    this->updateHomeServerControls(false);
    this->onDisklessToggled(this->ui->disklessCheckBox->isChecked());

    this->loadStorageRepositories();
    this->loadNetworks();
    this->loadHosts();
    this->loadTemplates();
    this->updateNavigationSelection();
}

NewVMWizard::~NewVMWizard()
{
    delete this->ui;
}

void NewVMWizard::setupUiPages()
{
    this->setWizardStyle(QWizard::ModernStyle);
    this->setOption(QWizard::HaveHelpButton, true);
    this->setOption(QWizard::HelpButtonOnRight, false);

    this->setPage(Page_Template, this->ui->pageTemplate);
    this->setPage(Page_Name, this->ui->pageName);
    this->setPage(Page_InstallationMedia, this->ui->pageInstallation);
    this->setPage(Page_HomeServer, this->ui->pageHomeServer);
    this->setPage(Page_CpuMemory, this->ui->pageCpuMemory);
    this->setPage(Page_Storage, this->ui->pageStorage);
    this->setPage(Page_Network, this->ui->pageNetworking);
    this->setPage(Page_Finish, this->ui->pageFinish);
    this->setStartId(Page_Template);

    this->ui->templateTree->setHeaderLabels({tr("Template"), tr("Type")});
    this->ui->templateTree->setSelectionMode(QAbstractItemView::SingleSelection);
    if (this->ui->templateTree->header())
    {
        this->ui->templateTree->header()->setStretchLastSection(false);
        this->ui->templateTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        this->ui->templateTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    }

    if (this->ui->diskTable->horizontalHeader())
    {
        this->ui->diskTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        this->ui->diskTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        this->ui->diskTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        this->ui->diskTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    }

    if (this->ui->networkTable->horizontalHeader())
    {
        this->ui->networkTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        this->ui->networkTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        this->ui->networkTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    }

    this->ui->coresPerSocketCombo->clear();
    for (int cores : {1, 2, 4, 8, 16})
        this->ui->coresPerSocketCombo->addItem(QString::number(cores), cores);

    this->ui->bootModeComboBox->clear();
    this->ui->bootModeComboBox->addItem(tr("Automatic (use template defaults)"), "auto");
    this->ui->bootModeComboBox->addItem(tr("BIOS"), "bios");
    this->ui->bootModeComboBox->addItem(tr("UEFI"), "uefi");
    this->ui->bootModeComboBox->addItem(tr("UEFI Secure Boot"), "secureboot");

    IsoDropDownBox* isoBox = qobject_cast<IsoDropDownBox*>(this->ui->isoComboBox);
    if (isoBox)
    {
        isoBox->SetConnection(this->m_connection);
        isoBox->SetVMRef(QString());
        isoBox->Refresh();
    }

    this->m_navigationPane = new WizardNavigationPane(this);
    QVector<WizardNavigationPane::Step> steps = {
        {tr("Template"), QIcon()},
        {tr("Name"), QIcon()},
        {tr("Installation Media"), QIcon()},
        {tr("Home Server"), QIcon()},
        {tr("CPU & Memory"), QIcon()},
        {tr("Storage"), QIcon()},
        {tr("Networking"), QIcon()},
        {tr("Finish"), QIcon()},
    };
    this->m_navigationPane->setSteps(steps);
    this->setSideWidget(this->m_navigationPane);
}

void NewVMWizard::loadTemplates()
{
    XenCache* cache = this->cache();
    if (!cache)
        return;

    this->ui->templateTree->clear();
    this->m_templateItems.clear();

    const bool showHidden = SettingsManager::instance().GetShowHiddenObjects();
    bool restrictVtpm = false;
    const QList<QSharedPointer<Host>> allHosts = cache->GetAll<Host>(XenObjectType::Host);
    for (const QSharedPointer<Host>& host : allHosts)
    {
        if (host && host->RestrictVtpm())
        {
            restrictVtpm = true;
            break;
        }
    }

    const QList<QSharedPointer<VM>> allVMs = cache->GetAll<VM>(XenObjectType::VM);
    for (const QSharedPointer<VM>& vm : allVMs)
    {
        if (!vm || !vm->IsValid())
            continue;

        if (!vm->IsTemplate() || vm->IsSnapshot())
            continue;

        if (!vm->Show(showHidden))
            continue;

        if (restrictVtpm)
        {
            const QString vtpmFlag = vm->Platform().value("vtpm").toString().toLower();
            if (vtpmFlag == "true")
                continue;
        }

        TemplateInfo info;
        info.ref = vm->OpaqueRef();
        info.name = vm->GetName();
        QString virtualizationType = vm->IsHVM() ? tr("HVM") : tr("PV");
        info.type = virtualizationType;
        info.description = vm->GetDescription();

        QTreeWidgetItem* item = new QTreeWidgetItem(this->ui->templateTree);
        item->setText(0, info.name);
        item->setText(1, virtualizationType);
        item->setData(0, Qt::UserRole, info.ref);
        info.item = item;

        this->m_templateItems.append(info);
    }

    if (!this->m_templateItems.isEmpty())
        this->ui->templateTree->setCurrentItem(this->m_templateItems.first().item);
}

void NewVMWizard::filterTemplates(const QString& filterText)
{
    const QString needle = filterText.trimmed();
    for (TemplateInfo& info : this->m_templateItems)
    {
        bool matches = needle.isEmpty()
            || info.name.contains(needle, Qt::CaseInsensitive)
            || info.type.contains(needle, Qt::CaseInsensitive)
            || info.description.contains(needle, Qt::CaseInsensitive);
        if (info.item)
            info.item->setHidden(!matches);
    }
}

void NewVMWizard::handleTemplateSelectionChanged()
{
    QTreeWidgetItem* current = this->ui->templateTree->currentItem();
    if (!current)
    {
        this->m_selectedTemplate.clear();
        this->ui->templateDescriptionLabel->setText(tr("Select a template to view its description."));
        return;
    }

    const QString ref = current->data(0, Qt::UserRole).toString();
    this->m_selectedTemplate = ref;

    auto it = std::find_if(this->m_templateItems.begin(), this->m_templateItems.end(),
                           [ref](const TemplateInfo& info) { return info.ref == ref; });
    if (it != this->m_templateItems.end())
    {
        QString desc = it->description.trimmed();
        if (desc.isEmpty())
            desc = tr("No description provided.");
        this->ui->templateDescriptionLabel->setText(desc);
        const QString currentName = this->ui->vmNameEdit->text().trimmed();
        if (!this->m_vmNameDirty || currentName.isEmpty() || currentName == this->m_lastTemplateName)
        {
            QSignalBlocker blocker(this->ui->vmNameEdit);
            this->m_settingVmName = true;
            this->ui->vmNameEdit->setText(it->name);
            this->m_lastTemplateName = it->name;
            this->m_vmNameDirty = false;
            this->m_settingVmName = false;
        }
    }

    XenCache* cache = this->cache();
    this->m_selectedTemplateRecord = cache ? cache->ResolveObjectData(XenObjectType::VM, ref) : QVariantMap();
    if (!this->m_selectedTemplateRecord.isEmpty())
    {
        long vcpusMax = this->m_selectedTemplateRecord.value("VCPUs_max", 1).toLongLong();
        long vcpusStartup = this->m_selectedTemplateRecord.value("VCPUs_at_startup", 1).toLongLong();
        qint64 memStaticMax = this->m_selectedTemplateRecord.value("memory_static_max").toLongLong() / (1024 * 1024);
        qint64 memDynMax = this->m_selectedTemplateRecord.value("memory_dynamic_max").toLongLong() / (1024 * 1024);
        qint64 memDynMin = this->m_selectedTemplateRecord.value("memory_dynamic_min").toLongLong() / (1024 * 1024);
        QVariantMap platform = this->m_selectedTemplateRecord.value("platform").toMap();
        qint64 coresPerSocket = platform.value("cores-per-socket").toString().toLongLong();
        if (coresPerSocket <= 0)
            coresPerSocket = 1;

        this->ui->vcpusMaxSpin->setValue(int(vcpusMax));
        this->ui->vcpusStartupSpin->setMaximum(int(vcpusMax));
        this->ui->vcpusStartupSpin->setValue(int(vcpusStartup));

        this->ui->memoryStaticMaxSpin->setValue(int(memStaticMax));
        this->ui->memoryDynamicMaxSpin->setMaximum(int(memStaticMax));
        this->ui->memoryDynamicMaxSpin->setValue(int(memDynMax));
        this->ui->memoryDynamicMinSpin->setMaximum(int(memDynMax));
        this->ui->memoryDynamicMinSpin->setValue(int(memDynMin));

        int coresIndex = this->ui->coresPerSocketCombo->findData(int(coresPerSocket));
        if (coresIndex == -1)
        {
            this->ui->coresPerSocketCombo->addItem(QString::number(coresPerSocket), int(coresPerSocket));
            coresIndex = this->ui->coresPerSocketCombo->findData(int(coresPerSocket));
        }
        if (coresIndex >= 0)
            this->ui->coresPerSocketCombo->setCurrentIndex(coresIndex);

        this->m_vcpuCount = vcpusStartup;
        this->m_vcpuMax = int(vcpusMax);
        this->m_memorySize = int(memStaticMax);
        this->m_memoryDynamicMin = int(memDynMin);
        this->m_memoryDynamicMax = int(memDynMax);
        this->m_memoryStaticMax = int(memStaticMax);
        this->m_coresPerSocket = int(coresPerSocket);
        this->m_originalVcpuAtStartup = int(vcpusStartup);
        this->m_originalCoresPerSocket = int(coresPerSocket);
    }

    this->loadTemplateDevices();
    this->updateVcpuControls();

    const QVariantMap otherConfig = this->m_selectedTemplateRecord.value("other_config").toMap();
    const bool isDefaultTemplate = otherConfig.contains("default_template");
    if (isDefaultTemplate && this->ui->copyBiosStringsCheckBox->isChecked())
    {
        this->ui->autoHomeServerRadio->setChecked(true);
        this->ui->specificHomeServerRadio->setEnabled(false);
        this->ui->homeServerList->setEnabled(false);
        this->ui->copyBiosStringsFromAffinityCheckBox->setEnabled(false);
    }
    else
    {
        this->ui->specificHomeServerRadio->setEnabled(true);
        this->updateHomeServerControls(this->ui->specificHomeServerRadio->isChecked());
    }
}

void NewVMWizard::loadTemplateDevices()
{
    this->m_disks.clear();
    this->m_networks.clear();

    XenCache* cache = this->cache();
    if (!cache || this->m_selectedTemplate.isEmpty())
    {
        this->updateDiskTable();
        this->updateNetworkTable();
        return;
    }

    // Get template record from cache
    QVariantMap templateRecord = cache->ResolveObjectData(XenObjectType::VM, this->m_selectedTemplate);
    if (templateRecord.isEmpty())
    {
        this->updateDiskTable();
        this->updateNetworkTable();
        return;
    }

    QDomElement provisionRoot;
    if (cache)
    {
        QSharedPointer<VM> templateVm = cache->ResolveObject<VM>(XenObjectType::VM, this->m_selectedTemplate);
        if (templateVm)
            provisionRoot = templateVm->ProvisionXml();
    }

    if (provisionRoot.isNull())
    {
        const QVariantMap otherConfig = templateRecord.value("other_config").toMap();
        const QString provisionXml = otherConfig.value("disks").toString();
        if (!provisionXml.isEmpty())
        {
            QDomDocument doc;
            if (doc.setContent(provisionXml))
                provisionRoot = doc.documentElement();
        }
    }

    if (!provisionRoot.isNull())
    {
        QDomNodeList disks = provisionRoot.elementsByTagName("disk");
        QString namePrefix = this->ui->vmNameEdit->text().trimmed();
        if (namePrefix.isEmpty())
            namePrefix = templateRecord.value("name_label").toString();

        for (int i = 0; i < disks.count(); ++i)
        {
            QDomElement diskEl = disks.at(i).toElement();
            if (diskEl.isNull())
                continue;

            DiskConfig disk;
            disk.device = diskEl.attribute("device");
            disk.bootable = (diskEl.attribute("bootable").toLower() == "true");
            disk.sizeBytes = diskEl.attribute("size").toLongLong();
            disk.vdiType = diskEl.attribute("type").toLower();
            if (disk.vdiType.isEmpty())
                disk.vdiType = "user";

            disk.name = QString("%1 Disk %2").arg(namePrefix, disk.device);
            disk.description = tr("Virtual disk");
            disk.mode = "RW";
            disk.canDelete = (disk.vdiType == "user");
            disk.canResize = true;
            disk.minSizeBytes = disk.sizeBytes;

            QString srUuid = diskEl.attribute("sr");
            if (!srUuid.isEmpty())
            {
                QList<QVariantMap> srs = cache->GetAllData(XenObjectType::SR);
                for (const QVariantMap& sr : srs)
                {
                    if (sr.value("uuid").toString() == srUuid)
                    {
                        disk.srRef = sr.value("ref").toString();
                        break;
                    }
                }
            }
            if (disk.srRef.isEmpty())
                disk.srRef = this->ui->defaultSrCombo->currentData().toString();

            this->m_disks.append(disk);
        }
    }

    if (this->m_disks.isEmpty())
    {
        // Get VBD references from template record
        QVariantList vbdRefs = templateRecord.value("VBDs").toList();
        for (const QVariant& vbdRefVar : vbdRefs)
        {
            QString vbdRef = vbdRefVar.toString();
            QVariantMap vbd = cache->ResolveObjectData(XenObjectType::VBD, vbdRef);

            QString vbdType = vbd.value("type").toString();
            if (vbdType != "Disk")
                continue;

            QString vdiRef = vbd.value("VDI").toString();
            QVariantMap vdiData = cache->ResolveObjectData(XenObjectType::VDI, vdiRef);

            DiskConfig disk;
            disk.vdiRef = vdiRef;
            disk.srRef = vdiData.value("SR").toString();
            disk.sizeBytes = vdiData.value("virtual_size").toLongLong();
            disk.device = vbd.value("userdevice").toString();
            disk.bootable = vbd.value("bootable").toBool();
            disk.name = vdiData.value("name_label").toString();
            disk.description = vdiData.value("name_description").toString();
            disk.mode = vbd.value("mode").toString();
            disk.vdiType = vdiData.value("type").toString().toLower();
            disk.sharable = vdiData.value("sharable", false).toBool();
            disk.readOnly = vdiData.value("read_only", false).toBool();
            disk.canDelete = false;
            disk.canResize = false;
            disk.minSizeBytes = 0;
            this->m_disks.append(disk);
        }
    }

    bool isDefaultTemplate = false;
    if (cache)
    {
        QSharedPointer<VM> templateVm = cache->ResolveObject<VM>(XenObjectType::VM, this->m_selectedTemplate);
        if (templateVm)
            isDefaultTemplate = templateVm->DefaultTemplate();
    }

    if (isDefaultTemplate)
    {
        const bool showHidden = SettingsManager::instance().GetShowHiddenObjects();
        QStringList networkRefs = cache->GetAllRefs(XenObjectType::Network);
        int deviceIndex = 0;

        for (const QString& networkRef : networkRefs)
        {
            QVariantMap networkData = cache->ResolveObjectData(XenObjectType::Network, networkRef);
            QVariantMap otherConfig = networkData.value("other_config", QVariantMap()).toMap();
            const QString nameLabel = networkData.value("name_label").toString();

            if (otherConfig.value("is_guest_installer_network", "false").toString() == "true")
                continue;
            if (!showHidden && otherConfig.value("HideFromXenCenter", "false").toString() == "true")
                continue;
            if (nameLabel.isEmpty())
                continue;

            const QString autoplug = otherConfig.value("automatic", "false").toString();
            if (autoplug == "false")
                continue;

            NetworkConfig network;
            network.networkRef = networkRef;
            network.device = QString::number(deviceIndex++);
            network.mac = QString();
            this->m_networks.append(network);
        }
    }
    else
    {
        // Get VIF references from template record
        QVariantList vifRefs = templateRecord.value("VIFs").toList();
        for (const QVariant& vifRefVar : vifRefs)
        {
            QString vifRef = vifRefVar.toString();
            QVariantMap vif = cache->ResolveObjectData(XenObjectType::VIF, vifRef);
            
            NetworkConfig network;
            network.networkRef = vif.value("network").toString();
            network.device = vif.value("device").toString();
            network.mac = vif.value("MAC").toString();
            this->m_networks.append(network);
        }
    }

    this->updateDiskTable();
    this->updateNetworkTable();
}

void NewVMWizard::loadHosts()
{
    XenCache* cache = this->cache();
    if (!cache)
        return;

    this->ui->homeServerList->clear();
    this->m_hosts.clear();

    QList<QVariantMap> hosts = cache->GetAllData(XenObjectType::Host);
    for (const QVariantMap& host : hosts)
    {
        HostInfo info;
        info.ref = host.value("ref").toString();
        info.name = host.value("name_label").toString();
        info.hostname = host.value("hostname").toString();
        this->m_hosts.append(info);

        QListWidgetItem* item = new QListWidgetItem(
            QString("%1 (%2)").arg(info.name, info.hostname), this->ui->homeServerList);
        item->setData(Qt::UserRole, info.ref);
    }
}

void NewVMWizard::loadStorageRepositories()
{
    XenCache* cache = this->cache();
    if (!cache)
        return;

    this->ui->defaultSrCombo->clear();
    this->m_storageRepositories.clear();

    QList<QVariantMap> srs = cache->GetAllData(XenObjectType::SR);
    for (const QVariantMap& sr : srs)
    {
        StorageRepositoryInfo info;
        info.ref = sr.value("ref").toString();
        info.name = sr.value("name_label").toString();
        info.type = sr.value("type").toString();
        this->m_storageRepositories.append(info);
        this->ui->defaultSrCombo->addItem(QString("%1 (%2)").arg(info.name, info.type), info.ref);
    }

    if (this->ui->defaultSrCombo->count() == 0)
        this->ui->defaultSrCombo->addItem(tr("No storage repositories available"), QString());

    QString initialSr = this->ui->defaultSrCombo->currentData().toString();
    if (!initialSr.isEmpty())
        this->applyDefaultSRToDisks(initialSr);
}

void NewVMWizard::loadNetworks()
{
    XenCache* cache = this->cache();
    if (!cache)
        return;

    this->m_availableNetworks.clear();

    QList<QVariantMap> nets = cache->GetAllData(XenObjectType::Network);
    for (const QVariantMap& net : nets)
    {
        NetworkInfo info;
        info.ref = net.value("ref").toString();
        info.name = net.value("name_label").toString();
        this->m_availableNetworks.append(info);
    }
}

void NewVMWizard::updateDiskTable()
{
    this->ui->diskTable->clearContents();
    this->ui->diskTable->setRowCount(this->m_disks.size());

    int row = 0;
    for (const DiskConfig& disk : this->m_disks)
    {
        XenCache* cache = this->cache();
        QVariantMap srRecord = cache ? cache->ResolveObjectData(XenObjectType::SR, disk.srRef) : QVariantMap();
        QString srName = srRecord.value("name_label").toString();
        QString sizeGB = QString::number(double(disk.sizeBytes) / (1024.0 * 1024.0 * 1024.0), 'f', 1);

        QString diskLabel = disk.name.isEmpty()
            ? QString("Disk %1").arg(disk.device)
            : disk.name;
        if (disk.bootable)
            diskLabel += tr(" (boot)");

        auto diskItem = new QTableWidgetItem(diskLabel);
        auto sizeItem = new QTableWidgetItem(tr("%1 GB").arg(sizeGB));
        auto srItem = new QTableWidgetItem(srName.isEmpty() ? tr("Unknown SR") : srName);
        auto modeItem = new QTableWidgetItem(disk.mode.isEmpty() ? tr("RW") : disk.mode);

        this->ui->diskTable->setItem(row, 0, diskItem);
        this->ui->diskTable->setItem(row, 1, sizeItem);
        this->ui->diskTable->setItem(row, 2, srItem);
        this->ui->diskTable->setItem(row, 3, modeItem);
        ++row;
    }
}

void NewVMWizard::updateNetworkTable()
{
    this->ui->networkTable->clearContents();
    this->ui->networkTable->setRowCount(this->m_networks.size());

    int row = 0;
    for (const NetworkConfig& network : this->m_networks)
    {
        XenCache* cache = this->cache();
        QVariantMap networkRecord = cache ? cache->ResolveObjectData(XenObjectType::Network, network.networkRef) : QVariantMap();
        QString networkName = networkRecord.value("name_label").toString();

        auto deviceItem = new QTableWidgetItem(network.device);
        auto networkItem = new QTableWidgetItem(networkName.isEmpty() ? tr("Unknown network") : networkName);
        auto macItem = new QTableWidgetItem(network.mac.isEmpty() ? tr("Auto") : network.mac);

        this->ui->networkTable->setItem(row, 0, deviceItem);
        this->ui->networkTable->setItem(row, 1, networkItem);
        this->ui->networkTable->setItem(row, 2, macItem);
        ++row;
    }
}

void NewVMWizard::updateHomeServerPage()
{
    XenCache* cache = this->cache();
    if (cache && !this->m_selectedTemplate.isEmpty())
    {
        QString suggestedHost;
        bool usingCd = this->ui->isoRadioButton->isChecked();
        QString isoVdiRef;
        if (usingCd)
        {
            IsoDropDownBox* isoBox = qobject_cast<IsoDropDownBox*>(this->ui->isoComboBox);
            isoVdiRef = isoBox ? isoBox->SelectedVdiRef() : this->ui->isoComboBox->currentData().toString();
        }

        if (!usingCd || isoVdiRef.isEmpty())
        {
            suggestedHost = VMHelpers::getVMStorageHost(this->m_connection, this->m_selectedTemplateRecord, true);
        }
        else
        {
            QVariantMap vdiData = cache->ResolveObjectData(XenObjectType::VDI, isoVdiRef);
            QString srRef = vdiData.value("SR").toString();
            QSharedPointer<SR> sr = cache->ResolveObject<SR>(XenObjectType::SR, srRef);
            Host* host = sr ? sr->GetFirstAttachedStorageHost() : nullptr;
            if (host)
                suggestedHost = host->OpaqueRef();
            if (suggestedHost.isEmpty())
                suggestedHost = VMHelpers::getVMStorageHost(this->m_connection, this->m_selectedTemplateRecord, false);
        }

        if (!suggestedHost.isEmpty() && this->ui->homeServerList->selectedItems().isEmpty())
        {
            for (int i = 0; i < this->ui->homeServerList->count(); ++i)
            {
                QListWidgetItem* item = this->ui->homeServerList->item(i);
                if (item && item->data(Qt::UserRole).toString() == suggestedHost)
                {
                    item->setSelected(true);
                    break;
                }
            }
        }
    }
}

void NewVMWizard::updateSummaryPage()
{
    QString templateName;
    auto it = std::find_if(this->m_templateItems.begin(), this->m_templateItems.end(),
                           [this](const TemplateInfo& info) { return info.ref == this->m_selectedTemplate; });
    if (it != this->m_templateItems.end())
        templateName = it->name;

    QStringList lines;
    lines << tr("Template: %1").arg(templateName.isEmpty() ? tr("None selected") : templateName);
    lines << tr("Name: %1").arg(this->ui->vmNameEdit->text().trimmed());
    if (this->m_supportsVcpuHotplug)
    {
        lines << tr("vCPUs: %1 (max %2)")
                     .arg(this->ui->vcpusStartupSpin->value())
                     .arg(this->ui->vcpusMaxSpin->value());
    }
    else
    {
        lines << tr("vCPUs: %1").arg(this->ui->vcpusMaxSpin->value());
    }
    lines << tr("Topology: %1").arg(this->ui->coresPerSocketCombo->currentText());
    lines << tr("Memory: %1 MiB (dynamic %2-%3)")
                 .arg(this->ui->memoryStaticMaxSpin->value())
                 .arg(this->ui->memoryDynamicMinSpin->value())
                 .arg(this->ui->memoryDynamicMaxSpin->value());
    lines << tr("Disks: %1").arg(this->m_disks.size());
    lines << tr("Networks: %1").arg(this->m_networks.size());

    QString installMethod;
    if (this->ui->isoRadioButton->isChecked())
    {
        installMethod = this->ui->isoComboBox->currentText();
    } else
    {
        installMethod = this->ui->urlLineEdit->text().trimmed();
    }
    lines << tr("Installation source: %1").arg(installMethod.isEmpty() ? tr("Not specified") : installMethod);

    this->ui->summaryTextBrowser->setPlainText(lines.join('\n'));
}

void NewVMWizard::updateHomeServerControls(bool enableSelection)
{
    this->ui->homeServerList->setEnabled(enableSelection);
    this->ui->copyBiosStringsFromAffinityCheckBox->setEnabled(enableSelection);
}

void NewVMWizard::updateIsoControls()
{
    bool isoMode = this->ui->isoRadioButton->isChecked();
    this->ui->isoComboBox->setEnabled(isoMode);
    this->ui->attachIsoButton->setEnabled(isoMode);
    this->ui->urlLineEdit->setEnabled(!isoMode);
}

void NewVMWizard::updateVcpuControls()
{
    XenCache* cache = this->cache();
    if (!cache || this->m_selectedTemplate.isEmpty())
        return;

    QSharedPointer<VM> templateVm = cache->ResolveObject<VM>(XenObjectType::VM, this->m_selectedTemplate);
    if (!templateVm)
        return;

    this->m_supportsVcpuHotplug = templateVm->SupportsVCPUHotplug();
    this->m_minVcpus = qMax(1, templateVm->MinVCPUs());
    this->m_maxVcpusAllowed = qMax(this->m_minVcpus, templateVm->MaxVCPUsAllowed());
    this->m_maxCoresPerSocket = qMax(1, int(templateVm->MaxCoresPerSocket()));

    this->ui->vcpusStartupLabel->setVisible(this->m_supportsVcpuHotplug);
    this->ui->vcpusStartupSpin->setVisible(this->m_supportsVcpuHotplug);
    this->ui->vcpusMaxLabel->setText(this->m_supportsVcpuHotplug
        ? tr("Maximum vCPUs:")
        : tr("vCPUs:"));

    this->ui->vcpusMaxSpin->setMinimum(this->m_minVcpus);
    this->ui->vcpusMaxSpin->setMaximum(this->m_maxVcpusAllowed);
    this->ui->vcpusStartupSpin->setMinimum(this->m_minVcpus);
    this->ui->vcpusStartupSpin->setMaximum(this->ui->vcpusMaxSpin->value());

    this->updateTopologyOptions(this->ui->vcpusMaxSpin->value());
    this->enforceVcpuTopology();
}

void NewVMWizard::enforceVcpuTopology()
{
    if (!this->isValidVcpu(this->ui->vcpusMaxSpin->value()))
    {
        int maxVcpus = this->ui->vcpusMaxSpin->maximum();
        int minVcpus = this->ui->vcpusMaxSpin->minimum();
        int candidate = this->ui->vcpusMaxSpin->value();

        while (candidate <= maxVcpus && !this->isValidVcpu(candidate))
            ++candidate;
        if (candidate > maxVcpus)
        {
            candidate = this->ui->vcpusMaxSpin->value();
            while (candidate >= minVcpus && !this->isValidVcpu(candidate))
                --candidate;
        }
        if (candidate >= minVcpus && candidate <= maxVcpus)
            this->ui->vcpusMaxSpin->setValue(candidate);
    }

    if (this->m_supportsVcpuHotplug)
    {
        this->ui->vcpusStartupSpin->setMaximum(this->ui->vcpusMaxSpin->value());
        if (this->ui->vcpusStartupSpin->value() > this->ui->vcpusMaxSpin->value())
            this->ui->vcpusStartupSpin->setValue(this->ui->vcpusMaxSpin->value());
    } else
    {
        this->ui->vcpusStartupSpin->setValue(this->ui->vcpusMaxSpin->value());
    }
}

void NewVMWizard::updateTopologyOptions(int vcpusMax)
{
    QSignalBlocker blocker(this->ui->coresPerSocketCombo);
    this->ui->coresPerSocketCombo->clear();

    int maxCores = this->m_maxCoresPerSocket > 0 ? qMin(vcpusMax, this->m_maxCoresPerSocket) : vcpusMax;
    for (int cores = 1; cores <= maxCores; ++cores)
    {
        if (vcpusMax % cores != 0)
            continue;

        int sockets = vcpusMax / cores;
        if (sockets > VM::MAX_SOCKETS)
            continue;

        this->ui->coresPerSocketCombo->addItem(VM::GetTopology(sockets, cores), cores);
    }

    if (this->m_originalVcpuAtStartup == vcpusMax &&
        this->ui->coresPerSocketCombo->findData(this->m_originalCoresPerSocket) == -1)
    {
        this->ui->coresPerSocketCombo->addItem(
            VM::GetTopology(0, this->m_originalCoresPerSocket),
            this->m_originalCoresPerSocket);
    }

    int currentCores = this->m_coresPerSocket;
    int coresIndex = this->ui->coresPerSocketCombo->findData(currentCores);
    if (coresIndex < 0)
        coresIndex = 0;
    if (coresIndex >= 0)
        this->ui->coresPerSocketCombo->setCurrentIndex(coresIndex);

    this->m_coresPerSocket = this->ui->coresPerSocketCombo->currentData().toInt();
}

bool NewVMWizard::isValidVcpu(int vcpus) const
{
    if (vcpus <= 0)
        return false;

    int maxCores = this->m_maxCoresPerSocket > 0 ? qMin(vcpus, this->m_maxCoresPerSocket) : vcpus;
    for (int cores = 1; cores <= maxCores; ++cores)
    {
        if (vcpus % cores != 0)
            continue;
        int sockets = vcpus / cores;
        if (sockets <= VM::MAX_SOCKETS)
            return true;
    }
    return false;
}

void NewVMWizard::applyDefaultSRToDisks(const QString& srRef)
{
    if (srRef.isEmpty())
        return;

    for (DiskConfig& disk : this->m_disks)
        disk.srRef = srRef;

    this->updateDiskTable();
}

void NewVMWizard::updateNavigationSelection()
{
    if (this->m_navigationPane)
        this->m_navigationPane->setCurrentStep(currentId());
}

void NewVMWizard::initializePage(int id)
{
    if (id == Page_Storage)
        this->updateDiskTable();
    else if (id == Page_Network)
        this->updateNetworkTable();
    else if (id == Page_HomeServer)
        this->updateHomeServerPage();
    else if (id == Page_Finish)
        this->updateSummaryPage();

    QWizard::initializePage(id);
}

bool NewVMWizard::validateCurrentPage()
{
    switch (currentId())
    {
    case Page_Template:
        if (this->m_selectedTemplate.isEmpty())
        {
            QMessageBox::warning(this, tr("Select Template"),
                                 tr("Please select a template before continuing."));
            return false;
        }
        break;
    case Page_Name:
        if (this->ui->vmNameEdit->text().trimmed().isEmpty())
        {
            QMessageBox::warning(this, tr("Enter Name"),
                                 tr("Please provide a name for the virtual machine."));
            return false;
        }
        break;
    case Page_InstallationMedia:
        if (this->ui->urlRadioButton->isChecked() && this->ui->urlLineEdit->text().trimmed().isEmpty())
        {
            QMessageBox::warning(this, tr("Installation Source"),
                                 tr("Specify the URL for the installation media."));
            return false;
        }
        break;
    case Page_HomeServer:
        if (this->ui->specificHomeServerRadio->isChecked() && this->ui->homeServerList->selectedItems().isEmpty())
        {
            QMessageBox::warning(this, tr("Select Home Server"),
                                 tr("Choose a home server or allow automatic placement."));
            return false;
        }
        break;
    case Page_CpuMemory:
    {
        int vcpusMax = this->ui->vcpusMaxSpin->value();
        if (!this->isValidVcpu(vcpusMax))
        {
            QMessageBox::warning(this, tr("CPU Topology"),
                                 tr("The selected vCPU count has no valid topology. Adjust the vCPU count."));
            return false;
        }

        int dynMin = this->ui->memoryDynamicMinSpin->value();
        int dynMax = this->ui->memoryDynamicMaxSpin->value();
        int staticMax = this->ui->memoryStaticMaxSpin->value();
        if (!(dynMin <= dynMax && dynMax <= staticMax))
        {
            QMessageBox::warning(this, tr("Memory Configuration"),
                                 tr("Ensure dynamic min ≤ dynamic max ≤ static max."));
            return false;
        }
        break;
    }
    case Page_Storage:
        if (this->ui->disklessCheckBox->isChecked())
            break;
        if (this->m_disks.isEmpty())
        {
            QMessageBox::warning(this, tr("Storage Configuration"),
                                 tr("The selected template has no disks. Add a disk before proceeding."));
            return false;
        }
        for (const DiskConfig& disk : this->m_disks)
        {
            if (disk.srRef.isEmpty())
            {
                QMessageBox::warning(this, tr("Storage Configuration"),
                                     tr("One or more disks have no storage repository selected."));
                return false;
            }
        }
        break;
    default:
        break;
    }

    return QWizard::validateCurrentPage();
}

void NewVMWizard::accept()
{
    this->m_vmName = this->ui->vmNameEdit->text().trimmed();
    this->m_vmDescription = this->ui->vmDescriptionEdit->toPlainText().trimmed();
    this->m_vcpuCount = this->m_supportsVcpuHotplug
        ? this->ui->vcpusStartupSpin->value()
        : this->ui->vcpusMaxSpin->value();
    this->m_vcpuMax = this->ui->vcpusMaxSpin->value();
    this->m_coresPerSocket = this->ui->coresPerSocketCombo->currentData().toInt();
    this->m_memoryDynamicMin = this->ui->memoryDynamicMinSpin->value();
    this->m_memoryDynamicMax = this->ui->memoryDynamicMaxSpin->value();
    this->m_memoryStaticMax = this->ui->memoryStaticMaxSpin->value();
    this->m_memorySize = this->m_memoryStaticMax;
    this->m_assignVtpm = this->ui->assignVtpmCheckBox->isChecked();
    this->m_installUrl = this->ui->urlRadioButton->isChecked() ? this->ui->urlLineEdit->text().trimmed() : QString();
    this->m_selectedIso = QString();
    if (this->ui->isoRadioButton->isChecked())
    {
        IsoDropDownBox* isoBox = qobject_cast<IsoDropDownBox*>(this->ui->isoComboBox);
        this->m_selectedIso = isoBox ? isoBox->SelectedVdiRef() : this->ui->isoComboBox->currentData().toString();
    }
    this->m_bootMode = this->ui->bootModeComboBox->currentData().toString();
    this->m_pvArgs = this->ui->pvBootArgsEdit->toPlainText().trimmed();

    if (this->ui->specificHomeServerRadio->isChecked() && !this->ui->homeServerList->selectedItems().isEmpty())
        this->m_selectedHost = this->ui->homeServerList->selectedItems().first()->data(Qt::UserRole).toString();
    else
        this->m_selectedHost.clear();

    this->createVirtualMachine();
    QWizard::accept();
}

void NewVMWizard::createVirtualMachine()
{
    if (!this->m_connection)
    {
        QMessageBox::critical(this, tr("Error"), tr("Xen connection not available"));
        return;
    }

    bool startImmediately = this->ui->startImmediatelyCheckBox->isChecked();

    if (this->m_selectedTemplate.isEmpty())
    {
        QMessageBox::warning(this, tr("No Template Selected"),
                             tr("Please select a template to create the VM from."));
        return;
    }

    XenConnection* connection = this->m_connection;
    if (!connection->GetSession())
    {
        QMessageBox::critical(this, tr("Connection Error"),
                              tr("Unable to configure devices because the Xen connection is no longer valid."));
        return;
    }
    CreateVMAction::InstallMethod installMethod = CreateVMAction::InstallMethod::None;
    if (!this->m_installUrl.isEmpty())
        installMethod = CreateVMAction::InstallMethod::Network;
    else if (!this->m_selectedIso.isEmpty())
        installMethod = CreateVMAction::InstallMethod::CD;

    CreateVMAction::BootMode bootMode = CreateVMAction::BootMode::Auto;
    if (this->m_bootMode == "bios")
        bootMode = CreateVMAction::BootMode::Bios;
    else if (this->m_bootMode == "uefi")
        bootMode = CreateVMAction::BootMode::Uefi;
    else if (this->m_bootMode == "secureboot")
        bootMode = CreateVMAction::BootMode::SecureUefi;

    QList<CreateVMAction::DiskConfig> disks;
    if (!this->ui->disklessCheckBox->isChecked())
    {
        for (const DiskConfig& disk : this->m_disks)
        {
            CreateVMAction::DiskConfig config;
            config.vdiRef = disk.vdiRef;
            config.srRef = disk.srRef;
            config.sizeBytes = disk.sizeBytes;
            config.device = disk.device;
            config.bootable = disk.bootable;
            config.nameLabel = disk.name;
            config.nameDescription = disk.description;
            config.mode = disk.mode;
            config.vdiType = disk.vdiType;
            config.sharable = disk.sharable;
            config.readOnly = disk.readOnly;
            disks.append(config);
        }
    }

    QList<CreateVMAction::VifConfig> vifs;
    for (const NetworkConfig& network : this->m_networks)
    {
        CreateVMAction::VifConfig config;
        config.networkRef = network.networkRef;
        config.device = network.device;
        config.mac = network.mac;
        vifs.append(config);
    }

    CreateVMAction* action = new CreateVMAction(
        connection,
        this->m_selectedTemplate,
        this->m_vmName,
        this->m_vmDescription,
        installMethod,
        this->m_pvArgs,
        this->m_selectedIso,
        this->m_installUrl,
        bootMode,
        this->m_selectedHost,
        this->m_vcpuMax,
        this->m_vcpuCount,
        this->m_memoryDynamicMin,
        this->m_memoryDynamicMax,
        this->m_memoryStaticMax,
        this->m_coresPerSocket,
        disks,
        vifs,
        startImmediately,
        this->m_assignVtpm,
        this);

    ActionProgressDialog* progressDialog = new ActionProgressDialog(action, this);
    progressDialog->setAttribute(Qt::WA_DeleteOnClose);

    int result = progressDialog->exec();
    if (result != QDialog::Accepted || action->HasError())
    {
        QString error = action->GetErrorMessage();
        QString step = action->GetDescription();
        QStringList details = action->GetErrorDetails();
        if (error.isEmpty())
            error = tr("Failed to create virtual machine '%1'.").arg(this->m_vmName);
        if (!step.isEmpty())
            error += tr("\n\nStep: %1").arg(step);
        if (!details.isEmpty())
            error += tr("\n\nDetails:\n- %1").arg(details.join("\n- "));
        QMessageBox::critical(this, tr("Failed to Create VM"), error);
        action->deleteLater();
        return;
    }

    action->deleteLater();

    QString message = tr("Virtual machine '%1' has been created successfully.").arg(this->m_vmName);
    if (startImmediately)
        message += tr("\n\nThe VM has been started.");
    MainWindow::instance()->ShowStatusMessage(message);
}

void NewVMWizard::onCurrentIdChanged(int id)
{
    if (id == Page_Finish)
        this->updateSummaryPage();
    this->updateNavigationSelection();
}

void NewVMWizard::onVmNameChanged(const QString& text)
{
    if (this->m_settingVmName)
        return;

    if (text.trimmed().isEmpty())
    {
        this->m_vmNameDirty = false;
        return;
    }

    this->m_vmNameDirty = (text.trimmed() != this->m_lastTemplateName);
}

void NewVMWizard::onAutoHomeServerToggled(bool checked)
{
    Q_UNUSED(checked)
    this->updateHomeServerControls(this->ui->specificHomeServerRadio->isChecked());
}

void NewVMWizard::onSpecificHomeServerToggled(bool checked)
{
    this->updateHomeServerControls(checked);
}

void NewVMWizard::onCopyBiosStringsToggled(bool checked)
{
    Q_UNUSED(checked)
    const QVariantMap otherConfig = this->m_selectedTemplateRecord.value("other_config").toMap();
    const bool isDefaultTemplate = otherConfig.contains("default_template");
    if (isDefaultTemplate && this->ui->copyBiosStringsCheckBox->isChecked())
    {
        this->ui->autoHomeServerRadio->setChecked(true);
        this->ui->specificHomeServerRadio->setEnabled(false);
        this->ui->homeServerList->setEnabled(false);
        this->ui->copyBiosStringsFromAffinityCheckBox->setEnabled(false);
    } else
    {
        this->ui->specificHomeServerRadio->setEnabled(true);
        this->updateHomeServerControls(this->ui->specificHomeServerRadio->isChecked());
    }
}

void NewVMWizard::onVcpusMaxChanged(int value)
{
    if (this->m_supportsVcpuHotplug)
    {
        this->ui->vcpusStartupSpin->setMaximum(value);
        if (this->ui->vcpusStartupSpin->value() > value)
            this->ui->vcpusStartupSpin->setValue(value);
    }
    else
    {
        this->ui->vcpusStartupSpin->setValue(value);
    }

    this->enforceVcpuTopology();
    this->updateTopologyOptions(this->ui->vcpusMaxSpin->value());
}

void NewVMWizard::onCoresPerSocketChanged(int index)
{
    Q_UNUSED(index);
    this->m_coresPerSocket = this->ui->coresPerSocketCombo->currentData().toInt();
}

void NewVMWizard::onMemoryStaticMaxChanged(int value)
{
    this->ui->memoryDynamicMaxSpin->setMaximum(value);
    if (this->ui->memoryDynamicMaxSpin->value() > value)
        this->ui->memoryDynamicMaxSpin->setValue(value);
}

void NewVMWizard::onMemoryDynamicMaxChanged(int value)
{
    this->ui->memoryDynamicMinSpin->setMaximum(value);
    if (this->ui->memoryDynamicMinSpin->value() > value)
        this->ui->memoryDynamicMinSpin->setValue(value);
}

void NewVMWizard::onIsoRadioToggled(bool checked)
{
    Q_UNUSED(checked)
    this->updateIsoControls();
}

void NewVMWizard::onUrlRadioToggled(bool checked)
{
    Q_UNUSED(checked)
    this->updateIsoControls();
}

void NewVMWizard::onDefaultSrChanged(int index)
{
    QString srRef = this->ui->defaultSrCombo->itemData(index).toString();
    if (!srRef.isEmpty())
        this->applyDefaultSRToDisks(srRef);
}

void NewVMWizard::onDisklessToggled(bool checked)
{
    bool enableDisks = !checked;
    this->ui->diskTable->setEnabled(enableDisks);
    this->ui->addDiskButton->setEnabled(enableDisks);
    this->ui->editDiskButton->setEnabled(enableDisks && !this->ui->diskTable->selectedItems().isEmpty());
    this->ui->removeDiskButton->setEnabled(enableDisks && !this->ui->diskTable->selectedItems().isEmpty());
    this->ui->storageOptionsGroup->setEnabled(enableDisks);
}

void NewVMWizard::onDiskTableSelectionChanged()
{
    if (this->ui->disklessCheckBox->isChecked())
    {
        this->ui->editDiskButton->setEnabled(false);
        this->ui->removeDiskButton->setEnabled(false);
        return;
    }

    bool hasSelection = !this->ui->diskTable->selectedItems().isEmpty();
    this->ui->editDiskButton->setEnabled(hasSelection);
    if (!hasSelection)
    {
        this->ui->removeDiskButton->setEnabled(false);
        return;
    }

    int row = this->ui->diskTable->currentRow();
    if (row >= 0 && row < this->m_disks.size())
        this->ui->removeDiskButton->setEnabled(this->m_disks[row].canDelete);
    else
        this->ui->removeDiskButton->setEnabled(false);
}

void NewVMWizard::onAddDiskClicked()
{
    QStringList usedDevices;
    usedDevices.reserve(this->m_disks.size());
    for (const DiskConfig& disk : this->m_disks)
        usedDevices.append(disk.device);

    QString defaultName = this->ui->vmNameEdit->text().trimmed();
    if (defaultName.isEmpty())
        defaultName = this->m_selectedTemplateRecord.value("name_label").toString();

    NewVirtualDiskDialog dialog(this->m_connection, QString(), this);
    dialog.setDialogMode(NewVirtualDiskDialog::DialogMode::Add);
    dialog.setWizardContext(defaultName, usedDevices, this->m_selectedHost);
    dialog.setInitialDisk(QString(),
                          QString(),
                          static_cast<qint64>(8) * 1024 * 1024 * 1024,
                          this->ui->defaultSrCombo->currentData().toString());
    dialog.setMinSizeBytes(0);
    dialog.setCanResize(true);
    if (dialog.exec() != QDialog::Accepted)
        return;

    DiskConfig disk;
    disk.name = dialog.getVDIName();
    disk.description = dialog.getVDIDescription();
    disk.srRef = dialog.getSelectedSR();
    disk.sizeBytes = dialog.getSize();
    disk.device = dialog.getDevicePosition();
    disk.bootable = false;
    disk.mode = dialog.getMode();
    disk.vdiType = "user";
    disk.readOnly = false;
    disk.sharable = false;
    disk.canDelete = true;
    disk.canResize = true;
    disk.minSizeBytes = 0;
    this->m_disks.append(disk);
    this->updateDiskTable();
}

void NewVMWizard::onEditDiskClicked()
{
    int row = this->ui->diskTable->currentRow();
    if (row < 0 || row >= this->m_disks.size())
        return;

    DiskConfig& disk = this->m_disks[row];

    QStringList usedDevices;
    usedDevices.reserve(this->m_disks.size());
    for (const DiskConfig& entry : this->m_disks)
    {
        if (&entry != &disk)
            usedDevices.append(entry.device);
    }

    QString defaultName = this->ui->vmNameEdit->text().trimmed();
    if (defaultName.isEmpty())
        defaultName = this->m_selectedTemplateRecord.value("name_label").toString();

    NewVirtualDiskDialog dialog(this->m_connection, QString(), this);
    dialog.setDialogMode(NewVirtualDiskDialog::DialogMode::Edit);
    dialog.setWizardContext(defaultName, usedDevices, this->m_selectedHost);
    dialog.setInitialDisk(disk.name, disk.description, disk.sizeBytes, disk.srRef);
    dialog.setMinSizeBytes(disk.minSizeBytes);
    dialog.setCanResize(disk.canResize);
    if (dialog.exec() != QDialog::Accepted)
        return;

    disk.name = dialog.getVDIName();
    disk.description = dialog.getVDIDescription();
    disk.srRef = dialog.getSelectedSR();
    if (disk.canResize)
        disk.sizeBytes = dialog.getSize();
    disk.mode = dialog.getMode();

    this->updateDiskTable();
}

void NewVMWizard::onRemoveDiskClicked()
{
    int row = this->ui->diskTable->currentRow();
    if (row < 0 || row >= this->m_disks.size())
        return;

    if (!this->m_disks[row].canDelete)
        return;

    this->m_disks.removeAt(row);
    this->updateDiskTable();
}

void NewVMWizard::onNetworkTableSelectionChanged()
{
    bool hasSelection = !this->ui->networkTable->selectedItems().isEmpty();
    this->ui->editNetworkButton->setEnabled(hasSelection);
    this->ui->removeNetworkButton->setEnabled(hasSelection);
}

void NewVMWizard::onAddNetworkClicked()
{
    if (!this->m_connection)
        return;

    int nextDeviceId = 0;
    for (const NetworkConfig& network : this->m_networks)
    {
        bool ok = false;
        int deviceId = network.device.toInt(&ok);
        if (ok && deviceId >= nextDeviceId)
            nextDeviceId = deviceId + 1;
    }

    VIFDialog dialog(this->m_connection, nextDeviceId, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QVariantMap vif = dialog.getVifSettings();
    NetworkConfig config;
    config.networkRef = vif.value("network").toString();
    config.device = vif.value("device").toString();
    config.mac = vif.value("MAC").toString();

    if (config.networkRef.isEmpty())
    {
        QMessageBox::warning(this, tr("Add NIC"), tr("Please select a network."));
        return;
    }

    this->m_networks.append(config);
    this->updateNetworkTable();
}

void NewVMWizard::onEditNetworkClicked()
{
    if (!this->m_connection)
        return;

    int row = this->ui->networkTable->currentRow();
    if (row < 0 || row >= this->m_networks.size())
        return;

    NetworkConfig& existing = this->m_networks[row];
    bool ok = false;
    int deviceId = existing.device.toInt(&ok);
    if (!ok)
        deviceId = row;

    QVariantMap vif;
    vif["network"] = existing.networkRef;
    vif["MAC"] = existing.mac;
    vif["device"] = QString::number(deviceId);
    vif["qos_algorithm_type"] = "";
    vif["qos_algorithm_params"] = QVariantMap();

    VIFDialog dialog(this->m_connection, vif, deviceId, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QVariantMap updated = dialog.getVifSettings();
    existing.networkRef = updated.value("network").toString();
    existing.device = updated.value("device").toString();
    existing.mac = updated.value("MAC").toString();

    if (existing.networkRef.isEmpty())
    {
        QMessageBox::warning(this, tr("Edit NIC"), tr("Please select a network."));
        return;
    }

    this->updateNetworkTable();
}

void NewVMWizard::onRemoveNetworkClicked()
{
    int row = this->ui->networkTable->currentRow();
    if (row < 0 || row >= this->m_networks.size())
        return;

    this->m_networks.removeAt(row);
    this->updateNetworkTable();
}

void NewVMWizard::onAttachIsoLibraryClicked()
{
    if (!this->m_connection)
    {
        QMessageBox::warning(this, tr("No Connection"),
                             tr("Unable to open the ISO library wizard because there is no active connection."));
        return;
    }

    MainWindow* mainWindow = qobject_cast<MainWindow*>(this->window());
    NewSRWizard wizard(this->m_connection, mainWindow);
    wizard.SetInitialSrType(SRType::NFS_ISO, false);

    if (wizard.exec() == QDialog::Accepted)
    {
        IsoDropDownBox* isoBox = qobject_cast<IsoDropDownBox*>(this->ui->isoComboBox);
        if (isoBox)
            isoBox->Refresh();
    }
}

void NewVMWizard::onNetworkContextMenuRequested(const QPoint& pos)
{
    QMenu menu(this);
    QAction* addAction = menu.addAction(tr("Add NIC..."));
    QAction* editAction = menu.addAction(tr("Properties..."));
    QAction* removeAction = menu.addAction(tr("Remove"));

    bool hasSelection = !this->ui->networkTable->selectedItems().isEmpty();
    editAction->setEnabled(hasSelection);
    removeAction->setEnabled(hasSelection);

    QAction* chosen = menu.exec(this->ui->networkTable->viewport()->mapToGlobal(pos));
    if (chosen == addAction)
        this->onAddNetworkClicked();
    else if (chosen == editAction)
        this->onEditNetworkClicked();
    else if (chosen == removeAction)
        this->onRemoveNetworkClicked();
}

void NewVMWizard::onDiskContextMenuRequested(const QPoint& pos)
{
    QMenu menu(this);
    QAction* addAction = menu.addAction(tr("Add..."));
    QAction* editAction = menu.addAction(tr("Edit..."));
    QAction* removeAction = menu.addAction(tr("Remove"));

    int row = this->ui->diskTable->currentRow();
    bool hasSelection = row >= 0 && row < this->m_disks.size();
    editAction->setEnabled(hasSelection);
    removeAction->setEnabled(hasSelection && this->m_disks[row].canDelete);

    QAction* chosen = menu.exec(this->ui->diskTable->viewport()->mapToGlobal(pos));
    if (chosen == addAction)
        this->onAddDiskClicked();
    else if (chosen == editAction)
        this->onEditDiskClicked();
    else if (chosen == removeAction)
        this->onRemoveDiskClicked();
}

XenCache* NewVMWizard::cache() const
{
    return m_connection ? m_connection->GetCache() : nullptr;
}
