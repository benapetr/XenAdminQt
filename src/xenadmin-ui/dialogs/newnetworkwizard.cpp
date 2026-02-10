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

#include "newnetworkwizard.h"
#include <QMessageBox>
#include <QSet>
#include <QSignalBlocker>
#include "xen/session.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/apiversion.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pif.h"
#include "xenlib/xen/network.h"
#include "xenlib/xen/network_sriov.h"
#include "xenlib/xen/actions/network/networkaction.h"
#include "xenlib/xen/actions/network/createbondaction.h"
#include "xenlib/xen/actions/network/createsriovaction.h"
#include "xenlib/xen/actions/network/createchinaction.h"
#include "xenlib/xen/asyncoperation.h"
#include "actionprogressdialog.h"
#include "../widgets/wizardnavigationpane.h"
#include "../settingsmanager.h"
#include "../mainwindow.h"

NewNetworkWizard::NewNetworkWizard(XenConnection* connection, const QSharedPointer<Pool>& pool, const QSharedPointer<Host>& host, QWidget* parent)
    : QWizard(parent), m_connection(connection), m_pool(pool), m_host(host)
{
    this->ui.setupUi(this);
    this->setWindowTitle(tr("New Network Wizard"));
    this->setWindowIcon(QIcon(":/icons/network-32.png"));

    this->setupWizardUi();
    this->configurePages();
    this->updateTypePage();
    this->updateNamePage();
    this->updateDetailsPage();
    this->updateBondDetailsPage();
    this->updateChinDetailsPage();
    this->updateSriovDetailsPage();
    this->updateNavigationSteps();

    connect(this, &QWizard::currentIdChanged, this, [this](int) {
        this->applyNetworkTypeFlow();
        this->updateNavigationSelection();
    });

    connect(this->ui.radioExternal, &QRadioButton::toggled, this, &NewNetworkWizard::onNetworkTypeChanged);
    connect(this->ui.radioInternal, &QRadioButton::toggled, this, &NewNetworkWizard::onNetworkTypeChanged);
    connect(this->ui.radioBonded, &QRadioButton::toggled, this, &NewNetworkWizard::onNetworkTypeChanged);
    connect(this->ui.radioChin, &QRadioButton::toggled, this, &NewNetworkWizard::onNetworkTypeChanged);
    connect(this->ui.radioSriov, &QRadioButton::toggled, this, &NewNetworkWizard::onNetworkTypeChanged);

    connect(this->ui.nameEdit, &QLineEdit::textChanged, this, &NewNetworkWizard::onNameChanged);
    connect(this->ui.descriptionEdit, &QTextEdit::textChanged, this, &NewNetworkWizard::onNameChanged);

    connect(this->ui.nicCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &NewNetworkWizard::onDetailsInputsChanged);
    connect(this->ui.vlanSpin, qOverload<int>(&QSpinBox::valueChanged), this, &NewNetworkWizard::onDetailsInputsChanged);
    connect(this->ui.mtuSpin, qOverload<int>(&QSpinBox::valueChanged), this, &NewNetworkWizard::onDetailsInputsChanged);
    connect(this->ui.autoAddCheck, &QCheckBox::toggled, this, &NewNetworkWizard::onDetailsInputsChanged);
    connect(this->ui.createSriovVlanCheck, &QCheckBox::toggled, this, &NewNetworkWizard::onDetailsInputsChanged);

    connect(this->ui.chinInterfaceCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &NewNetworkWizard::onChinSelectionChanged);
    connect(this->ui.chinAutoAddCheck, &QCheckBox::toggled, this, &NewNetworkWizard::onChinSelectionChanged);

    connect(this->ui.sriovNicCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &NewNetworkWizard::onSriovSelectionChanged);
    connect(this->ui.sriovAutoAddCheck, &QCheckBox::toggled, this, &NewNetworkWizard::onSriovSelectionChanged);
}

NewNetworkWizard::~NewNetworkWizard() = default;

void NewNetworkWizard::setupWizardUi()
{
    this->setWizardStyle(QWizard::ModernStyle);
    this->setOption(QWizard::HaveHelpButton, true);
    this->setOption(QWizard::HelpButtonOnRight, false);

    this->ui.radioExternal->setChecked(true);

    this->ui.vlanInfoFrame->setVisible(false);
    this->ui.mtuInfoFrame->setVisible(false);
    this->ui.chinWarningFrame->setVisible(false);
    this->ui.sriovWarningFrame->setVisible(false);

    this->ui.autoAddCheck->setChecked(true);
    this->ui.createSriovVlanCheck->setChecked(false);
    this->ui.chinAutoAddCheck->setChecked(true);
    this->ui.sriovAutoAddCheck->setChecked(true);

    this->ui.mtuSpin->setRange(68, 9000);
    this->ui.mtuSpin->setValue(1500);
    this->ui.vlanSpin->setRange(0, 4094);
    this->ui.vlanSpin->setSpecialValueText(tr("None"));

    this->m_navigationPane = new WizardNavigationPane(this);
    this->setSideWidget(this->m_navigationPane);
}

