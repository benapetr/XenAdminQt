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

#include "importwizard.h"
#include "../settingsmanager.h"
#include "ovfvalidationdialog.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/ovf/ovfpackage.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/pbd.h"
#include "xenlib/xen/network.h"
#include "xenlib/xen/vm.h"
#include "xenlib/utils/decompressgzaction.h"
#include "xenlib/xen/actions/sr/srrefreshaction.h"
#include "xenlib/xen/actions/vm/importvmaction.h"
#include "dialogs/actionprogressdialog.h"
#include <QDebug>
#include <QtWidgets>
#include <QDomDocument>

ImportWizard::ImportWizard(QWidget* parent)
    : ImportWizard(nullptr, parent)
{
}

ImportWizard::ImportWizard(XenConnection* connection, QWidget* parent)
    : ImportWizard(connection, QString(), parent)
{
}

ImportWizard::ImportWizard(XenConnection* connection, const QString& initialFilePath, QWidget* parent)
    : QWizard(parent)
    , m_connection(connection)
    , m_importType(ImportType_XVA)
    , m_verifyManifest(true)
    , m_startVMsAutomatically(false)
    , m_runFixups(false)
    , m_ovfHasManifest(false)
    , m_ovfHasSignature(false)
    , m_ovfAllowsNetworkSriov(true)
    , m_xvaTotalDiskSizeBytes(0)
    , m_diskImageCapacityBytes(0)
    , m_vcpuCount(1)
    , m_memoryMb(512)
    , m_bootMode(BootMode_Bios)
    , m_assignVtpm(false)
    , m_xvaAction(nullptr)
{
    this->setWindowTitle(tr("Import Virtual Machine"));
    this->setWindowIcon(QIcon(":/icons/vm-import-32.png"));
    this->setWizardStyle(QWizard::ModernStyle);
    this->setOption(QWizard::HaveHelpButton, true);
    this->setOption(QWizard::HelpButtonOnRight, false);

    this->setupWizardPages();

    connect(this, &QWizard::currentIdChanged, this, &ImportWizard::onCurrentIdChanged);

    // Pre-populate file path if one was provided (e.g. from a file association or CLI)
    if (!initialFilePath.isEmpty())
    {
        QLineEdit* filePathEdit = this->findChild<QLineEdit*>("filePathEdit");
        if (filePathEdit)
            filePathEdit->setText(initialFilePath);
    }

    qDebug() << "ImportWizard: Created Import Wizard";
}

void ImportWizard::setupWizardPages()
{
    // Add wizard pages
    this->setPage(Page_Source,   this->createSourcePage());
    this->setPage(Page_VmConfig, this->createVmConfigPage());
    this->setPage(Page_Eula,     this->createEulaPage());
    this->setPage(Page_Host,     this->createHostPage());
    this->setPage(Page_Storage,  this->createStoragePage());
    this->setPage(Page_Network,  this->createNetworkPage());
    this->setPage(Page_Security,     this->createSecurityPage());
    this->setPage(Page_Options,      this->createOptionsPage());
    this->setPage(Page_BootOptions,  this->createBootOptionsPage());
    this->setPage(Page_Finish,       this->createFinishPage());

    // Set the starting page
    this->setStartId(Page_Source);
}

// ─── Source page with dynamic nextId ─────────────────────────────────────────
// Local helper subclass so the source page can skip Page_VmConfig for non-VHD files.

namespace
{
    // ── Source page ───────────────────────────────────────────────────────────
    // Directs to VmConfig for VHD/VMDK, to Eula when OVF has EULAs, else to Host.
    class ImportSourcePage : public QWizardPage
    {
        public:
            explicit ImportSourcePage(QWidget* parent = nullptr)
                : QWizardPage(parent) {}

            int nextId() const override
            {
                auto* wiz = qobject_cast<ImportWizard*>(this->wizard());
                if (!wiz)
                    return ImportWizard::Page_Host;
                if (wiz->GetImportType() == ImportWizard::ImportType_VHD)
                    return ImportWizard::Page_VmConfig;
                if (wiz->GetImportType() == ImportWizard::ImportType_OVF && wiz->HasOvfEulas())
                    return ImportWizard::Page_Eula;
                return ImportWizard::Page_Host;
            }
    };

    // ── EULA page (OVF only) ──────────────────────────────────────────────────
    // Always proceeds to host selection after the user accepts all EULAs.
    class ImportEulaPage : public QWizardPage
    {
        public:
            explicit ImportEulaPage(QWidget* parent = nullptr)
                : QWizardPage(parent) {}

            int nextId() const override { return ImportWizard::Page_Host; }
    };

    // ── VmConfig page (VHD/VMDK only) ────────────────────────────────────────
    // Always proceeds to host selection.
    class ImportVmConfigPage : public QWizardPage
    {
        public:
            explicit ImportVmConfigPage(QWidget* parent = nullptr)
                : QWizardPage(parent) {}

            int nextId() const override { return ImportWizard::Page_Host; }
    };

    // ── Host page ─────────────────────────────────────────────────────────────
    // Always proceeds to storage selection.
    class ImportHostPage : public QWizardPage
    {
        public:
            explicit ImportHostPage(QWidget* parent = nullptr)
                : QWizardPage(parent) {}

            int nextId() const override { return ImportWizard::Page_Storage; }
    };

    // ── Storage page ──────────────────────────────────────────────────────────
    // Always proceeds to network selection.
    class ImportStoragePage : public QWizardPage
    {
        public:
            explicit ImportStoragePage(QWidget* parent = nullptr)
                : QWizardPage(parent) {}

            int nextId() const override { return ImportWizard::Page_Network; }
    };

    // ── Network page ──────────────────────────────────────────────────────────
    // XVA: skip Options → go straight to Finish.
    // OVF with manifest/signature: go to Security page before Options.
    // VHD / plain OVF: go to Options.
    class ImportNetworkPage : public QWizardPage
    {
        public:
            explicit ImportNetworkPage(QWidget* parent = nullptr)
                : QWizardPage(parent) {}

            int nextId() const override
            {
                auto* wiz = qobject_cast<ImportWizard*>(this->wizard());
                if (!wiz)
                    return ImportWizard::Page_Options;
                if (wiz->GetImportType() == ImportWizard::ImportType_XVA)
                    return ImportWizard::Page_Finish;
                if (wiz->GetImportType() == ImportWizard::ImportType_OVF && wiz->OvfHasSecurity())
                    return ImportWizard::Page_Security;
                return ImportWizard::Page_Options;
            }
    };

    // ── Security page (OVF only) ──────────────────────────────────────────────
    // Always proceeds to options.
    class ImportSecurityPage : public QWizardPage
    {
        public:
            explicit ImportSecurityPage(QWidget* parent = nullptr)
                : QWizardPage(parent) {}

            int nextId() const override { return ImportWizard::Page_Options; }
    };

    // ── Options page ──────────────────────────────────────────────────────────
    // VHD/VMDK: proceeds to Boot Options.  All others: proceeds to Finish.
    class ImportOptionsPage : public QWizardPage
    {
        public:
            explicit ImportOptionsPage(QWidget* parent = nullptr)
                : QWizardPage(parent) {}

            int nextId() const override
            {
                auto* wiz = qobject_cast<ImportWizard*>(this->wizard());
                if (wiz && wiz->GetImportType() == ImportWizard::ImportType_VHD)
                    return ImportWizard::Page_BootOptions;
                return ImportWizard::Page_Finish;
            }
    };

    // ── Boot options page (VHD/VMDK only) ────────────────────────────────────
    // Always proceeds to Finish.
    class ImportBootOptionsPage : public QWizardPage
    {
        public:
            explicit ImportBootOptionsPage(QWidget* parent = nullptr)
                : QWizardPage(parent) {}

            int nextId() const override { return ImportWizard::Page_Finish; }
    };

    // ── Finish page ───────────────────────────────────────────────────────────
    // Terminal page — no next page.
    class ImportFinishPage : public QWizardPage
    {
        public:
            explicit ImportFinishPage(QWidget* parent = nullptr)
                : QWizardPage(parent) {}

            int nextId() const override { return -1; }
    };
} // anonymous namespace

