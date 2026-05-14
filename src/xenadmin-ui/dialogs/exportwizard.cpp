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

#include "exportwizard.h"
#include "../settingsmanager.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/vm.h"
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>

ExportWizard::ExportWizard(QWidget* parent)
    : QWizard(parent)
    , m_connection(nullptr)
    , m_exportAsXVA(true)
    , m_formatComboBox(nullptr)
    , m_directoryLineEdit(nullptr)
    , m_directoryBrowseButton(nullptr)
    , m_fileNameLineEdit(nullptr)
    , m_vmListWidget(nullptr)
    , m_createManifestCheckBox(nullptr)
    , m_signApplianceCheckBox(nullptr)
    , m_encryptFilesCheckBox(nullptr)
    , m_createOVACheckBox(nullptr)
    , m_compressOVFCheckBox(nullptr)
    , m_verifyExportCheckBox(nullptr)
    , m_summaryTextEdit(nullptr)
{
    this->setWindowTitle(tr("Export Virtual Appliance"));
    this->setWindowIcon(QIcon(":/icons/export-32.png"));
    this->setPixmap(QWizard::LogoPixmap, QPixmap(":/images/export_wizard.png"));
    this->resize(600, 500);

    QString defaultPath = SettingsManager::instance().GetDefaultExportPath();
    if (defaultPath.isEmpty())
        defaultPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (defaultPath.isEmpty())
        defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    this->m_exportDirectory = defaultPath;

    // Add pages
    this->setPage(Page_Format, this->createFormatPage());
    this->setPage(Page_VMs, this->createVMsPage());
    this->setPage(Page_Options, this->createOptionsPage());
    this->setPage(Page_Finish, this->createFinishPage());

    connect(this, &QWizard::currentIdChanged, this, [this](int id) {
        if (id == Page_Finish)
            this->updateSummary();
    });
}

ExportWizard::ExportWizard(XenConnection* connection, QWidget* parent)
    : ExportWizard(parent)
{
    this->m_connection = connection;
}

void ExportWizard::SetConnection(XenConnection* connection)
{
    this->m_connection = connection;
}

void ExportWizard::SetPreselectedVMs(const QList<QSharedPointer<VM>>& vms)
{
    this->m_preselectedVMs = vms;

    // Pre-fill filename from first preselected VM
    if (!vms.isEmpty() && this->m_exportFileName.isEmpty())
    {
        QSharedPointer<VM> first = vms.first();
        if (first && first->IsValid())
        {
            this->m_exportFileName = first->GetName();
            if (this->m_fileNameLineEdit)
                this->m_fileNameLineEdit->setText(this->m_exportFileName);
        }
    }
}

QList<QSharedPointer<VM>> ExportWizard::GetSelectedVMs() const
{
    // XVA: single preselected VM — return it directly
    if (this->m_exportAsXVA)
        return this->m_preselectedVMs;

    // OVF: return VMs checked in the list widget
    QList<QSharedPointer<VM>> result;
    if (!this->m_vmListWidget || !this->m_connection)
        return this->m_preselectedVMs;  // fallback

    XenCache* cache = this->m_connection->GetCache();
    for (int i = 0; i < this->m_vmListWidget->count(); ++i)
    {
        QListWidgetItem* item = this->m_vmListWidget->item(i);
        if (!item || item->checkState() != Qt::Checked)
            continue;
        const QString ref = item->data(Qt::UserRole).toString();
        if (ref.isEmpty())
            continue;
        QSharedPointer<VM> vm = cache->ResolveObject<VM>(XenObjectType::VM, ref);
        if (vm && vm->IsValid())
            result << vm;
    }
    return result;
}

