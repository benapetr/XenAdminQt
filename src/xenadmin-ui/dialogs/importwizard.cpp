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
#include <QtWidgets>

ImportWizard::ImportWizard(QWidget* parent)
    : QWizard(parent), m_importType(ImportType_XVA), m_verifyManifest(true), m_startVMsAutomatically(false)
{
    this->setWindowTitle(tr("Import Virtual Machine"));
    this->setWindowIcon(QIcon(":/icons/vm-import-32.png"));
    this->setWizardStyle(QWizard::ModernStyle);
    this->setOption(QWizard::HaveHelpButton, true);
    this->setOption(QWizard::HelpButtonOnRight, false);

    this->setupWizardPages();

    // Connect signals
    connect(this, &QWizard::currentIdChanged, this, &ImportWizard::onCurrentIdChanged);

    qDebug() << "ImportWizard: Created Import Wizard";
}

void ImportWizard::setupWizardPages()
{
    // Add wizard pages
    this->setPage(Page_Source, this->createSourcePage());
    this->setPage(Page_Host, this->createHostPage());
    this->setPage(Page_Storage, this->createStoragePage());
    this->setPage(Page_Network, this->createNetworkPage());
    this->setPage(Page_Options, this->createOptionsPage());
    this->setPage(Page_Finish, this->createFinishPage());

    // Set the starting page
    this->setStartId(Page_Source);
}

QWizardPage* ImportWizard::createSourcePage()
{
    QWizardPage* page = new QWizardPage;
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

    // File info
    QGroupBox* infoGroup = new QGroupBox(tr("Supported Formats"));
    QLabel* infoLabel = new QLabel(tr("• XVA files (.xva, .xva.gz) - XenServer native format\n"
                                      "• OVF files (.ovf, .ova) - Open Virtualization Format\n"
                                      "• VHD files (.vhd, .vmdk) - Virtual disk images"));
    infoLabel->setWordWrap(true);

    QVBoxLayout* infoLayout = new QVBoxLayout;
    infoLayout->addWidget(infoLabel);
    infoGroup->setLayout(infoLayout);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addWidget(fileGroup);
    mainLayout->addWidget(typeGroup);
    mainLayout->addWidget(infoGroup);
    mainLayout->addStretch();

    page->setLayout(mainLayout);
    return page;
}

QWizardPage* ImportWizard::createHostPage()
{
    QWizardPage* page = new QWizardPage;
    page->setTitle(tr("Select Destination"));
    page->setSubTitle(tr("Choose where to import the virtual machine."));

    QLabel* hostLabel = new QLabel(tr("Target Server:"));
    QComboBox* hostCombo = new QComboBox;
    hostCombo->setObjectName("hostCombo");

    // Add example hosts
    hostCombo->addItem(tr("Local XenServer (xen-host-1)"), "host1");
    hostCombo->addItem(tr("Remote XenServer (xen-host-2)"), "host2");
    hostCombo->addItem(tr("XenServer Pool (production-pool)"), "pool1");

    QLabel* storageLabel = new QLabel(tr("Default Storage:"));
    QComboBox* storageCombo = new QComboBox;
    storageCombo->setObjectName("defaultStorageCombo");
    storageCombo->addItem(tr("Local storage"), "local");
    storageCombo->addItem(tr("Shared NFS storage"), "nfs");
    storageCombo->addItem(tr("iSCSI storage"), "iscsi");

    QLabel* infoLabel = new QLabel(tr("The virtual machine will be imported to the selected server. "
                                      "You can configure specific storage and network settings on the next pages."));
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("QLabel { color: #666; font-style: italic; }");

    QFormLayout* layout = new QFormLayout;
    layout->addRow(hostLabel, hostCombo);
    layout->addRow(storageLabel, storageCombo);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addLayout(layout);
    mainLayout->addWidget(infoLabel);
    mainLayout->addStretch();

    page->setLayout(mainLayout);
    return page;
}