QWizardPage* ImportWizard::createSourcePage()
{
    ImportSourcePage* page = new ImportSourcePage;
    page->setTitle(tr("Import Source"));
    page->setSubTitle(tr("Select the file to import."));

    // File selection group
    QGroupBox* fileGroup = new QGroupBox(tr("Source File"));

    QLineEdit* filePathEdit = new QLineEdit;
    filePathEdit->setObjectName("filePathEdit");
    filePathEdit->setPlaceholderText(tr("Select a file to import..."));

    QPushButton* browseButton = new QPushButton(tr("Browse..."));
    browseButton->setObjectName("browseButton");
    connect(browseButton, &QPushButton::clicked, this, &ImportWizard::onBrowseClicked);

    QHBoxLayout* fileLayout = new QHBoxLayout;
    fileLayout->addWidget(filePathEdit);
    fileLayout->addWidget(browseButton);
    fileGroup->setLayout(fileLayout);

    // Import type display
    QGroupBox* typeGroup = new QGroupBox(tr("Import Type"));
    QLabel* typeLabel = new QLabel(tr("Type will be detected automatically"));
    typeLabel->setObjectName("typeLabel");
    typeLabel->setStyleSheet("QLabel { color: #666; font-style: italic; }");

    QVBoxLayout* typeLayout = new QVBoxLayout;
    typeLayout->addWidget(typeLabel);
    typeGroup->setLayout(typeLayout);

    // OVF package metadata — hidden until an OVF/OVA is selected
    QGroupBox* ovfMetaGroup = new QGroupBox(tr("Package Information"));
    ovfMetaGroup->setObjectName("ovfMetaGroup");
    ovfMetaGroup->setVisible(false);

    QLabel* ovfMetaLabel = new QLabel;
    ovfMetaLabel->setObjectName("ovfMetaLabel");
    ovfMetaLabel->setWordWrap(true);

    QVBoxLayout* ovfMetaLayout = new QVBoxLayout;
    ovfMetaLayout->addWidget(ovfMetaLabel);
    ovfMetaGroup->setLayout(ovfMetaLayout);

    // XVA metadata — hidden until an XVA is selected
    QGroupBox* xvaMetaGroup = new QGroupBox(tr("Virtual Appliance Information"));
    xvaMetaGroup->setObjectName("xvaMetaGroup");
    xvaMetaGroup->setVisible(false);

    QLabel* xvaMetaLabel = new QLabel;
    xvaMetaLabel->setObjectName("xvaMetaLabel");
    xvaMetaLabel->setWordWrap(true);

    QVBoxLayout* xvaMetaLayout = new QVBoxLayout;
    xvaMetaLayout->addWidget(xvaMetaLabel);
    xvaMetaGroup->setLayout(xvaMetaLayout);

    // Disk image metadata — hidden until a VHD/VMDK is selected
    QGroupBox* diskMetaGroup = new QGroupBox(tr("Disk Image Information"));
    diskMetaGroup->setObjectName("diskMetaGroup");
    diskMetaGroup->setVisible(false);

    QLabel* diskMetaLabel = new QLabel;
    diskMetaLabel->setObjectName("diskMetaLabel");
    diskMetaLabel->setWordWrap(true);

    QVBoxLayout* diskMetaLayout = new QVBoxLayout;
    diskMetaLayout->addWidget(diskMetaLabel);
    diskMetaGroup->setLayout(diskMetaLayout);

    // File info
    QGroupBox* infoGroup = new QGroupBox(tr("Supported Formats"));
    QLabel* infoLabel = new QLabel(tr("• XVA files (.xva, .xva.gz) - XenServer native format\n"
                                      "• OVF files (.ovf, .ova) - Open Virtualization Format\n"
                                      "• VHD files (.vhd, .vmdk) - Virtual disk images\n\n"
                                      "Note: only local files are supported. "
                                      "HTTP/FTP URI imports are not available in this version."));
    infoLabel->setWordWrap(true);

    QVBoxLayout* infoLayout = new QVBoxLayout;
    infoLayout->addWidget(infoLabel);
    infoGroup->setLayout(infoLayout);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addWidget(fileGroup);
    mainLayout->addWidget(typeGroup);
    mainLayout->addWidget(ovfMetaGroup);
    mainLayout->addWidget(xvaMetaGroup);
    mainLayout->addWidget(diskMetaGroup);
    mainLayout->addWidget(infoGroup);
    mainLayout->addStretch();

    page->setLayout(mainLayout);
    return page;
}

QWizardPage* ImportWizard::createVmConfigPage()
{
    ImportVmConfigPage* page = new ImportVmConfigPage;
    page->setTitle(tr("Virtual Machine Configuration"));
    page->setSubTitle(tr("Specify the name and hardware settings for the imported virtual machine."));

    QLabel* nameLabel = new QLabel(tr("VM Name:"));
    QLineEdit* nameEdit = new QLineEdit;
    nameEdit->setObjectName("vmNameEdit");
    nameEdit->setPlaceholderText(tr("Enter a name for the virtual machine"));

    QLabel* cpuLabel = new QLabel(tr("vCPUs:"));
    QSpinBox* cpuSpin = new QSpinBox;
    cpuSpin->setObjectName("vcpuSpin");
    cpuSpin->setMinimum(1);
    cpuSpin->setMaximum(64);
    cpuSpin->setValue(1);

    QLabel* memLabel = new QLabel(tr("Memory (MB):"));
    QSpinBox* memSpin = new QSpinBox;
    memSpin->setObjectName("memorySpin");
    memSpin->setMinimum(64);
    memSpin->setMaximum(1024 * 1024);
    memSpin->setValue(512);
    memSpin->setSingleStep(256);

    QFormLayout* form = new QFormLayout;
    form->addRow(nameLabel, nameEdit);
    form->addRow(cpuLabel,  cpuSpin);
    form->addRow(memLabel,  memSpin);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addLayout(form);
    mainLayout->addStretch();

    page->setLayout(mainLayout);
    return page;
}

QWizardPage* ImportWizard::createHostPage()
{
    ImportHostPage* page = new ImportHostPage;
    page->setTitle(tr("Select Destination"));
    page->setSubTitle(tr("Choose the host or pool to import the virtual machine into."));

    QLabel* hostLabel = new QLabel(tr("Target Server:"));
    QComboBox* hostCombo = new QComboBox;
    hostCombo->setObjectName("hostCombo");
    // Items are populated in initializePage()

    QLabel* infoLabel = new QLabel(tr("The virtual machine will be imported to the selected server. "
                                      "You can configure storage and network settings on the following pages."));
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("QLabel { color: #666; font-style: italic; }");

    QFormLayout* formLayout = new QFormLayout;
    formLayout->addRow(hostLabel, hostCombo);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(infoLabel);
    mainLayout->addStretch();

    page->setLayout(mainLayout);
    return page;
}

QWizardPage* ImportWizard::createStoragePage()
{
    ImportStoragePage* page = new ImportStoragePage;
    page->setTitle(tr("Storage Configuration"));
    page->setSubTitle(tr("Select the storage repository for the imported virtual disks."));

    QLabel* srLabel = new QLabel(tr("Storage Repository:"));
    QComboBox* srCombo = new QComboBox;
    srCombo->setObjectName("storageCombo");
    // Items are populated in initializePage()

    QPushButton* rescanButton = new QPushButton(tr("Rescan"));
    rescanButton->setObjectName("storageRescanButton");
    rescanButton->setToolTip(tr("Run SR.scan on the selected storage repository to refresh VDI and free-space information."));
    connect(rescanButton, &QPushButton::clicked, this, &ImportWizard::onRescanStorageClicked);

    QHBoxLayout* srRow = new QHBoxLayout;
    srRow->addWidget(srLabel);
    srRow->addWidget(srCombo, 1);
    srRow->addWidget(rescanButton);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addLayout(srRow);
    mainLayout->addStretch();

    page->setLayout(mainLayout);
    return page;
}

QWizardPage* ImportWizard::createNetworkPage()
{
    ImportNetworkPage* page = new ImportNetworkPage;
    page->setTitle(tr("Network Configuration"));
    page->setSubTitle(tr("Select the target network(s) for the imported virtual machine."));

    // --- XVA / VHD single-network group ---
    QGroupBox* xvaNetGroup = new QGroupBox(tr("Target Network"));
    xvaNetGroup->setObjectName("xvaNetGroup");

    QComboBox* networkCombo = new QComboBox;
    networkCombo->setObjectName("networkCombo");
    // Items are populated in initializePage() / populateNetworkCombo()

    QFormLayout* xvaNetLayout = new QFormLayout;
    xvaNetLayout->addRow(tr("Target Network:"), networkCombo);
    xvaNetGroup->setLayout(xvaNetLayout);

    // --- OVF per-network mapping group ---
    // Rows are added dynamically in populateNetworkCombo() when OVF is detected
    QGroupBox* ovfMappingGroup = new QGroupBox(tr("OVF Network Mappings"));
    ovfMappingGroup->setObjectName("ovfMappingGroup");
    ovfMappingGroup->setVisible(false);

    QWidget* ovfMappingContainer = new QWidget;
    ovfMappingContainer->setObjectName("ovfMappingContainer");
    // Layout is created dynamically in populateNetworkCombo()

    QVBoxLayout* ovfGroupLayout = new QVBoxLayout;
    ovfGroupLayout->addWidget(ovfMappingContainer);
    ovfMappingGroup->setLayout(ovfGroupLayout);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addWidget(xvaNetGroup);
    mainLayout->addWidget(ovfMappingGroup);
    mainLayout->addStretch();

    page->setLayout(mainLayout);
    return page;
}

QWizardPage* ImportWizard::createOptionsPage()
{
    ImportOptionsPage* page = new ImportOptionsPage;
    page->setTitle(tr("Import Options"));
    page->setSubTitle(tr("Configure additional import options."));

    // Import options (run fixups, start automatically)
    QGroupBox* importGroup = new QGroupBox(tr("Import Options"));

    QCheckBox* runFixups = new QCheckBox(tr("Run operating system fixups"));
    runFixups->setObjectName("runFixups");
    runFixups->setChecked(false);

    QLabel* fixupSrLabel = new QLabel(tr("Fixup ISO SR:"));
    fixupSrLabel->setObjectName("fixupSrLabel");
    fixupSrLabel->setEnabled(false);

    QComboBox* fixupSrCombo = new QComboBox;
    fixupSrCombo->setObjectName("fixupSrCombo");
    fixupSrCombo->setEnabled(false);
    fixupSrCombo->addItem(tr("(none)"), QString());

    // Enable/disable fixup SR combo based on checkbox
    QObject::connect(runFixups, &QCheckBox::toggled, fixupSrLabel, &QLabel::setEnabled);
    QObject::connect(runFixups, &QCheckBox::toggled, fixupSrCombo, &QComboBox::setEnabled);

    QCheckBox* startAfterImport = new QCheckBox(tr("Start VMs automatically after import"));
    startAfterImport->setObjectName("startAfterImport");

    QFormLayout* fixupLayout = new QFormLayout;
    fixupLayout->addRow(runFixups);
    fixupLayout->addRow(fixupSrLabel, fixupSrCombo);

    QVBoxLayout* importLayout = new QVBoxLayout;
    importLayout->addLayout(fixupLayout);
    importLayout->addWidget(startAfterImport);
    importGroup->setLayout(importLayout);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addWidget(importGroup);
    mainLayout->addStretch();

    page->setLayout(mainLayout);
    return page;
}