void NewNetworkWizard::configurePages()
{
    this->setPage(Page_TypeSelect, this->ui.pageTypeSelect);
    this->setPage(Page_Name, this->ui.pageName);
    this->setPage(Page_Details, this->ui.pageDetails);
    this->setPage(Page_BondDetails, this->ui.pageBondDetails);
    this->setPage(Page_ChinDetails, this->ui.pageChinDetails);
    this->setPage(Page_SriovDetails, this->ui.pageSriovDetails);
    this->setStartId(Page_TypeSelect);

    this->applyNetworkTypeFlow();
}

void NewNetworkWizard::initializePage(int id)
{
    if (id == Page_TypeSelect)
    {
        this->updateTypePage();
    } else if (id == Page_Name)
    {
        this->updateNamePage();
    } else if (id == Page_Details)
    {
        this->updateDetailsPage();
    } else if (id == Page_BondDetails)
    {
        this->updateBondDetailsPage();
    } else if (id == Page_ChinDetails)
    {
        this->updateChinDetailsPage();
    } else if (id == Page_SriovDetails)
    {
        this->updateSriovDetailsPage();
    }

    QWizard::initializePage(id);
}

void NewNetworkWizard::applyNetworkTypeFlow()
{
    NetworkType type = this->selectedNetworkType();
    if (type == m_cachedType && this->currentId() != Page_TypeSelect)
        return;

    m_cachedType = type;

    this->updateNavigationSteps();
}

void NewNetworkWizard::updateNavigationSteps()
{
    QVector<WizardNavigationPane::Step> steps;
    steps.append({tr("Type"), QIcon()});

    NetworkType type = this->selectedNetworkType();
    if (type == NetworkType::Bonded)
    {
        steps.append({tr("Bond Details"), QIcon()});
    } else
    {
        steps.append({tr("Name"), QIcon()});
        if (type == NetworkType::CHIN)
            steps.append({tr("CHIN Details"), QIcon()});
        else if (type == NetworkType::SRIOV)
            steps.append({tr("SR-IOV Details"), QIcon()});
        else
            steps.append({tr("Details"), QIcon()});
    }

    if (this->m_navigationPane)
        this->m_navigationPane->setSteps(steps);

    this->updateNavigationSelection();
}

void NewNetworkWizard::updateNavigationSelection()
{
    if (!this->m_navigationPane)
        return;

    const NetworkType type = this->selectedNetworkType();
    int stepIndex = 0;
    switch (this->currentId())
    {
        case Page_TypeSelect:
            stepIndex = 0;
            break;
        case Page_BondDetails:
            stepIndex = 1;
            break;
        case Page_Name:
            stepIndex = 1;
            break;
        case Page_ChinDetails:
        case Page_SriovDetails:
        case Page_Details:
            stepIndex = (type == NetworkType::Bonded) ? 1 : 2;
            break;
        default:
            stepIndex = 0;
            break;
    }

    this->m_navigationPane->setCurrentStep(stepIndex);
}

NewNetworkWizard::NetworkType NewNetworkWizard::selectedNetworkType() const
{
    if (this->ui.radioBonded->isChecked())
        return NetworkType::Bonded;
    if (this->ui.radioChin->isChecked())
        return NetworkType::CHIN;
    if (this->ui.radioSriov->isChecked())
        return NetworkType::SRIOV;
    if (this->ui.radioInternal->isChecked())
        return NetworkType::Internal;
    return NetworkType::External;
}

