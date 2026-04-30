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
#include "ui_importwizard.h"
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
#include "xenlib/xen/vif.h"
#include "xenlib/utils/decompressgzaction.h"
#include "xenlib/xen/actions/sr/srrefreshaction.h"
#include "xenlib/xen/actions/vm/importvmaction.h"
#include "xenlib/xen/failure.h"
#include "dialogs/actionprogressdialog.h"
#include <QDebug>
#include <QtWidgets>
#include <QDomDocument>
#include <QRandomGenerator>
#include <QRegularExpression>

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
    , ui(new Ui::ImportWizard)
{
    this->ui->setupUi(this);

    this->setupWizardPages();

    connect(this, &QWizard::currentIdChanged, this, &ImportWizard::onCurrentIdChanged);

    // Pre-populate file path if one was provided (e.g. from a file association or CLI)
    if (!initialFilePath.isEmpty())
        this->ui->filePathEdit->setText(initialFilePath);

    qDebug() << "ImportWizard: Created Import Wizard";
}

void ImportWizard::setupWizardPages()
{
    this->setPage(Page_Source,      this->ui->pageSource);
    this->setPage(Page_VmConfig,    this->ui->pageVmConfig);
    this->setPage(Page_Eula,        this->ui->pageEula);
    this->setPage(Page_Host,        this->ui->pageHost);
    this->setPage(Page_Storage,     this->ui->pageStorage);
    this->setPage(Page_Network,     this->ui->pageNetwork);
    this->setPage(Page_Security,    this->ui->pageSecurity);
    this->setPage(Page_Options,     this->ui->pageOptions);
    this->setPage(Page_BootOptions, this->ui->pageBootOptions);
    this->setPage(Page_Finish,      this->ui->pageFinish);

    this->setStartId(Page_Source);

    // Signal connections that were previously inside the create*Page() factories
    connect(this->ui->browseButton, &QPushButton::clicked, this, &ImportWizard::onBrowseClicked);
    connect(this->ui->storageRescanButton, &QPushButton::clicked, this, &ImportWizard::onRescanStorageClicked);
    connect(this->ui->runFixups, &QCheckBox::toggled, this->ui->fixupSrLabel, &QLabel::setEnabled);
    connect(this->ui->runFixups, &QCheckBox::toggled, this->ui->fixupSrCombo, &QComboBox::setEnabled);
}

ImportWizard::~ImportWizard()
{
    delete this->ui;
}

int ImportWizard::nextId() const
{
    switch (this->currentId())
    {
        case Page_Source:
            if (this->m_importType == ImportType_VHD)
                return Page_VmConfig;
            if (this->m_importType == ImportType_OVF && this->HasOvfEulas())
                return Page_Eula;
            return Page_Host;
        case Page_Eula:
        case Page_VmConfig:
            return Page_Host;
        case Page_Host:
            return Page_Storage;
        case Page_Storage:
            return Page_Network;
        case Page_Network:
            if (this->m_importType == ImportType_XVA)
                return Page_Finish;
            if (this->m_importType == ImportType_OVF && this->OvfHasSecurity())
                return Page_Security;
            return Page_Options;
        case Page_Security:
            return Page_Options;
        case Page_Options:
            if (this->m_importType == ImportType_VHD)
                return Page_BootOptions;
            return Page_Finish;
        case Page_BootOptions:
            return Page_Finish;
        default:
            return -1;
    }
}


