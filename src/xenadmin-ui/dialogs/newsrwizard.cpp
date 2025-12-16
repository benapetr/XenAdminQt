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
#include "../mainwindow.h"
#include "../../xenlib/xenlib.h"
#include "../../xenlib/xencache.h"
#include "../../xenlib/xen/actions/sr/srcreateaction.h"
#include "../../xenlib/xen/actions/sr/srreattachaction.h"
#include "../../xenlib/xen/host.h"
#include "../../xenlib/xen/sr.h"
#include "../../xenlib/xen/xenapi/xenapi_SR.h"
#include "operationprogressdialog.h"
#include <QtWidgets>
#include <QDir>
#include <QStandardPaths>
#include <QTimer>

NewSRWizard::NewSRWizard(MainWindow* parent)
    : QWizard(parent), m_mainWindow(parent), m_selectedSRType(SRType::NFS), m_port(2049),
      m_iscsiUseChap(false), m_typePage(nullptr), m_namePage(nullptr), m_configPage(nullptr), m_summaryPage(nullptr)
{
    setWindowTitle(tr("New Storage Repository"));
    setWindowIcon(QIcon(":/icons/storage-32.png"));
    setFixedSize(700, 500);

    setupPages();

    // Connect to page changed signal to collect data when user navigates
    connect(this, &QWizard::currentIdChanged, this, &NewSRWizard::onPageChanged);

    qDebug() << "NewSRWizard: Created New Storage Repository Wizard";
}

void NewSRWizard::onPageChanged(int /* pageId */)
{
    // When user navigates away from a page, collect its data
    // This ensures m_srName, m_server, etc. are populated from UI

    if (m_namePage && currentPage() != m_namePage)
    {
        // Collect name/description from name page
        m_srName = m_namePage->getSRName();
        m_srDescription = m_namePage->getSRDescription();
        qDebug() << "NewSRWizard: Collected name =" << m_srName;
    }

    if (m_configPage && currentPage() != m_configPage)
    {
        // Collect configuration from config page
        m_server = m_configPage->getServer();
        m_serverPath = m_configPage->getServerPath();
        m_username = m_configPage->getUsername();
        m_password = m_configPage->getPassword();
        m_port = m_configPage->getPort();
        m_localPath = m_configPage->getLocalPath();
        m_localFilesystem = m_configPage->getLocalFilesystem();

        // Collect iSCSI configuration
        m_iscsiTarget = m_configPage->getISCSITarget();
        m_iscsiTargetIQN = m_configPage->getISCSITargetIQN();
        m_iscsiLUN = m_configPage->getISCSILUN();
        m_iscsiUseChap = m_configPage->getISCSIUseChap();
        m_iscsiChapUsername = m_configPage->getISCSIChapUsername();
        m_iscsiChapPassword = m_configPage->getISCSIChapPassword();

        qDebug() << "NewSRWizard: Collected config - server =" << m_server << ", path =" << m_serverPath;
    }
}

void NewSRWizard::setupPages()
{
    // Create and add wizard pages
    m_typePage = new SRTypeSelectionPage(this);
    m_namePage = new SRNameDescriptionPage(this);
    m_configPage = new SRConfigurationPage(this);
    m_summaryPage = new SRSummaryPage(this);

    setPage(Page_Type, m_typePage);
    setPage(Page_NameDescription, m_namePage);
    setPage(Page_Configuration, m_configPage);
    setPage(Page_Summary, m_summaryPage);

    // Connect signals
    connect(m_typePage, &SRTypeSelectionPage::srTypeChanged,
            this, &NewSRWizard::onSRTypeChanged);

    // Set start page
    setStartId(Page_Type);

    qDebug() << "NewSRWizard: Wizard pages setup complete";
}

void NewSRWizard::onSRTypeChanged(SRType srType)
{
    m_selectedSRType = srType;

    qDebug() << "NewSRWizard: SR type changed to" << static_cast<int>(srType);

    // Update configuration page for new SR type
    if (m_configPage)
    {
        m_configPage->updateForSRType(srType);
    }
}

bool NewSRWizard::validateCurrentPage()
{
    // Force validation of current page
    if (currentPage())
    {
        emit currentPage() -> completeChanged();
        return true;
    }
    return false;
}

// ========================================
// SR Type Selection Page Implementation
// ========================================

SRTypeSelectionPage::SRTypeSelectionPage(QWidget* parent)
    : QWizardPage(parent), m_layout(nullptr), m_typeButtonGroup(nullptr)
{
    setTitle(tr("Storage Repository Type"));
    setSubTitle(tr("Select the type of storage repository to create"));

    setupUI();
    setupSRTypeOptions();

    qDebug() << "SRTypeSelectionPage: Created";
}

void SRTypeSelectionPage::setupUI()
{
    m_layout = new QVBoxLayout(this);

    // Create type selection area
    QLabel* instructionLabel = new QLabel(tr("Choose the type of storage repository you want to create:"));
    instructionLabel->setWordWrap(true);
    m_layout->addWidget(instructionLabel);

    m_layout->addSpacing(10);

    // Create button group for radio buttons
    m_typeButtonGroup = new QButtonGroup(this);
    connect(m_typeButtonGroup, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked),
            this, &SRTypeSelectionPage::onTypeSelectionChanged);

    // Create radio buttons
    m_nfsRadio = new QRadioButton(tr("NFS VHD"), this);
    m_iscsiRadio = new QRadioButton(tr("Software iSCSI"), this);
    m_localStorageRadio = new QRadioButton(tr("Local Storage"), this);
    m_cifsRadio = new QRadioButton(tr("CIFS"), this);
    m_hbaRadio = new QRadioButton(tr("Hardware HBA"), this);
    m_fcoeRadio = new QRadioButton(tr("FCoE"), this);
    m_nfsIsoRadio = new QRadioButton(tr("NFS ISO Library"), this);
    m_cifsIsoRadio = new QRadioButton(tr("CIFS ISO Library"), this);

    // Add radio buttons to button group
    m_typeButtonGroup->addButton(m_nfsRadio, static_cast<int>(SRType::NFS));
    m_typeButtonGroup->addButton(m_iscsiRadio, static_cast<int>(SRType::iSCSI));
    m_typeButtonGroup->addButton(m_localStorageRadio, static_cast<int>(SRType::LocalStorage));
    m_typeButtonGroup->addButton(m_cifsRadio, static_cast<int>(SRType::CIFS));
    m_typeButtonGroup->addButton(m_hbaRadio, static_cast<int>(SRType::HBA));
    m_typeButtonGroup->addButton(m_fcoeRadio, static_cast<int>(SRType::FCoE));
    m_typeButtonGroup->addButton(m_nfsIsoRadio, static_cast<int>(SRType::NFS_ISO));
    m_typeButtonGroup->addButton(m_cifsIsoRadio, static_cast<int>(SRType::CIFS_ISO));

    // Create layout for radio buttons
    QVBoxLayout* radioLayout = new QVBoxLayout();
    radioLayout->addWidget(m_nfsRadio);
    radioLayout->addWidget(m_iscsiRadio);
    radioLayout->addWidget(m_localStorageRadio);
    radioLayout->addWidget(m_cifsRadio);
    radioLayout->addWidget(m_hbaRadio);
    radioLayout->addWidget(m_fcoeRadio);
    radioLayout->addWidget(m_nfsIsoRadio);
    radioLayout->addWidget(m_cifsIsoRadio);

    m_layout->addLayout(radioLayout);

    m_layout->addSpacing(15);

    // Description area
    m_typeDescriptionLabel = new QLabel(tr("Description:"));
    m_typeDescriptionLabel->setFont(QFont("", -1, QFont::Bold));
    m_layout->addWidget(m_typeDescriptionLabel);

    m_typeDescriptionText = new QTextEdit(this);
    m_typeDescriptionText->setMaximumHeight(100);
    m_typeDescriptionText->setReadOnly(true);
    m_layout->addWidget(m_typeDescriptionText);

    // Select NFS by default
    m_nfsRadio->setChecked(true);
    onTypeSelectionChanged();
}

void SRTypeSelectionPage::setupSRTypeOptions()
{
    // Set descriptions for each SR type
    qDebug() << "SRTypeSelectionPage: Setting up SR type options";
}

void SRTypeSelectionPage::onTypeSelectionChanged()
{
    SRType selectedType = getSelectedType();

    QString description;
    switch (selectedType)
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
                         "This allows you to store and access ISO images (CD/DVD images) "
                         "on a remote NFS server for virtual machine installations.");
        break;
    case SRType::CIFS_ISO:
        description = tr("Create an ISO library using CIFS/SMB file sharing. "
                         "This allows you to store and access ISO images (CD/DVD images) "
                         "on a Windows file server or Samba share.");
        break;
    }

    m_typeDescriptionText->setPlainText(description);

    emit srTypeChanged(selectedType);
    emit completeChanged();

    qDebug() << "SRTypeSelectionPage: Type selection changed to" << static_cast<int>(selectedType);
}

SRType SRTypeSelectionPage::getSelectedType() const
{
    if (m_typeButtonGroup->checkedId() >= 0)
    {
        return static_cast<SRType>(m_typeButtonGroup->checkedId());
    }
    return SRType::NFS; // Default
}