QWizardPage* ImportWizard::createFinishPage()
{
    ImportFinishPage* page = new ImportFinishPage;
    page->setTitle(tr("Ready to Import"));
    page->setSubTitle(tr("Review the import settings and start the import process."));

    QLabel* summaryLabel = new QLabel(tr("Import Summary:"));
    QTextEdit* summaryText = new QTextEdit;
    summaryText->setObjectName("summaryText");
    summaryText->setReadOnly(true);
    summaryText->setMaximumHeight(200);

    // Progress section
    QGroupBox* progressGroup = new QGroupBox(tr("Import Progress"));
    QProgressBar* progressBar = new QProgressBar;
    progressBar->setObjectName("progressBar");
    progressBar->setVisible(false);

    QLabel* statusLabel = new QLabel(tr("Click Finish to start the import process."));
    statusLabel->setObjectName("statusLabel");

    QVBoxLayout* progressLayout = new QVBoxLayout;
    progressLayout->addWidget(statusLabel);
    progressLayout->addWidget(progressBar);
    progressGroup->setLayout(progressLayout);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addWidget(summaryLabel);
    mainLayout->addWidget(summaryText);
    mainLayout->addWidget(progressGroup);
    mainLayout->addStretch();

    page->setLayout(mainLayout);
    return page;
}

QWizardPage* ImportWizard::createEulaPage()
{
    ImportEulaPage* page = new ImportEulaPage;
    page->setTitle(tr("End User License Agreement"));
    page->setSubTitle(tr("You must accept the license agreement(s) before proceeding."));

    QLabel* introLabel = new QLabel(
        tr("Please read the following license agreement(s). "
           "You must accept all agreements to continue importing this package."));
    introLabel->setWordWrap(true);

    // One tab per EULA document; tabs are populated in initializePage()
    QTabWidget* eulaTab = new QTabWidget;
    eulaTab->setObjectName("eulaTabWidget");

    QCheckBox* acceptCheck = new QCheckBox(
        tr("I have read and accept the above End User License Agreement(s)."));
    acceptCheck->setObjectName("eulaAcceptCheck");

    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(introLabel);
    layout->addWidget(eulaTab, 1);  // stretch so text area fills available height
    layout->addWidget(acceptCheck);

    page->setLayout(layout);
    return page;
}

QWizardPage* ImportWizard::createSecurityPage()
{
    ImportSecurityPage* page = new ImportSecurityPage;
    page->setTitle(tr("Security"));
    page->setSubTitle(tr("Review the security information for this package."));

    // Populated in initializePage() based on whether the package has a signature or manifest
    QLabel* infoLabel = new QLabel;
    infoLabel->setObjectName("securityInfoLabel");
    infoLabel->setWordWrap(true);

    QGroupBox* verifyGroup = new QGroupBox(tr("Verification"));

    QCheckBox* verifyCheck = new QCheckBox;
    verifyCheck->setObjectName("securityVerifyCheck");
    verifyCheck->setChecked(true);

    QVBoxLayout* verifyLayout = new QVBoxLayout;
    verifyLayout->addWidget(verifyCheck);
    verifyGroup->setLayout(verifyLayout);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addWidget(infoLabel);
    mainLayout->addSpacing(8);
    mainLayout->addWidget(verifyGroup);
    mainLayout->addStretch();

    page->setLayout(mainLayout);
    return page;
}

QWizardPage* ImportWizard::createBootOptionsPage()
{
    ImportBootOptionsPage* page = new ImportBootOptionsPage;
    page->setTitle(tr("Boot Options"));
    page->setSubTitle(tr("Select the firmware type and optional security hardware for the imported VM."));

    // ── Firmware group ────────────────────────────────────────────────────────
    QGroupBox* firmwareGroup = new QGroupBox(tr("Boot Firmware"));

    QRadioButton* biosRadio = new QRadioButton(tr("BIOS (Legacy)"));
    biosRadio->setObjectName("bootBiosRadio");
    biosRadio->setChecked(true);
    biosRadio->setToolTip(tr("Traditional BIOS/HVM boot. Compatible with most guest operating systems."));

    QRadioButton* uefiRadio = new QRadioButton(tr("UEFI"));
    uefiRadio->setObjectName("bootUefiRadio");
    uefiRadio->setToolTip(tr("UEFI firmware. Required by some modern operating systems."));

    QRadioButton* uefiSecureRadio = new QRadioButton(tr("UEFI Secure Boot"));
    uefiSecureRadio->setObjectName("bootUefiSecureRadio");
    uefiSecureRadio->setToolTip(tr("UEFI with Secure Boot enabled. The guest OS must support Secure Boot."));

    QVBoxLayout* firmwareLayout = new QVBoxLayout;
    firmwareLayout->addWidget(biosRadio);
    firmwareLayout->addWidget(uefiRadio);
    firmwareLayout->addWidget(uefiSecureRadio);
    firmwareGroup->setLayout(firmwareLayout);

    // ── Security hardware group ───────────────────────────────────────────────
    QGroupBox* secHwGroup = new QGroupBox(tr("Security Hardware"));

    QCheckBox* vtpmCheck = new QCheckBox(tr("Assign a virtual Trusted Platform Module (vTPM)"));
    vtpmCheck->setObjectName("vtpmCheck");
    vtpmCheck->setToolTip(tr("A vTPM allows the guest to store secrets and verify platform integrity.\n"
                             "Requires XenServer 22.26.0 / XCP-ng 8.3 or later."));

    QVBoxLayout* secHwLayout = new QVBoxLayout;
    secHwLayout->addWidget(vtpmCheck);
    secHwGroup->setLayout(secHwLayout);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addWidget(firmwareGroup);
    mainLayout->addSpacing(8);
    mainLayout->addWidget(secHwGroup);
    mainLayout->addStretch();

    page->setLayout(mainLayout);
    return page;
}