void ImportWizard::initializePage(int id)
{
    if (id == Page_Host)
        this->populateHostCombo();
    else if (id == Page_VmConfig)
    {
        // Pre-fill VM name from the source file's base name
        QLineEdit* nameEdit = this->ui->vmNameEdit;
        if (nameEdit && nameEdit->text().isEmpty())
        {
            const QString baseName = QFileInfo(this->m_sourceFilePath).completeBaseName();
            nameEdit->setText(baseName);
        }
    }
    else if (id == Page_Storage)
    {
        // Capture the host selection so populateStorageCombo can filter by PBD
        QComboBox* hostCombo = this->ui->hostCombo;
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
        QComboBox* hostCombo = this->ui->hostCombo;
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
        QRadioButton* biosRadio       = this->ui->bootBiosRadio;
        QRadioButton* uefiRadio       = this->ui->bootUefiRadio;
        QRadioButton* uefiSecureRadio = this->ui->bootUefiSecureRadio;
        if (biosRadio && uefiRadio && uefiSecureRadio)
        {
            biosRadio->setChecked(this->m_bootMode == BootMode_Bios);
            uefiRadio->setChecked(this->m_bootMode == BootMode_Uefi);
            uefiSecureRadio->setChecked(this->m_bootMode == BootMode_UefiSecure);
        }
        QCheckBox* vtpmCheck = this->ui->vtpmCheck;
        if (vtpmCheck)
            vtpmCheck->setChecked(this->m_assignVtpm);
    }
    else if (id == Page_Eula)
    {
        // Populate tab widget with EULA texts — one tab per EULA document
        QTabWidget* tabWidget = this->ui->eulaTabWidget;
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
        QCheckBox* acceptCheck = this->ui->eulaAcceptCheck;
        if (acceptCheck)
            acceptCheck->setChecked(false);
    }
    else if (id == Page_Security)
    {
        // Update info label and verify checkbox based on whether this package has
        // a producer signature or just a manifest.  Matches C# ImportSecurityPage.
        QLabel* infoLabel = this->ui->securityInfoLabel;
        QCheckBox* verifyCheck = this->ui->securityVerifyCheck;
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
        QTextEdit* summaryText = this->ui->summaryText;
        if (summaryText)
        {
            QComboBox* hostCombo = this->ui->hostCombo;
            QComboBox* srCombo = this->ui->storageCombo;
            QComboBox* netCombo = this->ui->networkCombo;

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
                QLineEdit* vmNameEdit = this->ui->vmNameEdit;
                QSpinBox*  vcpuSpin  = this->ui->vcpuSpin;
                QSpinBox*  memSpin   = this->ui->memorySpin;
                if (vmNameEdit && !vmNameEdit->text().trimmed().isEmpty())
                    summary += tr("VM Name: %1\n").arg(vmNameEdit->text().trimmed());
                if (vcpuSpin)
                    summary += tr("vCPUs: %1\n").arg(vcpuSpin->value());
                if (memSpin)
                    summary += tr("Memory: %1 MB\n").arg(memSpin->value());

                // Boot mode (read live from the radio buttons so the summary stays in sync
                // even if the user navigated back and changed the selection)
                QRadioButton* uefiSecureRadio = this->ui->bootUefiSecureRadio;
                QRadioButton* uefiRadio       = this->ui->bootUefiRadio;
                QString bootModeStr;
                if (uefiSecureRadio && uefiSecureRadio->isChecked())
                    bootModeStr = tr("UEFI Secure Boot");
                else if (uefiRadio && uefiRadio->isChecked())
                    bootModeStr = tr("UEFI");
                else
                    bootModeStr = tr("BIOS (Legacy)");
                summary += tr("Boot Firmware: %1\n").arg(bootModeStr);

                QCheckBox* vtpmCheck = this->ui->vtpmCheck;
                if (vtpmCheck && vtpmCheck->isChecked())
                    summary += tr("vTPM: Yes\n");
            }

            QCheckBox* startAfter = this->ui->startAfterImport;
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
    QComboBox* hostCombo = this->ui->hostCombo;
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
    QComboBox* srCombo = this->ui->storageCombo;
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

// ─── MAC helpers ─────────────────────────────────────────────────────────────

bool ImportWizard::isValidMac(const QString& mac)
{
    if (mac.isEmpty())
        return true;    // empty = auto-generate, always valid
    static const QRegularExpression re("^([0-9A-Fa-f]{2}[:\\-]){5}[0-9A-Fa-f]{2}$");
    return re.match(mac).hasMatch();
}

QString ImportWizard::generateMac()
{
    // Locally-administered, unicast prefix: 02:xx:xx:xx:xx:xx
    QRandomGenerator* rng = QRandomGenerator::global();
    return QString("02:%1:%2:%3:%4:%5")
        .arg(rng->bounded(256), 2, 16, QChar('0'))
        .arg(rng->bounded(256), 2, 16, QChar('0'))
        .arg(rng->bounded(256), 2, 16, QChar('0'))
        .arg(rng->bounded(256), 2, 16, QChar('0'))
        .arg(rng->bounded(256), 2, 16, QChar('0'));
}

void ImportWizard::populateNetworkCombo()
{
    QGroupBox* xvaNetGroup = this->ui->xvaNetGroup;
    QGroupBox* ovfMappingGroup = this->ui->ovfMappingGroup;
    QWidget* ovfMappingContainer = this->ui->ovfMappingContainer;

    // ── Helper: rebuild the ovfMappingContainer layout ────────────────────────
    auto clearMappingContainer = [](QWidget* container)
    {
        QLayout* old = container->layout();
        if (old)
        {
            QLayoutItem* item = nullptr;
            while ((item = old->takeAt(0)) != nullptr)
            {
                if (item->widget())
                    item->widget()->deleteLater();
                delete item;
            }
            delete old;
        }
    };

    if (this->m_importType == ImportType_OVF && !this->m_ovfNetworkNames.isEmpty())
    {
        // OVF path: show per-network mapping rows with network combo + MAC field + Generate button
        if (xvaNetGroup)
            xvaNetGroup->setVisible(false);

        if (ovfMappingContainer)
        {
            clearMappingContainer(ovfMappingContainer);

            QFormLayout* form = new QFormLayout;
            for (int i = 0; i < this->m_ovfNetworkNames.size(); i++)
            {
                // Network combo
                QComboBox* combo = new QComboBox;
                combo->setObjectName("ovfNetCombo_" + QString::number(i));
                this->populateNetworkComboBox(combo);

                // MAC address field
                QLineEdit* macEdit = new QLineEdit;
                macEdit->setObjectName("ovfMacEdit_" + QString::number(i));
                macEdit->setPlaceholderText(tr("Leave empty to auto-generate"));
                macEdit->setMaximumWidth(145);
                macEdit->setInputMask("HH:HH:HH:HH:HH:HH;_");

                // Generate button
                QPushButton* genBtn = new QPushButton(tr("Generate"));
                genBtn->setObjectName("ovfMacGenBtn_" + QString::number(i));
                genBtn->setFixedWidth(80);
                connect(genBtn, &QPushButton::clicked, macEdit, [macEdit]()
                {
                    macEdit->setText(ImportWizard::generateMac());
                });

                // Row widget: [combo] [MAC edit] [Generate]
                QHBoxLayout* rowLayout = new QHBoxLayout;
                rowLayout->setContentsMargins(0, 0, 0, 0);
                rowLayout->addWidget(combo, 1);
                rowLayout->addWidget(macEdit);
                rowLayout->addWidget(genBtn);
                QWidget* rowWidget = new QWidget;
                rowWidget->setLayout(rowLayout);

                form->addRow(this->m_ovfNetworkNames.at(i), rowWidget);
            }
            ovfMappingContainer->setLayout(form);
        }

        if (ovfMappingGroup)
            ovfMappingGroup->setVisible(true);
    }
    else if (this->m_importType == ImportType_XVA && this->m_xvaImportedVm && this->m_connection)
    {
        // XVA path with discovered VM: build one row per VIF using cached VIF objects
        XenCache* cache = this->m_connection->GetCache();
        const QString vmRef = this->m_xvaImportedVm->OpaqueRef();

        // Collect VIFs belonging to this VM, sort by device number
        QList<QSharedPointer<VIF>> vifs;
        if (cache)
        {
            const auto allVifs = cache->GetAll<VIF>();
            for (const auto& vif : allVifs)
            {
                if (vif && vif->IsValid() && vif->GetVMRef() == vmRef)
                    vifs << vif;
            }
            std::sort(vifs.begin(), vifs.end(), [](const QSharedPointer<VIF>& a, const QSharedPointer<VIF>& b)
            {
                return a->GetDevice().toInt() < b->GetDevice().toInt();
            });
        }

        this->m_xvaVifCount = vifs.size();

        if (!vifs.isEmpty())
        {
            // Show per-VIF rows in ovfMappingGroup (repurposed for XVA VIF rows)
            if (xvaNetGroup)
                xvaNetGroup->setVisible(false);

            if (ovfMappingContainer)
            {
                clearMappingContainer(ovfMappingContainer);

                QFormLayout* form = new QFormLayout;
                for (int i = 0; i < vifs.size(); i++)
                {
                    const auto& vif = vifs.at(i);
                    const int deviceNum = vif->GetDevice().toInt();

                    QComboBox* combo = new QComboBox;
                    combo->setObjectName("xvaVifCombo_" + QString::number(deviceNum));
                    this->populateNetworkComboBox(combo);

                    // Pre-select the VIF's current network if still available
                    const QString currentNetRef = vif->GetNetworkRef();
                    if (!currentNetRef.isEmpty())
                    {
                        const int idx = combo->findData(currentNetRef);
                        if (idx >= 0)
                            combo->setCurrentIndex(idx);
                    }

                    form->addRow(tr("eth%1 (VIF %1)").arg(deviceNum), combo);
                }
                ovfMappingContainer->setLayout(form);
            }

            if (ovfMappingGroup)
            {
                ovfMappingGroup->setTitle(tr("Network Interfaces"));
                ovfMappingGroup->setVisible(true);
            }
        }
        else
        {
            // No VIFs in cache yet — fall back to single combo
            this->m_xvaVifCount = 0;
            if (ovfMappingGroup)
                ovfMappingGroup->setVisible(false);

            QComboBox* networkCombo = this->ui->networkCombo;
            this->populateNetworkComboBox(networkCombo);

            if (xvaNetGroup)
                xvaNetGroup->setVisible(true);
        }
    }
    else
    {
        // XVA (pre-discovery) / VHD / VMDK: use the single networkCombo
        this->m_xvaVifCount = 0;
        if (ovfMappingGroup)
            ovfMappingGroup->setVisible(false);

        QComboBox* networkCombo = this->ui->networkCombo;
        this->populateNetworkComboBox(networkCombo);

        if (xvaNetGroup)
            xvaNetGroup->setVisible(true);
    }
}

void ImportWizard::populateFixupIsoCombo()
{
    QComboBox* fixupSrCombo = this->ui->fixupSrCombo;
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
    QGroupBox* group = this->ui->ovfMetaGroup;
    QLabel* label = this->ui->ovfMetaLabel;
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
    QGroupBox* group = this->ui->xvaMetaGroup;
    QLabel* label = this->ui->xvaMetaLabel;
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
    QGroupBox* group = this->ui->diskMetaGroup;
    QLabel* label = this->ui->diskMetaLabel;
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
        QLineEdit* filePathEdit = this->ui->filePathEdit;
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

        QLabel* typeLabel = this->ui->typeLabel;
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
        QLineEdit* nameEdit = this->ui->vmNameEdit;
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
        QCheckBox* acceptCheck = this->ui->eulaAcceptCheck;
        if (!acceptCheck || !acceptCheck->isChecked())
        {
            QMessageBox::warning(this, tr("License Agreement Required"),
                                 tr("You must accept the End User License Agreement(s) to continue."));
            return false;
        }
    }
    else if (currentId == Page_Storage)
    {
        QComboBox* srCombo = this->ui->storageCombo;
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

            // Surface auto-start failures with a diagnostic dialog.
            // This signal is emitted after the import completes (non-fatal), so the
            // wizard is long gone — use nullptr as parent (top-level warning dialog).
            // Mirrors C# VMOperationCommand::StartDiagnosisForm delegate pattern.
            connect(this->m_xvaAction, &ImportVmAction::vmStartFailed,
                this, [](const QString& /*vmRef*/, const QString& errorCode,
                          const QMap<QString, QString>& hostReasons)
                {
                    const QString title = tr("Could Not Start Imported VM");
                    if (errorCode == QLatin1String(Failure::NO_HOSTS_AVAILABLE) && !hostReasons.isEmpty())
                    {
                        // Build per-host reason list (mirrors C# CommandErrorDialog)
                        QString details;
                        for (auto it = hostReasons.cbegin(); it != hostReasons.cend(); ++it)
                        {
                            const QString reason = it.value().isEmpty()
                                                   ? tr("Can boot here")
                                                   : it.value();
                            details += QString("\n  %1: %2").arg(it.key(), reason);
                        }
                        QMessageBox::warning(nullptr, title,
                            tr("The imported VM could not be started — no host is able to run it.%1")
                            .arg(details));
                    }
                    else if (errorCode == QLatin1String(Failure::HA_OPERATION_WOULD_BREAK_FAILOVER_PLAN))
                    {
                        QMessageBox::warning(nullptr, title,
                            tr("The imported VM could not be started because doing so would "
                               "violate the configured HA failover plan.\n\n"
                               "You may be able to start it by reducing the HA failover "
                               "tolerance in the pool settings."));
                    }
                    else
                    {
                        QMessageBox::warning(nullptr, title,
                            tr("The imported VM could not be started automatically. "
                               "Please start it manually from the VM list."));
                    }
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
    QLineEdit* filePathEdit = this->ui->filePathEdit;
    if (filePathEdit)
        this->m_sourceFilePath = filePathEdit->text();

    QComboBox* hostCombo = this->ui->hostCombo;
    if (hostCombo && !this->m_selectedHost)
    {
        // Fallback: wizard was accepted without going through Page_Storage/Network
        // (e.g. direct finish) — resolve host now.
        this->m_selectedHost = this->m_connection
            ? this->m_connection->GetCache()->ResolveObject<Host>(hostCombo->currentData().toString())
            : nullptr;
    }

    // VHD/VMDK VM config
    QLineEdit* vmNameEdit = this->ui->vmNameEdit;
    if (vmNameEdit)
        this->m_vmName = vmNameEdit->text().trimmed();
    QSpinBox* vcpuSpin = this->ui->vcpuSpin;
    if (vcpuSpin)
        this->m_vcpuCount = vcpuSpin->value();
    QSpinBox* memorySpin = this->ui->memorySpin;
    if (memorySpin)
        this->m_memoryMb = memorySpin->value();

    QComboBox* networkCombo = this->ui->networkCombo;
    if (networkCombo)
    {
        this->m_selectedNetwork = this->m_connection
            ? this->m_connection->GetCache()->ResolveObject<Network>(networkCombo->currentData().toString())
            : nullptr;
    }

    // Collect OVF per-network mappings and MAC addresses (index-based combos created by populateNetworkCombo)
    this->m_ovfNetworkMappings.clear();
    this->m_ovfMacMappings.clear();
    for (int i = 0; i < this->m_ovfNetworkNames.size(); i++)
    {
        QComboBox* combo = this->ui->ovfMappingContainer->findChild<QComboBox*>("ovfNetCombo_" + QString::number(i));
        if (combo)
            this->m_ovfNetworkMappings[this->m_ovfNetworkNames.at(i)] = combo->currentData().toString();

        QLineEdit* macEdit = this->ui->ovfMappingContainer->findChild<QLineEdit*>("ovfMacEdit_" + QString::number(i));
        if (macEdit && !macEdit->text().isEmpty())
            this->m_ovfMacMappings[this->m_ovfNetworkNames.at(i)] = macEdit->text();
    }

    // For OVF with manifest/signature the Security page verify checkbox is
    // authoritative; for all other paths verification is not applicable.
    this->m_verifyManifest = true;
    if (this->m_importType == ImportType_OVF && (this->m_ovfHasManifest || this->m_ovfHasSignature))
    {
        QCheckBox* securityVerify = this->ui->securityVerifyCheck;
        if (securityVerify)
            this->m_verifyManifest = securityVerify->isChecked();
    }

    QCheckBox* runFixupsBox = this->ui->runFixups;
    this->m_runFixups = runFixupsBox ? runFixupsBox->isChecked() : false;

    QComboBox* fixupSrCombo = this->ui->fixupSrCombo;
    this->m_fixupIsoSrRef = (fixupSrCombo && this->m_runFixups) ? fixupSrCombo->currentData().toString() : QString();

    QCheckBox* startAfter = this->ui->startAfterImport;
    this->m_startVMsAutomatically = startAfter ? startAfter->isChecked() : false;

    // Boot options (VHD/VMDK only — collected from Page_BootOptions)
    this->m_bootMode  = BootMode_Bios;
    this->m_assignVtpm = false;
    if (this->m_importType == ImportType_VHD)
    {
        QRadioButton* uefiSecureRadio = this->ui->bootUefiSecureRadio;
        QRadioButton* uefiRadio       = this->ui->bootUefiRadio;
        if (uefiSecureRadio && uefiSecureRadio->isChecked())
            this->m_bootMode = BootMode_UefiSecure;
        else if (uefiRadio && uefiRadio->isChecked())
            this->m_bootMode = BootMode_Uefi;

        QCheckBox* vtpmCheck = this->ui->vtpmCheck;
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
        
        // If we have per-VIF rows (m_xvaVifCount > 0), collect from each row
        if (this->m_xvaVifCount > 0)
        {
            for (int i = 0; i < this->m_xvaVifCount; i++)
            {
                QComboBox* combo = this->ui->ovfMappingContainer->findChild<QComboBox*>("xvaVifCombo_" + QString::number(i));
                vifTargetNetworks << (combo ? combo->currentData().toString() : QString());
            }
        }
        else if (this->m_selectedNetwork)
        {
            // Fallback: single-network mode
            vifTargetNetworks << this->m_selectedNetwork->OpaqueRef();
        }
        
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

    QComboBox* srCombo = this->ui->storageCombo;
    const QString srRef = srCombo ? srCombo->currentData().toString() : QString();
    if (srRef.isEmpty())
        return;

    QPushButton* rescanButton = this->ui->storageRescanButton;
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
        QLineEdit* filePathEdit = this->ui->filePathEdit;
        if (filePathEdit)
            filePathEdit->setText(filePath);

        SettingsManager::instance().SetDefaultImportPath(QFileInfo(filePath).absolutePath());
    }
}