bool SRTypeSelectionPage::isComplete() const
{
    return m_typeButtonGroup->checkedButton() != nullptr;
}

int SRTypeSelectionPage::nextId() const
{
    return NewSRWizard::Page_NameDescription;
}

// ========================================
// SR Name and Description Page Implementation
// ========================================

SRNameDescriptionPage::SRNameDescriptionPage(QWidget* parent)
    : QWizardPage(parent), m_layout(nullptr), m_currentSRType(SRType::NFS)
{
    setTitle(tr("Storage Repository Name and Description"));
    setSubTitle(tr("Specify a name and description for the new storage repository"));

    setupUI();
}

void SRNameDescriptionPage::setupUI()
{
    m_layout = new QFormLayout(this);

    // Name field (required)
    m_nameLineEdit = new QLineEdit(this);
    m_nameLineEdit->setMaxLength(255);
    connect(m_nameLineEdit, &QLineEdit::textChanged, this, &QWizardPage::completeChanged);

    QLabel* nameLabel = new QLabel(tr("&Name:"));
    nameLabel->setBuddy(m_nameLineEdit);
    m_layout->addRow(nameLabel, m_nameLineEdit);

    // Description field (optional)
    m_descriptionTextEdit = new QTextEdit(this);
    m_descriptionTextEdit->setMaximumHeight(100);
    m_descriptionTextEdit->setPlaceholderText(tr("Optional description for the storage repository"));

    QLabel* descLabel = new QLabel(tr("&Description:"));
    descLabel->setBuddy(m_descriptionTextEdit);
    m_layout->addRow(descLabel, m_descriptionTextEdit);

    qDebug() << "SRNameDescriptionPage: Setup complete";
}

void SRNameDescriptionPage::initializePage()
{
    // Get the selected SR type from the previous page
    NewSRWizard* wizard = qobject_cast<NewSRWizard*>(this->wizard());
    if (wizard)
    {
        m_currentSRType = wizard->getSelectedSRType();
        generateDefaultName();
    }

    // Focus on name field
    m_nameLineEdit->setFocus();
    m_nameLineEdit->selectAll();
}

void SRNameDescriptionPage::generateDefaultName()
{
    QString defaultName;

    switch (m_currentSRType)
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

    // Add timestamp to make it unique
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm");
    defaultName += QString(" (%1)").arg(timestamp);

    m_nameLineEdit->setText(defaultName);

    qDebug() << "SRNameDescriptionPage: Generated default name:" << defaultName;
}

bool SRNameDescriptionPage::isComplete() const
{
    return !m_nameLineEdit->text().trimmed().isEmpty();
}

QString SRNameDescriptionPage::getSRName() const
{
    return m_nameLineEdit->text().trimmed();
}

QString SRNameDescriptionPage::getSRDescription() const
{
    return m_descriptionTextEdit->toPlainText().trimmed();
}

// ========================================
// SR Configuration Page Implementation
// ========================================

SRConfigurationPage::SRConfigurationPage(QWidget* parent)
    : QWizardPage(parent), m_currentSRType(SRType::NFS)
{
    setTitle(tr("Storage Repository Configuration"));
    setSubTitle(tr("Configure the connection settings for your storage repository"));

    setupUI();
}

void SRConfigurationPage::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);

    setupNetworkStorageConfig();
    setupISCSIConfig();
    setupLocalStorageConfig();
    setupFibreConfig();

    // Initially hide all configuration groups
    hideAllConfigurations();

    m_mainLayout->addStretch();

    qDebug() << "SRConfigurationPage: Setup complete";
}

void SRConfigurationPage::setupNetworkStorageConfig()
{
    // Network storage configuration (NFS, CIFS)
    m_networkConfigGroup = new QGroupBox(tr("Network Storage Configuration"), this);
    m_networkLayout = new QFormLayout(m_networkConfigGroup);

    // Server field
    m_serverLineEdit = new QLineEdit();
    m_serverLineEdit->setPlaceholderText(tr("e.g., 192.168.1.100 or nfs.example.com"));
    connect(m_serverLineEdit, &QLineEdit::textChanged, this, &SRConfigurationPage::onConfigurationChanged);
    m_networkLayout->addRow(tr("Server:"), m_serverLineEdit);

    // Server path field
    m_serverPathLineEdit = new QLineEdit();
    m_serverPathLineEdit->setPlaceholderText(tr("e.g., /exports/xen-storage"));
    connect(m_serverPathLineEdit, &QLineEdit::textChanged, this, &SRConfigurationPage::onConfigurationChanged);
    m_networkLayout->addRow(tr("Server Path:"), m_serverPathLineEdit);

    // Port field
    m_portSpinBox = new QSpinBox();
    m_portSpinBox->setRange(1, 65535);
    m_portSpinBox->setValue(2049); // Default NFS port
    connect(m_portSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SRConfigurationPage::onConfigurationChanged);
    m_networkLayout->addRow(tr("Port:"), m_portSpinBox);

    // Username field (for CIFS)
    m_usernameLineEdit = new QLineEdit();
    m_usernameLineEdit->setPlaceholderText(tr("Username for authentication (optional for NFS)"));
    connect(m_usernameLineEdit, &QLineEdit::textChanged, this, &SRConfigurationPage::onConfigurationChanged);
    m_networkLayout->addRow(tr("Username:"), m_usernameLineEdit);

    // Password field (for CIFS)
    m_passwordLineEdit = new QLineEdit();
    m_passwordLineEdit->setEchoMode(QLineEdit::Password);
    m_passwordLineEdit->setPlaceholderText(tr("Password for authentication (optional for NFS)"));
    connect(m_passwordLineEdit, &QLineEdit::textChanged, this, &SRConfigurationPage::onConfigurationChanged);
    m_networkLayout->addRow(tr("Password:"), m_passwordLineEdit);

    // Test connection button
    m_testConnectionButton = new QPushButton(tr("Test Connection"));
    connect(m_testConnectionButton, &QPushButton::clicked, this, &SRConfigurationPage::onTestConnection);
    m_networkLayout->addRow("", m_testConnectionButton);

    // Connection status label
    m_connectionStatusLabel = new QLabel();
    m_networkLayout->addRow(tr("Status:"), m_connectionStatusLabel);

    // Add spacer
    m_networkLayout->addRow("", new QLabel());

    // SR Reattach options (create new vs reattach existing)
    m_createNewSRRadio = new QRadioButton(tr("Create new SR"));
    m_createNewSRRadio->setChecked(true);
    connect(m_createNewSRRadio, &QRadioButton::toggled, this, &SRConfigurationPage::onCreateNewSRToggled);
    m_networkLayout->addRow("", m_createNewSRRadio);

    m_reattachExistingSRRadio = new QRadioButton(tr("Reattach existing SR"));
    m_reattachExistingSRRadio->setEnabled(false); // Disabled until SRs are found
    m_networkLayout->addRow("", m_reattachExistingSRRadio);

    // List of existing SRs found on the share
    m_existingSRsLabel = new QLabel(tr("Existing SRs:"));
    m_existingSRsLabel->setVisible(false);
    m_networkLayout->addRow(m_existingSRsLabel, static_cast<QWidget*>(nullptr));

    m_existingSRsList = new QListWidget();
    m_existingSRsList->setMaximumHeight(100);
    m_existingSRsList->setVisible(false);
    connect(m_existingSRsList, &QListWidget::itemSelectionChanged, this, &SRConfigurationPage::onExistingSRSelected);
    m_networkLayout->addRow("", m_existingSRsList);

    m_mainLayout->addWidget(m_networkConfigGroup);
}

