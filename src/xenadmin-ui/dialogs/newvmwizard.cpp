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

#include "newvmwizard.h"
#include "ui_newvmwizard.h"
#include "xencache.h"
#include "xen/network/connection.h"
#include "xen/xenapi/xenapi_VM.h"
#include "operationprogressdialog.h"
#include "xen/actions/vm/createvmaction.h"
#include "../widgets/wizardnavigationpane.h"
#include <QDebug>
#include <QtWidgets>
#include <QHeaderView>
#include <algorithm>

NewVMWizard::NewVMWizard(XenConnection* connection, QWidget* parent)
    : QWizard(parent),
      m_connection(connection),
      ui(new Ui::NewVMWizard),
      m_vcpuCount(1),
      m_memorySize(1024)
{
    this->ui->setupUi(this);
    setWindowTitle(tr("New Virtual Machine Wizard"));
    setWindowIcon(QIcon(":/icons/vm-create-32.png"));

    this->setupUiPages();

    connect(this, &QWizard::currentIdChanged, this, &NewVMWizard::onCurrentIdChanged);
    connect(this->ui->templateSearchEdit, &QLineEdit::textChanged, this, &NewVMWizard::filterTemplates);
    connect(this->ui->templateTree, &QTreeWidget::itemSelectionChanged,
            this, &NewVMWizard::handleTemplateSelectionChanged);

    connect(this->ui->autoHomeServerRadio, &QRadioButton::toggled,
            this, [this](bool checked) {
                Q_UNUSED(checked)
                this->updateHomeServerControls(this->ui->specificHomeServerRadio->isChecked());
            });
    connect(this->ui->specificHomeServerRadio, &QRadioButton::toggled,
            this, [this](bool checked) { this->updateHomeServerControls(checked); });

    connect(this->ui->isoRadioButton, &QRadioButton::toggled, this,
            [this](bool /*checked*/) { this->updateIsoControls(); });
    connect(this->ui->urlRadioButton, &QRadioButton::toggled, this,
            [this](bool /*checked*/) { this->updateIsoControls(); });

    connect(this->ui->defaultSrCombo,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                QString srRef = this->ui->defaultSrCombo->itemData(index).toString();
                if (!srRef.isEmpty())
                    this->applyDefaultSRToDisks(srRef);
            });

    connect(this->ui->diskTable, &QTableWidget::itemSelectionChanged, this, [this]() {
        bool hasSelection = !this->ui->diskTable->selectedItems().isEmpty();
        this->ui->editDiskButton->setEnabled(hasSelection);
        this->ui->removeDiskButton->setEnabled(hasSelection);
    });

    connect(this->ui->networkTable, &QTableWidget::itemSelectionChanged, this, [this]() {
        bool hasSelection = !this->ui->networkTable->selectedItems().isEmpty();
        this->ui->removeNetworkButton->setEnabled(hasSelection);
    });

    this->updateIsoControls();
    this->updateHomeServerControls(false);

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
    setWizardStyle(QWizard::ModernStyle);
    setOption(QWizard::HaveHelpButton, true);
    setOption(QWizard::HelpButtonOnRight, false);

    setPage(Page_Template, this->ui->pageTemplate);
    setPage(Page_Name, this->ui->pageName);
    setPage(Page_InstallationMedia, this->ui->pageInstallation);
    setPage(Page_HomeServer, this->ui->pageHomeServer);
    setPage(Page_CpuMemory, this->ui->pageCpuMemory);
    setPage(Page_Storage, this->ui->pageStorage);
    setPage(Page_Network, this->ui->pageNetworking);
    setPage(Page_Finish, this->ui->pageFinish);
    setStartId(Page_Template);

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

    if (this->ui->isoComboBox->count() == 0)
        this->ui->isoComboBox->addItem(tr("No ISO images detected"), QString());

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

    QList<QVariantMap> allVMs = cache->GetAllData("vm");
    for (const QVariant vmVar : allVMs)
    {
        QVariantMap vmRecord = vmVar.toMap();
        bool isTemplate = vmRecord.value("is_a_template").toBool();
        bool isSnapshot = vmRecord.value("is_a_snapshot").toBool();
        if (!isTemplate || isSnapshot)
            continue;

        TemplateInfo info;
        info.ref = vmRecord.value("ref").toString();
        info.name = vmRecord.value("name_label").toString();
        QString virtualizationType = vmRecord.value("HVM_boot_policy").toString().isEmpty() ? tr("PV") : tr("HVM");
        info.type = virtualizationType;
        info.description = vmRecord.value("name_description").toString();

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

        if (this->ui->vmNameEdit->text().trimmed().isEmpty())
            this->ui->vmNameEdit->setText(it->name);
    }

    XenCache* cache = this->cache();
    this->m_selectedTemplateRecord = cache ? cache->ResolveObjectData("vm", ref) : QVariantMap();
    if (!this->m_selectedTemplateRecord.isEmpty())
    {
        long vcpusMax = this->m_selectedTemplateRecord.value("VCPUs_max", 1).toLongLong();
        long vcpusStartup = this->m_selectedTemplateRecord.value("VCPUs_at_startup", 1).toLongLong();
        qint64 memStaticMax = this->m_selectedTemplateRecord.value("memory_static_max").toLongLong() / (1024 * 1024);
        qint64 memDynMax = this->m_selectedTemplateRecord.value("memory_dynamic_max").toLongLong() / (1024 * 1024);
        qint64 memDynMin = this->m_selectedTemplateRecord.value("memory_dynamic_min").toLongLong() / (1024 * 1024);

        this->ui->vcpusMaxSpin->setValue(int(vcpusMax));
        this->ui->vcpusStartupSpin->setMaximum(int(vcpusMax));
        this->ui->vcpusStartupSpin->setValue(int(vcpusStartup));

        this->ui->memoryStaticMaxSpin->setValue(int(memStaticMax));
        this->ui->memoryDynamicMaxSpin->setMaximum(int(memStaticMax));
        this->ui->memoryDynamicMaxSpin->setValue(int(memDynMax));
        this->ui->memoryDynamicMinSpin->setMaximum(int(memDynMax));
        this->ui->memoryDynamicMinSpin->setValue(int(memDynMin));

        this->m_vcpuCount = vcpusStartup;
        this->m_memorySize = int(memStaticMax);
    }

    this->loadTemplateDevices();
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
    QVariantMap templateRecord = cache->ResolveObjectData("vm", this->m_selectedTemplate);
    if (templateRecord.isEmpty())
    {
        this->updateDiskTable();
        this->updateNetworkTable();
        return;
    }

    // Get VBD references from template record
    QVariantList vbdRefs = templateRecord.value("VBDs").toList();
    for (const QVariant& vbdRefVar : vbdRefs)
    {
        QString vbdRef = vbdRefVar.toString();
        QVariantMap vbd = cache->ResolveObjectData("vbd", vbdRef);
        
        QString vbdType = vbd.value("type").toString();
        if (vbdType != "Disk")
            continue;

        QString vdiRef = vbd.value("VDI").toString();
        QVariantMap vdiData = cache->ResolveObjectData("vdi", vdiRef);

        DiskConfig disk;
        disk.vdiRef = vdiRef;
        disk.srRef = vdiData.value("SR").toString();
        disk.sizeBytes = vdiData.value("virtual_size").toLongLong();
        disk.device = vbd.value("userdevice").toString();
        disk.bootable = vbd.value("bootable").toBool();
        this->m_disks.append(disk);
    }

    // Get VIF references from template record
    QVariantList vifRefs = templateRecord.value("VIFs").toList();
    for (const QVariant& vifRefVar : vifRefs)
    {
        QString vifRef = vifRefVar.toString();
        QVariantMap vif = cache->ResolveObjectData("vif", vifRef);
        
        NetworkConfig network;
        network.networkRef = vif.value("network").toString();
        network.device = vif.value("device").toString();
        network.mac = vif.value("MAC").toString();
        this->m_networks.append(network);
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

    QList<QVariantMap> hosts = cache->GetAllData("host");
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

    QList<QVariantMap> srs = cache->GetAllData("sr");
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

    QList<QVariantMap> nets = cache->GetAllData("network");
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
        QVariantMap srRecord = cache ? cache->ResolveObjectData("sr", disk.srRef) : QVariantMap();
        QString srName = srRecord.value("name_label").toString();
        QString sizeGB = QString::number(double(disk.sizeBytes) / (1024.0 * 1024.0 * 1024.0), 'f', 1);

        auto diskItem = new QTableWidgetItem(QString("Disk %1%2").arg(disk.device,
                                                                      disk.bootable ? tr(" (boot)") : QString()));
        auto sizeItem = new QTableWidgetItem(tr("%1 GB").arg(sizeGB));
        auto srItem = new QTableWidgetItem(srName.isEmpty() ? tr("Unknown SR") : srName);
        auto modeItem = new QTableWidgetItem(tr("Read/write"));

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
        QVariantMap networkRecord = cache ? cache->ResolveObjectData("network", network.networkRef) : QVariantMap();
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
    lines << tr("vCPUs: %1 (max %2)")
                 .arg(this->ui->vcpusStartupSpin->value())
                 .arg(this->ui->vcpusMaxSpin->value());
    lines << tr("Memory: %1 MiB (dynamic %2-%3)")
                 .arg(this->ui->memoryStaticMaxSpin->value())
                 .arg(this->ui->memoryDynamicMinSpin->value())
                 .arg(this->ui->memoryDynamicMaxSpin->value());
    lines << tr("Disks: %1").arg(this->m_disks.size());
    lines << tr("Networks: %1").arg(this->m_networks.size());

    QString installMethod = this->ui->isoRadioButton->isChecked()
        ? this->ui->isoComboBox->currentText()
        : this->ui->urlLineEdit->text().trimmed();
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
        if (this->m_disks.isEmpty())
        {
            QMessageBox::warning(this, tr("Storage Configuration"),
                                 tr("The selected template has no disks. Add a disk before proceeding."));
            return false;
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
    this->m_vcpuCount = this->ui->vcpusStartupSpin->value();
    this->m_memorySize = this->ui->memoryStaticMaxSpin->value();
    this->m_assignVtpm = this->ui->assignVtpmCheckBox->isChecked();
    this->m_installUrl = this->ui->urlRadioButton->isChecked() ? this->ui->urlLineEdit->text().trimmed() : QString();
    this->m_selectedIso = this->ui->isoRadioButton->isChecked()
        ? this->ui->isoComboBox->currentData().toString()
        : QString();
    this->m_bootMode = this->ui->bootModeComboBox->currentData().toString();

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
    for (const DiskConfig& disk : this->m_disks)
    {
        CreateVMAction::DiskConfig config;
        config.vdiRef = disk.vdiRef;
        config.srRef = disk.srRef;
        config.sizeBytes = disk.sizeBytes;
        config.device = disk.device;
        config.bootable = disk.bootable;
        disks.append(config);
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
        QString(),
        this->m_selectedIso,
        this->m_installUrl,
        bootMode,
        this->m_selectedHost,
        this->m_vcpuCount,
        this->m_memorySize,
        disks,
        vifs,
        startImmediately,
        this->m_assignVtpm,
        this);

    OperationProgressDialog* progressDialog = new OperationProgressDialog(action, this);
    progressDialog->setAttribute(Qt::WA_DeleteOnClose);

    int result = progressDialog->exec();
    if (result != QDialog::Accepted || action->HasError())
    {
        QString error = action->GetErrorMessage();
        if (error.isEmpty())
            error = tr("Failed to create virtual machine '%1'.").arg(this->m_vmName);
        QMessageBox::critical(this, tr("Failed to Create VM"), error);
        action->deleteLater();
        return;
    }

    action->deleteLater();

    XenCache* cache = this->cache();
    if (cache && connection->GetSession())
    {
        cache->ClearType("vm");
        try
        {
            QVariantMap records = XenAPI::VM::get_all_records(connection->GetSession());
            cache->UpdateBulk("vm", records);
        } catch (const std::exception& exn)
        {
            qWarning() << "NewVMWizard: Failed to refresh VM records:" << exn.what();
        }
    }

    QString message = tr("Virtual machine '%1' has been created successfully.").arg(this->m_vmName);
    if (startImmediately)
        message += tr("\n\nThe VM has been started.");
    QMessageBox::information(this, tr("VM Created"), message);
}

void NewVMWizard::onCurrentIdChanged(int id)
{
    if (id == Page_Finish)
        this->updateSummaryPage();
    this->updateNavigationSelection();
}

XenCache* NewVMWizard::cache() const
{
    return m_connection ? m_connection->GetCache() : nullptr;
}