QWizardPage* ImportWizard::createStoragePage()
{
    QWizardPage* page = new QWizardPage;
    page->setTitle(tr("Storage Configuration"));
    page->setSubTitle(tr("Configure storage settings for the imported VM."));

    QLabel* srLabel = new QLabel(tr("Storage Repository:"));
    QComboBox* srCombo = new QComboBox;
    srCombo->setObjectName("storageCombo");
    srCombo->addItem(tr("Local storage"), "local");
    srCombo->addItem(tr("Shared NFS storage - /export/vms"), "nfs");
    srCombo->addItem(tr("iSCSI storage - 10.0.1.100"), "iscsi");

    QCheckBox* thinProvision = new QCheckBox(tr("Use thin provisioning (if supported)"));
    thinProvision->setObjectName("thinProvision");
    thinProvision->setChecked(true);

    QCheckBox* preserveMAC = new QCheckBox(tr("Preserve original MAC addresses"));
    preserveMAC->setObjectName("preserveMAC");

    // Storage mapping (simplified)
    QGroupBox* mappingGroup = new QGroupBox(tr("Storage Mapping"));
    QTextEdit* mappingText = new QTextEdit;
    mappingText->setObjectName("storageMappingText");
    mappingText->setMaximumHeight(100);
    mappingText->setPlainText(tr("Virtual disks will be imported to the selected storage repository."));
    mappingText->setReadOnly(true);

    QVBoxLayout* mappingLayout = new QVBoxLayout;
    mappingLayout->addWidget(mappingText);
    mappingGroup->setLayout(mappingLayout);

    QVBoxLayout* mainLayout = new QVBoxLayout;

    QFormLayout* optionsLayout = new QFormLayout;
    optionsLayout->addRow(srLabel, srCombo);
    optionsLayout->addRow("", thinProvision);
    optionsLayout->addRow("", preserveMAC);

    mainLayout->addLayout(optionsLayout);
    mainLayout->addWidget(mappingGroup);
    mainLayout->addStretch();

    page->setLayout(mainLayout);
    return page;
}

QWizardPage* ImportWizard::createNetworkPage()
{
    QWizardPage* page = new QWizardPage;
    page->setTitle(tr("Network Configuration"));
    page->setSubTitle(tr("Configure network settings for the imported VM."));

    QLabel* networkLabel = new QLabel(tr("Target Network:"));
    QComboBox* networkCombo = new QComboBox;
    networkCombo->setObjectName("networkCombo");
    networkCombo->addItem(tr("Pool-wide network associated with eth0"), "pool-network");
    networkCombo->addItem(tr("Host internal management network"), "host-network");
    networkCombo->addItem(tr("VLAN 100 (production)"), "vlan100");

    // Network mapping (simplified)
    QGroupBox* mappingGroup = new QGroupBox(tr("Network Interface Mapping"));
    QTextEdit* mappingText = new QTextEdit;
    mappingText->setObjectName("networkMappingText");
    mappingText->setMaximumHeight(100);
    mappingText->setPlainText(tr("Network interfaces will be mapped to the selected target network."));
    mappingText->setReadOnly(true);

    QVBoxLayout* mappingLayout = new QVBoxLayout;
    mappingLayout->addWidget(mappingText);
    mappingGroup->setLayout(mappingLayout);

    QVBoxLayout* mainLayout = new QVBoxLayout;

    QFormLayout* networkLayout = new QFormLayout;
    networkLayout->addRow(networkLabel, networkCombo);

    mainLayout->addLayout(networkLayout);
    mainLayout->addWidget(mappingGroup);
    mainLayout->addStretch();

    page->setLayout(mainLayout);
    return page;
}

QWizardPage* ImportWizard::createOptionsPage()
{
    QWizardPage* page = new QWizardPage;
    page->setTitle(tr("Import Options"));
    page->setSubTitle(tr("Configure additional import options."));

    // Security options (for OVF)
    QGroupBox* securityGroup = new QGroupBox(tr("Security Options"));

    QCheckBox* verifyManifest = new QCheckBox(tr("Verify digital signature and manifest"));
    verifyManifest->setObjectName("verifyManifest");
    verifyManifest->setChecked(true);

    QLineEdit* passwordEdit = new QLineEdit;
    passwordEdit->setObjectName("passwordEdit");
    passwordEdit->setPlaceholderText(tr("Password (if required)"));
    passwordEdit->setEchoMode(QLineEdit::Password);

    QVBoxLayout* securityLayout = new QVBoxLayout;
    securityLayout->addWidget(verifyManifest);
    securityLayout->addWidget(passwordEdit);
    securityGroup->setLayout(securityLayout);

    // Import options
    QGroupBox* importGroup = new QGroupBox(tr("Import Options"));

    QCheckBox* runFixups = new QCheckBox(tr("Run operating system fixups"));
    runFixups->setObjectName("runFixups");
    runFixups->setChecked(true);

    QCheckBox* startAfterImport = new QCheckBox(tr("Start VMs automatically after import"));
    startAfterImport->setObjectName("startAfterImport");

    QVBoxLayout* importLayout = new QVBoxLayout;
    importLayout->addWidget(runFixups);
    importLayout->addWidget(startAfterImport);
    importGroup->setLayout(importLayout);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addWidget(securityGroup);
    mainLayout->addWidget(importGroup);
    mainLayout->addStretch();

    page->setLayout(mainLayout);
    return page;
}