void SRConfigurationPage::setupISCSIConfig()
{
    // iSCSI specific configuration
    m_iscsiConfigGroup = new QGroupBox(tr("iSCSI Configuration"), this);
    m_iscsiLayout = new QFormLayout(m_iscsiConfigGroup);

    // Target field with scan button
    QHBoxLayout* targetLayout = new QHBoxLayout();
    m_iscsiTargetLineEdit = new QLineEdit();
    m_iscsiTargetLineEdit->setPlaceholderText(tr("e.g., 192.168.1.100:3260"));
    connect(m_iscsiTargetLineEdit, &QLineEdit::textChanged, this, &SRConfigurationPage::onConfigurationChanged);

    m_scanISCSIButton = new QPushButton(tr("Scan Target"));
    connect(m_scanISCSIButton, &QPushButton::clicked, this, &SRConfigurationPage::onScanISCSITarget);

    targetLayout->addWidget(m_iscsiTargetLineEdit);
    targetLayout->addWidget(m_scanISCSIButton);
    m_iscsiLayout->addRow(tr("Target:"), targetLayout);

    // IQN ComboBox (populated by scan)
    m_iscsiIqnLabel = new QLabel(tr("Target IQN:"));
    m_iscsiIqnComboBox = new QComboBox();
    m_iscsiIqnComboBox->setEnabled(false);
    m_iscsiIqnComboBox->addItem(tr("Click 'Scan Target' to discover IQNs"));
    connect(m_iscsiIqnComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SRConfigurationPage::onISCSIIqnSelected);
    m_iscsiLayout->addRow(m_iscsiIqnLabel, m_iscsiIqnComboBox);

    // LUN ComboBox (populated after IQN selected)
    m_iscsiLunLabel = new QLabel(tr("LUN:"));
    m_iscsiLunComboBox = new QComboBox();
    m_iscsiLunComboBox->setEnabled(false);
    m_iscsiLunComboBox->addItem(tr("Select an IQN first"));
    connect(m_iscsiLunComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SRConfigurationPage::onISCSILunSelected);
    m_iscsiLayout->addRow(m_iscsiLunLabel, m_iscsiLunComboBox);

    // CHAP authentication
    m_iscsiChapCheckBox = new QCheckBox(tr("Use CHAP authentication"));
    connect(m_iscsiChapCheckBox, &QCheckBox::toggled, this, &SRConfigurationPage::onConfigurationChanged);
    m_iscsiLayout->addRow("", m_iscsiChapCheckBox);

    m_iscsiChapUsernameLineEdit = new QLineEdit();
    m_iscsiChapUsernameLineEdit->setEnabled(false);
    m_iscsiLayout->addRow(tr("CHAP Username:"), m_iscsiChapUsernameLineEdit);

    m_iscsiChapPasswordLineEdit = new QLineEdit();
    m_iscsiChapPasswordLineEdit->setEchoMode(QLineEdit::Password);
    m_iscsiChapPasswordLineEdit->setEnabled(false);
    m_iscsiLayout->addRow(tr("CHAP Password:"), m_iscsiChapPasswordLineEdit);

    // Connect CHAP checkbox to enable/disable auth fields
    connect(m_iscsiChapCheckBox, &QCheckBox::toggled, [this](bool checked) {
        m_iscsiChapUsernameLineEdit->setEnabled(checked);
        m_iscsiChapPasswordLineEdit->setEnabled(checked);
    });

    m_mainLayout->addWidget(m_iscsiConfigGroup);
}

void SRConfigurationPage::setupLocalStorageConfig()
{
    // Local storage configuration
    m_localConfigGroup = new QGroupBox(tr("Local Storage Configuration"), this);
    m_localLayout = new QFormLayout(m_localConfigGroup);

    // Path field with browse button
    QHBoxLayout* pathLayout = new QHBoxLayout();
    m_localPathLineEdit = new QLineEdit();
    m_localPathLineEdit->setPlaceholderText(tr("e.g., /dev/sdb1 or /mnt/storage"));
    connect(m_localPathLineEdit, &QLineEdit::textChanged, this, &SRConfigurationPage::onConfigurationChanged);

    m_browseButton = new QPushButton(tr("Browse..."));
    connect(m_browseButton, &QPushButton::clicked, this, &SRConfigurationPage::onBrowseLocalPath);

    pathLayout->addWidget(m_localPathLineEdit);
    pathLayout->addWidget(m_browseButton);
    m_localLayout->addRow(tr("Device/Path:"), pathLayout);

    // Filesystem type
    m_filesystemComboBox = new QComboBox();
    m_filesystemComboBox->addItems({"ext3", "ext4", "xfs", "lvm"});
    m_filesystemComboBox->setCurrentText("ext3");
    connect(m_filesystemComboBox, &QComboBox::currentTextChanged,
            this, &SRConfigurationPage::onConfigurationChanged);
    m_localLayout->addRow(tr("Filesystem:"), m_filesystemComboBox);

    // Disk space info
    m_diskSpaceLabel = new QLabel(tr("Available space will be shown after selecting a device"));
    m_localLayout->addRow(tr("Available Space:"), m_diskSpaceLabel);

    m_mainLayout->addWidget(m_localConfigGroup);
}

void SRConfigurationPage::setupFibreConfig()
{
    // HBA/FCoE configuration (Fibre Channel devices)
    m_fibreConfigGroup = new QGroupBox(tr("Fibre Channel Configuration"), this);
    m_fibreLayout = new QVBoxLayout(m_fibreConfigGroup);

    // Info label
    QLabel* infoLabel = new QLabel(tr("Click 'Scan for Devices' to discover Fibre Channel storage devices."));
    infoLabel->setWordWrap(true);
    m_fibreLayout->addWidget(infoLabel);

    // Scan button
    m_scanFibreButton = new QPushButton(tr("Scan for Devices"));
    connect(m_scanFibreButton, &QPushButton::clicked, this, &SRConfigurationPage::onScanFibreDevices);
    m_fibreLayout->addWidget(m_scanFibreButton);

    // Status label
    m_fibreStatusLabel = new QLabel();
    m_fibreStatusLabel->setVisible(false);
    m_fibreLayout->addWidget(m_fibreStatusLabel);

    // Device list with checkboxes
    m_fibreDevicesList = new QListWidget();
    m_fibreDevicesList->setSelectionMode(QAbstractItemView::MultiSelection);
    m_fibreDevicesList->setMinimumHeight(200);
    connect(m_fibreDevicesList, &QListWidget::itemSelectionChanged,
            this, &SRConfigurationPage::onFibreDeviceSelectionChanged);
    m_fibreLayout->addWidget(m_fibreDevicesList);

    // Select All / Clear All buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_selectAllFibreButton = new QPushButton(tr("Select All"));
    m_selectAllFibreButton->setEnabled(false);
    connect(m_selectAllFibreButton, &QPushButton::clicked, this, &SRConfigurationPage::onSelectAllFibreDevices);
    buttonLayout->addWidget(m_selectAllFibreButton);

    m_clearAllFibreButton = new QPushButton(tr("Clear All"));
    m_clearAllFibreButton->setEnabled(false);
    connect(m_clearAllFibreButton, &QPushButton::clicked, this, &SRConfigurationPage::onClearAllFibreDevices);
    buttonLayout->addWidget(m_clearAllFibreButton);

    buttonLayout->addStretch();
    m_fibreLayout->addLayout(buttonLayout);

    // Note about multiple SRs
    QLabel* noteLabel = new QLabel(tr("<i>Note: One SR will be created for each selected device.</i>"));
    noteLabel->setWordWrap(true);
    m_fibreLayout->addWidget(noteLabel);

    m_mainLayout->addWidget(m_fibreConfigGroup);
}

void SRConfigurationPage::updateForSRType(SRType srType)
{
    m_currentSRType = srType;

    hideAllConfigurations();

    switch (srType)
    {
    case SRType::NFS:
    case SRType::NFS_ISO:
        m_networkConfigGroup->setVisible(true);
        m_portSpinBox->setValue(2049);
        m_usernameLineEdit->setVisible(false);
        m_passwordLineEdit->setVisible(false);
        m_networkLayout->labelForField(m_usernameLineEdit)->setVisible(false);
        m_networkLayout->labelForField(m_passwordLineEdit)->setVisible(false);
        break;

    case SRType::CIFS:
    case SRType::CIFS_ISO:
        m_networkConfigGroup->setVisible(true);
        m_portSpinBox->setValue(445);
        m_usernameLineEdit->setVisible(true);
        m_passwordLineEdit->setVisible(true);
        m_networkLayout->labelForField(m_usernameLineEdit)->setVisible(true);
        m_networkLayout->labelForField(m_passwordLineEdit)->setVisible(true);
        break;

    case SRType::iSCSI:
        m_iscsiConfigGroup->setVisible(true);
        break;

    case SRType::LocalStorage:
        m_localConfigGroup->setVisible(true);
        break;

    case SRType::HBA:
    case SRType::FCoE:
        m_fibreConfigGroup->setVisible(true);
        // Update group box title based on type
        m_fibreConfigGroup->setTitle(srType == SRType::HBA ? tr("HBA Configuration") : tr("FCoE Configuration"));
        break;
    }

    emit completeChanged();

    qDebug() << "SRConfigurationPage: Updated for SR type" << static_cast<int>(srType);
}

void SRConfigurationPage::hideAllConfigurations()
{
    m_networkConfigGroup->setVisible(false);
    m_iscsiConfigGroup->setVisible(false);
    m_localConfigGroup->setVisible(false);
    m_fibreConfigGroup->setVisible(false);
}

void SRConfigurationPage::initializePage()
{
    NewSRWizard* wizard = qobject_cast<NewSRWizard*>(this->wizard());
    if (wizard)
    {
        updateForSRType(wizard->getSelectedSRType());
    }
}

bool SRConfigurationPage::isComplete() const
{
    switch (m_currentSRType)
    {
    case SRType::NFS:
    case SRType::NFS_ISO:
    case SRType::CIFS:
    case SRType::CIFS_ISO:
        return validateNetworkConfig();

    case SRType::iSCSI:
        return validateISCSIConfig();

    case SRType::LocalStorage:
        return validateLocalConfig();

    case SRType::HBA:
    case SRType::FCoE:
        return validateFibreConfig();
    }

    return false;
}