void ImportWizard::initializePage(int id)
{
    if (id == Page_Host)
        this->populateHostCombo();
    else if (id == Page_VmConfig)
    {
        // Pre-fill VM name from the source file's base name
        QLineEdit* nameEdit = this->findChild<QLineEdit*>("vmNameEdit");
        if (nameEdit && nameEdit->text().isEmpty())
        {
            const QString baseName = QFileInfo(this->m_sourceFilePath).completeBaseName();
            nameEdit->setText(baseName);
        }
    }
    else if (id == Page_Storage)
    {
        // Capture the host selection so populateStorageCombo can filter by PBD
        QComboBox* hostCombo = this->findChild<QComboBox*>("hostCombo");
        if (hostCombo)
        {
            this->m_selectedHost = this->m_connection
                ? this->m_connection->GetCache()->ResolveObject<Host>(hostCombo->currentData().toString())
                : nullptr;
        }
        this->populateStorageCombo();
    }
    else if (id == Page_Network)
    {
        // Snapshot selected host so populateNetworkComboBox can filter by PIF
        QComboBox* hostCombo = this->findChild<QComboBox*>("hostCombo");
        if (hostCombo)
        {
            this->m_selectedHost = this->m_connection
                ? this->m_connection->GetCache()->ResolveObject<Host>(hostCombo->currentData().toString())
                : nullptr;
        }
        this->populateNetworkCombo();
    }
    else if (id == Page_Options)
        this->populateFixupIsoCombo();
    else if (id == Page_BootOptions)
    {
        // Restore the previously selected boot mode so navigating back/forward
        // doesn't reset the user's choice.
        QRadioButton* biosRadio       = this->findChild<QRadioButton*>("bootBiosRadio");
        QRadioButton* uefiRadio       = this->findChild<QRadioButton*>("bootUefiRadio");
        QRadioButton* uefiSecureRadio = this->findChild<QRadioButton*>("bootUefiSecureRadio");
        if (biosRadio && uefiRadio && uefiSecureRadio)
        {
            biosRadio->setChecked(this->m_bootMode == BootMode_Bios);
            uefiRadio->setChecked(this->m_bootMode == BootMode_Uefi);
            uefiSecureRadio->setChecked(this->m_bootMode == BootMode_UefiSecure);
        }
        QCheckBox* vtpmCheck = this->findChild<QCheckBox*>("vtpmCheck");
        if (vtpmCheck)
            vtpmCheck->setChecked(this->m_assignVtpm);
    }
    else if (id == Page_Eula)
    {
        // Populate tab widget with EULA texts — one tab per EULA document
        QTabWidget* tabWidget = this->findChild<QTabWidget*>("eulaTabWidget");
        if (tabWidget)
        {
            tabWidget->clear();
            for (int i = 0; i < this->m_ovfEulas.size(); i++)
            {
                QTextEdit* textEdit = new QTextEdit;
                textEdit->setReadOnly(true);
                textEdit->setPlainText(this->m_ovfEulas.at(i));
                const QString tabTitle = (this->m_ovfEulas.size() == 1)
                    ? tr("License Agreement")
                    : tr("License %1").arg(i + 1);
                tabWidget->addTab(textEdit, tabTitle);
            }
        }
        // Reset checkbox so user must explicitly accept on each visit
        QCheckBox* acceptCheck = this->findChild<QCheckBox*>("eulaAcceptCheck");
        if (acceptCheck)
            acceptCheck->setChecked(false);
    }
    else if (id == Page_Security)
    {
        // Update info label and verify checkbox based on whether this package has
        // a producer signature or just a manifest.  Matches C# ImportSecurityPage.
        QLabel* infoLabel = this->findChild<QLabel*>("securityInfoLabel");
        QCheckBox* verifyCheck = this->findChild<QCheckBox*>("securityVerifyCheck");
        if (infoLabel)
        {
            if (this->m_ovfHasSignature)
            {
                infoLabel->setText(
                    tr("This package contains a digital signature.\n\n"
                       "Verifying the producer signature confirms the package "
                       "was not modified after it was published."));
            } else
            {
                infoLabel->setText(
                    tr("This package contains a manifest file.\n\n"
                       "Verifying the manifest confirms that the package contents "
                       "have not been corrupted or tampered with."));
            }
        }
        if (verifyCheck)
        {
            verifyCheck->setText(this->m_ovfHasSignature
                ? tr("Verify producer signature")
                : tr("Verify manifest content"));
            verifyCheck->setChecked(this->m_verifyManifest);
        }
    }
    else if (id == Page_Finish)
    {
        QTextEdit* summaryText = this->findChild<QTextEdit*>("summaryText");
        if (summaryText)
        {
            QComboBox* hostCombo = this->findChild<QComboBox*>("hostCombo");
            QComboBox* srCombo = this->findChild<QComboBox*>("storageCombo");
            QComboBox* netCombo = this->findChild<QComboBox*>("networkCombo");

            QString hostText = hostCombo ? hostCombo->currentText() : tr("(none)");
            QString srText = srCombo ? srCombo->currentText() : tr("(none)");
            QString netText = netCombo ? netCombo->currentText() : tr("(none)");

            QString typeStr;
            switch (this->m_importType)
            {
                case ImportType_XVA: typeStr = tr("XVA"); break;
                case ImportType_OVF: typeStr = tr("OVF/OVA"); break;
                case ImportType_VHD: typeStr = tr("VHD/VMDK"); break;
            }

            QString summary;
            summary += tr("Source File: %1\n").arg(this->m_sourceFilePath.isEmpty() ? tr("No file selected") : this->m_sourceFilePath);
            summary += tr("Import Type: %1\n").arg(typeStr);
            summary += tr("Target: %1\n").arg(hostText);
            summary += tr("Storage: %1\n").arg(srText);
            summary += tr("Network: %1\n").arg(netText);

            if (this->m_importType == ImportType_VHD)
            {
                // VM config
                QLineEdit* vmNameEdit = this->findChild<QLineEdit*>("vmNameEdit");
                QSpinBox*  vcpuSpin  = this->findChild<QSpinBox*>("vcpuSpin");
                QSpinBox*  memSpin   = this->findChild<QSpinBox*>("memorySpin");
                if (vmNameEdit && !vmNameEdit->text().trimmed().isEmpty())
                    summary += tr("VM Name: %1\n").arg(vmNameEdit->text().trimmed());
                if (vcpuSpin)
                    summary += tr("vCPUs: %1\n").arg(vcpuSpin->value());
                if (memSpin)
                    summary += tr("Memory: %1 MB\n").arg(memSpin->value());

                // Boot mode (read live from the radio buttons so the summary stays in sync
                // even if the user navigated back and changed the selection)
                QRadioButton* uefiSecureRadio = this->findChild<QRadioButton*>("bootUefiSecureRadio");
                QRadioButton* uefiRadio       = this->findChild<QRadioButton*>("bootUefiRadio");
                QString bootModeStr;
                if (uefiSecureRadio && uefiSecureRadio->isChecked())
                    bootModeStr = tr("UEFI Secure Boot");
                else if (uefiRadio && uefiRadio->isChecked())
                    bootModeStr = tr("UEFI");
                else
                    bootModeStr = tr("BIOS (Legacy)");
                summary += tr("Boot Firmware: %1\n").arg(bootModeStr);

                QCheckBox* vtpmCheck = this->findChild<QCheckBox*>("vtpmCheck");
                if (vtpmCheck && vtpmCheck->isChecked())
                    summary += tr("vTPM: Yes\n");
            }

            QCheckBox* startAfter = this->findChild<QCheckBox*>("startAfterImport");
            if (startAfter && startAfter->isChecked())
                summary += tr("Start automatically: Yes\n");
            else
                summary += tr("Start automatically: No\n");

            summaryText->setPlainText(summary);
        }
    }

    QWizard::initializePage(id);
}

void ImportWizard::populateHostCombo()
{
    QComboBox* hostCombo = this->findChild<QComboBox*>("hostCombo");
    if (!hostCombo)
        return;

    hostCombo->clear();
    hostCombo->addItem(tr("(no affinity — import to pool)"), QString());

    if (!this->m_connection)
        return;

    XenCache* cache = this->m_connection->GetCache();
    if (!cache)
        return;

    QList<QSharedPointer<Host>> hosts = cache->GetAll<Host>();
    for (const QSharedPointer<Host>& host : hosts)
    {
        if (!host || !host->IsValid())
            continue;
        QString label = host->GetName();
        QString addr = host->GetAddress();
        if (!addr.isEmpty())
            label += QString(" (%1)").arg(addr);
        hostCombo->addItem(label, host->OpaqueRef());
    }
}

void ImportWizard::populateStorageCombo()
{
    QComboBox* srCombo = this->findChild<QComboBox*>("storageCombo");
    if (!srCombo)
        return;

    srCombo->clear();

    if (!this->m_connection)
    {
        srCombo->addItem(tr("(no connection)"), QString());
        return;
    }

    XenCache* cache = this->m_connection->GetCache();

    QList<QSharedPointer<SR>> srs = cache->GetAll<SR>();
    const bool hasHostAffinity = (this->m_selectedHost && this->m_selectedHost->IsValid());
    for (const QSharedPointer<SR>& sr : srs)
    {
        if (!sr || !sr->IsValid())
            continue;
        if (sr->IsToolsSR())
            continue;
        if (sr->IsISOLibrary())
            continue;
        if (!sr->AllowedOperations().contains("vdi_create"))
            continue;

        // When no host affinity is set, only offer shared SRs — a local SR won't be
        // accessible from all hosts in the pool. Matches C# StoragePickerPage behaviour.
        if (!hasHostAffinity && !sr->IsShared())
            continue;

        // If a specific host is selected, only include SRs that have
        // an attached (currently_attached == true) PBD for that host.
        if (hasHostAffinity)
        {
            const QString hostRef = this->m_selectedHost->OpaqueRef();
            bool attachedToHost = false;
            const QList<QSharedPointer<PBD>> pbds = sr->GetPBDs();
            for (const QSharedPointer<PBD>& pbd : pbds)
            {
                if (!pbd || !pbd->IsValid())
                    continue;
                if (pbd->GetHostRef() == hostRef && pbd->IsCurrentlyAttached())
                {
                    attachedToHost = true;
                    break;
                }
            }
            if (!attachedToHost)
                continue;
        }

        srCombo->addItem(sr->GetName(), sr->OpaqueRef());
    }

    if (srCombo->count() == 0)
        srCombo->addItem(tr("(no suitable storage repositories found)"), QString());
}

void ImportWizard::populateNetworkComboBox(QComboBox* combo)
{
    if (!combo)
        return;

    combo->clear();
    combo->addItem(tr("(do not attach)"), QString());

    if (!this->m_connection)
        return;

    XenCache* cache = this->m_connection->GetCache();

    // When a specific host is selected, record its PIF refs for filtering.
    // When no host is selected (no affinity), we build a per-host PIF map so that we
    // can include only networks that are visible on ALL hosts in the pool.
    QSet<QString> selectedHostPifRefs;
    QMap<QString, QSet<QString>> pifRefsByHost; // hostRef → set of PIF refs that host carries
    bool hasHostAffinity = (this->m_selectedHost && this->m_selectedHost->IsValid());

    if (hasHostAffinity)
    {
        const QStringList refs = this->m_selectedHost->GetPIFRefs();
        selectedHostPifRefs = QSet<QString>(refs.begin(), refs.end());
    }
    else
    {
        // No affinity — collect PIF sets for every host so we can intersect later
        const QList<QSharedPointer<Host>> hosts = cache->GetAll<Host>();
        for (const QSharedPointer<Host>& host : hosts)
        {
            if (!host || !host->IsValid())
                continue;
            const QStringList refs = host->GetPIFRefs();
            pifRefsByHost[host->OpaqueRef()] = QSet<QString>(refs.begin(), refs.end());
        }
    }

    QList<QSharedPointer<Network>> networks = cache->GetAll<Network>();
    for (const QSharedPointer<Network>& net : networks)
    {
        if (!net || !net->IsValid())
            continue;

        // Use Network::Show() — covers guest-installer, member, hidden-from-xencenter checks
        if (!net->Show(false))
            continue;

        // When OVF recommendations prohibit SR-IOV (allow-network-sriov == 0), hide SR-IOV
        // backed networks.  Matches C# ImportSelectNetworkPage.AllowSriovNetwork() logic.
        if (!this->m_ovfAllowsNetworkSriov && net->IsSriov())
            continue;

        const QStringList netPifRefs = net->GetPIFRefs();
        const bool isInternal = netPifRefs.isEmpty();

        if (hasHostAffinity)
        {
            // Include only networks with a PIF on the selected host, or internal networks
            if (!isInternal)
            {
                const QSet<QString> netPifSet(netPifRefs.begin(), netPifRefs.end());
                if ((selectedHostPifRefs & netPifSet).isEmpty())
                    continue;
            }
        }
        else if (!pifRefsByHost.isEmpty())
        {
            // No affinity: include only networks visible to ALL hosts (or internal)
            if (!isInternal)
            {
                const QSet<QString> netPifSet(netPifRefs.begin(), netPifRefs.end());
                bool visibleOnAllHosts = true;
                for (auto it = pifRefsByHost.constBegin(); it != pifRefsByHost.constEnd(); ++it)
                {
                    if ((it.value() & netPifSet).isEmpty())
                    {
                        visibleOnAllHosts = false;
                        break;
                    }
                }
                if (!visibleOnAllHosts)
                    continue;
            }
        }

        combo->addItem(net->GetName(), net->OpaqueRef());
    }
}

