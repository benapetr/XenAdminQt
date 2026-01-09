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
#include "../../xenlib/xen/actions/sr/srreattachaction.h"
#include "../../xenlib/xen/host.h"
#include "../../xenlib/xen/sr.h"
#include "../../xenlib/xen/xenapi/xenapi_SR.h"
#include "operationprogressdialog.h"

#include <QButtonGroup>
#include <QDateTime>
#include <QFileDialog>
#include <QIcon>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QProgressBar>
#include <QStorageInfo>
#include <QTextEdit>
#include <QTextStream>
#include <QVector>

NewSRWizard::NewSRWizard(XenConnection* connection, MainWindow* parent)
    : QWizard(parent),
      m_mainWindow(parent),
      m_connection(connection),
      ui(new Ui::NewSRWizard),
      m_navigationPane(nullptr),
      m_typeButtonGroup(nullptr),
      m_selectedSRType(SRType::NFS),
      m_port(2049),
      m_iscsiUseChap(false),
      m_forceReattach(false)
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
    switch (this->m_selectedSRType)
    {
        case SRType::NFS:
        case SRType::NFS_ISO:
        case SRType::CIFS:
        case SRType::CIFS_ISO:
            return this->validateNetworkConfig();
        case SRType::iSCSI:
            return this->validateISCSIConfig();
        case SRType::LocalStorage:
            return this->validateLocalConfig();
        case SRType::HBA:
        case SRType::FCoE:
            return this->validateFibreConfig();
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

    if (this->m_forceReattach && !this->m_reattachSrRef.isEmpty())
    {
        this->m_selectedSRUuid = this->m_reattachSrRef;
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

    if ((this->m_selectedSRType == SRType::CIFS || this->m_selectedSRType == SRType::CIFS_ISO) &&
        this->ui->usernameLineEdit->text().trimmed().isEmpty())
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

    deviceConfig["server"] = server;
    deviceConfig["serverpath"] = serverPath;

    if (this->m_selectedSRType == SRType::CIFS || this->m_selectedSRType == SRType::CIFS_ISO)
    {
        deviceConfig["type"] = "cifs";
        if (!this->ui->usernameLineEdit->text().isEmpty())
            deviceConfig["username"] = this->ui->usernameLineEdit->text().trimmed();
        if (!this->ui->passwordLineEdit->text().isEmpty())
            deviceConfig["password"] = this->ui->passwordLineEdit->text();
    }

    if (this->m_selectedSRType == SRType::NFS || this->m_selectedSRType == SRType::NFS_ISO)
        deviceConfig["probeversion"] = "";

    QString srTypeStr = (this->m_selectedSRType == SRType::NFS || this->m_selectedSRType == SRType::NFS_ISO) ? "nfs" : "cifs";

    try
    {
        QVariantList probeResult = XenAPI::SR::probe_ext(
            this->m_connection->GetSession(),
            masterRef,
            deviceConfig,
            srTypeStr,
            QVariantMap());

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
    } catch (const std::exception& ex)
    {
        this->ui->connectionStatusLabel->setText(tr("Connection failed: %1").arg(QString::fromUtf8(ex.what())));
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
    if (checked)
        this->ui->existingSRsList->clearSelection();
    this->onConfigurationChanged();
}

void NewSRWizard::onExistingSRSelected()
{
    if (this->ui->existingSRsList->currentItem())
        this->ui->reattachExistingSRRadio->setChecked(true);
    this->onConfigurationChanged();
}

void NewSRWizard::onChapToggled(bool checked)
{
    this->ui->iscsiChapUsernameLineEdit->setEnabled(checked);
    this->ui->iscsiChapPasswordLineEdit->setEnabled(checked);
    this->onConfigurationChanged();
}

void NewSRWizard::onScanISCSITarget()
{
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

    try
    {
        if (!this->m_connection || !this->m_connection->GetSession() || !this->m_connection->GetCache())
            throw std::runtime_error("Not connected to XenServer");

        QList<QVariantMap> pools = this->m_connection->GetCache()->GetAllData("pool");
        if (pools.isEmpty())
            throw std::runtime_error("No pool found");

        QString masterRef = pools.first().value("master").toString();

        QVariantList probeResult = XenAPI::SR::probe_ext(
            this->m_connection->GetSession(),
            masterRef,
            deviceConfig,
            "lvmoiscsi",
            QVariantMap());

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
    } catch (const std::exception& ex)
    {
        this->ui->iscsiIqnComboBox->clear();
        this->ui->iscsiIqnComboBox->addItem(tr("Scan failed"));
        this->ui->iscsiIqnComboBox->setEnabled(false);
        QMessageBox::critical(this, tr("Scan Failed"), tr("Failed to scan iSCSI target:\n\n%1").arg(ex.what()));
    }

    this->ui->scanISCSIButton->setEnabled(true);
    this->ui->iscsiTargetLineEdit->setEnabled(true);
    this->ui->scanISCSIButton->setText(tr("Scan Target"));
    this->onConfigurationChanged();
}

void NewSRWizard::onISCSIIqnSelected(int index)
{
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

    try
    {
        if (!this->m_connection || !this->m_connection->GetSession() || !this->m_connection->GetCache())
            throw std::runtime_error("Not connected to XenServer");

        QList<QVariantMap> pools = this->m_connection->GetCache()->GetAllData("pool");
        if (pools.isEmpty())
            throw std::runtime_error("No pool found");

        QString masterRef = pools.first().value("master").toString();

        QVariantList probeResult = XenAPI::SR::probe_ext(
            this->m_connection->GetSession(),
            masterRef,
            deviceConfig,
            "lvmoiscsi",
            QVariantMap());

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
    } catch (const std::exception& ex)
    {
        this->ui->iscsiLunComboBox->clear();
        this->ui->iscsiLunComboBox->addItem(tr("Scan failed"));
        this->ui->iscsiLunComboBox->setEnabled(false);
        QMessageBox::critical(this, tr("Scan Failed"), tr("Failed to scan for LUNs:\n\n%1").arg(ex.what()));
    }

    this->ui->iscsiIqnComboBox->setEnabled(true);
    this->ui->scanISCSIButton->setEnabled(true);
    this->onConfigurationChanged();
}

void NewSRWizard::onISCSILunSelected(int)
{
    this->onConfigurationChanged();
}

void NewSRWizard::onScanFibreDevices()
{
    this->ui->scanFibreButton->setEnabled(false);
    this->ui->scanFibreButton->setText(tr("Scanning..."));
    this->ui->fibreStatusLabel->setVisible(false);

    try
    {
        if (!this->m_connection || !this->m_connection->GetSession() || !this->m_connection->GetCache())
            throw std::runtime_error("Not connected to XenServer");

        QList<QVariantMap> pools = this->m_connection->GetCache()->GetAllData("pool");
        if (pools.isEmpty())
            throw std::runtime_error("No pool found");

        QString masterRef = pools.first().value("master").toString();
        QString srTypeStr = (this->m_selectedSRType == SRType::HBA) ? "lvmohba" : "lvmofcoe";

        QVariantMap deviceConfig;
        if (this->m_selectedSRType == SRType::FCoE)
            deviceConfig["provider"] = "fcoe";

        QVariantList probeResult = XenAPI::SR::probe_ext(
            this->m_connection->GetSession(),
            masterRef,
            deviceConfig,
            srTypeStr,
            QVariantMap());

        this->m_discoveredFibreDevices.clear();
        this->ui->fibreDevicesList->clear();

        for (const QVariant& resultVar : probeResult)
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
    } catch (const std::exception& ex)
    {
        this->ui->fibreStatusLabel->setText(tr("Scan failed: %1").arg(ex.what()));
        this->ui->fibreStatusLabel->setStyleSheet("QLabel { color: red; }");
        this->ui->fibreStatusLabel->setVisible(true);
        this->ui->selectAllFibreButton->setEnabled(false);
        this->ui->clearAllFibreButton->setEnabled(false);
        QMessageBox::critical(this, tr("Scan Failed"), tr("Failed to scan for Fibre Channel devices:\n\n%1").arg(ex.what()));
        this->m_discoveredFibreDevices.clear();
        this->ui->fibreDevicesList->clear();
    }

    this->ui->scanFibreButton->setEnabled(true);
    this->ui->scanFibreButton->setText(tr("Scan for Devices"));
    this->onConfigurationChanged();
}

void NewSRWizard::onFibreDeviceSelectionChanged()
{
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
            stream << QString("<b>Target:</b> %1<br>").arg(this->m_iscsiTarget);
            stream << QString("<b>Target IQN:</b> %1<br>").arg(this->m_iscsiTargetIQN);
            stream << QString("<b>LUN:</b> %1<br>").arg(this->m_iscsiLUN);
            if (this->m_iscsiUseChap)
                stream << QString("<b>CHAP User:</b> %1<br>").arg(this->m_iscsiChapUsername);
            break;
        case SRType::HBA:
        case SRType::FCoE:
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
    QVariantMap deviceConfig = this->getDeviceConfig();
    QVariantMap smConfig = this->getSMConfig();
    QString srTypeStr = this->getSRTypeString();
    QString contentType = this->getContentType();

    QSharedPointer<Host> coordinatorHost(new Host(this->m_connection, masterRef, this));

    AsyncOperation* srAction = nullptr;
    if (this->m_selectedSRUuid.isEmpty())
    {
        srAction = new SrCreateAction(
            this->m_connection,
            coordinatorHost,
            this->m_srName,
            this->m_srDescription,
            srTypeStr,
            contentType,
            deviceConfig,
            smConfig,
            this);
    } else
    {
        QSharedPointer<SR> srToReattach = QSharedPointer<SR>(new SR(this->m_connection, this->m_selectedSRUuid, this));
        srAction = new SrReattachAction(
            srToReattach,
            this->m_srName,
            this->m_srDescription,
            deviceConfig,
            this);
    }

    OperationProgressDialog* progressDialog = new OperationProgressDialog(srAction, this);
    progressDialog->setWindowTitle(this->m_selectedSRUuid.isEmpty() ? tr("Creating Storage Repository") : tr("Reattaching Storage Repository"));

    this->connect(srAction, &AsyncOperation::completed, this, [this, srAction, progressDialog]()
    {
        if (srAction->IsCompleted() && !srAction->HasError())
        {
            QString successMsg = this->m_selectedSRUuid.isEmpty()
                                     ? tr("Storage Repository '%1' has been created successfully.")
                                     : tr("Storage Repository '%1' has been reattached successfully.");
            QMessageBox::information(this, tr("Success"), successMsg.arg(this->m_srName));
            progressDialog->accept();
            QDialog::accept();
        } else
        {
            QMessageBox::critical(this, tr("Error"),
                                  tr("Failed to %1 Storage Repository:\n\n%2")
                                      .arg(this->m_selectedSRUuid.isEmpty() ? tr("create") : tr("reattach"))
                                      .arg(srAction->GetErrorMessage()));
            progressDialog->reject();
        }
    });

    srAction->RunAsync();
    progressDialog->exec();
}

QString NewSRWizard::getSRTypeString() const
{
    switch (this->m_selectedSRType)
    {
        case SRType::NFS:
            return "nfs";
        case SRType::iSCSI:
            return "lvmoiscsi";
        case SRType::LocalStorage:
            return "ext";
        case SRType::CIFS:
            return "cifs";
        case SRType::HBA:
            return "lvmohba";
        case SRType::FCoE:
            return "lvmofcoe";
        case SRType::NFS_ISO:
            return "nfs";
        case SRType::CIFS_ISO:
            return "cifs";
    }
    return "nfs";
}

QString NewSRWizard::getContentType() const
{
    return (this->m_selectedSRType == SRType::NFS_ISO || this->m_selectedSRType == SRType::CIFS_ISO) ? "iso" : "user";
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
            config["server"] = this->m_server;
            config["serverpath"] = this->m_serverPath;
            config["type"] = "cifs";
            if (!this->m_username.isEmpty())
                config["username"] = this->m_username;
            if (!this->m_password.isEmpty())
                config["password"] = this->m_password;
            break;
        case SRType::iSCSI:
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
            // Fibre Channel configurations are populated by SR backend
            break;
    }
    return config;
}

QVariantMap NewSRWizard::getSMConfig() const
{
    return QVariantMap();
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