void NewNetworkWizard::updateTypePage()
{
    this->ui.chinWarningFrame->setVisible(false);
    this->ui.sriovWarningFrame->setVisible(false);

    const bool hasSession = this->m_connection && this->m_connection->GetSession();
    const bool stockholmOrGreater = hasSession
        ? this->m_connection->GetSession()->ApiVersionMeets(APIVersion::API_2_15)
        : false;
    const bool kolkataOrGreater = hasSession
        ? this->m_connection->GetSession()->ApiVersionMeets(APIVersion::API_2_10)
        : false;

    QSharedPointer<Pool> pool = this->poolObject();
    const QList<QSharedPointer<Host>> hosts = pool ? pool->GetHosts() : (this->m_host ? QList<QSharedPointer<Host>>{this->m_host} : QList<QSharedPointer<Host>>());

    // CHIN availability
    if (stockholmOrGreater)
    {
        this->ui.radioChin->setVisible(false);
        this->ui.labelChinDesc->setVisible(false);
        this->ui.chinWarningFrame->setVisible(false);
    } else
    {
        bool canChin = true;
        QString chinWarning;
        bool chinRestricted = false;
        for (const QSharedPointer<Host>& host : hosts)
        {
            if (!host || !host->IsValid())
                continue;
            if (host->RestrictVSwitchController())
            {
                chinRestricted = true;
                break;
            }
        }
        if (chinRestricted)
        {
            canChin = false;
            chinWarning = tr("CHIN is disabled by licensing or feature restrictions.");
        } else if (!pool || !pool->vSwitchController())
        {
            canChin = false;
            chinWarning = tr("CHIN requires the vSwitch controller to be configured.");
        }

        this->ui.radioChin->setVisible(true);
        this->ui.labelChinDesc->setVisible(true);
        this->ui.radioChin->setEnabled(canChin);
        this->ui.labelChinDesc->setEnabled(canChin);
        this->ui.chinWarningText->setText(chinWarning);
        this->ui.chinWarningFrame->setVisible(!canChin && !chinWarning.isEmpty());
    }

    // SR-IOV availability
    if (!kolkataOrGreater)
    {
        this->ui.radioSriov->setVisible(false);
        this->ui.labelSriovDesc->setVisible(false);
        this->ui.sriovWarningFrame->setVisible(false);
    } else
    {
        bool sriovFeatureForbidden = false;
        bool sriovDisabled = false;
        for (const QSharedPointer<Host>& host : hosts)
        {
            if (!host || !host->IsValid())
                continue;
            sriovFeatureForbidden = sriovFeatureForbidden || host->RestrictSriovNetwork();
            sriovDisabled = sriovDisabled || host->SriovNetworkDisabled();
        }

        bool hasSriovNic = false;
        if (pool)
        {
            hasSriovNic = pool->HasSriovNic();
        } else if (this->m_host && this->m_host->IsValid())
        {
            XenCache* cache = this->m_connection ? this->m_connection->GetCache() : nullptr;
            if (cache)
            {
                const QList<QSharedPointer<PIF>> pifs = cache->GetAll<PIF>();
                for (const QSharedPointer<PIF>& pif : pifs)
                {
                    if (!pif || !pif->IsValid())
                        continue;
                    if (pif->GetHostRef() != this->m_host->OpaqueRef())
                        continue;
                    if (pif->SriovCapable())
                    {
                        hasSriovNic = true;
                        break;
                    }
                }
            }
        }

        bool hasNicCanEnableSriov = false;
        if (this->m_connection && this->m_connection->GetCache())
        {
            QSet<QString> hostRefs;
            for (const QSharedPointer<Host>& host : hosts)
            {
                if (!host || !host->IsValid())
                    continue;
                hostRefs.insert(host->OpaqueRef());
            }

            const QList<QSharedPointer<PIF>> pifs = this->m_connection->GetCache()->GetAll<PIF>();
            for (const QSharedPointer<PIF>& pif : pifs)
            {
                if (!pif || !pif->IsValid())
                    continue;
                if (!hostRefs.isEmpty() && !hostRefs.contains(pif->GetHostRef()))
                    continue;
                if (!pif->IsPhysical())
                    continue;
                if (!pif->SriovCapable() || pif->IsSriovPhysicalPIF())
                    continue;
                hasNicCanEnableSriov = true;
                break;
            }
        }

        bool canSriov = !sriovDisabled && !sriovFeatureForbidden && hasSriovNic && hasNicCanEnableSriov;
        QString sriovWarning;
        if (sriovDisabled)
            sriovWarning = tr("SR-IOV networking is disabled on this pool.");
        else if (sriovFeatureForbidden)
            sriovWarning = tr("SR-IOV networking is restricted by licensing.");
        else if (!hasSriovNic)
            sriovWarning = tr("No SR-IOV capable NICs were found in this pool.");
        else if (!hasNicCanEnableSriov)
            sriovWarning = tr("All SR-IOV capable NICs are already enabled.");

        this->ui.radioSriov->setVisible(true);
        this->ui.labelSriovDesc->setVisible(true);
        this->ui.radioSriov->setEnabled(canSriov);
        this->ui.labelSriovDesc->setEnabled(canSriov);
        this->ui.sriovWarningText->setText(sriovWarning);
        this->ui.sriovWarningFrame->setVisible(!canSriov && !sriovWarning.isEmpty());
    }

    if (this->ui.radioChin->isChecked() && (!this->ui.radioChin->isVisible() || !this->ui.radioChin->isEnabled()))
        this->ui.radioExternal->setChecked(true);
    if (this->ui.radioSriov->isChecked() && (!this->ui.radioSriov->isVisible() || !this->ui.radioSriov->isEnabled()))
        this->ui.radioExternal->setChecked(true);
}

void NewNetworkWizard::updateNamePage()
{
    if (this->m_existingNetworkNames.isEmpty())
    {
        const QStringList names = this->existingNetworkNames();
        for (const QString& name : names)
            this->m_existingNetworkNames.insert(name);
    }

    const QString defaultName = this->defaultNetworkName(this->selectedNetworkType());
    QStringList existing;
    for (const QString& name : this->m_existingNetworkNames)
        existing.append(name);
    const QString unique = this->makeUniqueName(defaultName, existing);
    if (this->ui.nameEdit->text().trimmed().isEmpty())
        this->ui.nameEdit->setText(unique);
}