bool SRConfigurationPage::validateNetworkConfig() const
{
    if (!m_networkConfigGroup->isVisible())
    {
        return false;
    }

    QString server = m_serverLineEdit->text().trimmed();
    QString path = m_serverPathLineEdit->text().trimmed();

    if (server.isEmpty() || path.isEmpty())
    {
        return false;
    }

    // For CIFS, username might be required
    if (m_currentSRType == SRType::CIFS || m_currentSRType == SRType::CIFS_ISO)
    {
        QString username = m_usernameLineEdit->text().trimmed();
        if (username.isEmpty())
        {
            return false; // CIFS typically requires authentication
        }
    }

    return true;
}

bool SRConfigurationPage::validateISCSIConfig() const
{
    if (!m_iscsiConfigGroup->isVisible())
    {
        return false;
    }

    QString target = m_iscsiTargetLineEdit->text().trimmed();

    // Check if IQN is selected from discovered targets
    bool hasValidIqn = m_iscsiIqnComboBox->isEnabled() &&
                       m_iscsiIqnComboBox->currentIndex() >= 0 &&
                       m_iscsiIqnComboBox->currentIndex() < m_discoveredIqns.size();

    // Check if LUN is selected
    bool hasValidLun = m_iscsiLunComboBox->isEnabled() &&
                       m_iscsiLunComboBox->currentIndex() >= 0 &&
                       m_iscsiLunComboBox->currentIndex() < m_discoveredLuns.size();

    return !target.isEmpty() && hasValidIqn && hasValidLun;
}

bool SRConfigurationPage::validateLocalConfig() const
{
    if (!m_localConfigGroup->isVisible())
    {
        return false;
    }

    QString path = m_localPathLineEdit->text().trimmed();
    return !path.isEmpty();
}

bool SRConfigurationPage::validateFibreConfig() const
{
    if (!m_fibreConfigGroup->isVisible())
    {
        return false;
    }

    // Check if at least one device is selected
    for (int i = 0; i < m_fibreDevicesList->count(); ++i)
    {
        QListWidgetItem* item = m_fibreDevicesList->item(i);
        if (item && item->checkState() == Qt::Checked)
        {
            return true; // At least one device selected
        }
    }

    return false; // No devices selected
}

void SRConfigurationPage::onConfigurationChanged()
{
    emit completeChanged();
}

void SRConfigurationPage::onTestConnection()
{
    // This button now performs SR.probe to scan for existing SRs and validate the NFS/CIFS share
    m_connectionStatusLabel->setText(tr("Scanning server..."));
    m_connectionStatusLabel->setStyleSheet("QLabel { color: blue; }");
    m_testConnectionButton->setEnabled(false);

    NewSRWizard* wizard = qobject_cast<NewSRWizard*>(this->wizard());
    if (!wizard || !wizard->mainWindow() || !wizard->mainWindow()->xenLib())
    {
        m_connectionStatusLabel->setText(tr("Error: Not connected to XenServer"));
        m_connectionStatusLabel->setStyleSheet("QLabel { color: red; }");
        m_testConnectionButton->setEnabled(true);
        return;
    }

    XenLib* xenLib = wizard->mainWindow()->xenLib();

    // Get coordinator host
    QList<QVariantMap> pools = xenLib->getCache()->getAll("pool");
    if (pools.isEmpty())
    {
        m_connectionStatusLabel->setText(tr("Error: Failed to get pool information"));
        m_connectionStatusLabel->setStyleSheet("QLabel { color: red; }");
        m_testConnectionButton->setEnabled(true);
        return;
    }

    QString masterRef = pools.first().value("master").toString();

    // Build device config for probe
    QVariantMap deviceConfig;
    QString server = m_serverLineEdit->text().trimmed();
    QString serverPath = m_serverPathLineEdit->text().trimmed();

    if (server.isEmpty() || serverPath.isEmpty())
    {
        m_connectionStatusLabel->setText(tr("Error: Server and path are required"));
        m_connectionStatusLabel->setStyleSheet("QLabel { color: red; }");
        m_testConnectionButton->setEnabled(true);
        return;
    }

    deviceConfig["server"] = server;
    deviceConfig["serverpath"] = serverPath;

    if (m_currentSRType == SRType::CIFS || m_currentSRType == SRType::CIFS_ISO)
    {
        deviceConfig["type"] = "cifs";
        if (!m_usernameLineEdit->text().isEmpty())
            deviceConfig["username"] = m_usernameLineEdit->text().trimmed();
        if (!m_passwordLineEdit->text().isEmpty())
            deviceConfig["password"] = m_passwordLineEdit->text();
    }

    // Add empty probeversion to get NFS version info
    if (m_currentSRType == SRType::NFS || m_currentSRType == SRType::NFS_ISO)
    {
        deviceConfig["probeversion"] = "";
    }

    QString srTypeStr = (m_currentSRType == SRType::NFS || m_currentSRType == SRType::NFS_ISO) ? "nfs" : "cifs";

    qDebug() << "SRConfigurationPage: Probing" << srTypeStr << "at" << server << ":" << serverPath;

    // Run SR.probe_ext in a separate thread via AsyncOperation wrapper
    // For now, we'll do a simpler synchronous probe (matching C# behavior with progress dialog)
    try
    {
        QVariantList probeResult = XenAPI::SR::probe_ext(
            xenLib->getConnection()->getSession(),
            masterRef,
            deviceConfig,
            srTypeStr,
            QVariantMap());

        // Clear existing SRs list
        m_foundSRs.clear();
        m_existingSRsList->clear();

        // Parse results - probe_ext returns list of SR info
        if (probeResult.isEmpty())
        {
            m_connectionStatusLabel->setText(tr("Connection successful - No existing SRs found"));
            m_connectionStatusLabel->setStyleSheet("QLabel { color: green; }");

            // Hide reattach UI since no SRs found
            m_reattachExistingSRRadio->setEnabled(false);
            m_existingSRsLabel->setVisible(false);
            m_existingSRsList->setVisible(false);
            m_createNewSRRadio->setChecked(true);
        } else
        {
            m_connectionStatusLabel->setText(tr("Connection successful - Found %1 existing SR(s)").arg(probeResult.size()));
            m_connectionStatusLabel->setStyleSheet("QLabel { color: green; }");

            // Show reattach UI and populate list
            m_reattachExistingSRRadio->setEnabled(true);
            m_existingSRsLabel->setVisible(true);
            m_existingSRsList->setVisible(true);

            // Populate existing SRs list
            for (const QVariant& srVar : probeResult)
            {
                QVariantMap srInfo = srVar.toMap();
                QString uuid = srInfo.value("uuid").toString();
                QString name = srInfo.value("name_label", tr("Unnamed SR")).toString();
                QString description = srInfo.value("name_description").toString();

                if (!uuid.isEmpty())
                {
                    m_foundSRs[uuid] = name;

                    // Add to list widget
                    QString displayText = name;
                    if (!description.isEmpty())
                        displayText += QString(" - %1").arg(description);

                    QListWidgetItem* item = new QListWidgetItem(displayText);
                    item->setData(Qt::UserRole, uuid); // Store UUID in item data
                    m_existingSRsList->addItem(item);
                }
            }

            qDebug() << "SRConfigurationPage: Found" << probeResult.size() << "existing SRs";
        }
    } catch (const std::exception& ex)
    {
        QString errorMsg = QString::fromUtf8(ex.what());
        m_connectionStatusLabel->setText(tr("Connection failed: %1").arg(errorMsg));
        m_connectionStatusLabel->setStyleSheet("QLabel { color: red; }");
        qDebug() << "SRConfigurationPage: Probe failed:" << errorMsg;
    } catch (...)
    {
        m_connectionStatusLabel->setText(tr("Connection failed - Unknown error"));
        m_connectionStatusLabel->setStyleSheet("QLabel { color: red; }");
        qDebug() << "SRConfigurationPage: Probe failed with unknown error";
    }

    m_testConnectionButton->setEnabled(true);
}

void SRConfigurationPage::onBrowseLocalPath()
{
    QString currentPath = m_localPathLineEdit->text().trimmed();
    if (currentPath.isEmpty())
    {
        currentPath = "/dev";
    }

    QString selectedPath = QFileDialog::getExistingDirectory(this,
                                                             tr("Select Storage Device or Directory"), currentPath);

    if (!selectedPath.isEmpty())
    {
        m_localPathLineEdit->setText(selectedPath);

        // Update disk space information
        QStorageInfo storage(selectedPath);
        if (storage.isValid())
        {
            qint64 availableBytes = storage.bytesAvailable();
            QString sizeText = QString("%1 GB available")
                                   .arg(availableBytes / (1024 * 1024 * 1024));
            m_diskSpaceLabel->setText(sizeText);
        }
    }
}

QString SRConfigurationPage::getServer() const
{
    return m_serverLineEdit ? m_serverLineEdit->text().trimmed() : QString();
}

QString SRConfigurationPage::getServerPath() const
{
    return m_serverPathLineEdit ? m_serverPathLineEdit->text().trimmed() : QString();
}

QString SRConfigurationPage::getUsername() const
{
    return m_usernameLineEdit ? m_usernameLineEdit->text().trimmed() : QString();
}

QString SRConfigurationPage::getPassword() const
{
    return m_passwordLineEdit ? m_passwordLineEdit->text() : QString();
}

int SRConfigurationPage::getPort() const
{
    return m_portSpinBox ? m_portSpinBox->value() : 0;
}