void ImportWizard::populateNetworkCombo()
{
    QGroupBox* xvaNetGroup = this->findChild<QGroupBox*>("xvaNetGroup");
    QGroupBox* ovfMappingGroup = this->findChild<QGroupBox*>("ovfMappingGroup");
    QWidget* ovfMappingContainer = this->findChild<QWidget*>("ovfMappingContainer");

    if (this->m_importType == ImportType_OVF && !this->m_ovfNetworkNames.isEmpty())
    {
        // OVF path: show per-network mapping rows, hide the single combo group
        if (xvaNetGroup)
            xvaNetGroup->setVisible(false);

        if (ovfMappingContainer)
        {
            // Delete any old layout + child widgets
            QLayout* oldLayout = ovfMappingContainer->layout();
            if (oldLayout)
            {
                QLayoutItem* item = nullptr;
                while ((item = oldLayout->takeAt(0)) != nullptr)
                {
                    if (item->widget())
                        item->widget()->deleteLater();
                    delete item;
                }
                delete oldLayout;
            }

            // Build a new form layout with one row per OVF network name
            QFormLayout* form = new QFormLayout;
            for (int i = 0; i < this->m_ovfNetworkNames.size(); i++)
            {
                QComboBox* combo = new QComboBox;
                combo->setObjectName("ovfNetCombo_" + QString::number(i));
                this->populateNetworkComboBox(combo);
                form->addRow(this->m_ovfNetworkNames.at(i), combo);
            }
            ovfMappingContainer->setLayout(form);
        }

        if (ovfMappingGroup)
            ovfMappingGroup->setVisible(true);
    }
    else
    {
        // XVA / VHD / VMDK: use the single networkCombo
        if (ovfMappingGroup)
            ovfMappingGroup->setVisible(false);

        QComboBox* networkCombo = this->findChild<QComboBox*>("networkCombo");
        this->populateNetworkComboBox(networkCombo);

        if (xvaNetGroup)
            xvaNetGroup->setVisible(true);
    }
}

void ImportWizard::populateFixupIsoCombo()
{
    QComboBox* fixupSrCombo = this->findChild<QComboBox*>("fixupSrCombo");
    if (!fixupSrCombo)
        return;

    fixupSrCombo->clear();
    fixupSrCombo->addItem(tr("(none)"), QString());

    if (!this->m_connection)
        return;

    XenCache* cache = this->m_connection->GetCache();
    if (!cache)
        return;

    // Fixup ISO SR must be an ISO library (type == "iso") with at least one VDI
    QList<QSharedPointer<SR>> srs = cache->GetAll<SR>();
    for (const QSharedPointer<SR>& sr : srs)
    {
        if (!sr || !sr->IsValid())
            continue;
        if (!sr->IsISOLibrary())
            continue;
        if (sr->IsToolsSR())
            continue;
        fixupSrCombo->addItem(sr->GetName(), sr->OpaqueRef());
    }
}

void ImportWizard::updateOvfMetadataDisplay()
{
    QGroupBox* group = this->findChild<QGroupBox*>("ovfMetaGroup");
    QLabel* label = this->findChild<QLabel*>("ovfMetaLabel");
    if (!group || !label)
        return;

    if (this->m_importType != ImportType_OVF || this->m_ovfVirtualSystemNames.isEmpty())
    {
        group->setVisible(false);
        return;
    }

    QStringList lines;

    if (!this->m_ovfPackageName.isEmpty())
        lines << tr("<b>Package:</b> %1").arg(this->m_ovfPackageName.toHtmlEscaped());

    const int vmCount = this->m_ovfVirtualSystemNames.size();
    if (vmCount == 1)
        lines << tr("<b>Virtual machine:</b> %1").arg(this->m_ovfVirtualSystemNames.first().toHtmlEscaped());
    else if (vmCount > 1)
        lines << tr("<b>Virtual machines (%1):</b> %2").arg(vmCount)
                                                       .arg(this->m_ovfVirtualSystemNames.join(", ").toHtmlEscaped());

    if (!this->m_ovfNetworkNames.isEmpty())
        lines << tr("<b>Networks:</b> %1").arg(this->m_ovfNetworkNames.join(", ").toHtmlEscaped());

    if (!this->m_ovfEulas.isEmpty())
        lines << tr("<b>EULA:</b> This package contains a license agreement that you must accept.");

    if (this->m_ovfHasManifest)
        lines << tr("<b>Manifest:</b> Present");

    if (this->m_ovfHasSignature)
        lines << tr("<b>Signature:</b> Present");

    label->setText(lines.join("<br>"));
    group->setVisible(true);
}

void ImportWizard::updateXvaMetadataDisplay()
{
    QGroupBox* group = this->findChild<QGroupBox*>("xvaMetaGroup");
    QLabel* label = this->findChild<QLabel*>("xvaMetaLabel");
    if (!group || !label)
        return;

    if (this->m_importType != ImportType_XVA || this->m_xvaVmNames.isEmpty())
    {
        group->setVisible(false);
        return;
    }

    QStringList lines;

    const int vmCount = this->m_xvaVmNames.size();
    if (vmCount == 1)
        lines << tr("<b>Virtual machine:</b> %1").arg(this->m_xvaVmNames.first().toHtmlEscaped());
    else if (vmCount > 1)
        lines << tr("<b>Virtual machines (%1):</b> %2").arg(vmCount)
                                                       .arg(this->m_xvaVmNames.join(", ").toHtmlEscaped());

    if (this->m_xvaTotalDiskSizeBytes > 0)
    {
        double gb = static_cast<double>(this->m_xvaTotalDiskSizeBytes) / (1024.0 * 1024.0 * 1024.0);
        lines << tr("<b>Total disk size:</b> %1 GB").arg(gb, 0, 'f', 1);
    }

    label->setText(lines.join("<br>"));
    group->setVisible(true);
}

bool ImportWizard::inspectXvaTar(const QString& filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return false;

    // Read first TAR header block (512 bytes)
    QByteArray headerBlock = f.read(512);
    if (headerBlock.size() < 512)
    {
        f.close();
        return false;
    }

    // Parse filename — NUL-terminated in bytes 0..99
    const QByteArray nameField = headerBlock.left(100);
    const int nulPos = nameField.indexOf('\0');
    const QString entryName = (nulPos > 0) ? QString::fromLatin1(nameField.left(nulPos))
                                            : QString::fromLatin1(nameField);

    if (entryName != "ova.xml")
    {
        f.close();
        return false;
    }

    // Parse file size from octal field at bytes 124..135
    bool ok = false;
    const QByteArray sizeField = headerBlock.mid(124, 12);
    qint64 fileSize = sizeField.trimmed().toLongLong(&ok, 8);
    if (!ok || fileSize <= 0 || fileSize > 10 * 1024 * 1024)
    {
        f.close();
        return false;
    }

    // Read ova.xml content (skip TAR checksum/type fields — data starts right after header)
    const QByteArray ovaXmlData = f.read(fileSize);
    f.close();

    if (ovaXmlData.size() != fileSize)
        return false;

    // Parse ova.xml
    // XVA ova.xml structure:
    //   <appliance> <vm handle="0"> ... <name>label</name> ... </vm>
    //               <vdi handle="1"> ... <virtual_size>bytes</virtual_size> ... </vdi>
    //   </appliance>
    QDomDocument doc;
    QString parseError;
    if (!doc.setContent(ovaXmlData, &parseError))
    {
        qDebug() << "ImportWizard: failed to parse ova.xml:" << parseError;
        return false;
    }

    const QDomElement root = doc.documentElement();

    // Collect VM names
    const QDomNodeList vmNodes = root.elementsByTagName("vm");
    for (int i = 0; i < vmNodes.count(); i++)
    {
        const QDomElement vmEl = vmNodes.at(i).toElement();
        const QDomNodeList nameNodes = vmEl.elementsByTagName("name");
        if (nameNodes.count() > 0)
        {
            const QString name = nameNodes.at(0).toElement().text().trimmed();
            if (!name.isEmpty())
                this->m_xvaVmNames << name;
        }
    }

    // Collect total virtual disk size
    const QDomNodeList vdiNodes = root.elementsByTagName("vdi");
    for (int i = 0; i < vdiNodes.count(); i++)
    {
        const QDomElement vdiEl = vdiNodes.at(i).toElement();
        const QDomNodeList sizeNodes = vdiEl.elementsByTagName("virtual_size");
        if (sizeNodes.count() > 0)
        {
            bool szOk = false;
            const qint64 sz = sizeNodes.at(0).toElement().text().trimmed().toLongLong(&szOk);
            if (szOk && sz > 0)
                this->m_xvaTotalDiskSizeBytes += sz;
        }
    }

    return !this->m_xvaVmNames.isEmpty();
}