void NewNetworkWizard::updateDetailsPage()
{
    const bool external = this->selectedNetworkType() == NetworkType::External;

    this->ui.labelNic->setVisible(external);
    this->ui.nicCombo->setVisible(external);
    this->ui.labelVlan->setVisible(external);
    this->ui.vlanSpin->setVisible(external);
    this->ui.labelMtu->setVisible(external);
    this->ui.mtuSpin->setVisible(external);
    this->ui.vlanInfoFrame->setVisible(false);
    this->ui.mtuInfoFrame->setVisible(false);

    if (external)
    {
        this->populateExternalNics();

        QSharedPointer<Host> host = this->coordinatorHost();
        const bool vlan0Allowed = host ? host->vSwitchNetworkBackend() : false;
        this->ui.vlanSpin->setMinimum(vlan0Allowed ? 0 : 1);
        if (this->ui.vlanSpin->value() < this->ui.vlanSpin->minimum())
            this->ui.vlanSpin->setValue(this->ui.vlanSpin->minimum());

        QString pifRef = this->ui.nicCombo->currentData().toString();
        QSharedPointer<PIF> pif = (this->m_connection && this->m_connection->GetCache())
            ? this->m_connection->GetCache()->ResolveObject<PIF>(pifRef)
            : QSharedPointer<PIF>();
        this->ui.createSriovVlanCheck->setVisible(pif && pif->IsSriovPhysicalPIF());
        if (pif && pif->IsValid())
        {
            const int minMtu = 68;
            const int maxMtu = std::min(9000, static_cast<int>(pif->GetMTU()));
            this->ui.mtuSpin->setMinimum(minMtu);
            this->ui.mtuSpin->setMaximum(maxMtu);
            this->ui.mtuSpin->setEnabled(minMtu != maxMtu);
            if (this->ui.mtuSpin->value() < minMtu || this->ui.mtuSpin->value() > maxMtu)
                this->ui.mtuSpin->setValue(minMtu);
        }
    } else
    {
        this->ui.createSriovVlanCheck->setVisible(false);
    }

    this->updateVlanValidation();
    this->updateMtuValidation();
}

void NewNetworkWizard::updateBondDetailsPage()
{
    if (this->ui.bondDetailsWidget)
    {
        if (this->m_pool && this->m_pool->IsValid())
            this->ui.bondDetailsWidget->SetPool(this->m_pool);
        else if (this->m_host && this->m_host->IsValid())
            this->ui.bondDetailsWidget->SetHost(this->m_host);
        else
            this->ui.bondDetailsWidget->Refresh();
    }
}

void NewNetworkWizard::updateChinDetailsPage()
{
    this->populateChinInterfaces();
}

void NewNetworkWizard::updateSriovDetailsPage()
{
    this->populateSriovNics();
}

bool NewNetworkWizard::validateCurrentPage()
{
    const int id = this->currentId();
    if (id == Page_Name)
        return this->validateNamePage();
    if (id == Page_Details)
        return this->validateDetailsPage();
    if (id == Page_BondDetails)
        return this->validateBondDetailsPage();
    if (id == Page_ChinDetails)
        return this->validateChinDetailsPage();
    if (id == Page_SriovDetails)
        return this->validateSriovDetailsPage();

    return QWizard::validateCurrentPage();
}

int NewNetworkWizard::nextId() const
{
    const NetworkType type = this->selectedNetworkType();
    switch (this->currentId())
    {
        case Page_TypeSelect:
            return type == NetworkType::Bonded ? Page_BondDetails : Page_Name;
        case Page_Name:
            if (type == NetworkType::CHIN)
                return Page_ChinDetails;
            if (type == NetworkType::SRIOV)
                return Page_SriovDetails;
            if (type == NetworkType::Bonded)
                return Page_BondDetails;
            return Page_Details;
        case Page_Details:
        case Page_BondDetails:
        case Page_ChinDetails:
        case Page_SriovDetails:
        default:
            return -1;
    }
}

bool NewNetworkWizard::validateNamePage()
{
    const QString name = this->ui.nameEdit->text().trimmed();
    if (name.isEmpty())
    {
        QMessageBox::warning(this, tr("Invalid Input"), tr("Please enter a name for the network."));
        return false;
    }

    const QStringList existing = this->existingNetworkNames();
    if (existing.contains(name))
    {
        const QString unique = this->makeUniqueName(name, existing);
        this->ui.nameEdit->setText(unique);
    }

    return true;
}

bool NewNetworkWizard::validateDetailsPage()
{
    if (this->selectedNetworkType() != NetworkType::External)
        return true;

    if (this->ui.nicCombo->currentIndex() < 0)
        return false;

    return !this->m_vlanError && !this->m_mtuError;
}