QString SRConfigurationPage::getLocalPath() const
{
    return m_localPathLineEdit ? m_localPathLineEdit->text().trimmed() : QString();
}

QString SRConfigurationPage::getLocalFilesystem() const
{
    return m_filesystemComboBox ? m_filesystemComboBox->currentText() : QString("ext3");
}

QString SRConfigurationPage::getISCSITarget() const
{
    if (!m_iscsiTargetLineEdit)
        return QString();

    QString target = m_iscsiTargetLineEdit->text().trimmed();

    // If user selected an IQN, use the IP from that IQN (handles multi-homing)
    int iqnIndex = m_iscsiIqnComboBox->currentIndex();
    if (iqnIndex >= 0 && iqnIndex < m_discoveredIqns.size())
    {
        const ISCSIIqnInfo& iqnInfo = m_discoveredIqns[iqnIndex];
        // Return IP:port format
        return QString("%1:%2").arg(iqnInfo.ipAddress).arg(iqnInfo.port);
    }

    return target;
}

QString SRConfigurationPage::getISCSITargetIQN() const
{
    if (!m_iscsiIqnComboBox)
        return QString();

    int index = m_iscsiIqnComboBox->currentIndex();
    if (index >= 0 && index < m_discoveredIqns.size())
    {
        return m_discoveredIqns[index].targetIQN;
    }

    return QString();
}

QString SRConfigurationPage::getISCSILUN() const
{
    if (!m_iscsiLunComboBox)
        return QString();

    int index = m_iscsiLunComboBox->currentIndex();
    if (index >= 0 && index < m_discoveredLuns.size())
    {
        // Return LUN ID as string
        return QString::number(m_discoveredLuns[index].lunId);
    }

    return QString();
}

bool SRConfigurationPage::getISCSIUseChap() const
{
    return m_iscsiChapCheckBox ? m_iscsiChapCheckBox->isChecked() : false;
}

QString SRConfigurationPage::getISCSIChapUsername() const
{
    return m_iscsiChapUsernameLineEdit ? m_iscsiChapUsernameLineEdit->text().trimmed() : QString();
}

QString SRConfigurationPage::getISCSIChapPassword() const
{
    return m_iscsiChapPasswordLineEdit ? m_iscsiChapPasswordLineEdit->text() : QString();
}

QString SRConfigurationPage::getSelectedSRUuid() const
{
    // If reattach radio is selected and an SR is selected in the list, return its UUID
    if (m_reattachExistingSRRadio && m_reattachExistingSRRadio->isChecked() &&
        m_existingSRsList && m_existingSRsList->currentItem())
    {
        // Get UUID from item data
        return m_existingSRsList->currentItem()->data(Qt::UserRole).toString();
    }
    return QString(); // Empty = create new SR
}

bool SRConfigurationPage::isCreatingNewSR() const
{
    return m_createNewSRRadio && m_createNewSRRadio->isChecked();
}

void SRConfigurationPage::onCreateNewSRToggled(bool checked)
{
    // When "Create new SR" is toggled, update UI state
    if (checked && m_existingSRsList)
    {
        m_existingSRsList->clearSelection();
    }
    emit completeChanged();
}

void SRConfigurationPage::onExistingSRSelected()
{
    // When user selects an existing SR, auto-select reattach radio
    if (m_existingSRsList && m_existingSRsList->currentItem())
    {
        m_reattachExistingSRRadio->setChecked(true);
    }
    emit completeChanged();
}

void SRConfigurationPage::onScanISCSITarget()
{
    // Scan for IQNs on the iSCSI target
    qDebug() << "NewSRWizard: Scanning iSCSI target for IQNs...";

    QString target = m_iscsiTargetLineEdit->text().trimmed();
    if (target.isEmpty())
    {
        QMessageBox::warning(this, tr("Invalid Target"), tr("Please enter an iSCSI target address."));
        return;
    }

    // Parse target:port
    QString host = target;
    quint16 port = 3260; // Default iSCSI port
    if (target.contains(":"))
    {
        QStringList parts = target.split(":");
        host = parts[0];
        port = parts[1].toUShort();
    }

    // Build device_config for probe
    QVariantMap deviceConfig;
    deviceConfig["target"] = host;
    deviceConfig["port"] = port;

    // Add CHAP if configured
    if (m_iscsiChapCheckBox->isChecked())
    {
        QString chapUser = m_iscsiChapUsernameLineEdit->text().trimmed();
        QString chapPass = m_iscsiChapPasswordLineEdit->text();

        if (chapUser.isEmpty())
        {
            QMessageBox::warning(this, tr("Invalid CHAP"), tr("Please enter a CHAP username or disable CHAP authentication."));
            return;
        }

        deviceConfig["chapuser"] = chapUser;
        deviceConfig["chappassword"] = chapPass;
    }

    // Disable UI during scan
    m_scanISCSIButton->setEnabled(false);
    m_iscsiTargetLineEdit->setEnabled(false);
    m_scanISCSIButton->setText(tr("Scanning..."));

    try
    {
        MainWindow* mainWindow = qobject_cast<NewSRWizard*>(wizard())->mainWindow();
        if (!mainWindow)
        {
            throw std::runtime_error("No main window available");
        }

        XenLib* xenLib = mainWindow->xenLib();
        if (!xenLib || !xenLib->getConnection() || !xenLib->getConnection()->getSession())
        {
            throw std::runtime_error("Not connected to XenServer");
        }

        XenSession* session = xenLib->getConnection()->getSession();

        // Get pool master for probe
        QList<QVariantMap> pools = xenLib->getCache()->getAll("pool");
        if (pools.isEmpty())
        {
            throw std::runtime_error("No pool found");
        }

        QString masterRef = pools.first().value("master").toString();

        // Call SR.probe_ext to discover IQNs
        qDebug() << "Calling SR.probe_ext for IQN discovery with target:" << host << "port:" << port;
        QVariantList probeResult = XenAPI::SR::probe_ext(session, masterRef, deviceConfig, "lvmoiscsi", QVariantMap());

        qDebug() << "SR.probe_ext returned" << probeResult.size() << "results";

        // Clear previous results
        m_discoveredIqns.clear();
        m_iscsiIqnComboBox->clear();

        // Parse IQN results
        // The probe_ext for iSCSI returns a list of configuration maps with IQNs
        for (const QVariant& resultVar : probeResult)
        {
            QVariantMap result = resultVar.toMap();

            // For iSCSI IQN scan, the result contains 'configuration' with targetIQN
            QVariantMap config = result.value("configuration").toMap();
            QString targetIQN = config.value("targetIQN").toString();
            QString ipAddress = config.value("target").toString();
            quint16 portNum = config.value("port", 3260).toUInt();

            if (!targetIQN.isEmpty())
            {
                ISCSIIqnInfo iqnInfo;
                iqnInfo.targetIQN = targetIQN;
                iqnInfo.ipAddress = ipAddress.isEmpty() ? host : ipAddress;
                iqnInfo.port = portNum == 0 ? port : portNum;
                iqnInfo.index = m_discoveredIqns.size();

                m_discoveredIqns.append(iqnInfo);

                QString displayText = QString("%1 (%2:%3)").arg(targetIQN).arg(iqnInfo.ipAddress).arg(iqnInfo.port);
                m_iscsiIqnComboBox->addItem(displayText);

                qDebug() << "  Found IQN:" << targetIQN << "at" << iqnInfo.ipAddress << ":" << iqnInfo.port;
            }
        }

        if (m_discoveredIqns.isEmpty())
        {
            m_iscsiIqnComboBox->addItem(tr("No IQNs found on target"));
            m_iscsiIqnComboBox->setEnabled(false);
            QMessageBox::information(this, tr("No IQNs Found"),
                                     tr("No iSCSI targets were found on %1:%2.\n\n"
                                        "Please verify the target address and network connectivity.")
                                         .arg(host)
                                         .arg(port));
        } else
        {
            m_iscsiIqnComboBox->setEnabled(true);
            m_iscsiIqnLabel->setEnabled(true);

            // Auto-select if only one IQN found
            if (m_discoveredIqns.size() == 1)
            {
                m_iscsiIqnComboBox->setCurrentIndex(0);
            }

            QMessageBox::information(this, tr("Scan Complete"),
                                     tr("Found %n iSCSI target(s) on %1:%2", "", m_discoveredIqns.size()).arg(host).arg(port));
        }
    } catch (const std::exception& e)
    {
        qDebug() << "NewSRWizard: iSCSI IQN scan failed:" << e.what();
        m_iscsiIqnComboBox->clear();
        m_iscsiIqnComboBox->addItem(tr("Scan failed"));
        m_iscsiIqnComboBox->setEnabled(false);
        QMessageBox::critical(this, tr("Scan Failed"), tr("Failed to scan iSCSI target:\n\n%1").arg(e.what()));
    }

    // Re-enable UI
    m_scanISCSIButton->setEnabled(true);
    m_iscsiTargetLineEdit->setEnabled(true);
    m_scanISCSIButton->setText(tr("Scan Target"));

    emit completeChanged();
}