bool ImportWizard::inspectDiskImage(const QString& filePath)
{
    this->m_diskImageCapacityBytes = 0;
    this->m_diskImageFormatName.clear();

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return false;

    // VHD: last 512 bytes are the footer.
    // Cookie at footer offset 0 is "conectix" (8 bytes).
    // Original size (virtual disk capacity) is at footer offset 40, big-endian uint64.
    const qint64 fileSize = f.size();
    if (fileSize >= 512)
    {
        // Read the first 512 bytes to detect magic inline with detectImportType(),
        // but for capacity we need the footer (last 512 bytes).
        if (f.seek(fileSize - 512))
        {
            const QByteArray footer = f.read(512);
            if (footer.size() == 512 && footer.startsWith("conectix"))
            {
                // VHD footer: bytes 40-47 = original size (big-endian uint64)
                quint64 capacity = 0;
                for (int i = 0; i < 8; i++)
                    capacity = (capacity << 8) | static_cast<quint8>(footer.at(40 + i));
                if (capacity > 0)
                {
                    this->m_diskImageCapacityBytes = static_cast<qint64>(capacity);
                    this->m_diskImageFormatName = "VHD";
                    f.close();
                    return true;
                }
            }
        }
    }

    // VMDK: sparse extent has a 4-byte little-endian magic 0x564D444B at offset 0.
    // Capacity is a uint64 at offset 8 in the header (sector count; multiply by 512 for bytes).
    if (f.seek(0))
    {
        const QByteArray header = f.read(16);
        if (header.size() >= 16)
        {
            const quint32 magic =
                static_cast<quint32>(static_cast<quint8>(header.at(0)))        |
                (static_cast<quint32>(static_cast<quint8>(header.at(1))) << 8)  |
                (static_cast<quint32>(static_cast<quint8>(header.at(2))) << 16) |
                (static_cast<quint32>(static_cast<quint8>(header.at(3))) << 24);

            if (magic == 0x564D444B) // "KDMV" little-endian
            {
                quint64 sectors = 0;
                for (int i = 0; i < 8; i++)
                    sectors |= (static_cast<quint64>(static_cast<quint8>(header.at(8 + i))) << (8 * i));
                if (sectors > 0)
                {
                    this->m_diskImageCapacityBytes = static_cast<qint64>(sectors) * 512;
                    this->m_diskImageFormatName = "VMDK";
                    f.close();
                    return true;
                }
            }
        }
    }

    f.close();
    return false;
}

void ImportWizard::updateDiskImageDisplay()
{
    QGroupBox* group = this->findChild<QGroupBox*>("diskMetaGroup");
    QLabel* label = this->findChild<QLabel*>("diskMetaLabel");
    if (!group || !label)
        return;

    if ((this->m_importType != ImportType_VHD) || this->m_diskImageCapacityBytes <= 0)
    {
        group->setVisible(false);
        return;
    }

    QStringList lines;
    lines << tr("<b>Format:</b> %1").arg(this->m_diskImageFormatName.toHtmlEscaped());
    const double gb = static_cast<double>(this->m_diskImageCapacityBytes) / (1024.0 * 1024.0 * 1024.0);
    lines << tr("<b>Virtual disk capacity:</b> %1 GB").arg(gb, 0, 'f', 1);

    label->setText(lines.join("<br>"));
    group->setVisible(true);
}