bool NewNetworkWizard::validateBondDetailsPage()
{
    return this->ui.bondDetailsWidget ? this->ui.bondDetailsWidget->CanCreateBond(this) : false;
}

bool NewNetworkWizard::validateChinDetailsPage()
{
    return this->ui.chinInterfaceCombo->currentIndex() >= 0;
}

bool NewNetworkWizard::validateSriovDetailsPage()
{
    if (this->ui.sriovNicCombo->currentIndex() < 0)
        return false;

    QMessageBox warning(this);
    warning.setIcon(QMessageBox::Warning);
    warning.setWindowTitle(tr("SR-IOV Network"));
    warning.setText(tr("Creating an SR-IOV network may require a host reboot to apply changes."));
    warning.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    warning.setDefaultButton(QMessageBox::Ok);
    return warning.exec() == QMessageBox::Ok;
}

void NewNetworkWizard::accept()
{
    if (!this->m_connection)
    {
        QWizard::accept();
        return;
    }

    NetworkType type = this->selectedNetworkType();
    XenCache* cache = this->m_connection->GetCache();

    QString name = this->ui.nameEdit->text().trimmed();
    QString description = this->ui.descriptionEdit->toPlainText().trimmed();

    AsyncOperation* action = nullptr;

    if (type == NetworkType::Bonded)
    {
        QStringList selectedPifRefs = this->ui.bondDetailsWidget
            ? this->ui.bondDetailsWidget->SelectedPifRefs()
            : QStringList();
        const QString bondMode = this->ui.bondDetailsWidget
            ? this->ui.bondDetailsWidget->BondMode()
            : QString();
        const QString hashing = this->ui.bondDetailsWidget
            ? this->ui.bondDetailsWidget->HashingAlgorithm()
            : QString();
        name = this->ui.bondDetailsWidget
            ? this->ui.bondDetailsWidget->BondName()
            : name;

        action = new CreateBondAction(this->m_connection,
                                      name,
                                      selectedPifRefs,
                                      this->ui.bondDetailsWidget ? this->ui.bondDetailsWidget->AutoPlug() : true,
                                      this->ui.bondDetailsWidget ? this->ui.bondDetailsWidget->MTU() : 1500,
                                      bondMode,
                                      hashing,
                                      nullptr);
    } else if (type == NetworkType::CHIN)
    {
        if (!cache)
            return;

        const QString transportRef = this->ui.chinInterfaceCombo->currentData().toString();
        QSharedPointer<Network> transport = cache->ResolveObject<Network>(transportRef);
        if (!transport || !transport->IsValid())
            return;

        QVariantMap data;
        QVariantMap otherConfig;
        otherConfig.insert("automatic", this->ui.chinAutoAddCheck->isChecked() ? "true" : "false");

        data.insert("name_label", name);
        data.insert("name_description", description);
        data.insert("other_config", otherConfig);
        data.insert("tags", QVariantList());
        data.insert("MTU", transport->GetMTU());
        data.insert("managed", true);

        QSharedPointer<Network> network(new Network(this->m_connection, QString()));
        network->SetLocalData(data);

        action = new CreateChinAction(this->m_connection, network, transport, nullptr);
    } else if (type == NetworkType::SRIOV)
    {
        if (!cache)
            return;

        const QString hostPifRef = this->ui.sriovNicCombo->currentData().toString();
        QSharedPointer<PIF> selectedPif = cache->ResolveObject<PIF>(hostPifRef);
        if (!selectedPif || !selectedPif->IsValid())
            return;

        QStringList sriovPifRefs;
        const QString device = selectedPif->GetDevice();
        const QList<QSharedPointer<PIF>> pifs = cache->GetAll<PIF>(XenObjectType::PIF);
        for (const QSharedPointer<PIF>& pif : pifs)
        {
            if (!pif || !pif->IsValid())
                continue;
            if (!pif->IsPhysical() || pif->IsBondNIC())
                continue;
            if (!pif->SriovCapable() || pif->IsSriovPhysicalPIF())
                continue;
            if (pif->GetDevice() != device)
                continue;
            sriovPifRefs.append(pif->OpaqueRef());
        }

        if (sriovPifRefs.isEmpty())
            return;

        action = new CreateSriovAction(this->m_connection,
                                       name,
                                       description,
                                       sriovPifRefs,
                                       this->ui.sriovAutoAddCheck->isChecked(),
                                       nullptr);
    } else
    {
        QVariantMap data;
        QVariantMap otherConfig;
        otherConfig.insert("automatic", this->ui.autoAddCheck->isChecked() ? "true" : "false");

        data.insert("name_label", name);
        data.insert("name_description", description);
        data.insert("other_config", otherConfig);
        data.insert("tags", QVariantList());
        data.insert("managed", true);

        if (type == NetworkType::External)
        {
            const int mtu = this->ui.mtuSpin->value();
            data.insert("MTU", mtu);
        }

        QSharedPointer<Network> network(new Network(this->m_connection, QString()));
        network->SetLocalData(data);

        if (type == NetworkType::External)
        {
            if (!cache)
                return;

            const QString pifRef = this->ui.nicCombo->currentData().toString();
            QSharedPointer<PIF> basePif = cache->ResolveObject<PIF>(pifRef);
            if (!basePif || !basePif->IsValid())
                return;

            if (this->ui.createSriovVlanCheck->isVisible() && this->ui.createSriovVlanCheck->isChecked())
            {
                QStringList sriovRefs = basePif->SriovPhysicalPIFOfRefs();
                if (!sriovRefs.isEmpty())
                {
                    QSharedPointer<NetworkSriov> sriov = cache->ResolveObject<NetworkSriov>(sriovRefs.first());
                    QSharedPointer<PIF> logical = sriov ? sriov->GetLogicalPIF() : QSharedPointer<PIF>();
                    if (logical && logical->IsValid())
                        basePif = logical;
                    else
                    {
                        QMessageBox::warning(this, tr("New Network"),
                                             tr("Unable to determine the SR-IOV logical interface for the selected NIC."));
                        return;
                    }
                }
            }

            action = new NetworkAction(network, basePif, this->ui.vlanSpin->value(), nullptr);
        } else
        {
            action = new NetworkAction(network, true, nullptr);
        }
    }

    if (!action)
        return;

    ActionProgressDialog* progress = new ActionProgressDialog(action, this);
    progress->setAttribute(Qt::WA_DeleteOnClose);
    progress->show();
    action->RunAsync(true);

    QWizard::accept();
}