void SRConfigurationPage::onISCSIIqnSelected(int index)
{
    // When IQN is selected, scan for LUNs on that target
    qDebug() << "NewSRWizard: IQN selected, scanning for LUNs...";

    if (index < 0 || index >= m_discoveredIqns.size())
    {
        m_iscsiLunComboBox->clear();
        m_iscsiLunComboBox->addItem(tr("Select an IQN first"));
        m_iscsiLunComboBox->setEnabled(false);
        emit completeChanged();
        return;
    }

    const ISCSIIqnInfo& iqnInfo = m_discoveredIqns[index];

    // Build device_config for LUN probe
    QVariantMap deviceConfig;
    deviceConfig["target"] = iqnInfo.ipAddress;
    deviceConfig["port"] = iqnInfo.port;
    deviceConfig["targetIQN"] = iqnInfo.targetIQN;

    // Add CHAP if configured
    if (m_iscsiChapCheckBox->isChecked())
    {
        deviceConfig["chapuser"] = m_iscsiChapUsernameLineEdit->text().trimmed();
        deviceConfig["chappassword"] = m_iscsiChapPasswordLineEdit->text();
    }

    // Disable UI during scan
    m_iscsiIqnComboBox->setEnabled(false);
    m_scanISCSIButton->setEnabled(false);

    try
    {
        MainWindow* mainWindow = qobject_cast<NewSRWizard*>(wizard())->mainWindow();
        if (!mainWindow)
        {
            throw std::runtime_error("No main window available");
        }

        XenLib* xenLib = mainWindow->xenLib();
        if (!xenLib || !xenLib->getConnection() || !xenLib->getConnection()->getSession())
        {
            throw std::runtime_error("Not connected to XenServer");
        }

        XenSession* session = xenLib->getConnection()->getSession();

        // Get pool master
        QList<QVariantMap> pools = xenLib->getCache()->getAll("pool");
        if (pools.isEmpty())
        {
            throw std::runtime_error("No pool found");
        }

        QString masterRef = pools.first().value("master").toString();

        // Call SR.probe_ext to discover LUNs
        qDebug() << "Calling SR.probe_ext for LUN discovery on IQN:" << iqnInfo.targetIQN;
        QVariantList probeResult = XenAPI::SR::probe_ext(session, masterRef, deviceConfig, "lvmoiscsi", QVariantMap());

        qDebug() << "SR.probe_ext returned" << probeResult.size() << "LUN results";

        // Clear previous results
        m_discoveredLuns.clear();
        m_iscsiLunComboBox->clear();

        // Parse LUN results
        for (const QVariant& resultVar : probeResult)
        {
            QVariantMap result = resultVar.toMap();

            // LUN info is in 'extra' or direct fields
            QVariantMap extra = result.value("extra").toMap();

            int lunId = result.value("LUNid", extra.value("LUNid", -1)).toInt();
            QString scsiId = result.value("SCSIid", extra.value("SCSIid")).toString();
            QString vendor = result.value("vendor", extra.value("vendor")).toString();
            QString serial = result.value("serial", extra.value("serial")).toString();
            qint64 size = result.value("size", extra.value("size", 0)).toLongLong();

            if (lunId >= 0)
            {
                ISCSILunInfo lunInfo;
                lunInfo.lunId = lunId;
                lunInfo.scsiId = scsiId;
                lunInfo.vendor = vendor;
                lunInfo.serial = serial;
                lunInfo.size = size;

                m_discoveredLuns.append(lunInfo);

                // Format display: "LUN 0: vendor model (size)"
                QString sizeStr = size > 0 ? QString(" (%1 GB)").arg(size / 1073741824.0, 0, 'f', 2) : "";
                QString displayText = QString("LUN %1").arg(lunId);
                if (!vendor.isEmpty() || !serial.isEmpty())
                {
                    displayText += QString(": %1 %2").arg(vendor).arg(serial);
                }
                displayText += sizeStr;

                m_iscsiLunComboBox->addItem(displayText);

                qDebug() << "  Found" << displayText;
            }
        }

        if (m_discoveredLuns.isEmpty())
        {
            m_iscsiLunComboBox->addItem(tr("No LUNs found"));
            m_iscsiLunComboBox->setEnabled(false);
            QMessageBox::information(this, tr("No LUNs Found"),
                                     tr("No LUNs were found on target %1.\n\n"
                                        "Please verify the iSCSI configuration.")
                                         .arg(iqnInfo.targetIQN));
        } else
        {
            m_iscsiLunComboBox->setEnabled(true);
            m_iscsiLunLabel->setEnabled(true);

            // Auto-select if only one LUN
            if (m_discoveredLuns.size() == 1)
            {
                m_iscsiLunComboBox->setCurrentIndex(0);
            }
        }
    } catch (const std::exception& e)
    {
        qDebug() << "NewSRWizard: iSCSI LUN scan failed:" << e.what();
        m_iscsiLunComboBox->clear();
        m_iscsiLunComboBox->addItem(tr("Scan failed"));
        m_iscsiLunComboBox->setEnabled(false);
        QMessageBox::critical(this, tr("Scan Failed"), tr("Failed to scan for LUNs:\n\n%1").arg(e.what()));
    }

    // Re-enable UI
    m_iscsiIqnComboBox->setEnabled(true);
    m_scanISCSIButton->setEnabled(true);

    emit completeChanged();
}

void SRConfigurationPage::onISCSILunSelected(int index)
{
    // LUN selected - just update validation state
    Q_UNUSED(index);
    emit completeChanged();
}

void SRConfigurationPage::onScanFibreDevices()
{
    // Scan for Fibre Channel (HBA/FCoE) devices
    qDebug() << "NewSRWizard: Scanning for Fibre Channel devices...";

    // Disable UI during scan
    m_scanFibreButton->setEnabled(false);
    m_scanFibreButton->setText(tr("Scanning..."));
    m_fibreStatusLabel->setVisible(false);

    try
    {
        MainWindow* mainWindow = qobject_cast<NewSRWizard*>(wizard())->mainWindow();
        if (!mainWindow)
        {
            throw std::runtime_error("No main window available");
        }

        XenLib* xenLib = mainWindow->xenLib();
        if (!xenLib || !xenLib->getConnection() || !xenLib->getConnection()->getSession())
        {
            throw std::runtime_error("Not connected to XenServer");
        }

        XenSession* session = xenLib->getConnection()->getSession();

        // Get pool master for probe
        QList<QVariantMap> pools = xenLib->getCache()->getAll("pool");
        if (pools.isEmpty())
        {
            throw std::runtime_error("No pool found");
        }

        QString masterRef = pools.first().value("master").toString();

        // Determine SR type string
        QString srTypeStr = (m_currentSRType == SRType::HBA) ? "lvmohba" : "lvmofcoe";

        // Call SR.probe_ext with empty device_config for HBA/FCoE scan
        QVariantMap deviceConfig;
        if (m_currentSRType == SRType::FCoE)
        {
            // FCoE might need provider specified
            deviceConfig["provider"] = "fcoe";
        }

        qDebug() << "Calling SR.probe_ext for" << srTypeStr << "device discovery";
        QVariantList probeResult = XenAPI::SR::probe_ext(session, masterRef, deviceConfig, srTypeStr, QVariantMap());

        qDebug() << "SR.probe_ext returned" << probeResult.size() << "Fibre Channel devices";

        // Clear previous results
        m_discoveredFibreDevices.clear();
        m_fibreDevicesList->clear();

        // Parse Fibre Channel device results
        for (const QVariant& resultVar : probeResult)
        {
            QVariantMap result = resultVar.toMap();

            // Parse device info from configuration and extra_info
            QVariantMap config = result.value("configuration").toMap();
            QVariantMap extra = result.value("extra_info").toMap();

            // Merge both maps for easier access
            QVariantMap deviceInfo = config;
            for (auto it = extra.begin(); it != extra.end(); ++it)
            {
                if (!deviceInfo.contains(it.key()))
                    deviceInfo[it.key()] = it.value();
            }

            FibreChannelDevice device;
            device.scsiId = deviceInfo.value("SCSIid", deviceInfo.value("scsiid")).toString();
            device.vendor = deviceInfo.value("vendor").toString();
            device.serial = deviceInfo.value("serial").toString();
            device.path = deviceInfo.value("path").toString();
            device.adapter = deviceInfo.value("adapter").toString();
            device.channel = deviceInfo.value("channel").toString();
            device.id = deviceInfo.value("id").toString();
            device.lun = deviceInfo.value("lun").toString();
            device.nameLabel = deviceInfo.value("name_label").toString();
            device.nameDescription = deviceInfo.value("name_description").toString();
            device.eth = deviceInfo.value("eth").toString(); // For FCoE
            device.poolMetadataDetected = deviceInfo.value("pool_metadata_detected", false).toBool();

            // Parse size (might be string with units or number)
            QString sizeStr = deviceInfo.value("size").toString();
            device.size = 0;
            if (!sizeStr.isEmpty())
            {
                // Try parsing as number first
                bool ok;
                device.size = sizeStr.toLongLong(&ok);
                if (!ok)
                {
                    // Try parsing with units (KB, MB, GB)
                    QString lowerSize = sizeStr.toLower();
                    if (lowerSize.contains("kb"))
                    {
                        device.size = lowerSize.replace("kb", "").trimmed().toLongLong() * 1024;
                    } else if (lowerSize.contains("mb"))
                    {
                        device.size = lowerSize.replace("mb", "").trimmed().toLongLong() * 1024 * 1024;
                    } else if (lowerSize.contains("gb"))
                    {
                        device.size = lowerSize.replace("gb", "").trimmed().toLongLong() * 1024 * 1024 * 1024;
                    }
                }
            } else
            {
                device.size = deviceInfo.value("size").toLongLong();
            }

            if (!device.scsiId.isEmpty())
            {
                m_discoveredFibreDevices.append(device);

                // Format display: "vendor serial (size GB) - SCSIid"
                QString sizeStr = device.size > 0 ? QString(" (%1 GB)").arg(device.size / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2) : "";

                QString displayText = QString("%1 %2%3").arg(device.vendor).arg(device.serial).arg(sizeStr);
                if (!device.scsiId.isEmpty())
                {
                    displayText += QString(" - %1").arg(device.scsiId);
                }

                // Add to list with checkbox
                QListWidgetItem* item = new QListWidgetItem(displayText);
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(Qt::Unchecked);
                m_fibreDevicesList->addItem(item);

                qDebug() << "  Found device:" << displayText;
            }
        }

        if (m_discoveredFibreDevices.isEmpty())
        {
            m_fibreStatusLabel->setText(tr("No Fibre Channel devices found."));
            m_fibreStatusLabel->setStyleSheet("QLabel { color: orange; }");
            m_fibreStatusLabel->setVisible(true);
            m_selectAllFibreButton->setEnabled(false);
            m_clearAllFibreButton->setEnabled(false);

            QMessageBox::information(this, tr("No Devices Found"),
                                     tr("No Fibre Channel devices were detected.\n\n"
                                        "Please verify that:\n"
                                        "• Fibre Channel HBA is installed and configured\n"
                                        "• Storage array is connected and powered on\n"
                                        "• LUNs are properly zoned/masked to this host"));
        } else
        {
            m_fibreStatusLabel->setText(tr("Found %n device(s). Select devices to create SRs.", "", m_discoveredFibreDevices.size()));
            m_fibreStatusLabel->setStyleSheet("QLabel { color: green; }");
            m_fibreStatusLabel->setVisible(true);
            m_selectAllFibreButton->setEnabled(true);
            m_clearAllFibreButton->setEnabled(true);
        }
    } catch (const std::exception& e)
    {
        qDebug() << "NewSRWizard: Fibre Channel scan failed:" << e.what();
        m_discoveredFibreDevices.clear();
        m_fibreDevicesList->clear();
        m_fibreStatusLabel->setText(tr("Scan failed: %1").arg(e.what()));
        m_fibreStatusLabel->setStyleSheet("QLabel { color: red; }");
        m_fibreStatusLabel->setVisible(true);
        m_selectAllFibreButton->setEnabled(false);
        m_clearAllFibreButton->setEnabled(false);
        QMessageBox::critical(this, tr("Scan Failed"), tr("Failed to scan for Fibre Channel devices:\n\n%1").arg(e.what()));
    }

    // Re-enable UI
    m_scanFibreButton->setEnabled(true);
    m_scanFibreButton->setText(tr("Scan for Devices"));

    emit completeChanged();
}