bool ImportWizard::validateCurrentPage()
{
    int currentId = this->currentId();

    if (currentId == Page_Source)
    {
        QLineEdit* filePathEdit = this->findChild<QLineEdit*>("filePathEdit");
        QString filePath = filePathEdit ? filePathEdit->text().trimmed() : QString();
        if (filePath.isEmpty())
        {
            QMessageBox::warning(this, tr("Invalid Input"),
                                 tr("Please select a file to import."));
            return false;
        }

        if (!QFile::exists(filePath))
        {
            QMessageBox::warning(this, tr("File Not Found"),
                                 tr("The selected file does not exist."));
            return false;
        }

        // ── Decompress .gz files before any further processing ────────────
        // C# equivalent: ImportSourcePage.Uncompress() / _unzipWorker_DoWork()
        // We decompress to a sibling file (same dir, without the .gz suffix).
        // The decompressed file is what the rest of the wizard and the action use.
        if (filePath.endsWith(".gz", Qt::CaseInsensitive))
        {
            // Compute output path: strip .gz suffix
            const QString decompPath = filePath.left(filePath.length() - 3);

            if (QFile::exists(decompPath))
            {
                const auto reply = QMessageBox::question(
                    this, tr("File Already Exists"),
                    tr("The decompressed file already exists:\n%1\n\nOverwrite it?").arg(decompPath),
                    QMessageBox::Yes | QMessageBox::No);
                if (reply != QMessageBox::Yes)
                    return false;
                QFile::remove(decompPath);
            }

            DecompressGzAction* op = new DecompressGzAction(filePath, decompPath, this);
            ActionProgressDialog dlg(op, this);
            dlg.setShowCancel(true);
            dlg.exec();  // starts the op via showEvent, blocks until done or closed

            if (op->IsCancelled() || op->IsFailed())
            {
                QFile::remove(decompPath);  // clean up partial output if any
                return false;
            }

            // Update file path and the UI line edit to the decompressed file
            filePath = decompPath;
            if (filePathEdit)
                filePathEdit->setText(filePath);
        }

        // Detect and validate import type; caches result in m_importType
        QString detectedType = this->detectImportType(filePath);
        if (detectedType.isEmpty())
        {
            QMessageBox::warning(this, tr("Unsupported File"),
                                 tr("The selected file format is not supported."));
            return false;
        }

        this->m_sourceFilePath = filePath;
        SettingsManager::instance().SetDefaultImportPath(QFileInfo(filePath).absolutePath());

        QLabel* typeLabel = this->findChild<QLabel*>("typeLabel");
        if (typeLabel)
            typeLabel->setText(tr("Detected: %1").arg(detectedType));

        // For OVF/OVA packages: parse metadata and block if encrypted
        if (this->m_importType == ImportType_OVF)
        {
            OvfPackage pkg(filePath);
            if (!pkg.IsValid())
            {
                QMessageBox::warning(this, tr("Invalid OVF Package"),
                                     tr("The OVF/OVA file could not be read:\n%1").arg(pkg.ParseError()));
                return false;
            }
            if (pkg.HasEncryption())
            {
                QMessageBox::warning(this, tr("Encrypted OVF Not Supported"),
                                     tr("This OVF/OVA package is encrypted. "
                                        "Encrypted OVF packages cannot be imported."));
                return false;
            }

            // Show validation warnings unless the user has opted out
            // C# equivalent: ImportSourcePage.LoadAppliance() → OvfValidationDialog
            if (!SettingsManager::instance().GetIgnoreOvfValidationWarnings())
            {
                QStringList warnings;
                if (!pkg.Validate(warnings))
                {
                    // Structural failure — cannot continue
                    QMessageBox::warning(this, tr("Invalid OVF Package"),
                                         tr("OVF package validation failed:\n%1").arg(warnings.join("\n")));
                    return false;
                }
                if (!warnings.isEmpty())
                {
                    OvfValidationDialog dlg(warnings, this);
                    if (dlg.exec() != QDialog::Accepted)
                        return false;
                }
            }

            // Cache metadata for downstream pages and metadata display
            this->m_ovfPackageName          = pkg.PackageName();
            this->m_ovfVirtualSystemNames   = pkg.VirtualSystemNames();
            this->m_ovfNetworkNames         = pkg.NetworkNames();
            this->m_ovfEulas                = pkg.Eulas();
            this->m_ovfHasManifest          = pkg.HasManifest();
            this->m_ovfHasSignature         = pkg.HasSignature();
            this->m_ovfAllowsNetworkSriov   = pkg.AllowsNetworkSriov();
            this->updateOvfMetadataDisplay();
        }
        else
        {
            // Clear any stale OVF metadata when switching file types
            this->m_ovfPackageName.clear();
            this->m_ovfVirtualSystemNames.clear();
            this->m_ovfNetworkNames.clear();
            this->m_ovfEulas.clear();
            this->m_ovfHasManifest = false;
            this->m_ovfHasSignature = false;
            this->m_ovfAllowsNetworkSriov = true;
            this->updateOvfMetadataDisplay();

            // Try to read XVA metadata for display
            this->m_xvaVmNames.clear();
            this->m_xvaTotalDiskSizeBytes = 0;
            this->m_diskImageCapacityBytes = 0;
            this->m_diskImageFormatName.clear();
            if (this->m_importType == ImportType_XVA)
            {
                this->inspectXvaTar(filePath);  // Non-fatal if it fails

                // Fall back to file size if metadata couldn't supply disk capacity
                if (this->m_xvaTotalDiskSizeBytes == 0)
                    this->m_xvaTotalDiskSizeBytes = QFileInfo(filePath).size();
            }
            else if (this->m_importType == ImportType_VHD)
            {
                if (!this->inspectDiskImage(filePath))
                {
                    // Could not parse disk image header — fall back to file size for capacity.
                    // Warn the user; they can still continue if the format is otherwise valid.
                    const qint64 fileBytes = QFileInfo(filePath).size();
                    this->m_diskImageCapacityBytes = fileBytes;
                    this->m_diskImageFormatName = tr("Unknown");
                    QMessageBox::warning(this, tr("Disk Image Format"),
                        tr("The disk image format could not be determined from the file header. "
                           "The file size (%1 bytes) will be used as the virtual disk capacity.\n\n"
                           "If the file is a valid VHD or VMDK, you can continue. "
                           "Otherwise the import may fail.").arg(fileBytes));
                }
            }
            this->updateXvaMetadataDisplay();
            this->updateDiskImageDisplay();
        }
    }
    else if (currentId == Page_VmConfig)
    {
        QLineEdit* nameEdit = this->findChild<QLineEdit*>("vmNameEdit");
        if (!nameEdit || nameEdit->text().trimmed().isEmpty())
        {
            QMessageBox::warning(this, tr("Invalid Input"),
                                 tr("Please enter a name for the virtual machine."));
            return false;
        }
    }
    else if (currentId == Page_Eula)
    {
        // The user must explicitly accept all EULAs to continue.
        // Matches C# ImportEulaPage.EnableNext() gated on m_checkBoxAccept.
        QCheckBox* acceptCheck = this->findChild<QCheckBox*>("eulaAcceptCheck");
        if (!acceptCheck || !acceptCheck->isChecked())
        {
            QMessageBox::warning(this, tr("License Agreement Required"),
                                 tr("You must accept the End User License Agreement(s) to continue."));
            return false;
        }
    }
    else if (currentId == Page_Storage)
    {
        QComboBox* srCombo = this->findChild<QComboBox*>("storageCombo");
        const QString srRef = srCombo ? srCombo->currentData().toString() : QString();
        if (srRef.isEmpty())
        {
            QMessageBox::warning(this, tr("No Storage Selected"),
                                 tr("Please select a storage repository."));
            return false;
        }

        // Resolve and cache the selected SR for use later (avoids repeated lookups)
        this->m_selectedSR = this->m_connection
            ? this->m_connection->GetCache()->ResolveObject<SR>(srRef)
            : nullptr;

        // Capacity check: warn (non-blocking) if the SR's free space is smaller
        // than the estimated import size. Matches C# non-blocking behavior.
        if (this->m_selectedSR && this->m_selectedSR->IsValid())
        {
            const qint64 freeBytes = this->m_selectedSR->FreeSpace();
            qint64 requiredBytes = 0;
            if (this->m_importType == ImportType_XVA)
                requiredBytes = this->m_xvaTotalDiskSizeBytes;
            else if (this->m_importType == ImportType_VHD)
                requiredBytes = this->m_diskImageCapacityBytes;

            if (freeBytes > 0 && requiredBytes > 0 && freeBytes < requiredBytes)
            {
                const double freeGb = static_cast<double>(freeBytes)    / (1024.0 * 1024.0 * 1024.0);
                const double reqGb  = static_cast<double>(requiredBytes) / (1024.0 * 1024.0 * 1024.0);
                const QString msg = tr("The selected storage repository may not have enough free space.\n\n"
                                       "Available: %1 GB\n"
                                       "Required:  %2 GB\n\n"
                                       "You can still proceed, but the import may fail if the SR is full.")
                                    .arg(freeGb, 0, 'f', 1)
                                    .arg(reqGb,  0, 'f', 1);
                QMessageBox::warning(this, tr("Low Storage Space"), msg);
                // Non-blocking — fall through to QWizard::validateCurrentPage()
            }
        }

        // ── XVA: start the upload now, exactly as C# StoragePickerPage.PageLeaveCore ──
        // For OVF and VHD the upload is handled by separate actions started after the wizard.
        if (this->m_importType == ImportType_XVA)
        {
            // If we already have the imported VM (e.g. user navigated back and forward
            // again), there is nothing to do — the action is already running.
            if (this->m_xvaImportedVm && this->m_xvaImportedVm->IsValid())
                return true;

            // Clean up a previous failed/cancelled attempt so the user can retry.
            if (this->m_xvaAction)
            {
                if (!this->m_xvaAction->IsFailed() && !this->m_xvaAction->IsCancelled())
                {
                    // Upload is in progress — wizard shouldn't have reached here, but guard.
                    return false;
                }
                this->m_xvaAction->deleteLater();
                this->m_xvaAction = nullptr;
                this->m_xvaImportedVm.clear();
            }

            const QString hostRef = this->m_selectedHost ? this->m_selectedHost->OpaqueRef() : QString();
            const QString srRef2  = this->m_selectedSR   ? this->m_selectedSR->OpaqueRef()   : QString();

            this->m_xvaAction = new ImportVmAction(
                this->m_connection, hostRef, this->m_sourceFilePath, srRef2, this);

            // Show upload progress.  The dialog calls RunAsync() in showEvent.
            // We close it early — when the VM is discovered — rather than waiting
            // for the full action to finish (wizard-wait phase follows).
            ActionProgressDialog* uploadDlg = new ActionProgressDialog(this->m_xvaAction, this);
            uploadDlg->setShowCancel(true);

            // When VM is found in the XAPI cache the action emits vmDiscovered (from the
            // worker thread).  Qt::QueuedConnection ensures the slot runs on the UI thread.
            connect(this->m_xvaAction, &ImportVmAction::vmDiscovered, this,
                [this, uploadDlg](const QString& discoveredRef)
                {
                    // Resolve the VM object from cache and keep it for the network page.
                    if (this->m_connection && this->m_connection->GetCache())
                    {
                        this->m_xvaImportedVm = this->m_connection->GetCache()
                            ->ResolveObject<VM>(XenObjectType::VM, discoveredRef);
                    }
                    // Close the upload dialog — exec() will return Accepted.
                    if (uploadDlg)
                        uploadDlg->accept();
                }, Qt::QueuedConnection);

            // If the action fails or is cancelled before the VM is found, the upload
            // dialog's own onOperationCompleted() will call switchToErrorState() and
            // the user closes it manually (exec returns Rejected).

            const int dlgResult = uploadDlg->exec();
            uploadDlg->deleteLater();

            if (dlgResult != QDialog::Accepted || !this->m_xvaImportedVm || !this->m_xvaImportedVm->IsValid())
            {
                // Upload failed, was cancelled, or VM not found — clean up and stay on page.
                if (this->m_xvaAction)
                {
                    this->m_xvaAction->deleteLater();
                    this->m_xvaAction = nullptr;
                }
                this->m_xvaImportedVm.clear();
                return false;
            }

            // Upload succeeded and VM is in cache — advance to Page_Network.
            return true;
        }
    }

    return QWizard::validateCurrentPage();
}

void ImportWizard::accept()
{
    // Collect all configuration choices into result members before closing
    QLineEdit* filePathEdit = this->findChild<QLineEdit*>("filePathEdit");
    if (filePathEdit)
        this->m_sourceFilePath = filePathEdit->text();

    QComboBox* hostCombo = this->findChild<QComboBox*>("hostCombo");
    if (hostCombo && !this->m_selectedHost)
    {
        // Fallback: wizard was accepted without going through Page_Storage/Network
        // (e.g. direct finish) — resolve host now.
        this->m_selectedHost = this->m_connection
            ? this->m_connection->GetCache()->ResolveObject<Host>(hostCombo->currentData().toString())
            : nullptr;
    }

    // VHD/VMDK VM config
    QLineEdit* vmNameEdit = this->findChild<QLineEdit*>("vmNameEdit");
    if (vmNameEdit)
        this->m_vmName = vmNameEdit->text().trimmed();
    QSpinBox* vcpuSpin = this->findChild<QSpinBox*>("vcpuSpin");
    if (vcpuSpin)
        this->m_vcpuCount = vcpuSpin->value();
    QSpinBox* memorySpin = this->findChild<QSpinBox*>("memorySpin");
    if (memorySpin)
        this->m_memoryMb = memorySpin->value();

    QComboBox* networkCombo = this->findChild<QComboBox*>("networkCombo");
    if (networkCombo)
    {
        this->m_selectedNetwork = this->m_connection
            ? this->m_connection->GetCache()->ResolveObject<Network>(networkCombo->currentData().toString())
            : nullptr;
    }

    // Collect OVF per-network mappings (index-based combos created by populateNetworkCombo)
    this->m_ovfNetworkMappings.clear();
    for (int i = 0; i < this->m_ovfNetworkNames.size(); i++)
    {
        QComboBox* combo = this->findChild<QComboBox*>("ovfNetCombo_" + QString::number(i));
        if (combo)
            this->m_ovfNetworkMappings[this->m_ovfNetworkNames.at(i)] = combo->currentData().toString();
    }

    // For OVF with manifest/signature the Security page verify checkbox is
    // authoritative; for all other paths verification is not applicable.
    this->m_verifyManifest = true;
    if (this->m_importType == ImportType_OVF && (this->m_ovfHasManifest || this->m_ovfHasSignature))
    {
        QCheckBox* securityVerify = this->findChild<QCheckBox*>("securityVerifyCheck");
        if (securityVerify)
            this->m_verifyManifest = securityVerify->isChecked();
    }

    QCheckBox* runFixupsBox = this->findChild<QCheckBox*>("runFixups");
    this->m_runFixups = runFixupsBox ? runFixupsBox->isChecked() : false;

    QComboBox* fixupSrCombo = this->findChild<QComboBox*>("fixupSrCombo");
    this->m_fixupIsoSrRef = (fixupSrCombo && this->m_runFixups) ? fixupSrCombo->currentData().toString() : QString();

    QCheckBox* startAfter = this->findChild<QCheckBox*>("startAfterImport");
    this->m_startVMsAutomatically = startAfter ? startAfter->isChecked() : false;

    // Boot options (VHD/VMDK only — collected from Page_BootOptions)
    this->m_bootMode  = BootMode_Bios;
    this->m_assignVtpm = false;
    if (this->m_importType == ImportType_VHD)
    {
        QRadioButton* uefiSecureRadio = this->findChild<QRadioButton*>("bootUefiSecureRadio");
        QRadioButton* uefiRadio       = this->findChild<QRadioButton*>("bootUefiRadio");
        if (uefiSecureRadio && uefiSecureRadio->isChecked())
            this->m_bootMode = BootMode_UefiSecure;
        else if (uefiRadio && uefiRadio->isChecked())
            this->m_bootMode = BootMode_Uefi;

        QCheckBox* vtpmCheck = this->findChild<QCheckBox*>("vtpmCheck");
        this->m_assignVtpm = vtpmCheck ? vtpmCheck->isChecked() : false;
    }

    qDebug() << "ImportWizard: Accept — file:" << this->m_sourceFilePath
             << "host:" << (this->m_selectedHost ? this->m_selectedHost->OpaqueRef() : "(none)")
             << "sr:" << (this->m_selectedSR ? this->m_selectedSR->OpaqueRef() : "(none)")
             << "network:" << (this->m_selectedNetwork ? this->m_selectedNetwork->OpaqueRef() : "(none)")
             << "start:" << this->m_startVMsAutomatically;

    // XVA: call endWizard() to unblock the action's wizard-wait phase.
    // The action will then do VIF remapping and optional VM start in the background.
    // This mirrors C# ImportWizard.FinishWizard() → action.EndWizard(...).
    if (this->m_importType == ImportType_XVA && this->m_xvaAction)
    {
        QStringList vifTargetNetworks;
        if (this->m_selectedNetwork)
            vifTargetNetworks << this->m_selectedNetwork->OpaqueRef();
        this->m_xvaAction->endWizard(this->m_startVMsAutomatically, vifTargetNetworks);
    }

    QWizard::accept();
}

