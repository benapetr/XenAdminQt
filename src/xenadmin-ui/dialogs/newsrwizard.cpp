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

#include "newsrwizard.h"
#include "ui_newsrwizard.h"

#include "../mainwindow.h"
#include "../widgets/wizardnavigationpane.h"
#include "../../xenlib/xencache.h"
#include "xen/network/connection.h"
#include "../../xenlib/xen/actions/sr/srcreateaction.h"
#include "../../xenlib/xen/actions/sr/srintroduceaction.h"
#include "../../xenlib/xen/actions/sr/srreattachaction.h"
#include "../../xenlib/xen/actions/delegatedasyncoperation.h"
#include "../../xenlib/operations/parallelaction.h"
#include "../../xenlib/xen/host.h"
#include "../../xenlib/xen/sr.h"
#include "../../xenlib/xen/xenapi/xenapi_SR.h"
#include "actionprogressdialog.h"

#include <QButtonGroup>
#include <QDateTime>
#include <QFileDialog>
#include <QIcon>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QProgressBar>
#include <QCheckBox>
#include <QGroupBox>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QStorageInfo>
#include <QTextEdit>
#include <QTextStream>
#include <QVector>
#include <QPushButton>
#include <QSignalBlocker>

#include "../../xenlib/xen/network/connectionsmanager.h"

NewSRWizard::NewSRWizard(XenConnection* connection, MainWindow* parent)
    : QWizard(parent),
      m_mainWindow(parent),
      m_connection(connection),
      ui(new Ui::NewSRWizard)
{
    this->ui->setupUi(this);
    this->setWindowTitle(tr("New Storage Repository"));
    this->setWindowIcon(QIcon(":/icons/storage-32.png"));
    this->setWizardStyle(QWizard::ModernStyle);
    this->setOption(QWizard::HaveHelpButton, true);
    this->setOption(QWizard::HelpButtonOnRight, false);
    this->setMinimumSize(800, 600);

    this->setupPages();
    this->setupNavigationPane();
    this->initializeTypePage();
    this->initializeNamePage();
    this->initializeConfigurationPage();
    this->initializeSummaryPage();

    this->connect(this, &QWizard::currentIdChanged, this, &NewSRWizard::onPageChanged);

    this->onSRTypeChanged();
    this->updateNavigationSelection();
}

NewSRWizard::NewSRWizard(XenConnection* connection, const QSharedPointer<SR>& srToReattach, MainWindow* parent)
    : NewSRWizard(connection, parent)
{
    this->applyReattachDefaults(srToReattach);
}

NewSRWizard::~NewSRWizard()
{
    delete this->ui;
}

void NewSRWizard::SetInitialSrType(SRType srType, bool lockTypes)
{
    setSrTypeSelection(srType, lockTypes);
}

void NewSRWizard::setupPages()
{
    this->setPage(Page_Type, this->ui->pageType);
    this->setPage(Page_NameDescription, this->ui->pageName);
    this->setPage(Page_Configuration, this->ui->pageConfiguration);
    this->setPage(Page_Summary, this->ui->pageSummary);
    this->setStartId(Page_Type);
}

void NewSRWizard::setupNavigationPane()
{
    this->m_navigationPane = new WizardNavigationPane(this);
    QVector<WizardNavigationPane::Step> steps = {
        {tr("Type"), QIcon()},
        {tr("Name"), QIcon()},
        {tr("Location"), QIcon()},
        {tr("Summary"), QIcon()},
    };
    this->m_navigationPane->setSteps(steps);
    this->setSideWidget(this->m_navigationPane);
}

void NewSRWizard::initializeTypePage()
{
    this->m_typeButtonGroup = new QButtonGroup(this);
    this->m_typeButtonGroup->addButton(this->ui->nfsRadio, static_cast<int>(SRType::NFS));
    this->m_typeButtonGroup->addButton(this->ui->iscsiRadio, static_cast<int>(SRType::iSCSI));
    this->m_typeButtonGroup->addButton(this->ui->localStorageRadio, static_cast<int>(SRType::LocalStorage));
    this->m_typeButtonGroup->addButton(this->ui->cifsRadio, static_cast<int>(SRType::CIFS));
    this->m_typeButtonGroup->addButton(this->ui->hbaRadio, static_cast<int>(SRType::HBA));
    this->m_typeButtonGroup->addButton(this->ui->fcoeRadio, static_cast<int>(SRType::FCoE));
    this->m_typeButtonGroup->addButton(this->ui->nfsIsoRadio, static_cast<int>(SRType::NFS_ISO));
    this->m_typeButtonGroup->addButton(this->ui->cifsIsoRadio, static_cast<int>(SRType::CIFS_ISO));

    this->connect(this->m_typeButtonGroup, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked),
            this, &NewSRWizard::onSRTypeChanged);
}

void NewSRWizard::initializeNamePage()
{
    this->connect(this->ui->nameLineEdit, &QLineEdit::textChanged, this, &NewSRWizard::onNameTextChanged);
}

void NewSRWizard::initializeConfigurationPage()
{
    this->connect(this->ui->serverLineEdit, &QLineEdit::textChanged, this, &NewSRWizard::onConfigurationChanged);
    this->connect(this->ui->serverPathLineEdit, &QLineEdit::textChanged, this, &NewSRWizard::onConfigurationChanged);
    this->connect(this->ui->usernameLineEdit, &QLineEdit::textChanged, this, &NewSRWizard::onConfigurationChanged);
    this->connect(this->ui->passwordLineEdit, &QLineEdit::textChanged, this, &NewSRWizard::onConfigurationChanged);
    this->connect(this->ui->portSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &NewSRWizard::onConfigurationChanged);
    this->connect(this->ui->testConnectionButton, &QPushButton::clicked, this, &NewSRWizard::onTestConnection);
    this->connect(this->ui->createNewSRRadio, &QRadioButton::toggled, this, &NewSRWizard::onCreateNewSRToggled);
    this->connect(this->ui->existingSRsList, &QListWidget::itemSelectionChanged, this, &NewSRWizard::onExistingSRSelected);

    this->connect(this->ui->iscsiTargetLineEdit, &QLineEdit::textChanged, this, &NewSRWizard::onConfigurationChanged);
    this->connect(this->ui->scanISCSIButton, &QPushButton::clicked, this, &NewSRWizard::onScanISCSITarget);
    this->connect(this->ui->iscsiIqnComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NewSRWizard::onISCSIIqnSelected);
    this->connect(this->ui->iscsiLunComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NewSRWizard::onISCSILunSelected);
    this->connect(this->ui->iscsiChapCheckBox, &QCheckBox::toggled, this, &NewSRWizard::onChapToggled);

    this->connect(this->ui->localPathLineEdit, &QLineEdit::textChanged, this, &NewSRWizard::onConfigurationChanged);
    this->connect(this->ui->browseButton, &QPushButton::clicked, this, &NewSRWizard::onBrowseLocalPath);
    this->connect(this->ui->filesystemComboBox, &QComboBox::currentTextChanged, this, &NewSRWizard::onConfigurationChanged);

    this->connect(this->ui->scanFibreButton, &QPushButton::clicked, this, &NewSRWizard::onScanFibreDevices);
    this->connect(this->ui->selectAllFibreButton, &QPushButton::clicked, this, &NewSRWizard::onSelectAllFibreDevices);
    this->connect(this->ui->clearAllFibreButton, &QPushButton::clicked, this, &NewSRWizard::onClearAllFibreDevices);
    this->connect(this->ui->fibreDevicesList, &QListWidget::itemSelectionChanged, this, &NewSRWizard::onFibreDeviceSelectionChanged);

    // C# has a dedicated provisioning page for iSCSI/HBA (default SR type vs clustered gfs2).
    // Keep the same behavior in Qt using an inline group on the configuration page.
    this->m_provisioningGroup = new QGroupBox(tr("Provisioning"), this->ui->pageConfiguration);
    this->m_standardProvisioningRadio = new QRadioButton(tr("Standard storage (default)"), this->m_provisioningGroup);
    this->m_gfs2ProvisioningRadio = new QRadioButton(tr("Clustered storage (gfs2)"), this->m_provisioningGroup);
    this->m_standardProvisioningRadio->setChecked(true);

    auto* provisioningLayout = new QVBoxLayout(this->m_provisioningGroup);
    provisioningLayout->addWidget(this->m_standardProvisioningRadio);
    provisioningLayout->addWidget(this->m_gfs2ProvisioningRadio);

    if (auto* configLayout = qobject_cast<QVBoxLayout*>(this->ui->pageConfiguration->layout()))
    {
        configLayout->insertWidget(0, this->m_provisioningGroup);
    }

    this->connect(this->m_standardProvisioningRadio, &QRadioButton::toggled, this, &NewSRWizard::onConfigurationChanged);
    this->connect(this->m_gfs2ProvisioningRadio, &QRadioButton::toggled, this, &NewSRWizard::onConfigurationChanged);

    this->resetISCSIState();
    this->resetFibreState();
    this->updateNetworkReattachUI(false);
    this->updateConfigurationSection();
}

void NewSRWizard::initializeSummaryPage()
{
    this->ui->creationProgressBar->setVisible(false);
    this->ui->creationStatusLabel->clear();
}

void NewSRWizard::onPageChanged(int pageId)
{
    this->updateNavigationSelection();

    if (pageId == Page_NameDescription)
    {
        if (!this->m_forceReattach)
            this->generateDefaultName();
        this->ui->nameLineEdit->setFocus();
        this->ui->nameLineEdit->selectAll();
    }

    if (pageId == Page_Configuration)
    {
        this->updateConfigurationSection();
    }

    if (pageId == Page_Summary)
    {
        this->collectNameAndDescription();
        this->collectConfiguration();
        this->updateSummary();
    }
}