bool ExportWizard::ValidateXvaDestination(QWidget* parent, QString* fullPath) const
{
    if (!fullPath)
        return false;

    const QString directory = this->m_exportDirectory.trimmed();
    const QString fileName  = this->m_exportFileName.trimmed();

    if (directory.isEmpty())
    {
        QMessageBox::warning(parent, tr("Export VM"), tr("Please select an export directory."));
        return false;
    }

    if (fileName.isEmpty())
    {
        QMessageBox::warning(parent, tr("Export VM"), tr("Please enter an export file name."));
        return false;
    }

    if (QDir::isAbsolutePath(fileName) || fileName.contains('/') || fileName.contains('\\') ||
        fileName == "." || fileName == "..")
    {
        QMessageBox::warning(parent, tr("Export VM"), tr("Please enter a file name only, without path separators."));
        return false;
    }

    QDir dir(directory);
    if (!dir.exists())
    {
        const QMessageBox::StandardButton reply = QMessageBox::question(
            parent, tr("Create Export Directory"),
            tr("The export directory does not exist:\n%1\n\nCreate it?").arg(directory),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return false;

        if (!QDir().mkpath(directory))
        {
            QMessageBox::warning(parent, tr("Export VM"),
                                 tr("Could not create the export directory:\n%1").arg(directory));
            return false;
        }
        dir = QDir(directory);
    }

    QString path = dir.filePath(fileName);
    if (!path.toLower().endsWith(".xva"))
        path += ".xva";

    if (QFile::exists(path))
    {
        const QMessageBox::StandardButton reply = QMessageBox::question(
            parent, tr("Overwrite Export"),
            tr("The export file already exists:\n%1\n\nOverwrite it?").arg(path),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return false;
    }

    *fullPath = path;
    return true;
}

void ExportWizard::initializePage(int id)
{
    if (id == Page_VMs)
        this->populateVmList();

    QWizard::initializePage(id);
}

void ExportWizard::populateVmList()
{
    if (!this->m_vmListWidget)
        return;

    this->m_vmListWidget->clear();

    // Collect preselected refs for fast lookup
    QSet<QString> preselectedRefs;
    for (const QSharedPointer<VM>& vm : this->m_preselectedVMs)
    {
        if (vm && vm->IsValid())
            preselectedRefs.insert(vm->OpaqueRef());
    }

    if (this->m_connection)
    {
        XenCache* cache = this->m_connection->GetCache();
        const QList<QSharedPointer<VM>> vms = cache->GetAll<VM>();
        for (const QSharedPointer<VM>& vm : vms)
        {
            if (!vm || !vm->IsValid())
                continue;
            if (vm->IsControlDomain())
                continue;
            if (vm->IsTemplate() || vm->IsSnapshot())
                continue;
            if (!vm->GetAllowedOperations().contains("export"))
                continue;

            QListWidgetItem* item = new QListWidgetItem(vm->GetName());
            item->setData(Qt::UserRole, vm->OpaqueRef());
            item->setCheckState(preselectedRefs.contains(vm->OpaqueRef()) ? Qt::Checked : Qt::Unchecked);
            this->m_vmListWidget->addItem(item);
        }
    }

    if (this->m_vmListWidget->count() == 0)
    {
        // Fallback: show preselected VMs even when cache isn't available
        for (const QSharedPointer<VM>& vm : this->m_preselectedVMs)
        {
            if (!vm || !vm->IsValid())
                continue;
            QListWidgetItem* item = new QListWidgetItem(vm->GetName());
            item->setData(Qt::UserRole, vm->OpaqueRef());
            item->setCheckState(Qt::Checked);
            this->m_vmListWidget->addItem(item);
        }
    }

    if (this->m_vmListWidget->count() == 0)
    {
        QListWidgetItem* placeholder = new QListWidgetItem(tr("(no exportable VMs found)"));
        placeholder->setFlags(Qt::NoItemFlags);
        this->m_vmListWidget->addItem(placeholder);
    }
}

QWizardPage* ExportWizard::createFormatPage()
{
    QWizardPage* page = new QWizardPage;
    page->setTitle(tr("Format and Destination"));
    page->setSubTitle(tr("Select the export format and destination for your virtual machines."));

    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout;

    // Format selection group
    QGroupBox* formatGroup = new QGroupBox(tr("Export Format"));
    QVBoxLayout* formatLayout = new QVBoxLayout;

    this->m_formatComboBox = new QComboBox;
    this->m_formatComboBox->addItem(tr("XVA Package (.xva)"), true);
    this->m_formatComboBox->addItem(tr("OVF/OVA Package (.ovf)"), false);
    this->m_formatComboBox->setCurrentIndex(0);

    connect(this->m_formatComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ExportWizard::onFormatChanged);

    formatLayout->addWidget(new QLabel(tr("Format:")));
    formatLayout->addWidget(this->m_formatComboBox);
    formatGroup->setLayout(formatLayout);

    // Destination group
    QGroupBox* destinationGroup = new QGroupBox(tr("Export Destination"));
    QFormLayout* destinationLayout = new QFormLayout;

    // Directory selection
    QHBoxLayout* dirLayout = new QHBoxLayout;
    this->m_directoryLineEdit = new QLineEdit;
    this->m_directoryLineEdit->setText(this->m_exportDirectory);
    this->m_directoryBrowseButton = new QPushButton(tr("Browse..."));

    connect(this->m_directoryBrowseButton, &QPushButton::clicked,
            this, &ExportWizard::onDirectoryBrowse);
    connect(this->m_directoryLineEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        this->m_exportDirectory = text;
    });

    dirLayout->addWidget(this->m_directoryLineEdit);
    dirLayout->addWidget(this->m_directoryBrowseButton);

    // File name
    this->m_fileNameLineEdit = new QLineEdit;
    this->m_fileNameLineEdit->setPlaceholderText(tr("appliance"));
    if (!this->m_exportFileName.isEmpty())
        this->m_fileNameLineEdit->setText(this->m_exportFileName);
    connect(this->m_fileNameLineEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        this->m_exportFileName = text;
    });

    destinationLayout->addRow(tr("Directory:"), dirLayout);
    destinationLayout->addRow(tr("File name:"), this->m_fileNameLineEdit);
    destinationGroup->setLayout(destinationLayout);

    mainLayout->addWidget(formatGroup);
    mainLayout->addWidget(destinationGroup);
    mainLayout->addStretch();

    page->setLayout(mainLayout);
    return page;
}