void ImportWizard::reject()
{
    // XVA: cancel the running upload/wait action, matching C# ImportWizard.OnCancel().
    // EndWizard(false, {}) wakes the wizard-wait condition; Cancel() stops any further work.
    if (this->m_importType == ImportType_XVA && this->m_xvaAction)
    {
        if (!this->m_xvaAction->IsFailed() && !this->m_xvaAction->IsCancelled())
        {
            this->m_xvaAction->endWizard(false, QStringList());
            this->m_xvaAction->Cancel();
        }
    }
    QWizard::reject();
}

QString ImportWizard::detectImportType(const QString& filePath)
{
    const QString lower = filePath.toLower();

    // ── Content-based detection ──────────────────────────────────────────
    // Read the first 512 bytes to determine actual file format, regardless of extension.
    QFile f(filePath);
    QByteArray header;
    if (f.open(QIODevice::ReadOnly))
    {
        header = f.read(512);
        f.close();
    }

    // GZip magic: 0x1F 0x8B
    const bool isGzip = (header.size() >= 2) &&
                        (static_cast<unsigned char>(header[0]) == 0x1F) &&
                        (static_cast<unsigned char>(header[1]) == 0x8B);

    // POSIX/GNU TAR magic: "ustar" at offset 257 (in a 512-byte TAR header block)
    const bool isTar = (header.size() >= 262) &&
                       (header.mid(257, 5) == QByteArrayLiteral("ustar"));

    // Old-format TAR: no magic, but NUL-padded 100-byte filename field at offset 0.
    // XVA files created by XenServer use this format. Heuristic: first char printable,
    // rest of name field contains at least some printable ASCII before the NUL.
    const bool isOldTar = !isTar && (header.size() >= 512) &&
                          (static_cast<unsigned char>(header[0]) >= 0x20) &&
                          (static_cast<unsigned char>(header[0]) < 0x7F) &&
                          (header.indexOf('\0', 0) > 0) &&    // NUL-terminated name
                          (header.indexOf('\0', 0) < 100);    // within 100-byte name field

    // OVF XML: starts with "<?xml" or "<Envelope"
    const bool isOvfXml = header.startsWith("<?xml") || header.startsWith("<Envelope") ||
                          header.startsWith("\xEF\xBB\xBF<?xml");  // BOM-prefixed XML

    // ── Map content to import type ────────────────────────────────────────

    // XVA.GZ: gzip-compressed TAR (the GZ wraps a TAR)
    if (isGzip && (lower.endsWith(".xva.gz") || lower.endsWith(".xva")))
    {
        this->m_importType = ImportType_XVA;
        return tr("XenServer Virtual Appliance (XVA, compressed)");
    }

    // OVA.GZ: gzip-compressed OVA
    if (isGzip && (lower.endsWith(".ova.gz") || lower.endsWith(".ova")))
    {
        this->m_importType = ImportType_OVF;
        return tr("Open Virtualization Format (OVA, compressed)");
    }

    // Plain GZ without a recognised inner extension — reject
    if (isGzip)
    {
        // Fall through to extension-based check for unusual naming
    }

    // TAR/XVA: a TAR that isn't OVA-named
    if ((isTar || isOldTar) && !lower.endsWith(".ova"))
    {
        this->m_importType = ImportType_XVA;
        return tr("XenServer Virtual Appliance (XVA)");
    }

    // OVA: a TAR with .ova extension
    if ((isTar || isOldTar) && lower.endsWith(".ova"))
    {
        this->m_importType = ImportType_OVF;
        return tr("Open Virtualization Format (OVA)");
    }

    // OVF XML descriptor
    if (isOvfXml || lower.endsWith(".ovf"))
    {
        this->m_importType = ImportType_OVF;
        return tr("Open Virtualization Format (OVF)");
    }

    // VHD: starts with "conectix" cookie at offset 0 (VHD footer) or offset 512 (VHD with header)
    if (header.startsWith("conectix") || (header.size() >= 520 && header.mid(512, 8).startsWith("conectix")))
    {
        this->m_importType = ImportType_VHD;
        return tr("Virtual Hard Disk (VHD)");
    }

    // VMDK: sparse extent header magic 0x44 0x4D 0x44 0x4B ("KDMV" little-endian)
    if (header.size() >= 4 &&
        static_cast<unsigned char>(header[0]) == 0x4B &&
        static_cast<unsigned char>(header[1]) == 0x44 &&
        static_cast<unsigned char>(header[2]) == 0x4D &&
        static_cast<unsigned char>(header[3]) == 0x56)
    {
        this->m_importType = ImportType_VHD;
        return tr("Virtual Hard Disk (VMDK)");
    }

    // ── Extension-only fallback (file too small / empty / no content read) ────
    if (lower.endsWith(".xva") || lower.endsWith(".xva.gz"))
    {
        this->m_importType = ImportType_XVA;
        return tr("XenServer Virtual Appliance (XVA)");
    }
    if (lower.endsWith(".ovf") || lower.endsWith(".ova") || lower.endsWith(".ova.gz"))
    {
        this->m_importType = ImportType_OVF;
        return tr("Open Virtualization Format (OVF/OVA)");
    }
    if (lower.endsWith(".vhd") || lower.endsWith(".vmdk"))
    {
        this->m_importType = ImportType_VHD;
        return tr("Virtual Hard Disk (VHD/VMDK)");
    }

    return QString(); // Unsupported type
}

void ImportWizard::onCurrentIdChanged(int id)
{
    Q_UNUSED(id)
}

void ImportWizard::onRescanStorageClicked()
{
    if (!this->m_connection)
        return;

    QComboBox* srCombo = this->findChild<QComboBox*>("storageCombo");
    const QString srRef = srCombo ? srCombo->currentData().toString() : QString();
    if (srRef.isEmpty())
        return;

    QPushButton* rescanButton = this->findChild<QPushButton*>("storageRescanButton");
    if (rescanButton)
        rescanButton->setEnabled(false);

    // C# equivalent: SrRefreshAction(sr).RunAsync() from SrPicker.ScanSRs()
    SrRefreshAction* action = new SrRefreshAction(this->m_connection, srRef, this);
    connect(action, &AsyncOperation::completed, this, [this, rescanButton]()
    {
        // Re-populate the combo so updated free-space figures are visible
        this->populateStorageCombo();
        if (rescanButton)
            rescanButton->setEnabled(true);
    });
    action->RunAsync();
}

void ImportWizard::onBrowseClicked()
{
    QString filter = tr("All Supported Files (*.xva *.xva.gz *.ovf *.ova *.ova.gz *.vhd *.vmdk);;"
                        "XVA Files (*.xva *.xva.gz);;"
                        "OVF/OVA Files (*.ovf *.ova *.ova.gz);;"
                        "VHD/VMDK Files (*.vhd *.vmdk);;"
                        "All Files (*)");

    QString startDir = SettingsManager::instance().GetDefaultImportPath();

    QString filePath = QFileDialog::getOpenFileName(this,
                                                    tr("Select File to Import"),
                                                    startDir,
                                                    filter);

    if (!filePath.isEmpty())
    {
        QLineEdit* filePathEdit = this->findChild<QLineEdit*>("filePathEdit");
        if (filePathEdit)
            filePathEdit->setText(filePath);

        SettingsManager::instance().SetDefaultImportPath(QFileInfo(filePath).absolutePath());
    }
}