QWizardPage* ImportWizard::createFinishPage()
{
    QWizardPage* page = new QWizardPage;
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

void ImportWizard::initializePage(int id)
{
    if (id == Page_Finish)
    {
        // Update summary on finish page
        QTextEdit* summaryText = findChild<QTextEdit*>("summaryText");
        if (summaryText)
        {
            QLineEdit* filePathEdit = findChild<QLineEdit*>("filePathEdit");
            QString filePath = filePathEdit ? filePathEdit->text() : tr("No file selected");
            QString summary;
            summary += tr("Source File: %1\n").arg(filePath);
            summary += tr("Import Type: %1\n").arg(m_importType == ImportType_XVA ? tr("XVA") : m_importType == ImportType_OVF ? tr("OVF")
                                                                                                                               : tr("VHD"));
            summary += tr("Target: %1\n").arg("Selected Server");
            summary += tr("Storage: %1\n").arg("Selected Storage Repository");
            summary += tr("Network: %1\n").arg("Selected Network");

            summaryText->setPlainText(summary);
        }
    }

    QWizard::initializePage(id);
}

bool ImportWizard::validateCurrentPage()
{
    int currentId = this->currentId();

    if (currentId == Page_Source)
    {
        QLineEdit* filePathEdit = findChild<QLineEdit*>("filePathEdit");
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

        // Detect and validate import type
        QString detectedType = detectImportType(filePath);
        if (detectedType.isEmpty())
        {
            QMessageBox::warning(this, tr("Unsupported File"),
                                 tr("The selected file format is not supported."));
            return false;
        }

        // Update UI with detected type
        QLabel* typeLabel = findChild<QLabel*>("typeLabel");
        if (typeLabel)
        {
            typeLabel->setText(tr("Detected: %1").arg(detectedType));
        }
    }

    return QWizard::validateCurrentPage();
}

void ImportWizard::accept()
{
    // Collect all the configuration data
    QLineEdit* filePathEdit = findChild<QLineEdit*>("filePathEdit");
    m_sourceFilePath = filePathEdit ? filePathEdit->text() : QString();
    m_verifyManifest = findChild<QCheckBox*>("verifyManifest")->isChecked();
    m_startVMsAutomatically = findChild<QCheckBox*>("startAfterImport")->isChecked();

    qDebug() << "ImportWizard: Starting import of:" << m_sourceFilePath
             << "Type:" << m_importType << "Start VMs:" << m_startVMsAutomatically;

    performImport();
    QWizard::accept();
}

void ImportWizard::performImport()
{
    // Show progress
    QProgressBar* progressBar = findChild<QProgressBar*>("progressBar");
    QLabel* statusLabel = findChild<QLabel*>("statusLabel");

    if (progressBar && statusLabel)
    {
        progressBar->setVisible(true);
        progressBar->setRange(0, 0); // Indeterminate progress
        statusLabel->setText(tr("Importing virtual machine..."));
    }

    // TODO: Integrate with XenLib to actually perform the import
    // For now, just show a confirmation message

    QString message = tr("Import of '%1' started successfully.").arg(QFileInfo(m_sourceFilePath).fileName());

    if (m_startVMsAutomatically)
    {
        message += tr("\n\nVMs will be started automatically after import completes.");
    }

    QMessageBox::information(this, tr("Import Started"), message);
}

QString ImportWizard::detectImportType(const QString& filePath)
{
    QString suffix = QFileInfo(filePath).suffix().toLower();

    if (suffix == "xva" || filePath.endsWith(".xva.gz", Qt::CaseInsensitive))
    {
        m_importType = ImportType_XVA;
        return tr("XenServer Virtual Appliance (XVA)");
    } else if (suffix == "ovf" || suffix == "ova")
    {
        m_importType = ImportType_OVF;
        return tr("Open Virtualization Format (OVF)");
    } else if (suffix == "vhd" || suffix == "vmdk")
    {
        m_importType = ImportType_VHD;
        return tr("Virtual Hard Disk (VHD)");
    }

    return QString(); // Unsupported type
}

void ImportWizard::onCurrentIdChanged(int id)
{
    Q_UNUSED(id)
    // Handle page changes if needed
}

void ImportWizard::onSourceTypeChanged()
{
    // Handle import type changes if needed
    updateWizardForImportType();
}

void ImportWizard::onBrowseClicked()
{
    QString filter = tr("All Supported Files (*.xva *.xva.gz *.ovf *.ova *.vhd *.vmdk);;"
                        "XVA Files (*.xva *.xva.gz);;"
                        "OVF Files (*.ovf *.ova);;"
                        "VHD Files (*.vhd *.vmdk);;"
                        "All Files (*)");

    QString filePath = QFileDialog::getOpenFileName(this,
                                                    tr("Select File to Import"),
                                                    QString(),
                                                    filter);

    if (!filePath.isEmpty())
    {
        QLineEdit* filePathEdit = findChild<QLineEdit*>("filePathEdit");
        if (filePathEdit)
        {
            filePathEdit->setText(filePath);
        }
    }
}

void ImportWizard::updateWizardForImportType()
{
    // This method could be used to show/hide different pages based on import type
    // For now, we keep it simple and show all pages for all types
}

void ImportWizard::onImportStarted()
{
    // Handle import start
}