void SRConfigurationPage::onFibreDeviceSelectionChanged()
{
    // Update validation when selection changes
    emit completeChanged();
}

void SRConfigurationPage::onSelectAllFibreDevices()
{
    for (int i = 0; i < m_fibreDevicesList->count(); ++i)
    {
        QListWidgetItem* item = m_fibreDevicesList->item(i);
        if (item)
        {
            item->setCheckState(Qt::Checked);
        }
    }
    emit completeChanged();
}

void SRConfigurationPage::onClearAllFibreDevices()
{
    for (int i = 0; i < m_fibreDevicesList->count(); ++i)
    {
        QListWidgetItem* item = m_fibreDevicesList->item(i);
        if (item)
        {
            item->setCheckState(Qt::Unchecked);
        }
    }
    emit completeChanged();
}

QList<SRConfigurationPage::FibreChannelDevice> SRConfigurationPage::getSelectedFibreDevices() const
{
    QList<FibreChannelDevice> selectedDevices;

    for (int i = 0; i < m_fibreDevicesList->count() && i < m_discoveredFibreDevices.size(); ++i)
    {
        QListWidgetItem* item = m_fibreDevicesList->item(i);
        if (item && item->checkState() == Qt::Checked)
        {
            selectedDevices.append(m_discoveredFibreDevices[i]);
        }
    }

    return selectedDevices;
}

// ========================================
// SR Summary Page Implementation
// ========================================

SRSummaryPage::SRSummaryPage(QWidget* parent)
    : QWizardPage(parent), m_creationStarted(false)
{
    setTitle(tr("Storage Repository Summary"));
    setSubTitle(tr("Review the storage repository configuration and create it"));

    setupUI();
}

void SRSummaryPage::setupUI()
{
    m_layout = new QVBoxLayout(this);

    // Summary title
    m_summaryTitleLabel = new QLabel(tr("Storage Repository Configuration Summary:"));
    m_summaryTitleLabel->setFont(QFont("", -1, QFont::Bold));
    m_layout->addWidget(m_summaryTitleLabel);

    // Summary content
    m_summaryTextEdit = new QTextEdit(this);
    m_summaryTextEdit->setReadOnly(true);
    m_summaryTextEdit->setMaximumHeight(200);
    m_layout->addWidget(m_summaryTextEdit);

    m_layout->addSpacing(20);

    // Creation progress (initially hidden)
    QLabel* progressLabel = new QLabel(tr("Creation Progress:"));
    progressLabel->setFont(QFont("", -1, QFont::Bold));
    m_layout->addWidget(progressLabel);

    m_creationProgressBar = new QProgressBar(this);
    m_creationProgressBar->setVisible(false);
    m_layout->addWidget(m_creationProgressBar);

    m_creationStatusLabel = new QLabel();
    m_layout->addWidget(m_creationStatusLabel);

    m_layout->addStretch();
}

void SRSummaryPage::initializePage()
{
    updateSummary();
    m_creationStarted = false;
    m_creationProgressBar->setVisible(false);
    m_creationStatusLabel->clear();
}

void SRSummaryPage::updateSummary()
{
    NewSRWizard* wizard = qobject_cast<NewSRWizard*>(this->wizard());
    if (!wizard)
    {
        return;
    }

    QString summary;
    QTextStream stream(&summary);

    SRType srType = wizard->getSelectedSRType();
    QString srName = wizard->getSRName();
    QString srDescription = wizard->getSRDescription();

    stream << QString("<b>Storage Repository Type:</b> %1<br>").arg(formatSRTypeString(srType));
    stream << QString("<b>Name:</b> %1<br>").arg(srName);

    if (!srDescription.isEmpty())
    {
        stream << QString("<b>Description:</b> %1<br>").arg(srDescription);
    }

    stream << "<br>";

    // Add type-specific configuration details
    switch (srType)
    {
    case SRType::NFS:
    case SRType::NFS_ISO:
    case SRType::CIFS:
    case SRType::CIFS_ISO:
        stream << QString("<b>Server:</b> %1<br>").arg(wizard->getServer());
        stream << QString("<b>Server Path:</b> %1<br>").arg(wizard->getServerPath());
        stream << QString("<b>Port:</b> %1<br>").arg(wizard->getPort());
        if (srType == SRType::CIFS || srType == SRType::CIFS_ISO)
        {
            stream << QString("<b>Username:</b> %1<br>").arg(wizard->getUsername());
            if (!wizard->getPassword().isEmpty())
            {
                stream << QString("<b>Password:</b> %1<br>").arg(QString("*").repeated(wizard->getPassword().length()));
            }
        }
        break;

    case SRType::LocalStorage:
        stream << QString("<b>Device/Path:</b> %1<br>").arg(wizard->getLocalPath());
        stream << QString("<b>Filesystem:</b> %1<br>").arg(wizard->getLocalFilesystem());
        break;

    case SRType::iSCSI:
        // Would show iSCSI-specific config here
        stream << "<b>Configuration:</b> iSCSI settings configured<br>";
        break;

    case SRType::HBA:
    case SRType::FCoE:
        stream << QString("<b>Configuration:</b> %1 hardware will be detected automatically<br>")
                      .arg(srType == SRType::HBA ? "HBA" : "FCoE");
        break;
    }

    m_summaryTextEdit->setHtml(summary);

    qDebug() << "SRSummaryPage: Updated summary for" << srName;
}