void NewNetworkWizard::onNetworkTypeChanged()
{
    this->applyNetworkTypeFlow();
    this->updateTypePage();
    this->updateNamePage();
    this->updateDetailsPage();
}

void NewNetworkWizard::onNameChanged()
{
    this->updateNamePage();
}

void NewNetworkWizard::onDetailsInputsChanged()
{
    if (this->selectedNetworkType() == NetworkType::External)
    {
        const QString pifRef = this->ui.nicCombo->currentData().toString();
        QSharedPointer<PIF> pif = (this->m_connection && this->m_connection->GetCache())
            ? this->m_connection->GetCache()->ResolveObject<PIF>(pifRef)
            : QSharedPointer<PIF>();
        this->ui.createSriovVlanCheck->setVisible(pif && pif->IsSriovPhysicalPIF());
        if (pif && pif->IsValid())
        {
            const int minMtu = 68;
            const int maxMtu = std::min(9000, static_cast<int>(pif->GetMTU()));
            this->ui.mtuSpin->setMinimum(minMtu);
            this->ui.mtuSpin->setMaximum(maxMtu);
            this->ui.mtuSpin->setEnabled(minMtu != maxMtu);
            if (this->ui.mtuSpin->value() < minMtu || this->ui.mtuSpin->value() > maxMtu)
                this->ui.mtuSpin->setValue(minMtu);
        }

        XenCache* cache = this->m_connection ? this->m_connection->GetCache() : nullptr;
        if (cache && pif && pif->IsValid())
        {
            const bool wantSriov = this->ui.createSriovVlanCheck->isVisible() && this->ui.createSriovVlanCheck->isChecked();
            QList<int> vlansInUse;
            const QString device = pif->GetDevice();
            const QList<QSharedPointer<PIF>> pifs = cache->GetAll<PIF>(XenObjectType::PIF);
            for (const QSharedPointer<PIF>& other : pifs)
            {
                if (!other || !other->IsValid())
                    continue;
                if (other->GetDevice() != device)
                    continue;
                const bool isSriov = other->IsSriovLogicalPIF();
                if (wantSriov != isSriov)
                    continue;
                vlansInUse.append(other->GetVLAN());
            }

            const int current = this->ui.vlanSpin->value();
            if (vlansInUse.contains(current))
            {
                const int minVlan = this->ui.vlanSpin->minimum();
                const int maxVlan = this->ui.vlanSpin->maximum();
                for (int candidate = minVlan; candidate <= maxVlan; ++candidate)
                {
                    if (!vlansInUse.contains(candidate))
                    {
                        QSignalBlocker blocker(this->ui.vlanSpin);
                        this->ui.vlanSpin->setValue(candidate);
                        break;
                    }
                }
            }
        }
    }
    this->updateVlanValidation();
    this->updateMtuValidation();
}

void NewNetworkWizard::onChinSelectionChanged()
{
    // no-op placeholder for now
}

void NewNetworkWizard::onSriovSelectionChanged()
{
    // no-op placeholder for now
}