QWizardPage* ExportWizard::createVMsPage()
{
    QWizardPage* page = new QWizardPage;
    page->setTitle(tr("Virtual Machines"));
    page->setSubTitle(tr("Select the virtual machines to export."));

    QVBoxLayout* layout = new QVBoxLayout;

    QLabel* instructionLabel = new QLabel(tr("Select the virtual machines you want to export:"));
    layout->addWidget(instructionLabel);

    this->m_vmListWidget = new QListWidget;
    this->m_vmListWidget->setSelectionMode(QAbstractItemView::NoSelection);
    layout->addWidget(this->m_vmListWidget);

    // Select / deselect all buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    QPushButton* selectAllButton   = new QPushButton(tr("Select All"));
    QPushButton* deselectAllButton = new QPushButton(tr("Deselect All"));

    connect(selectAllButton, &QPushButton::clicked, [this]() {
        for (int i = 0; i < this->m_vmListWidget->count(); ++i)
        {
            QListWidgetItem* item = this->m_vmListWidget->item(i);
            if (item->flags() & Qt::ItemIsEnabled)
                item->setCheckState(Qt::Checked);
        }
    });

    connect(deselectAllButton, &QPushButton::clicked, [this]() {
        for (int i = 0; i < this->m_vmListWidget->count(); ++i)
        {
            QListWidgetItem* item = this->m_vmListWidget->item(i);
            if (item->flags() & Qt::ItemIsEnabled)
                item->setCheckState(Qt::Unchecked);
        }
    });

    buttonLayout->addWidget(selectAllButton);
    buttonLayout->addWidget(deselectAllButton);
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);

    page->setLayout(layout);
    return page;
}

QWizardPage* ExportWizard::createOptionsPage()
{
    QWizardPage* page = new QWizardPage;
    page->setTitle(tr("Export Options"));
    page->setSubTitle(tr("Configure additional export options."));

    QVBoxLayout* layout = new QVBoxLayout;

    // OVF-specific options group
    QGroupBox* ovfGroup = new QGroupBox(tr("OVF Options"));
    QVBoxLayout* ovfLayout = new QVBoxLayout;

    this->m_createManifestCheckBox = new QCheckBox(tr("Create manifest"));
    this->m_createManifestCheckBox->setToolTip(tr("Create a manifest file (.mf) to verify package integrity"));
    this->m_createManifestCheckBox->setChecked(true);

    this->m_signApplianceCheckBox = new QCheckBox(tr("Sign appliance"));
    this->m_signApplianceCheckBox->setToolTip(tr("Digitally sign the appliance for verification"));

    this->m_encryptFilesCheckBox = new QCheckBox(tr("Encrypt files"));
    this->m_encryptFilesCheckBox->setToolTip(tr("Encrypt the exported files"));

    this->m_createOVACheckBox = new QCheckBox(tr("Create OVA package"));
    this->m_createOVACheckBox->setToolTip(tr("Package all files into a single OVA file"));

    this->m_compressOVFCheckBox = new QCheckBox(tr("Compress OVF files"));
    this->m_compressOVFCheckBox->setToolTip(tr("Compress the OVF files to reduce size"));
    this->m_compressOVFCheckBox->setChecked(true);

    ovfLayout->addWidget(this->m_createManifestCheckBox);
    ovfLayout->addWidget(this->m_signApplianceCheckBox);
    ovfLayout->addWidget(this->m_encryptFilesCheckBox);
    ovfLayout->addWidget(this->m_createOVACheckBox);
    ovfLayout->addWidget(this->m_compressOVFCheckBox);
    ovfGroup->setLayout(ovfLayout);
    ovfGroup->setVisible(!this->m_exportAsXVA);
    ovfGroup->setObjectName("ovfGroup");

    // General options group
    QGroupBox* generalGroup = new QGroupBox(tr("General Options"));
    QVBoxLayout* generalLayout = new QVBoxLayout;

    this->m_verifyExportCheckBox = new QCheckBox(tr("Verify export on completion"));
    this->m_verifyExportCheckBox->setToolTip(tr("Verify the exported files after completion"));
    this->m_verifyExportCheckBox->setChecked(true);

    generalLayout->addWidget(this->m_verifyExportCheckBox);
    generalGroup->setLayout(generalLayout);

    layout->addWidget(ovfGroup);
    layout->addWidget(generalGroup);
    layout->addStretch();

    page->setLayout(layout);
    return page;
}