QString SRSummaryPage::formatSRTypeString(SRType srType)
{
    switch (srType)
    {
    case SRType::NFS:
        return tr("NFS Virtual Hard Disk Storage");
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

bool SRSummaryPage::isComplete() const
{
    // Always allow finishing
    return true;
}

// ========================================
// NewSRWizard - SR Creation Implementation
// ========================================

void NewSRWizard::accept()
{
    qDebug() << "NewSRWizard::accept() - Starting SR creation";

    // Collect final data from all pages
    if (m_namePage)
    {
        m_srName = m_namePage->getSRName();
        m_srDescription = m_namePage->getSRDescription();
    }

    if (m_configPage)
    {
        m_server = m_configPage->getServer();
        m_serverPath = m_configPage->getServerPath();
        m_username = m_configPage->getUsername();
        m_password = m_configPage->getPassword();
        m_port = m_configPage->getPort();
        m_localPath = m_configPage->getLocalPath();
        m_localFilesystem = m_configPage->getLocalFilesystem();

        // Collect iSCSI configuration
        m_iscsiTarget = m_configPage->getISCSITarget();
        m_iscsiTargetIQN = m_configPage->getISCSITargetIQN();
        m_iscsiLUN = m_configPage->getISCSILUN();
        m_iscsiUseChap = m_configPage->getISCSIUseChap();
        m_iscsiChapUsername = m_configPage->getISCSIChapUsername();
        m_iscsiChapPassword = m_configPage->getISCSIChapPassword();

        // Collect SR reattach option
        m_selectedSRUuid = m_configPage->getSelectedSRUuid();
    }

    // Get coordinator host from pool
    XenLib* xenLib = m_mainWindow->xenLib();
    if (!xenLib || !xenLib->isConnected())
    {
        QMessageBox::critical(this, tr("Error"),
                              tr("Not connected to XenServer. Please reconnect and try again."));
        return;
    }

    // Get pool and coordinator host
    QList<QVariantMap> pools = xenLib->getCache()->getAll("pool");
    if (pools.isEmpty())
    {
        QMessageBox::critical(this, tr("Error"),
                              tr("Failed to get pool information. Connection may be lost."));
        return;
    }

    QVariantMap poolData = pools.first();
    QString masterRef = poolData.value("master").toString();
    QVariantMap hostData = xenLib->getCache()->resolve("host", masterRef);

    if (hostData.isEmpty())
    {
        QMessageBox::critical(this, tr("Error"),
                              tr("Failed to get coordinator host. Connection may be lost."));
        return;
    }

    // Build device config and SM config
    QVariantMap deviceConfig = getDeviceConfig();
    QVariantMap smConfig = getSMConfig();

    // Get SR type and content type strings
    QString srTypeStr = getSRTypeString();
    QString contentType = getContentType();

    qDebug() << "NewSRWizard: Creating/Reattaching SR"
             << "\n  Name:" << m_srName
             << "\n  Type:" << srTypeStr
             << "\n  Content:" << contentType
             << "\n  UUID:" << (m_selectedSRUuid.isEmpty() ? "none (create new)" : m_selectedSRUuid)
             << "\n  DeviceConfig:" << deviceConfig;

    // Create Host object for SR Action
    Host* coordinatorHost = new Host(xenLib->getConnection(), masterRef, this);

    AsyncOperation* srAction = nullptr;

    // Decide between Create and Reattach based on UUID
    // Matches C# NewSRWizard.cs GetActions() lines 560-609
    if (m_selectedSRUuid.isEmpty())
    {
        // No UUID = Create new SR
        srAction = new SrCreateAction(
            xenLib->getConnection(),
            coordinatorHost,
            m_srName,
            m_srDescription,
            srTypeStr,
            contentType,
            deviceConfig,
            smConfig,
            this);
    } else
    {
        // UUID present = Reattach existing SR
        // Note: We need to get the actual SR object for SrReattachAction
        // For now, create a simple SR wrapper
        SR* srToReattach = new SR(xenLib->getConnection(), m_selectedSRUuid, this);

        srAction = new SrReattachAction(
            srToReattach,
            m_srName,
            m_srDescription,
            deviceConfig,
            this);
    }

    // Show progress dialog
    OperationProgressDialog* progressDialog = new OperationProgressDialog(srAction, this);
    progressDialog->setWindowTitle(m_selectedSRUuid.isEmpty() ? tr("Creating Storage Repository") : tr("Reattaching Storage Repository"));

    connect(srAction, &AsyncOperation::completed, this, [this, srAction, progressDialog]() {
        if (srAction->isCompleted() && !srAction->hasError())
        {
            QString successMsg = m_selectedSRUuid.isEmpty()
                                     ? tr("Storage Repository '%1' has been created successfully.")
                                     : tr("Storage Repository '%1' has been reattached successfully.");

            qDebug() << "NewSRWizard: SR operation completed successfully";
            QMessageBox::information(this, tr("Success"), successMsg.arg(m_srName));
            progressDialog->accept();
            QDialog::accept(); // Close wizard
        } else
        {
            qDebug() << "NewSRWizard: SR operation failed:" << srAction->errorMessage();
            QMessageBox::critical(this, tr("Error"),
                                  tr("Failed to %1 Storage Repository:\n\n%2")
                                      .arg(m_selectedSRUuid.isEmpty() ? "create" : "reattach")
                                      .arg(srAction->errorMessage()));
            progressDialog->reject();
            // Don't close wizard - let user modify settings and retry
        }
    });

    // Start the action
    srAction->runAsync();
    progressDialog->exec();
}

QString NewSRWizard::getSRTypeString() const
{
    // Convert SRType enum to XenAPI SR type string
    // Matches C# SrWizardType.Type property
    switch (m_selectedSRType)
    {
    case SRType::NFS:
        return "nfs";
    case SRType::iSCSI:
        return "lvmoiscsi";
    case SRType::LocalStorage:
        return "ext"; // Could also be "lvm" or "xfs" depending on filesystem
    case SRType::CIFS:
        return "cifs";
    case SRType::HBA:
        return "lvmohba";
    case SRType::FCoE:
        return "lvmofcoe";
    case SRType::NFS_ISO:
        return "nfs"; // Same as NFS, but content type differs
    case SRType::CIFS_ISO:
        return "cifs"; // Same as CIFS, but content type differs
    }

    return "nfs"; // Default fallback
}

QString NewSRWizard::getContentType() const
{
    // Return "iso" for ISO libraries, "user" for everything else
    // Matches C# SrWizardType.ContentType property
    switch (m_selectedSRType)
    {
    case SRType::NFS_ISO:
    case SRType::CIFS_ISO:
        return "iso";
    default:
        return "user";
    }
}

QVariantMap NewSRWizard::getDeviceConfig() const
{
    // Build device_config map for XenAPI SR.create call
    // Matches C# UpdateWizardContent() in NewSRWizard.cs
    QVariantMap config;

    switch (m_selectedSRType)
    {
    case SRType::NFS:
    case SRType::NFS_ISO:
        // NFS device config: server, serverpath, options (optional)
        config["server"] = m_server;
        config["serverpath"] = m_serverPath;
        // Note: NFS options (nfsversion, etc.) could be added here if UI supports them
        break;

    case SRType::CIFS:
    case SRType::CIFS_ISO:
        // CIFS device config: server, serverpath, username, password
        config["server"] = m_server;
        config["serverpath"] = m_serverPath;
        config["type"] = "cifs";

        if (!m_username.isEmpty())
        {
            config["username"] = m_username;
        }

        if (!m_password.isEmpty())
        {
            // Note: SrCreateAction will convert this to a secret
            config["password"] = m_password;
        }
        break;

    case SRType::iSCSI:
        // iSCSI device config: target, targetIQN, SCSIid (derived from LUN)
        // Matches C# LVMoISCSI.cs GetDeviceConfig() lines 688-742
        config["target"] = m_iscsiTarget;
        config["targetIQN"] = m_iscsiTargetIQN;

        // Parse target to get IP and port
        if (m_iscsiTarget.contains(":"))
        {
            QStringList parts = m_iscsiTarget.split(":");
            if (parts.size() == 2)
            {
                config["target"] = parts[0]; // IP only
                config["port"] = parts[1];   // Port
            }
        } else
        {
            config["port"] = "3260"; // Default iSCSI port
        }

        // LUN or SCSIid
        if (!m_iscsiLUN.isEmpty())
        {
            // For now, use LUN directly. In real implementation, we'd get SCSIid from probe
            // C# uses LunMap to convert LUN index to ISCSIInfo with ScsiID
            config["LUNid"] = m_iscsiLUN;
        }

        // CHAP authentication
        if (m_iscsiUseChap)
        {
            config["chapuser"] = m_iscsiChapUsername;
            // Note: SrCreateAction will convert chappassword to a secret
            config["chappassword"] = m_iscsiChapPassword;
        }
        break;

    case SRType::LocalStorage:
        // Local storage device config: device path
        config["device"] = m_localPath;
        break;

    case SRType::HBA:
    case SRType::FCoE:
        // HBA/FCoE config is more complex, involves device scanning
        // TODO: Implement full HBA/FCoE support
        break;
    }

    return config;
}

QVariantMap NewSRWizard::getSMConfig() const
{
    // Build SM config (Storage Manager specific configuration)
    // This is usually empty for most SR types
    // Matches C# SrWizardType.SMConfig property
    QVariantMap smConfig;

    // Currently no SM config is needed for basic SR types
    // Future: Could add thin provisioning, allocation quantum, etc.

    return smConfig;
}