QString NewNetworkWizard::defaultNetworkName(NetworkType type) const
{
    switch (type)
    {
        case NetworkType::External:
            return tr("New Network");
        case NetworkType::Internal:
        case NetworkType::CHIN:
            return tr("New Private Network");
        case NetworkType::SRIOV:
            return tr("New SR-IOV Network");
        case NetworkType::Bonded:
            return tr("New Bonded Network");
    }
    return tr("New Network");
}

QStringList NewNetworkWizard::existingNetworkNames() const
{
    QStringList names;
    if (!this->m_connection || !this->m_connection->GetCache())
        return names;

    const QList<QSharedPointer<Network>> networks = this->m_connection->GetCache()->GetAll<Network>();
    for (const QSharedPointer<Network>& network : networks)
    {
        if (network && network->IsValid())
            names.append(network->GetName());
    }
    return names;
}

QString NewNetworkWizard::makeUniqueName(const QString& base, const QStringList& existing) const
{
    if (!existing.contains(base))
        return base;

    int counter = 1;
    QString candidate;
    do
    {
        candidate = QString("%1 (%2)").arg(base, QString::number(counter));
        counter++;
    } while (existing.contains(candidate));

    return candidate;
}

void NewNetworkWizard::populateExternalNics()
{
    if (this->m_populatingNics)
        return;

    this->m_populatingNics = true;
    this->ui.nicCombo->clear();

    XenCache* cache = this->m_connection ? this->m_connection->GetCache() : nullptr;
    if (!cache)
    {
        this->m_populatingNics = false;
        return;
    }

    QSharedPointer<Host> host = this->coordinatorHost();
    if (!host)
    {
        this->m_populatingNics = false;
        return;
    }

    const bool showHidden = SettingsManager::instance().GetShowHiddenObjects();
    const QList<QSharedPointer<PIF>> pifs = cache->GetAll<PIF>(XenObjectType::PIF);
    for (const QSharedPointer<PIF>& pif : pifs)
    {
        if (!pif || !pif->IsValid())
            continue;
        if (pif->GetHostRef() != host->OpaqueRef())
            continue;
        if (!pif->IsPhysical())
            continue;
        if (pif->IsBondMember())
            continue;
        if (!pif->Show(showHidden))
            continue;

        QString label = QString("%1 (%2)").arg(pif->GetName(), pif->GetDevice());
        this->ui.nicCombo->addItem(label, pif->OpaqueRef());
    }

    if (this->ui.nicCombo->count() > 0)
        this->ui.nicCombo->setCurrentIndex(0);

    this->m_populatingNics = false;
}

void NewNetworkWizard::populateChinInterfaces()
{
    this->ui.chinInterfaceCombo->clear();

    XenCache* cache = this->m_connection ? this->m_connection->GetCache() : nullptr;
    if (!cache)
        return;

    const bool showHidden = SettingsManager::instance().GetShowHiddenObjects();
    QSet<QString> addedNetworks;
    QList<QSharedPointer<PIF>> pifs = cache->GetAll<PIF>(XenObjectType::PIF);

    QSharedPointer<Pool> pool = this->poolObject();
    QSharedPointer<Host> host = pool ? QSharedPointer<Host>() : this->m_host;
    for (const QSharedPointer<PIF>& pif : pifs)
    {
        if (!pif || !pif->IsValid())
            continue;
        if (host && pif->GetHostRef() != host->OpaqueRef())
            continue;
        if (!pif->IsManagementInterface())
            continue;

        QSharedPointer<Network> network = pif->GetNetwork();
        if (!network || !network->IsValid())
            continue;
        if (!network->Show(showHidden))
            continue;
        if (addedNetworks.contains(network->OpaqueRef()))
            continue;

        addedNetworks.insert(network->OpaqueRef());
        this->ui.chinInterfaceCombo->addItem(network->GetName(), network->OpaqueRef());
    }

    if (this->ui.chinInterfaceCombo->count() > 0)
        this->ui.chinInterfaceCombo->setCurrentIndex(0);
}

void NewNetworkWizard::populateSriovNics()
{
    this->ui.sriovNicCombo->clear();

    XenCache* cache = this->m_connection ? this->m_connection->GetCache() : nullptr;
    if (!cache)
        return;

    QSharedPointer<Host> host = this->coordinatorHost();
    if (!host)
        return;

    QList<QSharedPointer<PIF>> pifs = cache->GetAll<PIF>(XenObjectType::PIF);
    for (const QSharedPointer<PIF>& pif : pifs)
    {
        if (!pif || !pif->IsValid())
            continue;
        if (pif->GetHostRef() != host->OpaqueRef())
            continue;
        if (!pif->IsPhysical())
            continue;
        if (pif->IsBondMaster())
            continue;
        if (!pif->SriovCapable())
            continue;
        if (pif->IsSriovPhysicalPIF())
            continue;

        this->ui.sriovNicCombo->addItem(pif->GetName(), pif->OpaqueRef());
    }

    if (this->ui.sriovNicCombo->count() > 0)
        this->ui.sriovNicCombo->setCurrentIndex(0);
}