QWizardPage* ExportWizard::createFinishPage()
{
    QWizardPage* page = new QWizardPage;
    page->setTitle(tr("Summary"));
    page->setSubTitle(tr("Review your export settings."));
    page->setFinalPage(true);

    QVBoxLayout* layout = new QVBoxLayout;

    QLabel* reviewLabel = new QLabel(tr("Please review the settings below and click Finish to start the export:"));
    layout->addWidget(reviewLabel);

    this->m_summaryTextEdit = new QPlainTextEdit;
    this->m_summaryTextEdit->setReadOnly(true);
    layout->addWidget(this->m_summaryTextEdit);

    page->setLayout(layout);
    return page;
}

void ExportWizard::onFormatChanged()
{
    this->m_exportAsXVA = this->m_formatComboBox->currentData().toBool();

    if (this->m_exportAsXVA)
        this->setWindowTitle(tr("Export VM as XVA"));
    else
        this->setWindowTitle(tr("Export Virtual Appliance"));

    // Show/hide OVF-specific options on the options page
    if (this->currentId() == Page_Options)
    {
        QGroupBox* ovfGroup = this->page(Page_Options)->findChild<QGroupBox*>("ovfGroup");
        if (ovfGroup)
            ovfGroup->setVisible(!this->m_exportAsXVA);
    }
}

void ExportWizard::onDirectoryBrowse()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Export Directory"),
        this->m_directoryLineEdit->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty())
    {
        this->m_directoryLineEdit->setText(dir);
        this->m_exportDirectory = dir;
    }
}

int ExportWizard::nextId() const
{
    switch (this->currentId())
    {
        case Page_Format:
            // XVA exports a single preselected VM — skip VM selection page
            return this->m_exportAsXVA ? Page_Options : Page_VMs;
        case Page_VMs:
            return Page_Options;
        case Page_Options:
            return Page_Finish;
        default:
            return -1;
    }
}

void ExportWizard::updateSummary()
{
    QString summary;

    summary += tr("Export Format: %1\n").arg(this->m_exportAsXVA ? tr("XVA Package") : tr("OVF Package"));
    summary += tr("Destination: %1\n").arg(this->m_exportDirectory);
    summary += tr("File Name: %1\n\n").arg(this->m_exportFileName);

    // VM list
    const QList<QSharedPointer<VM>> selectedVMs = this->GetSelectedVMs();
    if (!selectedVMs.isEmpty())
    {
        summary += tr("Virtual Machines:\n");
        for (const QSharedPointer<VM>& vm : selectedVMs)
        {
            if (vm && vm->IsValid())
                summary += tr("  \u2022 %1\n").arg(vm->GetName());
        }
        summary += "\n";
    } else if (!this->m_selectedObjectName.isEmpty())
    {
        summary += tr("Virtual Machine: %1\n\n").arg(this->m_selectedObjectName);
    }

    // Options (OVF only)
    if (!this->m_exportAsXVA && this->m_createManifestCheckBox)
    {
        summary += tr("Export Options:\n");
        summary += tr("  \u2022 Create manifest: %1\n").arg(this->m_createManifestCheckBox->isChecked() ? tr("Yes") : tr("No"));
        summary += tr("  \u2022 Sign appliance: %1\n").arg(this->m_signApplianceCheckBox->isChecked() ? tr("Yes") : tr("No"));
        summary += tr("  \u2022 Encrypt files: %1\n").arg(this->m_encryptFilesCheckBox->isChecked() ? tr("Yes") : tr("No"));
        summary += tr("  \u2022 Create OVA package: %1\n").arg(this->m_createOVACheckBox->isChecked() ? tr("Yes") : tr("No"));
        summary += tr("  \u2022 Compress files: %1\n").arg(this->m_compressOVFCheckBox->isChecked() ? tr("Yes") : tr("No"));
        summary += "\n";
    }

    if (this->m_verifyExportCheckBox)
    {
        summary += tr("General Options:\n");
        summary += tr("  \u2022 Verify export: %1\n").arg(this->m_verifyExportCheckBox->isChecked() ? tr("Yes") : tr("No"));
    }

    if (this->m_summaryTextEdit)
        this->m_summaryTextEdit->setPlainText(summary);
}