void NewSRWizard::onSRTypeChanged()
{
    if (!this->m_typeButtonGroup)
        return;
    this->clearPlannedProbeSelections();

    int id = this->m_typeButtonGroup->checkedId();
    if (id < 0)
        id = static_cast<int>(SRType::NFS);

    this->m_selectedSRType = static_cast<SRType>(id);

    QString description;
    switch (this->m_selectedSRType)
    {
        case SRType::NFS:
            description = tr("Create a storage repository using Network File System (NFS). "
                             "NFS allows you to store virtual machine disks on a remote NFS server. "
                             "This is useful for shared storage between multiple hosts.");
            break;
        case SRType::iSCSI:
            description = tr("Create a storage repository using Internet Small Computer Systems Interface (iSCSI). "
                             "iSCSI allows you to access remote storage over a network using standard Ethernet infrastructure. "
                             "This provides high-performance shared storage.");
            break;
        case SRType::LocalStorage:
            description = tr("Create a storage repository using local disk storage. "
                             "This uses storage devices directly attached to the host server. "
                             "Local storage cannot be shared between multiple hosts.");
            break;
        case SRType::CIFS:
            description = tr("Create a storage repository using Common Internet File System (CIFS/SMB). "
                             "CIFS allows you to store virtual machine disks on a Windows file server "
                             "or Samba share.");
            break;
        case SRType::HBA:
            description = tr("Create a storage repository using Hardware Host Bus Adapter (HBA). "
                             "This provides direct access to Fibre Channel storage devices "
                             "through dedicated hardware adapters.");
            break;
        case SRType::FCoE:
            description = tr("Create a storage repository using Fibre Channel over Ethernet (FCoE). "
                             "FCoE allows Fibre Channel storage traffic to run over standard Ethernet networks, "
                             "providing high-performance storage connectivity.");
            break;
        case SRType::NFS_ISO:
            description = tr("Create an ISO library using Network File System (NFS). "
                             "This allows you to store and access ISO images "
                             "on a remote NFS server for virtual machine installations.");
            break;
        case SRType::CIFS_ISO:
            description = tr("Create an ISO library using CIFS/SMB file sharing. "
                             "This allows you to store and access ISO images "
                             "on a Windows file server or Samba share.");
            break;
    }

    this->ui->typeDescriptionText->setPlainText(description);
    this->updateConfigurationSection();

    if (QWizardPage* typePage = this->page(Page_Type))
        emit typePage->completeChanged();
}

void NewSRWizard::onNameTextChanged()
{
    if (QWizardPage* namePage = this->page(Page_NameDescription))
        emit namePage->completeChanged();
}

void NewSRWizard::onConfigurationChanged()
{
    this->clearPlannedProbeSelections();
    if (QWizardPage* configPage = this->page(Page_Configuration))
        emit configPage->completeChanged();
}

bool NewSRWizard::validateCurrentPage()
{
    switch (this->currentId())
    {
        case Page_Type:
            return this->validateTypePage();
        case Page_NameDescription:
            return this->validateNamePage();
        case Page_Configuration:
            return this->validateConfigurationPage();
        default:
            return true;
    }
}

bool NewSRWizard::validateTypePage() const
{
    return this->m_typeButtonGroup && this->m_typeButtonGroup->checkedButton();
}

bool NewSRWizard::validateNamePage() const
{
    return !this->ui->nameLineEdit->text().trimmed().isEmpty();
}

bool NewSRWizard::validateConfigurationPage() const
{
    NewSRWizard* self = const_cast<NewSRWizard*>(this);
    switch (this->m_selectedSRType)
    {
        case SRType::NFS:
        case SRType::NFS_ISO:
        case SRType::CIFS:
        case SRType::CIFS_ISO:
            return this->validateNetworkConfig();
        case SRType::iSCSI:
            if (!this->validateISCSIConfig())
                return false;
            return self->evaluateIscsiProbeDecision();
        case SRType::LocalStorage:
            return this->validateLocalConfig();
        case SRType::HBA:
        case SRType::FCoE:
            if (!this->validateFibreConfig())
                return false;
            return self->evaluateFibreProbeDecision();
    }
    return false;
}