void NewNetworkWizard::updateVlanValidation()
{
    if (this->selectedNetworkType() != NetworkType::External)
    {
        this->m_vlanError = false;
        this->ui.vlanInfoFrame->setVisible(false);
        return;
    }

    QString warning;
    bool error = false;
    if (!this->vlanValueIsValid(&warning, &error))
        error = true;

    this->m_vlanError = error;
    if (!warning.isEmpty())
    {
        this->ui.vlanInfoText->setText(warning);
        this->ui.vlanInfoFrame->setVisible(true);
    } else
    {
        this->ui.vlanInfoFrame->setVisible(false);
    }
}

void NewNetworkWizard::updateMtuValidation()
{
    if (this->selectedNetworkType() != NetworkType::External)
    {
        this->m_mtuError = false;
        this->ui.mtuInfoFrame->setVisible(false);
        return;
    }

    QString warning;
    bool error = false;
    if (!this->mtuValueIsValid(&warning, &error))
        error = true;

    this->m_mtuError = error;
    if (!warning.isEmpty())
    {
        this->ui.mtuInfoText->setText(warning);
        this->ui.mtuInfoFrame->setVisible(true);
    } else
    {
        this->ui.mtuInfoFrame->setVisible(false);
    }
}

bool NewNetworkWizard::vlanValueIsValid(QString* warningText, bool* isError) const
{
    if (warningText)
        warningText->clear();
    if (isError)
        *isError = false;

    const int value = this->ui.vlanSpin->value();
    if (value < this->ui.vlanSpin->minimum() || value > this->ui.vlanSpin->maximum())
    {
        if (warningText)
            *warningText = tr("VLAN ID must be between %1 and %2.")
                .arg(this->ui.vlanSpin->minimum())
                .arg(this->ui.vlanSpin->maximum());
        if (isError)
            *isError = true;
        return false;
    }

    XenCache* cache = this->m_connection ? this->m_connection->GetCache() : nullptr;
    if (cache)
    {
        const QString pifRef = this->ui.nicCombo->currentData().toString();
        QSharedPointer<PIF> selectedPif = cache->ResolveObject<PIF>(pifRef);
        if (!selectedPif || !selectedPif->IsValid())
        {
            if (warningText)
                *warningText = tr("Please select a network interface.");
            if (isError)
                *isError = true;
            return false;
        }
        if (selectedPif && selectedPif->IsValid())
        {
            const QString device = selectedPif->GetDevice();
            const bool wantSriov = this->ui.createSriovVlanCheck->isVisible() && this->ui.createSriovVlanCheck->isChecked();
            const QList<QSharedPointer<PIF>> pifs = cache->GetAll<PIF>(XenObjectType::PIF);
            for (const QSharedPointer<PIF>& pif : pifs)
            {
                if (!pif || !pif->IsValid())
                    continue;
                if (pif->GetDevice() != device)
                    continue;

                const bool isSriov = pif->IsSriovLogicalPIF();
                if (wantSriov != isSriov)
                    continue;

                if (pif->GetVLAN() == value)
                {
                    if (warningText)
                        *warningText = tr("This VLAN ID is already in use on the selected interface.");
                    if (isError)
                        *isError = true;
                    return false;
                }
            }
        }
    }

    if (value == 0 && warningText)
        *warningText = tr("VLAN 0 will be created on this interface.");

    return true;
}

bool NewNetworkWizard::mtuValueIsValid(QString* warningText, bool* isError) const
{
    if (warningText)
        warningText->clear();
    if (isError)
        *isError = false;

    const int value = this->ui.mtuSpin->value();
    if (value < this->ui.mtuSpin->minimum() || value > this->ui.mtuSpin->maximum())
    {
        if (warningText)
            *warningText = tr("MTU must be between %1 and %2.")
                .arg(this->ui.mtuSpin->minimum())
                .arg(this->ui.mtuSpin->maximum());
        if (isError)
            *isError = true;
        return false;
    }

    return true;
}

QSharedPointer<Host> NewNetworkWizard::coordinatorHost() const
{
    if (this->m_host && this->m_host->IsValid())
        return this->m_host;

    if (this->m_pool && this->m_pool->IsValid())
        return this->m_pool->GetMasterHost();

    if (this->m_connection && this->m_connection->GetCache())
    {
        QSharedPointer<Pool> pool = this->m_connection->GetCache()->GetPoolOfOne();
        if (pool && pool->IsValid())
            return pool->GetMasterHost();
    }

    return QSharedPointer<Host>();
}

QSharedPointer<Pool> NewNetworkWizard::poolObject() const
{
    if (this->m_pool && this->m_pool->IsValid())
        return this->m_pool;

    if (this->m_connection && this->m_connection->GetCache())
    {
        return this->m_connection->GetCache()->GetPoolOfOne();
    }

    return QSharedPointer<Pool>();
}