void NewSRWizard::generateDefaultName()
{
    if (!this->ui->nameLineEdit->text().trimmed().isEmpty())
        return;

    QString defaultName;
    switch (this->m_selectedSRType)
    {
        case SRType::NFS:
            defaultName = tr("NFS Storage");
            break;
        case SRType::iSCSI:
            defaultName = tr("iSCSI Storage");
            break;
        case SRType::LocalStorage:
            defaultName = tr("Local Storage");
            break;
        case SRType::CIFS:
            defaultName = tr("CIFS Storage");
            break;
        case SRType::HBA:
            defaultName = tr("HBA Storage");
            break;
        case SRType::FCoE:
            defaultName = tr("FCoE Storage");
            break;
        case SRType::NFS_ISO:
            defaultName = tr("NFS ISO Library");
            break;
        case SRType::CIFS_ISO:
            defaultName = tr("CIFS ISO Library");
            break;
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm");
    this->ui->nameLineEdit->setText(QString("%1 (%2)").arg(defaultName, timestamp));
}

void NewSRWizard::collectNameAndDescription()
{
    this->m_srName = this->ui->nameLineEdit->text().trimmed();
    this->m_srDescription = this->ui->descriptionTextEdit->toPlainText().trimmed();
}

void NewSRWizard::collectConfiguration()
{
    this->m_server = this->ui->serverLineEdit->text().trimmed();
    this->m_serverPath = this->ui->serverPathLineEdit->text().trimmed();
    this->m_username = this->ui->usernameLineEdit->text().trimmed();
    this->m_password = this->ui->passwordLineEdit->text();
    this->m_port = this->ui->portSpinBox->value();
    this->m_localPath = this->ui->localPathLineEdit->text().trimmed();
    this->m_localFilesystem = this->ui->filesystemComboBox->currentText();

    this->m_iscsiTarget = this->ui->iscsiTargetLineEdit->text().trimmed();

    int iqnIndex = this->ui->iscsiIqnComboBox->currentIndex();
    if (iqnIndex >= 0 && iqnIndex < this->m_discoveredIqns.size())
        this->m_iscsiTargetIQN = this->m_discoveredIqns[iqnIndex].targetIQN;
    else
        this->m_iscsiTargetIQN.clear();

    int lunIndex = this->ui->iscsiLunComboBox->currentIndex();
    if (lunIndex >= 0 && lunIndex < this->m_discoveredLuns.size())
        this->m_iscsiLUN = QString::number(this->m_discoveredLuns[lunIndex].lunId);
    else
        this->m_iscsiLUN.clear();

    this->m_iscsiUseChap = this->ui->iscsiChapCheckBox->isChecked();
    this->m_iscsiChapUsername = this->ui->iscsiChapUsernameLineEdit->text().trimmed();
    this->m_iscsiChapPassword = this->ui->iscsiChapPasswordLineEdit->text();

    if (this->m_forceReattach && this->m_srToReattach)
    {
        this->m_selectedSRUuid = this->m_srToReattach ? this->m_srToReattach->GetUUID() : QString();
    } else if (this->ui->reattachExistingSRRadio->isChecked() && this->ui->existingSRsList->currentItem())
    {
        this->m_selectedSRUuid = this->ui->existingSRsList->currentItem()->data(Qt::UserRole).toString();
    } else
    {
        this->m_selectedSRUuid.clear();
    }
}

void NewSRWizard::updateNavigationSelection()
{
    if (this->m_navigationPane)
        this->m_navigationPane->setCurrentStep(this->currentId());
}

void NewSRWizard::updateConfigurationSection()
{
    this->hideAllConfigurations();
    this->clearPlannedProbeSelections();

    if (this->m_provisioningGroup)
    {
        const bool showProvisioning = this->m_selectedSRType == SRType::iSCSI || this->m_selectedSRType == SRType::HBA;
        this->m_provisioningGroup->setVisible(showProvisioning);
        if (!showProvisioning && this->m_standardProvisioningRadio)
            this->m_standardProvisioningRadio->setChecked(true);
    }

    switch (this->m_selectedSRType)
    {
        case SRType::NFS:
        case SRType::NFS_ISO:
            this->ui->networkConfigGroup->setVisible(true);
            this->ui->portSpinBox->setValue(2049);
            this->ui->usernameLineEdit->setVisible(false);
            this->ui->passwordLineEdit->setVisible(false);
            this->ui->networkLayout->labelForField(this->ui->usernameLineEdit)->setVisible(false);
            this->ui->networkLayout->labelForField(this->ui->passwordLineEdit)->setVisible(false);
            break;
        case SRType::CIFS:
        case SRType::CIFS_ISO:
            this->ui->networkConfigGroup->setVisible(true);
            this->ui->portSpinBox->setValue(445);
            this->ui->usernameLineEdit->setVisible(true);
            this->ui->passwordLineEdit->setVisible(true);
            this->ui->networkLayout->labelForField(this->ui->usernameLineEdit)->setVisible(true);
            this->ui->networkLayout->labelForField(this->ui->passwordLineEdit)->setVisible(true);
            break;
        case SRType::iSCSI:
            this->resetISCSIState();
            this->ui->iscsiConfigGroup->setVisible(true);
            break;
        case SRType::LocalStorage:
            this->ui->localConfigGroup->setVisible(true);
            break;
        case SRType::HBA:
        case SRType::FCoE:
            this->resetFibreState();
            this->ui->fibreConfigGroup->setVisible(true);
            this->ui->fibreConfigGroup->setTitle(this->m_selectedSRType == SRType::HBA ? tr("HBA Configuration") : tr("FCoE Configuration"));
            break;
    }

    if (!(this->m_selectedSRType == SRType::NFS || this->m_selectedSRType == SRType::NFS_ISO ||
          this->m_selectedSRType == SRType::CIFS || this->m_selectedSRType == SRType::CIFS_ISO))
    {
        this->updateNetworkReattachUI(false);
    }

    this->onConfigurationChanged();
}

void NewSRWizard::hideAllConfigurations()
{
    this->ui->networkConfigGroup->setVisible(false);
    this->ui->iscsiConfigGroup->setVisible(false);
    this->ui->localConfigGroup->setVisible(false);
    this->ui->fibreConfigGroup->setVisible(false);
}

bool NewSRWizard::validateNetworkConfig() const
{
    if (!this->ui->networkConfigGroup->isVisible())
        return false;

    if (this->ui->serverLineEdit->text().trimmed().isEmpty() || this->ui->serverPathLineEdit->text().trimmed().isEmpty())
        return false;

    return true;
}

bool NewSRWizard::validateISCSIConfig() const
{
    if (!this->ui->iscsiConfigGroup->isVisible())
        return false;

    if (this->ui->iscsiTargetLineEdit->text().trimmed().isEmpty())
        return false;

    bool hasValidIqn = this->ui->iscsiIqnComboBox->isEnabled() &&
                       this->ui->iscsiIqnComboBox->currentIndex() >= 0 &&
                       this->ui->iscsiIqnComboBox->currentIndex() < this->m_discoveredIqns.size();

    bool hasValidLun = this->ui->iscsiLunComboBox->isEnabled() &&
                       this->ui->iscsiLunComboBox->currentIndex() >= 0 &&
                       this->ui->iscsiLunComboBox->currentIndex() < this->m_discoveredLuns.size();

    return hasValidIqn && hasValidLun;
}

bool NewSRWizard::validateLocalConfig() const
{
    return this->ui->localConfigGroup->isVisible() && !this->ui->localPathLineEdit->text().trimmed().isEmpty();
}

bool NewSRWizard::validateFibreConfig() const
{
    if (!this->ui->fibreConfigGroup->isVisible())
        return false;

    for (int i = 0; i < this->ui->fibreDevicesList->count(); ++i)
    {
        QListWidgetItem* item = this->ui->fibreDevicesList->item(i);
        if (item && item->checkState() == Qt::Checked)
            return true;
    }
    return false;
}

void NewSRWizard::resetISCSIState()
{
    this->ui->iscsiIqnComboBox->clear();
    this->ui->iscsiIqnComboBox->addItem(tr("Click 'Scan Target' to discover IQNs"));
    this->ui->iscsiIqnComboBox->setEnabled(false);

    this->ui->iscsiLunComboBox->clear();
    this->ui->iscsiLunComboBox->addItem(tr("Select an IQN first"));
    this->ui->iscsiLunComboBox->setEnabled(false);

    this->ui->iscsiChapCheckBox->setChecked(false);
    this->ui->iscsiChapUsernameLineEdit->setEnabled(false);
    this->ui->iscsiChapUsernameLineEdit->clear();
    this->ui->iscsiChapPasswordLineEdit->setEnabled(false);
    this->ui->iscsiChapPasswordLineEdit->clear();

    this->m_discoveredIqns.clear();
    this->m_discoveredLuns.clear();
}

void NewSRWizard::resetFibreState()
{
    this->ui->fibreDevicesList->clear();
    this->ui->fibreStatusLabel->clear();
    this->ui->fibreStatusLabel->setVisible(false);
    this->ui->selectAllFibreButton->setEnabled(false);
    this->ui->clearAllFibreButton->setEnabled(false);
    this->m_discoveredFibreDevices.clear();
}

void NewSRWizard::updateNetworkReattachUI(bool enabled)
{
    this->ui->reattachExistingSRRadio->setEnabled(enabled);
    this->ui->existingSRsLabel->setVisible(enabled);
    this->ui->existingSRsList->setVisible(enabled);

    if (!enabled)
    {
        this->ui->createNewSRRadio->setChecked(true);
        this->ui->existingSRsList->clear();
    }
}

void NewSRWizard::applyReattachDefaults(const QSharedPointer<SR>& srToReattach)
{
    if (!srToReattach)
        return;

    this->m_srToReattach = srToReattach;
    this->m_forceReattach = true;
    this->m_reattachSrRef = srToReattach->OpaqueRef();

    this->setWindowTitle(tr("Attach Storage Repository"));

    this->m_srName = srToReattach->GetName();
    this->m_srDescription = srToReattach->GetDescription();
    this->ui->nameLineEdit->setText(this->m_srName);
    this->ui->descriptionTextEdit->setPlainText(this->m_srDescription);

    if (this->ui->createNewSRRadio)
        this->ui->createNewSRRadio->setEnabled(false);
    if (this->ui->reattachExistingSRRadio)
    {
        this->ui->reattachExistingSRRadio->setChecked(true);
        this->ui->reattachExistingSRRadio->setEnabled(false);
    }
    if (this->ui->existingSRsLabel)
        this->ui->existingSRsLabel->setVisible(false);
    if (this->ui->existingSRsList)
    {
        this->ui->existingSRsList->clear();
        this->ui->existingSRsList->setVisible(false);
    }

    QString srType = srToReattach->GetType();
    const QVariantMap smConfig = srToReattach->SMConfig();
    if (srType == "iso")
    {
        const QString isoType = smConfig.value("iso_type").toString();
        if (isoType == "cifs")
            srType = "cifs_iso";
        else if (isoType == "nfs_iso")
            srType = "nfs_iso";
    }

    if (srType == "nfs")
        setSrTypeSelection(SRType::NFS, true);
    else if (srType == "lvmoiscsi")
        setSrTypeSelection(SRType::iSCSI, true);
    else if (srType == "cifs")
        setSrTypeSelection(SRType::CIFS, true);
    else if (srType == "lvmohba")
        setSrTypeSelection(SRType::HBA, true);
    else if (srType == "lvmofcoe")
        setSrTypeSelection(SRType::FCoE, true);
    else if (srType == "nfs_iso")
        setSrTypeSelection(SRType::NFS_ISO, true);
    else if (srType == "cifs_iso")
        setSrTypeSelection(SRType::CIFS_ISO, true);
    else
        setSrTypeSelection(SRType::LocalStorage, false);
}

void NewSRWizard::setSrTypeSelection(SRType srType, bool lockTypes)
{
    this->m_selectedSRType = srType;

    if (this->m_typeButtonGroup)
    {
        QAbstractButton* button = this->m_typeButtonGroup->button(static_cast<int>(srType));
        if (button)
            button->setChecked(true);
    }
    else
    {
        return;
    }

    if (lockTypes)
    {
        for (QAbstractButton* button : this->m_typeButtonGroup->buttons())
        {
            button->setEnabled(button->isChecked());
        }
    }

    this->onSRTypeChanged();
}

void NewSRWizard::onTestConnection()
{
    this->ui->connectionStatusLabel->setText(tr("Scanning server..."));
    this->ui->connectionStatusLabel->setStyleSheet("QLabel { color: blue; }");
    this->ui->testConnectionButton->setEnabled(false);

    if (!this->m_connection || !this->m_connection->GetCache())
    {
        this->ui->connectionStatusLabel->setText(tr("Error: Not connected to XenServer"));
        this->ui->connectionStatusLabel->setStyleSheet("QLabel { color: red; }");
        this->ui->testConnectionButton->setEnabled(true);
        return;
    }

    QList<QVariantMap> pools = this->m_connection->GetCache()->GetAllData("pool");
    if (pools.isEmpty())
    {
        this->ui->connectionStatusLabel->setText(tr("Error: Failed to get pool information"));
        this->ui->connectionStatusLabel->setStyleSheet("QLabel { color: red; }");
        this->ui->testConnectionButton->setEnabled(true);
        return;
    }

    QString masterRef = pools.first().value("master").toString();

    QVariantMap deviceConfig;
    QString server = this->ui->serverLineEdit->text().trimmed();
    QString serverPath = this->ui->serverPathLineEdit->text().trimmed();
    if (server.isEmpty() || serverPath.isEmpty())
    {
        this->ui->connectionStatusLabel->setText(tr("Error: Server and path are required"));
        this->ui->connectionStatusLabel->setStyleSheet("QLabel { color: red; }");
        this->ui->testConnectionButton->setEnabled(true);
        return;
    }

    if (this->m_selectedSRType == SRType::NFS)
    {
        deviceConfig["server"] = server;
        deviceConfig["serverpath"] = serverPath;
        deviceConfig["probeversion"] = "";
    } else if (this->m_selectedSRType == SRType::CIFS)
    {
        deviceConfig["server"] = server;
        deviceConfig["serverpath"] = serverPath;
        if (!this->ui->usernameLineEdit->text().isEmpty())
            deviceConfig["username"] = this->ui->usernameLineEdit->text().trimmed();
        if (!this->ui->passwordLineEdit->text().isEmpty())
            deviceConfig["password"] = this->ui->passwordLineEdit->text();
    } else if (this->m_selectedSRType == SRType::NFS_ISO)
    {
        QString location = serverPath;
        if (!location.startsWith(":") && !location.startsWith("/"))
            location.prepend('/');
        deviceConfig["location"] = QString("%1:%2").arg(server, location);
        deviceConfig["type"] = "nfs_iso";
    } else if (this->m_selectedSRType == SRType::CIFS_ISO)
    {
        QString location = serverPath;
        if (!location.startsWith("//"))
        {
            const QString normalizedPath = location.startsWith("/") ? location.mid(1) : location;
            location = QString("//%1/%2").arg(server, normalizedPath);
        }
        deviceConfig["location"] = location;
        deviceConfig["type"] = "cifs";
        if (!this->ui->usernameLineEdit->text().isEmpty())
            deviceConfig["username"] = this->ui->usernameLineEdit->text().trimmed();
        if (!this->ui->passwordLineEdit->text().isEmpty())
            deviceConfig["cifspassword"] = this->ui->passwordLineEdit->text();
    }

    QString srTypeStr = "nfs";
    if (this->m_selectedSRType == SRType::CIFS)
        srTypeStr = "smb";
    else if (this->m_selectedSRType == SRType::NFS_ISO || this->m_selectedSRType == SRType::CIFS_ISO)
        srTypeStr = "iso";

    QVariantList probeResult;
    QString probeError;
    if (this->runProbeExtWithProgress(tr("Testing Storage Connection"),
                                      masterRef,
                                      deviceConfig,
                                      srTypeStr,
                                      probeResult,
                                      probeError))
    {
        this->m_foundSRs.clear();
        this->ui->existingSRsList->clear();

        if (probeResult.isEmpty())
        {
            this->ui->connectionStatusLabel->setText(tr("Connection successful - No existing SRs found"));
            this->ui->connectionStatusLabel->setStyleSheet("QLabel { color: green; }");
            this->updateNetworkReattachUI(false);
        } else
        {
            this->ui->connectionStatusLabel->setText(tr("Connection successful - Found %1 existing SR(s)").arg(probeResult.size()));
            this->ui->connectionStatusLabel->setStyleSheet("QLabel { color: green; }");
            this->updateNetworkReattachUI(true);

            for (const QVariant& srVar : probeResult)
            {
                QVariantMap srInfo = srVar.toMap();
                QString uuid = srInfo.value("uuid").toString();
                QString name = srInfo.value("name_label", tr("Unnamed SR")).toString();
                QString description = srInfo.value("name_description").toString();

                if (uuid.isEmpty())
                    continue;

                this->m_foundSRs[uuid] = name;
                QString displayText = description.isEmpty() ? name : QString("%1 - %2").arg(name, description);
                QListWidgetItem* item = new QListWidgetItem(displayText);
                item->setData(Qt::UserRole, uuid);
                this->ui->existingSRsList->addItem(item);
            }
        }
    } else
    {
        this->ui->connectionStatusLabel->setText(tr("Connection failed: %1").arg(probeError));
        this->ui->connectionStatusLabel->setStyleSheet("QLabel { color: red; }");
        this->updateNetworkReattachUI(false);
    }

    this->ui->testConnectionButton->setEnabled(true);
    this->onConfigurationChanged();
}

void NewSRWizard::onBrowseLocalPath()
{
    QString currentPath = this->ui->localPathLineEdit->text().trimmed();
    if (currentPath.isEmpty())
        currentPath = "/dev";

    QString selectedPath = QFileDialog::getExistingDirectory(this, tr("Select Storage Device or Directory"), currentPath);
    if (selectedPath.isEmpty())
        return;

    this->ui->localPathLineEdit->setText(selectedPath);

    QStorageInfo storage(selectedPath);
    if (storage.isValid())
    {
        qint64 availableBytes = storage.bytesAvailable();
        QString sizeText = availableBytes > 0 ? tr("%1 GB available").arg(availableBytes / (1024 * 1024 * 1024)) : tr("Unknown");
        this->ui->diskSpaceLabel->setText(sizeText);
    }
}

void NewSRWizard::onCreateNewSRToggled(bool checked)
{
    this->clearPlannedProbeSelections();
    if (checked)
        this->ui->existingSRsList->clearSelection();
    this->onConfigurationChanged();
}

void NewSRWizard::onExistingSRSelected()
{
    this->clearPlannedProbeSelections();
    if (this->ui->existingSRsList->currentItem())
        this->ui->reattachExistingSRRadio->setChecked(true);
    this->onConfigurationChanged();
}

void NewSRWizard::onChapToggled(bool checked)
{
    this->clearPlannedProbeSelections();
    this->ui->iscsiChapUsernameLineEdit->setEnabled(checked);
    this->ui->iscsiChapPasswordLineEdit->setEnabled(checked);
    this->onConfigurationChanged();
}

bool NewSRWizard::runProbeExtWithProgress(const QString& title,
                                          const QString& masterRef,
                                          const QVariantMap& deviceConfig,
                                          const QString& srType,
                                          QVariantList& probeResult,
                                          QString& errorMessage)
{
    probeResult.clear();
    errorMessage.clear();

    QSharedPointer<QVariantList> result(new QVariantList());
    QSharedPointer<QString> probeError(new QString());

    auto* action = new DelegatedAsyncOperation(
        this->m_connection,
        title,
        tr("Scanning storage..."),
        [masterRef, deviceConfig, srType, result, probeError](DelegatedAsyncOperation* op)
        {
            try
            {
                *result = XenAPI::SR::probe_ext(op->GetSession(),
                                                masterRef,
                                                deviceConfig,
                                                srType,
                                                QVariantMap());
            } catch (const std::exception& ex)
            {
                *probeError = QString::fromUtf8(ex.what());
                throw std::runtime_error(probeError->toStdString());
            }
        },
        this);

    ActionProgressDialog progressDialog(action, this);
    progressDialog.setWindowTitle(title);
    progressDialog.exec();

    if (action->HasError())
    {
        errorMessage = probeError->isEmpty() ? action->GetErrorMessage() : *probeError;
        return false;
    }

    probeResult = *result;
    return true;
}

void NewSRWizard::onScanISCSITarget()
{
    this->clearPlannedProbeSelections();
    QString target = this->ui->iscsiTargetLineEdit->text().trimmed();
    if (target.isEmpty())
    {
        QMessageBox::warning(this, tr("Invalid Target"), tr("Please enter an iSCSI target address."));
        return;
    }

    QString host = target;
    quint16 port = 3260;
    if (target.contains(":"))
    {
        const QStringList parts = target.split(":");
        if (parts.size() == 2)
        {
            host = parts[0];
            port = parts[1].toUShort();
        }
    }

    QVariantMap deviceConfig;
    deviceConfig["target"] = host;
    deviceConfig["port"] = port;

    if (this->ui->iscsiChapCheckBox->isChecked())
    {
        QString chapUser = this->ui->iscsiChapUsernameLineEdit->text().trimmed();
        if (chapUser.isEmpty())
        {
            QMessageBox::warning(this, tr("Invalid CHAP"), tr("Please enter a CHAP username or disable CHAP authentication."));
            return;
        }
        deviceConfig["chapuser"] = chapUser;
        deviceConfig["chappassword"] = this->ui->iscsiChapPasswordLineEdit->text();
    }

    this->ui->scanISCSIButton->setEnabled(false);
    this->ui->iscsiTargetLineEdit->setEnabled(false);
    this->ui->scanISCSIButton->setText(tr("Scanning..."));

    QList<QVariantMap> pools = this->m_connection->GetCache()->GetAllData("pool");
    if (pools.isEmpty())
    {
        this->ui->scanISCSIButton->setEnabled(true);
        this->ui->iscsiTargetLineEdit->setEnabled(true);
        this->ui->scanISCSIButton->setText(tr("Scan Target"));
        QMessageBox::critical(this, tr("Scan Failed"), tr("Failed to scan iSCSI target:\n\nNo pool found"));
        return;
    }
    const QString masterRef = pools.first().value("master").toString();

    QVariantList probeResult;
    QString probeError;
    if (this->runProbeExtWithProgress(tr("Scanning iSCSI Target"),
                                      masterRef,
                                      deviceConfig,
                                      this->getSelectedBlockSrType(),
                                      probeResult,
                                      probeError))
    {
        this->m_discoveredIqns.clear();
        this->ui->iscsiIqnComboBox->clear();

        for (const QVariant& resultVar : probeResult)
        {
            QVariantMap result = resultVar.toMap();
            QVariantMap config = result.value("configuration").toMap();

            ISCSIIqnInfo info;
            info.targetIQN = config.value("targetIQN").toString();
            info.ipAddress = config.value("target").toString();
            info.port = config.value("port", port).toUInt();
            info.index = this->m_discoveredIqns.size();

            if (info.ipAddress.isEmpty())
                info.ipAddress = host;

            if (!info.targetIQN.isEmpty())
            {
                this->m_discoveredIqns.append(info);
                this->ui->iscsiIqnComboBox->addItem(QString("%1 (%2:%3)").arg(info.targetIQN).arg(info.ipAddress).arg(info.port));
            }
        }

        if (this->m_discoveredIqns.isEmpty())
        {
            this->ui->iscsiIqnComboBox->addItem(tr("No IQNs found on target"));
            this->ui->iscsiIqnComboBox->setEnabled(false);
            QMessageBox::information(this, tr("No IQNs Found"),
                                     tr("No iSCSI targets were found on %1:%2.\n\nPlease verify the target address and network connectivity.")
                                         .arg(host)
                                         .arg(port));
        } else
        {
            this->ui->iscsiIqnComboBox->setEnabled(true);
            if (this->m_discoveredIqns.size() == 1)
                this->ui->iscsiIqnComboBox->setCurrentIndex(0);
        }
    } else
    {
        this->ui->iscsiIqnComboBox->clear();
        this->ui->iscsiIqnComboBox->addItem(tr("Scan failed"));
        this->ui->iscsiIqnComboBox->setEnabled(false);
        QMessageBox::critical(this, tr("Scan Failed"), tr("Failed to scan iSCSI target:\n\n%1").arg(probeError));
    }

    this->ui->scanISCSIButton->setEnabled(true);
    this->ui->iscsiTargetLineEdit->setEnabled(true);
    this->ui->scanISCSIButton->setText(tr("Scan Target"));
    this->onConfigurationChanged();
}

void NewSRWizard::onISCSIIqnSelected(int index)
{
    this->clearPlannedProbeSelections();
    if (index < 0 || index >= this->m_discoveredIqns.size())
    {
        this->ui->iscsiLunComboBox->clear();
        this->ui->iscsiLunComboBox->addItem(tr("Select an IQN first"));
        this->ui->iscsiLunComboBox->setEnabled(false);
        this->onConfigurationChanged();
        return;
    }

    const ISCSIIqnInfo& iqnInfo = this->m_discoveredIqns[index];

    QVariantMap deviceConfig;
    deviceConfig["target"] = iqnInfo.ipAddress;
    deviceConfig["port"] = iqnInfo.port;
    deviceConfig["targetIQN"] = iqnInfo.targetIQN;

    if (this->ui->iscsiChapCheckBox->isChecked())
    {
        deviceConfig["chapuser"] = this->ui->iscsiChapUsernameLineEdit->text().trimmed();
        deviceConfig["chappassword"] = this->ui->iscsiChapPasswordLineEdit->text();
    }

    this->ui->iscsiIqnComboBox->setEnabled(false);
    this->ui->scanISCSIButton->setEnabled(false);

    QList<QVariantMap> pools = this->m_connection->GetCache()->GetAllData("pool");
    if (pools.isEmpty())
    {
        this->ui->iscsiIqnComboBox->setEnabled(true);
        this->ui->scanISCSIButton->setEnabled(true);
        this->ui->iscsiLunComboBox->clear();
        this->ui->iscsiLunComboBox->addItem(tr("Scan failed"));
        this->ui->iscsiLunComboBox->setEnabled(false);
        QMessageBox::critical(this, tr("Scan Failed"), tr("Failed to scan for LUNs:\n\nNo pool found"));
        return;
    }
    const QString masterRef = pools.first().value("master").toString();

    QVariantList probeResult;
    QString probeError;
    if (this->runProbeExtWithProgress(tr("Scanning iSCSI LUNs"),
                                      masterRef,
                                      deviceConfig,
                                      this->getSelectedBlockSrType(),
                                      probeResult,
                                      probeError))
    {
        this->m_discoveredLuns.clear();
        this->ui->iscsiLunComboBox->clear();

        for (const QVariant& resultVar : probeResult)
        {
            QVariantMap result = resultVar.toMap();
            QVariantMap extra = result.value("extra").toMap();

            ISCSILunInfo info;
            info.lunId = result.value("LUNid", extra.value("LUNid", -1)).toInt();
            info.scsiId = result.value("SCSIid", extra.value("SCSIid")).toString();
            info.vendor = result.value("vendor", extra.value("vendor")).toString();
            info.serial = result.value("serial", extra.value("serial")).toString();
            info.size = result.value("size", extra.value("size", 0)).toLongLong();

            if (info.lunId < 0)
                continue;

            this->m_discoveredLuns.append(info);

            QString sizeStr = info.size > 0 ? QString(" (%1 GB)").arg(info.size / 1073741824.0, 0, 'f', 2) : QString();
            QString displayText = QString("LUN %1: %2 %3%4")
                                      .arg(info.lunId)
                                      .arg(info.vendor)
                                      .arg(info.serial)
                                      .arg(sizeStr);
            this->ui->iscsiLunComboBox->addItem(displayText);
        }

        if (this->m_discoveredLuns.isEmpty())
        {
            this->ui->iscsiLunComboBox->addItem(tr("No LUNs found"));
            this->ui->iscsiLunComboBox->setEnabled(false);
            QMessageBox::information(this, tr("No LUNs Found"),
                                     tr("No LUNs were found on target %1.\n\nPlease verify the iSCSI configuration.")
                                         .arg(iqnInfo.targetIQN));
        } else
        {
            this->ui->iscsiLunComboBox->setEnabled(true);
            if (this->m_discoveredLuns.size() == 1)
                this->ui->iscsiLunComboBox->setCurrentIndex(0);
        }
    } else
    {
        this->ui->iscsiLunComboBox->clear();
        this->ui->iscsiLunComboBox->addItem(tr("Scan failed"));
        this->ui->iscsiLunComboBox->setEnabled(false);
        QMessageBox::critical(this, tr("Scan Failed"), tr("Failed to scan for LUNs:\n\n%1").arg(probeError));
    }

    this->ui->iscsiIqnComboBox->setEnabled(true);
    this->ui->scanISCSIButton->setEnabled(true);
    this->onConfigurationChanged();
}

void NewSRWizard::onISCSILunSelected(int)
{
    this->clearPlannedProbeSelections();
    this->onConfigurationChanged();
}

void NewSRWizard::onScanFibreDevices()
{
    this->clearPlannedProbeSelections();
    this->ui->scanFibreButton->setEnabled(false);
    this->ui->scanFibreButton->setText(tr("Scanning..."));
    this->ui->fibreStatusLabel->setVisible(false);

    if (!this->m_connection || !this->m_connection->GetSession() || !this->m_connection->GetCache())
    {
        this->ui->fibreStatusLabel->setText(tr("Scan failed: Not connected to XenServer"));
        this->ui->fibreStatusLabel->setStyleSheet("QLabel { color: red; }");
        this->ui->fibreStatusLabel->setVisible(true);
        this->ui->scanFibreButton->setEnabled(true);
        this->ui->scanFibreButton->setText(tr("Scan for Devices"));
        return;
    }

    QList<QVariantMap> pools = this->m_connection->GetCache()->GetAllData("pool");
    if (pools.isEmpty())
    {
        this->ui->fibreStatusLabel->setText(tr("Scan failed: No pool found"));
        this->ui->fibreStatusLabel->setStyleSheet("QLabel { color: red; }");
        this->ui->fibreStatusLabel->setVisible(true);
        this->ui->scanFibreButton->setEnabled(true);
        this->ui->scanFibreButton->setText(tr("Scan for Devices"));
        return;
    }

    const QString masterRef = pools.first().value("master").toString();
    const QString srTypeStr = (this->m_selectedSRType == SRType::HBA) ? this->getSelectedBlockSrType() : "lvmofcoe";

    QVariantMap deviceConfig;
    if (this->m_selectedSRType == SRType::FCoE)
        deviceConfig["provider"] = "fcoe";

    QSharedPointer<QVariantList> probeResult(new QVariantList());
    QSharedPointer<QString> probeError(new QString());

    auto* action = new DelegatedAsyncOperation(
        this->m_connection,
        tr("Scanning Fibre Channel devices"),
        tr("Scanning storage devices..."),
        [masterRef, srTypeStr, deviceConfig, probeResult, probeError](DelegatedAsyncOperation* op)
        {
            try
            {
                *probeResult = XenAPI::SR::probe_ext(op->GetSession(),
                                                     masterRef,
                                                     deviceConfig,
                                                     srTypeStr,
                                                     QVariantMap());
            } catch (const std::exception& ex)
            {
                *probeError = QString::fromUtf8(ex.what());
                throw std::runtime_error(probeError->toStdString());
            }
        },
        this);

    ActionProgressDialog progressDialog(action, this);
    progressDialog.setWindowTitle(tr("Scanning for Devices"));
    progressDialog.exec();

    if (action->HasError())
    {
        const QString err = probeError->isEmpty() ? action->GetErrorMessage() : *probeError;
        this->ui->fibreStatusLabel->setText(tr("Scan failed: %1").arg(err));
        this->ui->fibreStatusLabel->setStyleSheet("QLabel { color: red; }");
        this->ui->fibreStatusLabel->setVisible(true);
        this->ui->selectAllFibreButton->setEnabled(false);
        this->ui->clearAllFibreButton->setEnabled(false);
        QMessageBox::critical(this, tr("Scan Failed"), tr("Failed to scan for Fibre Channel devices:\n\n%1").arg(err));
        this->m_discoveredFibreDevices.clear();
        this->ui->fibreDevicesList->clear();
    } else
    {
        this->m_discoveredFibreDevices.clear();
        this->ui->fibreDevicesList->clear();

        for (const QVariant& resultVar : *probeResult)
        {
            QVariantMap result = resultVar.toMap();
            QVariantMap config = result.value("configuration").toMap();
            QVariantMap extra = result.value("extra_info").toMap();

            FibreChannelDevice device;
            device.scsiId = config.value("SCSIid", config.value("scsiid")).toString();
            device.vendor = config.value("vendor").toString();
            device.serial = config.value("serial").toString();
            device.path = config.value("path").toString();
            device.adapter = config.value("adapter").toString();
            device.channel = config.value("channel").toString();
            device.id = config.value("id").toString();
            device.lun = config.value("lun").toString();
            device.nameLabel = config.value("name_label").toString();
            device.nameDescription = config.value("name_description").toString();
            device.eth = config.value("eth").toString();
            device.poolMetadataDetected = config.value("pool_metadata_detected", false).toBool();
            device.existingSrUuid = result.value("uuid").toString();
            device.existingSrConfiguration = config;

            QString sizeStr = config.value("size").toString();
            device.size = config.value("size").toLongLong();
            if (!sizeStr.isEmpty())
            {
                bool ok = false;
                qint64 sizeVal = sizeStr.toLongLong(&ok);
                if (ok)
                    device.size = sizeVal;
            }

            if (device.scsiId.isEmpty())
                continue;

            this->m_discoveredFibreDevices.append(device);

            QString displayText = QString("%1 %2").arg(device.vendor, device.serial);
            if (device.size > 0)
                displayText += QString(" (%1 GB)").arg(device.size / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
            displayText += QString(" - %1").arg(device.scsiId);

            QListWidgetItem* item = new QListWidgetItem(displayText);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
            this->ui->fibreDevicesList->addItem(item);
        }

        if (this->m_discoveredFibreDevices.isEmpty())
        {
            this->ui->fibreStatusLabel->setText(tr("No Fibre Channel devices found."));
            this->ui->fibreStatusLabel->setStyleSheet("QLabel { color: orange; }");
            this->ui->fibreStatusLabel->setVisible(true);
            this->ui->selectAllFibreButton->setEnabled(false);
            this->ui->clearAllFibreButton->setEnabled(false);
            QMessageBox::information(this, tr("No Devices Found"),
                                     tr("No Fibre Channel devices were detected.\n\n"
                                        "Please verify that the HBAs are installed and connected."));
        } else
        {
            this->ui->fibreStatusLabel->setText(tr("Found %n device(s). Select devices to create SRs.", "", this->m_discoveredFibreDevices.size()));
            this->ui->fibreStatusLabel->setStyleSheet("QLabel { color: green; }");
            this->ui->fibreStatusLabel->setVisible(true);
            this->ui->selectAllFibreButton->setEnabled(true);
            this->ui->clearAllFibreButton->setEnabled(true);
        }
    }

    this->ui->scanFibreButton->setEnabled(true);
    this->ui->scanFibreButton->setText(tr("Scan for Devices"));
    this->onConfigurationChanged();
}

void NewSRWizard::onFibreDeviceSelectionChanged()
{
    this->clearPlannedProbeSelections();
    this->onConfigurationChanged();
}

void NewSRWizard::onSelectAllFibreDevices()
{
    for (int i = 0; i < this->ui->fibreDevicesList->count(); ++i)
    {
        QListWidgetItem* item = this->ui->fibreDevicesList->item(i);
        if (item)
            item->setCheckState(Qt::Checked);
    }
    this->onConfigurationChanged();
}

void NewSRWizard::onClearAllFibreDevices()
{
    for (int i = 0; i < this->ui->fibreDevicesList->count(); ++i)
    {
        QListWidgetItem* item = this->ui->fibreDevicesList->item(i);
        if (item)
            item->setCheckState(Qt::Unchecked);
    }
    this->onConfigurationChanged();
}

QList<NewSRWizard::FibreChannelDevice> NewSRWizard::getSelectedFibreDevices() const
{
    QList<FibreChannelDevice> devices;
    for (int i = 0; i < this->ui->fibreDevicesList->count() && i < this->m_discoveredFibreDevices.size(); ++i)
    {
        QListWidgetItem* item = this->ui->fibreDevicesList->item(i);
        if (item && item->checkState() == Qt::Checked)
            devices.append(this->m_discoveredFibreDevices[i]);
    }
    return devices;
}

QString NewSRWizard::getSelectedBlockSrType() const
{
    if (this->m_selectedSRType != SRType::iSCSI && this->m_selectedSRType != SRType::HBA)
        return this->getSRTypeString();

    if (this->m_gfs2ProvisioningRadio && this->m_gfs2ProvisioningRadio->isChecked())
        return "gfs2";

    return this->m_selectedSRType == SRType::iSCSI ? "lvmoiscsi" : "lvmohba";
}

QString NewSRWizard::getAlternativeBlockSrType(const QString& srType) const
{
    if (srType == "gfs2")
    {
        if (this->m_selectedSRType == SRType::iSCSI)
            return "lvmoiscsi";
        if (this->m_selectedSRType == SRType::HBA)
            return "lvmohba";
    } else if (srType == "lvmoiscsi" || srType == "lvmohba")
    {
        return "gfs2";
    }

    return QString();
}

QVariantMap NewSRWizard::normalizeProbeConfig(const QVariantMap& config)
{
    QVariantMap normalized = config;
    if (normalized.contains("scsiid") && !normalized.contains("SCSIid"))
        normalized["SCSIid"] = normalized.value("scsiid");
    if (normalized.contains("targetiqn") && !normalized.contains("targetIQN"))
        normalized["targetIQN"] = normalized.value("targetiqn");
    return normalized;
}

void NewSRWizard::clearPlannedProbeSelections()
{
    this->m_selectedSRUuid.clear();
    this->m_iscsiProbeSelectedConfig.clear();
    this->m_plannedFibreDevices.clear();
    this->m_hasPlannedFibreDevices = false;
    this->m_hasEvaluatedProbeDecisions = false;
}

QList<QVariantMap> NewSRWizard::probeForExistingSrs(const QVariantMap& deviceConfig, QString& usedSrType, QString& error) const
{
    QList<QVariantMap> matches;
    error.clear();

    if (!this->m_connection || !this->m_connection->GetCache())
    {
        error = tr("Not connected to XenServer.");
        return matches;
    }

    const QList<QVariantMap> pools = this->m_connection->GetCache()->GetAllData("pool");
    if (pools.isEmpty())
    {
        error = tr("No pool found.");
        return matches;
    }

    const QString masterRef = pools.first().value("master").toString();
    usedSrType = this->getSelectedBlockSrType();

    QVariantList probeResult;
    QString probeError;
    if (!const_cast<NewSRWizard*>(this)->runProbeExtWithProgress(tr("Probing Storage"),
                                                                  masterRef,
                                                                  deviceConfig,
                                                                  usedSrType,
                                                                  probeResult,
                                                                  probeError))
    {
        error = probeError;
        return matches;
    }

    for (const QVariant& probeEntry : probeResult)
    {
        const QVariantMap map = probeEntry.toMap();
        if (!map.value("uuid").toString().isEmpty())
            matches.append(map);
    }

    if (!matches.isEmpty())
        return matches;

    const QString altType = this->getAlternativeBlockSrType(usedSrType);
    if (altType.isEmpty() || this->m_selectedSRType == SRType::FCoE)
        return matches;

    QVariantList altProbeResult;
    QString altProbeError;
    if (!const_cast<NewSRWizard*>(this)->runProbeExtWithProgress(tr("Probing Storage"),
                                                                  masterRef,
                                                                  deviceConfig,
                                                                  altType,
                                                                  altProbeResult,
                                                                  altProbeError))
    {
        return matches;
    }

    for (const QVariant& probeEntry : altProbeResult)
    {
        const QVariantMap map = probeEntry.toMap();
        if (!map.value("uuid").toString().isEmpty())
            matches.append(map);
    }

    if (!matches.isEmpty())
        usedSrType = altType;

    return matches;
}

bool NewSRWizard::isSrUuidInAnyConnectedPool(const QString& srUuid, XenConnection** outConnection, QString* outName) const
{
    if (outConnection)
        *outConnection = nullptr;
    if (outName)
        outName->clear();

    if (srUuid.isEmpty())
        return false;

    Xen::ConnectionsManager* manager = Xen::ConnectionsManager::instance();
    if (!manager)
        return false;

    const QList<XenConnection*> connections = manager->GetAllConnections();
    for (XenConnection* connection : connections)
    {
        if (!connection || !connection->GetCache())
            continue;

        const QList<QVariantMap> srs = connection->GetCache()->GetAllData("sr");
        for (const QVariantMap& srData : srs)
        {
            if (srData.value("uuid").toString() != srUuid)
                continue;

            if (outConnection)
                *outConnection = connection;
            if (outName)
                *outName = srData.value("name_label").toString();
            return true;
        }
    }

    return false;
}

NewSRWizard::ExistingSrDecision NewSRWizard::askExistingSrDecision(const QString& title,
                                                                   const QString& details,
                                                                   bool foundExisting,
                                                                   bool allowFormat,
                                                                   bool showRepeatCheckbox,
                                                                   bool& repeatForRemaining) const
{
    QMessageBox msgBox(const_cast<NewSRWizard*>(this));
    msgBox.setWindowTitle(title);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText(foundExisting ? tr("A storage repository already exists on this device.")
                                 : tr("No storage repository was found on this device."));
    msgBox.setInformativeText(details);

    QPushButton* reattachButton = nullptr;
    if (foundExisting)
        reattachButton = msgBox.addButton(tr("Reattach"), QMessageBox::AcceptRole);
    QPushButton* formatButton = nullptr;
    if (allowFormat)
        formatButton = msgBox.addButton(tr("Format"), QMessageBox::DestructiveRole);
    QPushButton* cancelButton = msgBox.addButton(QMessageBox::Cancel);
    msgBox.setDefaultButton(cancelButton);

    QCheckBox repeatBox(tr("Apply to remaining devices"), &msgBox);
    if (showRepeatCheckbox)
        msgBox.setCheckBox(&repeatBox);

    msgBox.exec();
    repeatForRemaining = showRepeatCheckbox && repeatBox.isChecked();

    if (msgBox.clickedButton() == reattachButton)
        return ExistingSrDecision::Reattach;
    if (msgBox.clickedButton() == formatButton)
        return ExistingSrDecision::Format;
    return ExistingSrDecision::Cancel;
}

bool NewSRWizard::evaluateIscsiProbeDecision()
{
    if (this->m_hasEvaluatedProbeDecisions && this->m_selectedSRType == SRType::iSCSI)
        return true;

    QVariantMap deviceConfig = this->getDeviceConfig();
    QString usedSrType;
    QString probeError;
    const QList<QVariantMap> matches = this->probeForExistingSrs(deviceConfig, usedSrType, probeError);
    if (!probeError.isEmpty())
    {
        QMessageBox::critical(this, tr("Probe Failed"), tr("Failed to probe iSCSI LUN:\n\n%1").arg(probeError));
        return false;
    }

    if (this->m_forceReattach && this->m_srToReattach)
    {
        const QString expectedUuid = this->m_srToReattach->GetUUID();
        for (const QVariantMap& match : matches)
        {
            if (match.value("uuid").toString() == expectedUuid)
            {
                this->m_selectedSRUuid = expectedUuid;
                this->m_iscsiProbeSelectedConfig = normalizeProbeConfig(match.value("configuration").toMap());
                this->m_hasEvaluatedProbeDecisions = true;
                return true;
            }
        }

        QMessageBox::warning(this,
                             tr("Incorrect LUN"),
                             tr("The selected LUN does not contain the expected SR '%1'.")
                                 .arg(this->m_srToReattach->GetName()));
        return false;
    }

    if (matches.isEmpty())
    {
        bool repeat = false;
        const ExistingSrDecision choice = this->askExistingSrDecision(
            tr("No Existing SR"),
            tr("The selected LUN does not contain an existing SR.\n\nContinuing will format this LUN and create a new SR."),
            false,
            true,
            false,
            repeat);
        if (choice != ExistingSrDecision::Format)
            return false;

        this->m_selectedSRUuid.clear();
        this->m_iscsiProbeSelectedConfig.clear();
        this->m_hasEvaluatedProbeDecisions = true;
        return true;
    }

    const QVariantMap existing = matches.first();
    const QString existingUuid = existing.value("uuid").toString();
    const QVariantMap existingConfig = normalizeProbeConfig(existing.value("configuration").toMap());

    XenConnection* usedByConnection = nullptr;
    QString usedByName;
    const bool inUseAnywhere = this->isSrUuidInAnyConnectedPool(existingUuid, &usedByConnection, &usedByName);
    const bool inCurrentConnection = inUseAnywhere && usedByConnection == this->m_connection;
    const bool inOtherConnection = inUseAnywhere && usedByConnection && usedByConnection != this->m_connection;

    if (inCurrentConnection)
    {
        QMessageBox::warning(this,
                             tr("LUN Already In Use"),
                             tr("The selected LUN already belongs to SR '%1' on this pool.\nChoose another LUN.")
                                 .arg(usedByName.isEmpty() ? existingUuid : usedByName));
        return false;
    }

    const QString details = inOtherConnection
                                ? tr("SR UUID: %1\nThis SR appears to be attached on another connected pool.\nYou can only reattach it.")
                                      .arg(existingUuid)
                                : tr("SR UUID: %1\nChoose whether to reattach the existing SR or format the LUN to create a new one.")
                                      .arg(existingUuid);
    bool repeat = false;
    const ExistingSrDecision choice = this->askExistingSrDecision(tr("Existing SR Found"),
                                                                   details,
                                                                   true,
                                                                   !inOtherConnection,
                                                                   false,
                                                                   repeat);
    if (choice == ExistingSrDecision::Cancel)
        return false;

    if (choice == ExistingSrDecision::Reattach)
    {
        this->m_selectedSRUuid = existingUuid;
        this->m_iscsiProbeSelectedConfig = existingConfig;
    } else
    {
        this->m_selectedSRUuid.clear();
        this->m_iscsiProbeSelectedConfig.clear();
    }

    // Keep selected backend aligned with probe result (C# probe can flip between gfs2/lvmoiscsi).
    if (this->m_gfs2ProvisioningRadio && this->m_standardProvisioningRadio)
    {
        QSignalBlocker blockStd(this->m_standardProvisioningRadio);
        QSignalBlocker blockGfs2(this->m_gfs2ProvisioningRadio);
        if (usedSrType == "gfs2")
            this->m_gfs2ProvisioningRadio->setChecked(true);
        else
            this->m_standardProvisioningRadio->setChecked(true);
    }

    this->m_hasEvaluatedProbeDecisions = true;
    return true;
}

bool NewSRWizard::evaluateFibreProbeDecision()
{
    QList<FibreChannelDevice> selectedDevices = this->getSelectedFibreDevices();
    if (selectedDevices.isEmpty())
        return false;

    QList<FibreChannelDevice> existingCandidates;
    QList<FibreChannelDevice> emptyCandidates;

    for (FibreChannelDevice device : selectedDevices)
    {
        QVariantMap probeConfig;
        probeConfig["SCSIid"] = device.scsiId;
        if (this->m_selectedSRType == SRType::FCoE)
            probeConfig["path"] = device.path;

        QString usedType;
        QString probeError;
        const QList<QVariantMap> matches = this->probeForExistingSrs(probeConfig, usedType, probeError);
        if (!probeError.isEmpty())
        {
            QMessageBox::critical(this,
                                  tr("Probe Failed"),
                                  tr("Failed to probe Fibre Channel device %1:\n\n%2").arg(device.scsiId, probeError));
            return false;
        }

        if (!matches.isEmpty())
        {
            const QVariantMap existing = matches.first();
            device.existingSrUuid = existing.value("uuid").toString();
            device.existingSrConfiguration = normalizeProbeConfig(existing.value("configuration").toMap());
            existingCandidates.append(device);

            if (this->m_gfs2ProvisioningRadio && this->m_standardProvisioningRadio)
            {
                QSignalBlocker blockStd(this->m_standardProvisioningRadio);
                QSignalBlocker blockGfs2(this->m_gfs2ProvisioningRadio);
                if (usedType == "gfs2")
                    this->m_gfs2ProvisioningRadio->setChecked(true);
                else
                    this->m_standardProvisioningRadio->setChecked(true);
            }
        } else
        {
            device.existingSrUuid.clear();
            device.existingSrConfiguration.clear();
            emptyCandidates.append(device);
        }
    }

    QList<FibreChannelDevice> finalPlans;
    bool repeat = false;
    ExistingSrDecision repeatedChoice = ExistingSrDecision::Cancel;

    for (int i = 0; i < existingCandidates.size(); ++i)
    {
        FibreChannelDevice device = existingCandidates.at(i);
        XenConnection* usedByConnection = nullptr;
        QString usedByName;
        const bool inUseAnywhere = this->isSrUuidInAnyConnectedPool(device.existingSrUuid, &usedByConnection, &usedByName);
        const bool inCurrentConnection = inUseAnywhere && usedByConnection == this->m_connection;
        const bool inOtherConnection = inUseAnywhere && usedByConnection && usedByConnection != this->m_connection;

        if (inCurrentConnection)
        {
            QMessageBox::warning(this,
                                 tr("Device Already In Use"),
                                 tr("Device %1 is already used by SR '%2' on this pool.")
                                     .arg(device.scsiId, usedByName.isEmpty() ? device.existingSrUuid : usedByName));
            return false;
        }

        ExistingSrDecision decision = repeatedChoice;
        if (!repeat)
        {
            const QString details =
                inOtherConnection
                    ? tr("Device: %1\nExisting SR UUID: %2\nThis SR is attached on another connected pool.\nYou can only reattach it.")
                          .arg(device.scsiId, device.existingSrUuid)
                    : tr("Device: %1\nExisting SR UUID: %2\nChoose reattach or format.")
                          .arg(device.scsiId, device.existingSrUuid);
            decision = this->askExistingSrDecision(tr("Existing SR Found"),
                                                   details,
                                                   true,
                                                   !inOtherConnection,
                                                   (i + 1) < existingCandidates.size(),
                                                   repeat);
            repeatedChoice = decision;
        }

        if (decision == ExistingSrDecision::Cancel)
            return false;

        if (decision == ExistingSrDecision::Format)
        {
            device.existingSrUuid.clear();
            device.existingSrConfiguration.clear();
        }
        finalPlans.append(device);
    }

    repeat = false;
    repeatedChoice = ExistingSrDecision::Cancel;
    for (int i = 0; i < emptyCandidates.size(); ++i)
    {
        FibreChannelDevice device = emptyCandidates.at(i);
        ExistingSrDecision decision = repeatedChoice;
        if (!repeat)
        {
            const QString details =
                tr("Device: %1\nNo existing SR was found.\nFormatting will create a new SR on this LUN.")
                    .arg(device.scsiId);
            decision = this->askExistingSrDecision(tr("No Existing SR"),
                                                   details,
                                                   false,
                                                   true,
                                                   (i + 1) < emptyCandidates.size(),
                                                   repeat);
            repeatedChoice = decision;
        }

        if (decision != ExistingSrDecision::Format)
            return false;

        finalPlans.append(device);
    }

    this->m_plannedFibreDevices = finalPlans;
    this->m_hasPlannedFibreDevices = true;
    this->m_hasEvaluatedProbeDecisions = true;
    return !this->m_plannedFibreDevices.isEmpty();
}

QList<NewSRWizard::PlannedAction> NewSRWizard::buildPlannedActions(const QSharedPointer<Host>& coordinatorHost, QString& error) const
{
    QList<PlannedAction> plans;
    error.clear();

    if (!coordinatorHost)
    {
        error = tr("No coordinator host is available.");
        return plans;
    }

    const QString defaultType = this->getSRTypeString();
    const QString defaultContentType = this->getContentType();
    const QVariantMap defaultDeviceConfig = this->getDeviceConfig();
    const QVariantMap defaultSmConfig = this->getSMConfig();

    auto appendPlan = [&](const QString& srUuid, const QVariantMap& deviceConfig, const QVariantMap& smConfig, const QString& forcedType = QString())
    {
        PlannedAction plan;
        plan.coordinatorHost = coordinatorHost;
        plan.srName = this->m_srName;
        plan.srDescription = this->m_srDescription;
        plan.srType = forcedType.isEmpty() ? defaultType : forcedType;
        plan.contentType = defaultContentType;
        plan.deviceConfig = deviceConfig;
        plan.smConfig = smConfig;
        plan.srUuid = srUuid;

        if (srUuid.isEmpty())
        {
            plan.mode = ActionMode::Create;
        } else if (this->shouldUseIntroduce(srUuid))
        {
            plan.mode = ActionMode::Introduce;
        } else
        {
            plan.mode = ActionMode::Reattach;
            if (this->m_srToReattach && this->m_srToReattach->GetUUID() == srUuid)
            {
                plan.srToReattach = this->m_srToReattach;
            } else
            {
                const QString srRef = this->getExistingSRRefByUuid(srUuid);
                if (!srRef.isEmpty())
                {
                    plan.srToReattach = QSharedPointer<SR>(new SR(this->m_connection, srRef, const_cast<NewSRWizard*>(this)));
                }
            }
        }

        plans.append(plan);
    };

    if (this->m_selectedSRType == SRType::HBA || this->m_selectedSRType == SRType::FCoE)
    {
        const QList<FibreChannelDevice> selectedDevices =
            this->m_hasPlannedFibreDevices ? this->m_plannedFibreDevices : this->getSelectedFibreDevices();
        if (selectedDevices.isEmpty())
        {
            error = tr("Select at least one Fibre Channel device.");
            return plans;
        }

        for (const FibreChannelDevice& device : selectedDevices)
        {
            QVariantMap deviceConfig;
            deviceConfig["SCSIid"] = device.scsiId;
            if (this->m_selectedSRType == SRType::FCoE)
            {
                deviceConfig["path"] = device.path;
            }

            // If probe found an existing SR on this LUN, prefer its device config/uuid.
            const QString existingUuid = device.existingSrUuid.trimmed();
            if (!device.existingSrConfiguration.isEmpty())
                deviceConfig = device.existingSrConfiguration;
            if (deviceConfig.isEmpty())
            {
                deviceConfig["SCSIid"] = device.scsiId;
                if (this->m_selectedSRType == SRType::FCoE)
                    deviceConfig["path"] = device.path;
            }

            appendPlan(existingUuid, deviceConfig, defaultSmConfig);
        }

        return plans;
    }

    if (!this->m_selectedSRUuid.isEmpty())
    {
        appendPlan(this->m_selectedSRUuid, defaultDeviceConfig, defaultSmConfig);
    } else
    {
        appendPlan(QString(), defaultDeviceConfig, defaultSmConfig);
    }

    return plans;
}

AsyncOperation* NewSRWizard::createActionFromPlan(const PlannedAction& plan) const
{
    switch (plan.mode)
    {
        case ActionMode::Create:
            return new SrCreateAction(this->m_connection,
                                      plan.coordinatorHost,
                                      plan.srName,
                                      plan.srDescription,
                                      plan.srType,
                                      plan.contentType,
                                      plan.deviceConfig,
                                      plan.smConfig,
                                      const_cast<NewSRWizard*>(this));
        case ActionMode::Introduce:
            return new SrIntroduceAction(this->m_connection,
                                         plan.srUuid,
                                         plan.srName,
                                         plan.srDescription,
                                         plan.srType,
                                         plan.contentType,
                                         plan.deviceConfig,
                                         const_cast<NewSRWizard*>(this));
        case ActionMode::Reattach:
            if (!plan.srToReattach)
                return nullptr;
            return new SrReattachAction(plan.srToReattach,
                                        plan.srName,
                                        plan.srDescription,
                                        plan.deviceConfig,
                                        const_cast<NewSRWizard*>(this));
    }

    return nullptr;
}

QString NewSRWizard::getExistingSRRefByUuid(const QString& srUuid) const
{
    if (!this->m_connection || !this->m_connection->GetCache() || srUuid.isEmpty())
        return QString();

    const QList<QVariantMap> allSRs = this->m_connection->GetCache()->GetAllData("sr");
    for (const QVariantMap& srData : allSRs)
    {
        if (srData.value("uuid").toString() != srUuid)
            continue;

        QString srRef = srData.value("ref").toString();
        if (srRef.isEmpty())
            srRef = srData.value("opaque_ref").toString();
        return srRef;
    }

    return QString();
}

bool NewSRWizard::shouldUseIntroduce(const QString& srUuid) const
{
    if (srUuid.isEmpty())
        return false;

    if (!this->m_srToReattach)
        return true;

    return this->m_srToReattach->GetConnection() != this->m_connection;
}

QString NewSRWizard::getLocalSrTypeString() const
{
    const QString fs = this->m_localFilesystem.trimmed().toLower();
    if (fs == "xfs")
        return "xfs";
    if (fs == "lvm")
        return "lvm";

    // ext3/ext4 and generic ext map to "ext", same as C# local storage type.
    return "ext";
}

void NewSRWizard::updateSummary()
{
    QString summary;
    QTextStream stream(&summary);

    stream << QString("<b>Storage Repository Type:</b> %1<br>").arg(this->formatSRTypeString(this->m_selectedSRType));
    stream << QString("<b>Name:</b> %1<br>").arg(this->m_srName);
    if (!this->m_srDescription.isEmpty())
        stream << QString("<b>Description:</b> %1<br>").arg(this->m_srDescription);

    stream << "<br>";

    switch (this->m_selectedSRType)
    {
        case SRType::NFS:
        case SRType::NFS_ISO:
        case SRType::CIFS:
        case SRType::CIFS_ISO:
            stream << QString("<b>Server:</b> %1<br>").arg(this->m_server);
            stream << QString("<b>Server Path:</b> %1<br>").arg(this->m_serverPath);
            stream << QString("<b>Port:</b> %1<br>").arg(this->m_port);
            if (this->m_selectedSRType == SRType::CIFS || this->m_selectedSRType == SRType::CIFS_ISO)
            {
                stream << QString("<b>Username:</b> %1<br>").arg(this->m_username);
                if (!this->m_password.isEmpty())
                    stream << QString("<b>Password:</b> %1<br>").arg(QString("*").repeated(this->m_password.length()));
            }
            break;
        case SRType::LocalStorage:
            stream << QString("<b>Device/Path:</b> %1<br>").arg(this->m_localPath);
            stream << QString("<b>Filesystem:</b> %1<br>").arg(this->m_localFilesystem);
            break;
        case SRType::iSCSI:
            if (this->m_provisioningGroup && this->m_provisioningGroup->isVisible())
                stream << QString("<b>Provisioning:</b> %1<br>")
                              .arg(this->m_gfs2ProvisioningRadio && this->m_gfs2ProvisioningRadio->isChecked()
                                       ? tr("Clustered (gfs2)")
                                       : tr("Standard"));
            stream << QString("<b>Target:</b> %1<br>").arg(this->m_iscsiTarget);
            stream << QString("<b>Target IQN:</b> %1<br>").arg(this->m_iscsiTargetIQN);
            stream << QString("<b>LUN:</b> %1<br>").arg(this->m_iscsiLUN);
            if (this->m_iscsiUseChap)
                stream << QString("<b>CHAP User:</b> %1<br>").arg(this->m_iscsiChapUsername);
            break;
        case SRType::HBA:
        case SRType::FCoE:
            if (this->m_provisioningGroup && this->m_provisioningGroup->isVisible())
                stream << QString("<b>Provisioning:</b> %1<br>")
                              .arg(this->m_gfs2ProvisioningRadio && this->m_gfs2ProvisioningRadio->isChecked()
                                       ? tr("Clustered (gfs2)")
                                       : tr("Standard"));
            stream << tr("<b>Configuration:</b> Selected Fibre Channel devices will be used.<br>");
            break;
    }

    this->ui->summaryTextEdit->setHtml(summary);
}

void NewSRWizard::accept()
{
    this->collectNameAndDescription();
    this->collectConfiguration();

    if (!this->m_connection || !this->m_connection->IsConnected() || !this->m_connection->GetCache())
    {
        QMessageBox::critical(this, tr("Error"), tr("Not connected to XenServer. Please reconnect and try again."));
        return;
    }

    QList<QVariantMap> pools = this->m_connection->GetCache()->GetAllData("pool");
    if (pools.isEmpty())
    {
        QMessageBox::critical(this, tr("Error"), tr("Failed to get pool information. Connection may be lost."));
        return;
    }

    QString masterRef = pools.first().value("master").toString();

    QSharedPointer<Host> coordinatorHost(new Host(this->m_connection, masterRef, this));

    QString planningError;
    const QList<PlannedAction> plans = this->buildPlannedActions(coordinatorHost, planningError);
    if (!planningError.isEmpty() || plans.isEmpty())
    {
        QMessageBox::critical(this,
                              tr("Error"),
                              planningError.isEmpty() ? tr("No storage operation can be started with current selection.")
                                                      : planningError);
        return;
    }

    QList<AsyncOperation*> actions;
    actions.reserve(plans.size());
    for (const PlannedAction& plan : plans)
    {
        AsyncOperation* action = this->createActionFromPlan(plan);
        if (!action)
        {
            qDeleteAll(actions);
            QMessageBox::critical(this, tr("Error"), tr("Failed to prepare storage operation."));
            return;
        }
        actions.append(action);
    }

    AsyncOperation* rootAction = nullptr;
    if (actions.size() == 1)
    {
        rootAction = actions.first();
    } else
    {
        rootAction = new ParallelAction(tr("Creating Storage Repositories"),
                                        tr("Creating storage repositories..."),
                                        tr("Storage repository operations completed"),
                                        actions,
                                        this->m_connection,
                                        false,
                                        true,
                                        ParallelAction::DEFAULT_MAX_PARALLEL_OPERATIONS,
                                        this);
    }

    ActionProgressDialog progressDialog(rootAction, this);
    if (plans.size() == 1)
    {
        switch (plans.first().mode)
        {
            case ActionMode::Create:
                progressDialog.setWindowTitle(tr("Creating Storage Repository"));
                break;
            case ActionMode::Introduce:
                progressDialog.setWindowTitle(tr("Introducing Storage Repository"));
                break;
            case ActionMode::Reattach:
                progressDialog.setWindowTitle(tr("Reattaching Storage Repository"));
                break;
        }
    } else
    {
        progressDialog.setWindowTitle(tr("Creating Storage Repositories"));
    }

    const int dialogResult = progressDialog.exec();
    Q_UNUSED(dialogResult);

    if (rootAction->IsCompleted() && !rootAction->HasError())
    {
        const QString successMsg = plans.size() == 1
                                       ? (plans.first().mode == ActionMode::Create
                                              ? tr("Storage Repository '%1' has been created successfully.")
                                              : (plans.first().mode == ActionMode::Introduce
                                                     ? tr("Storage Repository '%1' has been introduced successfully.")
                                                     : tr("Storage Repository '%1' has been reattached successfully.")))
                                             .arg(this->m_srName)
                                       : tr("%1 storage repository operations finished successfully.").arg(plans.size());
        QMessageBox::information(this, tr("Success"), successMsg);
        QDialog::accept();
        return;
    }

    if (rootAction->HasError())
    {
        QMessageBox::critical(this,
                              tr("Error"),
                              tr("Failed to complete storage operation:\n\n%1").arg(rootAction->GetErrorMessage()));
    }
}

QString NewSRWizard::getSRTypeString() const
{
    switch (this->m_selectedSRType)
    {
        case SRType::NFS:
            return "nfs";
        case SRType::iSCSI:
            return this->getSelectedBlockSrType();
        case SRType::LocalStorage:
            return this->getLocalSrTypeString();
        case SRType::CIFS:
            return "smb";
        case SRType::HBA:
            return this->getSelectedBlockSrType();
        case SRType::FCoE:
            return "lvmofcoe";
        case SRType::NFS_ISO:
        case SRType::CIFS_ISO:
            return "iso";
    }
    return "nfs";
}

QString NewSRWizard::getContentType() const
{
    switch (this->m_selectedSRType)
    {
        case SRType::NFS_ISO:
        case SRType::CIFS_ISO:
            return "iso";
        default:
            return QString();
    }
}

QVariantMap NewSRWizard::getDeviceConfig() const
{
    QVariantMap config;
    switch (this->m_selectedSRType)
    {
        case SRType::NFS:
        case SRType::NFS_ISO:
            config["server"] = this->m_server;
            config["serverpath"] = this->m_serverPath;
            break;
        case SRType::CIFS:
        case SRType::CIFS_ISO:
        {
            if (this->m_selectedSRType == SRType::CIFS_ISO)
            {
                QString sharePath = this->m_serverPath;
                if (!sharePath.startsWith("//"))
                {
                    const QString normalizedPath = sharePath.startsWith("/") ? sharePath.mid(1) : sharePath;
                    sharePath = QString("//%1/%2").arg(this->m_server, normalizedPath);
                }
                config["location"] = sharePath;
                config["type"] = "cifs";

                // ISO shares can optionally point to a sub-path via iso_path.
                const QStringList bits = sharePath.split("/", Qt::SkipEmptyParts);
                if (bits.size() > 2)
                {
                    config["location"] = QString("//%1/%2").arg(bits[0], bits[1]);
                    config["iso_path"] = QString("/") + bits.mid(2).join("/");
                }

                if (!this->m_username.isEmpty())
                    config["username"] = this->m_username;
                if (!this->m_password.isEmpty())
                    config["cifspassword"] = this->m_password;
            } else
            {
                config["server"] = this->m_server;
                config["serverpath"] = this->m_serverPath;
                if (!this->m_username.isEmpty())
                    config["username"] = this->m_username;
                if (!this->m_password.isEmpty())
                    config["password"] = this->m_password;
            }
            break;
        }
        case SRType::iSCSI:
            if (!this->m_iscsiProbeSelectedConfig.isEmpty() && !this->m_selectedSRUuid.isEmpty())
            {
                config = normalizeProbeConfig(this->m_iscsiProbeSelectedConfig);
                if (this->m_iscsiUseChap)
                {
                    config["chapuser"] = this->m_iscsiChapUsername;
                    config["chappassword"] = this->m_iscsiChapPassword;
                }
                break;
            }
            config["target"] = this->m_iscsiTarget;
            config["targetIQN"] = this->m_iscsiTargetIQN;
            if (this->m_iscsiTarget.contains(":"))
            {
                const QStringList parts = this->m_iscsiTarget.split(":");
                if (parts.size() == 2)
                {
                    config["target"] = parts[0];
                    config["port"] = parts[1];
                }
            } else
            {
                config["port"] = "3260";
            }
            if (!this->m_iscsiLUN.isEmpty())
                config["LUNid"] = this->m_iscsiLUN;
            if (this->m_iscsiUseChap)
            {
                config["chapuser"] = this->m_iscsiChapUsername;
                config["chappassword"] = this->m_iscsiChapPassword;
            }
            break;
        case SRType::LocalStorage:
            config["device"] = this->m_localPath;
            break;
        case SRType::HBA:
        case SRType::FCoE:
            // Per-device configs are built in buildPlannedActions().
            break;
    }

    if (this->m_selectedSRType == SRType::NFS_ISO)
    {
        QString location = this->m_serverPath;
        if (!location.startsWith(":") && !location.startsWith("/"))
            location.prepend('/');
        config["location"] = QString("%1:%2").arg(this->m_server, location);
        config["type"] = "nfs_iso";
    }

    return config;
}

QVariantMap NewSRWizard::getSMConfig() const
{
    QVariantMap smConfig;
    if (this->m_selectedSRType == SRType::NFS_ISO)
        smConfig["iso_type"] = "nfs_iso";
    else if (this->m_selectedSRType == SRType::CIFS_ISO)
        smConfig["iso_type"] = "cifs";

    return smConfig;
}

QString NewSRWizard::formatSRTypeString(SRType srType) const
{
    switch (srType)
    {
        case SRType::NFS:
            return tr("NFS Virtual Disk Storage");
        case SRType::iSCSI:
            return tr("Software iSCSI");
        case SRType::LocalStorage:
            return tr("Local Storage");
        case SRType::CIFS:
            return tr("CIFS Storage");
        case SRType::HBA:
            return tr("Hardware HBA (Fibre Channel)");
        case SRType::FCoE:
            return tr("Fibre Channel over Ethernet (FCoE)");
        case SRType::NFS_ISO:
            return tr("NFS ISO Library");
        case SRType::CIFS_ISO:
            return tr("CIFS ISO Library");
    }
    return tr("Unknown");
}
