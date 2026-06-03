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
#include "ui_exportwizard.h"
#include "../settingsmanager.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/session.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/vm.h"
#include <QListWidget>
#include <QPushButton>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QStorageInfo>

ExportWizard::ExportWizard(QWidget* parent)
    : QWizard(parent)
    , ui(new Ui::ExportWizard)
    , m_connection(nullptr)
    , m_exportAsXVA(true)
    , m_eulaListWidget(nullptr)
{
    QString defaultPath = SettingsManager::instance().GetDefaultExportPath();
    if (defaultPath.isEmpty())
        defaultPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (defaultPath.isEmpty())
        defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    this->m_exportDirectory = defaultPath;

    this->ui->setupUi(this);
    this->setupWizardPages();
}

ExportWizard::ExportWizard(XenConnection* connection, QWidget* parent)
    : ExportWizard(parent)
{
    this->m_connection = connection;
}

ExportWizard::~ExportWizard()
{
    delete this->ui;
}

void ExportWizard::setupWizardPages()
{
    // setupUi() registers the .ui QWizardPage children in enum order:
    // Page_Format, Page_VMs, Page_Options, Page_Finish, Page_Rbac, Page_Eula.
    this->m_eulaListWidget = this->ui->eulaListWidget;

    connect(this->ui->addEulaButton, &QPushButton::clicked, this, &ExportWizard::onAddEula);
    connect(this->ui->removeEulaButton, &QPushButton::clicked, this, &ExportWizard::onRemoveEula);

    this->ui->pageFinish->setFinalPage(true);

    // Populate format combo items (item data drives XVA vs OVF logic)
    this->ui->formatComboBox->addItem(tr("XVA Package (.xva)"), true);
    this->ui->formatComboBox->addItem(tr("OVF/OVA Package (.ovf)"), false);
    this->ui->formatComboBox->setCurrentIndex(0);

    // Pre-fill directory from stored default
    this->ui->directoryLineEdit->setText(this->m_exportDirectory);

    connect(this->ui->formatComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ExportWizard::onFormatChanged);
    connect(this->ui->directoryBrowseButton, &QPushButton::clicked,
            this, &ExportWizard::onDirectoryBrowse);
    connect(this->ui->directoryLineEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        this->m_exportDirectory = text;
    });
    connect(this->ui->fileNameLineEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        this->m_exportFileName = text;
    });
    connect(this->ui->selectAllButton, &QPushButton::clicked, [this]() {
        for (int i = 0; i < this->ui->vmListWidget->count(); ++i)
        {
            QListWidgetItem* item = this->ui->vmListWidget->item(i);
            if (item->flags() & Qt::ItemIsEnabled)
                item->setCheckState(Qt::Checked);
        }
    });
    connect(this->ui->deselectAllButton, &QPushButton::clicked, [this]() {
        for (int i = 0; i < this->ui->vmListWidget->count(); ++i)
        {
            QListWidgetItem* item = this->ui->vmListWidget->item(i);
            if (item->flags() & Qt::ItemIsEnabled)
                item->setCheckState(Qt::Unchecked);
        }
    });

    // OVF options page: sign / manifest linkage, cert browse.
    // Matches C# ExportOptionsPage checkbox-changed handlers.
    connect(this->ui->certBrowseButton, &QPushButton::clicked,
            this, &ExportWizard::onCertBrowse);
    connect(this->ui->signApplianceCheckBox, &QCheckBox::toggled,
            this, &ExportWizard::onSignApplianceToggled);
    connect(this->ui->createManifestCheckBox, &QCheckBox::toggled,
            this, &ExportWizard::onManifestToggled);
}

void ExportWizard::SetConnection(XenConnection* connection)
{
    this->m_connection = connection;
}

void ExportWizard::SetOvfModeOnly()
{
    // Matches C# ExportAppliancePage.OvfModeOnly:
    // Show only the OVF/OVA option — XVA is not valid for vApp export.
    // Remove the XVA item (index 0) from the combo and select the OVF item.
    const int xvaIndex = this->ui->formatComboBox->findData(true); // data=true means XVA
    if (xvaIndex >= 0)
        this->ui->formatComboBox->removeItem(xvaIndex);
    this->ui->formatComboBox->setEnabled(false);
    this->m_exportAsXVA = false;
    this->setWindowTitle(tr("Export Virtual Appliance"));
    this->ui->ovfGroup->setVisible(true);
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
            this->ui->fileNameLineEdit->setText(this->m_exportFileName);
        }
    }
}

bool ExportWizard::verifyExport() const
{
    return this->ui->verifyExportCheckBox->isChecked();
}

bool ExportWizard::createManifest() const
{
    return this->ui->createManifestCheckBox->isChecked();
}

bool ExportWizard::createOva() const
{
    return this->ui->createOVACheckBox->isChecked();
}

bool ExportWizard::compressOVF() const
{
    return this->ui->compressOVFCheckBox->isChecked();
}

bool ExportWizard::signAppliance() const
{
    return this->ui->signApplianceCheckBox->isChecked();
}

void ExportWizard::setExportFileName(const QString& fileName)
{
    this->m_exportFileName = fileName;
    this->ui->fileNameLineEdit->setText(fileName);
}

QList<QSharedPointer<VM>> ExportWizard::GetSelectedVMs() const
{
    // XVA: single preselected VM — return it directly
    if (this->m_exportAsXVA)
        return this->m_preselectedVMs;

    // OVF: return VMs checked in the list widget
    QList<QSharedPointer<VM>> result;
    if (!this->m_connection)
        return this->m_preselectedVMs;  // fallback

    XenCache* cache = this->m_connection->GetCache();
    for (int i = 0; i < this->ui->vmListWidget->count(); ++i)
    {
        QListWidgetItem* item = this->ui->vmListWidget->item(i);
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

QStringList ExportWizard::GetEulas() const
{
    // Matches C# ExportEulaPage.Eulas: return the list of EULA file paths.
    QStringList result;
    if (!this->m_eulaListWidget)
        return result;
    for (int i = 0; i < this->m_eulaListWidget->count(); ++i)
    {
        QListWidgetItem* item = this->m_eulaListWidget->item(i);
        if (item)
            result << item->text();
    }
    return result;
}

QString ExportWizard::GetCertificatePath() const
{
    return this->ui->certPathLineEdit->text().trimmed();
}

QString ExportWizard::GetCertificatePassword() const
{
    return this->ui->certPasswordLineEdit->text();
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

    // Check write permission: attempt to create/delete a probe file (matches C# CheckPermissions).
    {
        const QString probeFile = dir.filePath("_xenadmin_probe_");
        QFile f(probeFile);
        if (!f.open(QIODevice::WriteOnly))
        {
            QMessageBox::warning(parent, tr("Export VM"),
                                 tr("You do not have write permission to the export directory:\n%1").arg(directory));
            return false;
        }
        f.close();
        f.remove();
    }

    // Check available disk space (warn if low — we can't know exact VM size upfront,
    // but an obviously-full disk should be flagged before a long download starts)
    const QStorageInfo storageInfo(dir.absolutePath());
    if (storageInfo.isValid() && storageInfo.isReady())
    {
        const qint64 available = storageInfo.bytesAvailable();
        // Warn if less than 100 MiB free (a full XVA is typically several GiB)
        static const qint64 MIN_FREE_BYTES = Q_INT64_C(100) * 1024 * 1024;
        if (available < MIN_FREE_BYTES)
        {
            const QMessageBox::StandardButton reply = QMessageBox::warning(
                parent, tr("Disk Space Warning"),
                tr("The destination volume has very little free space (%1 MB available).\n\n"
                   "Export files are typically several gigabytes. Continue anyway?")
                    .arg(available / (1024 * 1024)),
                QMessageBox::Yes | QMessageBox::No);
            if (reply != QMessageBox::Yes)
                return false;
        }
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

bool ExportWizard::ValidateOvfDestination(QWidget* parent, QString* resolvedPath) const
{
    if (!resolvedPath)
        return false;

    const QString directory = this->m_exportDirectory.trimmed();
    const QString fileName  = this->m_exportFileName.trimmed();

    if (directory.isEmpty())
    {
        QMessageBox::warning(parent, tr("Export Appliance"), tr("Please select a destination directory."));
        return false;
    }

    if (fileName.isEmpty())
    {
        QMessageBox::warning(parent, tr("Export Appliance"), tr("Please enter an appliance name."));
        return false;
    }

    if (fileName.contains('/') || fileName.contains('\\') || fileName == "." || fileName == "..")
    {
        QMessageBox::warning(parent, tr("Export Appliance"),
                             tr("Please enter an appliance name without path separators."));
        return false;
    }

    // Destination directory must already exist (C# CheckDestinationFolderExists).
    QDir dir(directory);
    if (!dir.exists())
    {
        QMessageBox::warning(parent, tr("Export Appliance"),
                             tr("The destination directory does not exist:\n%1").arg(directory));
        return false;
    }

    // Check write permission via touch file (C# CheckPermissions).
    {
        const QString probeFile = dir.filePath("_xenadmin_probe_");
        QFile f(probeFile);
        if (!f.open(QIODevice::WriteOnly))
        {
            QMessageBox::warning(parent, tr("Export Appliance"),
                                 tr("You do not have write permission to the destination directory:\n%1").arg(directory));
            return false;
        }
        f.close();
        f.remove();
    }

    // Build primary descriptor path, stripping any user-supplied extension first.
    QString baseName = fileName;
    if (baseName.toLower().endsWith(".ovf") || baseName.toLower().endsWith(".ova"))
        baseName = baseName.left(baseName.length() - 4);

    // Determine extension from format combo (OVF vs OVA).
    const bool isOva = !this->m_exportAsXVA &&
                       this->ui->formatComboBox->currentData().toString().toLower().contains("ova");
    const QString ext = isOva ? ".ova" : ".ovf";
    const QString primaryPath = dir.filePath(baseName + ext);

    // Check for an existing OVA file or OVF sub-folder (C# CheckApplianceExists).
    // For OVF the action creates a sub-folder dir/name/; for OVA it writes dir/name.ova.
    bool alreadyExists = QFile::exists(dir.filePath(baseName + ".ova")) ||
                         QDir(dir.filePath(baseName)).exists();
    if (alreadyExists)
    {
        const QMessageBox::StandardButton reply = QMessageBox::question(
            parent, tr("Overwrite Appliance"),
            tr("An appliance named '%1' already exists in:\n%2\n\nOverwrite it?")
                .arg(baseName, directory),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return false;
    }

    *resolvedPath = primaryPath;
    return true;
}

void ExportWizard::initializePage(int id)
{
    if (id == Page_VMs)
        this->populateVmList();
    else if (id == Page_Rbac)
        this->updateRbacPage();
    else if (id == Page_Finish)
        this->updateSummary();

    QWizard::initializePage(id);
}

void ExportWizard::populateVmList()
{
    this->ui->vmListWidget->clear();

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
            // OVF export requires Halted or Suspended; XVA export accepts any power state.
            if (this->m_exportAsXVA ? !vm->IsXvaExportable() : !vm->IsOvfExportable())
                continue;

            QListWidgetItem* item = new QListWidgetItem(vm->GetName());
            item->setData(Qt::UserRole, vm->OpaqueRef());
            item->setCheckState(preselectedRefs.contains(vm->OpaqueRef()) ? Qt::Checked : Qt::Unchecked);
            this->ui->vmListWidget->addItem(item);
        }
    }

    if (this->ui->vmListWidget->count() == 0)
    {
        // Fallback: show preselected VMs even when cache isn't available
        for (const QSharedPointer<VM>& vm : this->m_preselectedVMs)
        {
            if (!vm || !vm->IsValid())
                continue;
            QListWidgetItem* item = new QListWidgetItem(vm->GetName());
            item->setData(Qt::UserRole, vm->OpaqueRef());
            item->setCheckState(Qt::Checked);
            this->ui->vmListWidget->addItem(item);
        }
    }

    if (this->ui->vmListWidget->count() == 0)
    {
        QListWidgetItem* placeholder = new QListWidgetItem(tr("(no exportable VMs found)"));
        placeholder->setFlags(Qt::NoItemFlags);
        this->ui->vmListWidget->addItem(placeholder);
    }
}

void ExportWizard::onAddEula()
{
    if (!this->m_eulaListWidget)
        return;

    // Matches C# MAX_EULA_DOCS = 25.
    if (this->m_eulaListWidget->count() >= 25)
    {
        QMessageBox::information(this, tr("Add EULA"),
                                 tr("A maximum of 25 EULA documents may be added."));
        return;
    }

    const QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("Select EULA File"),
        QString(),
        tr("Text Files (*.txt *.rtf);;All Files (*)"));

    for (const QString& f : files)
    {
        if (f.isEmpty())
            continue;
        // Don't add duplicates.
        if (this->m_eulaListWidget->findItems(f, Qt::MatchExactly).isEmpty())
            this->m_eulaListWidget->addItem(f);
    }
}

void ExportWizard::onRemoveEula()
{
    if (!this->m_eulaListWidget)
        return;
    const QList<QListWidgetItem*> selected = this->m_eulaListWidget->selectedItems();
    for (QListWidgetItem* item : selected)
        delete this->m_eulaListWidget->takeItem(this->m_eulaListWidget->row(item));
}

void ExportWizard::onCertBrowse()
{
    const QString file = QFileDialog::getOpenFileName(
        this,
        tr("Select Certificate"),
        QString(),
        tr("Certificate Files (*.pem *.pfx *.p12 *.crt *.cer);;All Files (*)"));
    if (!file.isEmpty())
        this->ui->certPathLineEdit->setText(file);
}

void ExportWizard::onSignApplianceToggled(bool checked)
{
    this->ui->certWidget->setVisible(checked);
    // Signing requires a manifest — auto-check manifest when signing is enabled.
    // Matches C# m_checkBoxSign_CheckedChanged.
    if (checked)
        this->ui->createManifestCheckBox->setChecked(true);
}

void ExportWizard::onManifestToggled(bool checked)
{
    // Unchecking manifest forces sign off (can't sign without a manifest).
    // Matches C# m_checkBoxManifest_CheckedChanged.
    if (!checked)
        this->ui->signApplianceCheckBox->setChecked(false);
}

void ExportWizard::onFormatChanged()
{
    this->m_exportAsXVA = this->ui->formatComboBox->currentData().toBool();

    if (this->m_exportAsXVA)
        this->setWindowTitle(tr("Export VM as XVA"));
    else
        this->setWindowTitle(tr("Export Virtual Appliance"));

    // Show/hide OVF-specific options on the options page
    this->ui->ovfGroup->setVisible(!this->m_exportAsXVA);
}

void ExportWizard::onDirectoryBrowse()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Export Directory"),
        this->ui->directoryLineEdit->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty())
    {
        this->ui->directoryLineEdit->setText(dir);
        // m_exportDirectory is kept in sync via the textChanged signal
    }
}

bool ExportWizard::validateCurrentPage()
{
    // VM selection page (OVF only): require at least one VM checked.
    // Matches C#: m_buttonNextEnabled = ExportAsXva ? count == 1 : count > 0
    if (this->currentId() == Page_VMs)
    {
        int checkedCount = 0;
        for (int i = 0; i < this->ui->vmListWidget->count(); ++i)
        {
            const QListWidgetItem* item = this->ui->vmListWidget->item(i);
            if (item && (item->flags() & Qt::ItemIsEnabled) && item->checkState() == Qt::Checked)
                ++checkedCount;
        }
        if (checkedCount == 0)
        {
            QMessageBox::warning(this, tr("Export Virtual Appliance"),
                                 tr("Please select at least one virtual machine to export."));
            return false;
        }
    }

    // Format page: lightweight destination validation + RBAC check.
    // Matches C# ExportAppliancePage.EnableNext() guards and AddRbacPage().
    if (this->currentId() == Page_Format)
    {
        const QString directory = this->m_exportDirectory.trimmed();
        const QString fileName  = this->m_exportFileName.trimmed();

        if (directory.isEmpty())
        {
            QMessageBox::warning(this, tr("Export"), tr("Please select a destination directory."));
            return false;
        }

        if (fileName.isEmpty())
        {
            QMessageBox::warning(this, tr("Export"),
                                 this->m_exportAsXVA ? tr("Please enter an export file name.")
                                                     : tr("Please enter an appliance name."));
            return false;
        }

        // RBAC check. XVA: task.create + http/get_export (ExportVmAction.StaticRBACDependencies).
        // OVF/OVA: ApplianceAction.StaticRBACDependencies (C# uses this for appliance export).
        QStringList required;
        if (this->m_exportAsXVA)
        {
            required << "task.create" << "http/get_export";
        } else
        {
            // Matches ApplianceAction.StaticRBACDependencies from C#.
            required << "VM.add_to_other_config"
                     << "VM.create"
                     << "VM.destroy"
                     << "VM.hard_shutdown"
                     << "VM.remove_from_other_config"
                     << "VM.set_HVM_boot_params"
                     << "VM.start"
                     << "VDI.create"
                     << "VDI.destroy"
                     << "VBD.create"
                     << "VBD.eject"
                     << "VIF.create"
                     << "Host.call_plugin";
        }
        this->hasApiPermissions(required, &this->m_blockingRbacMissing);
        // Only show RBAC page when permissions are actually missing (saves a click for superusers).
    }

    // RBAC page: block if any required permission is missing.
    if (this->currentId() == Page_Rbac)
    {
        if (!this->m_blockingRbacMissing.isEmpty())
            return false;
    }

    // Options page: validate certificate and encryption when enabled.
    // Matches C# ExportOptionsPage.CheckSignature() + CheckEncryption().
    if (this->currentId() == Page_Options)
    {
        if (this->ui->signApplianceCheckBox->isChecked())
        {
            const QString certPath = this->ui->certPathLineEdit->text().trimmed();
            if (certPath.isEmpty())
            {
                QMessageBox::warning(this, tr("Sign Appliance"),
                                     tr("Please select a certificate file for signing."));
                return false;
            }
            if (!QFile::exists(certPath))
            {
                QMessageBox::warning(this, tr("Sign Appliance"),
                                     tr("The certificate file does not exist:\n%1").arg(certPath));
                return false;
            }
        }

    }

    return QWizard::validateCurrentPage();
}

int ExportWizard::nextId() const
{
    switch (this->currentId())
    {
        case Page_Format:
            // If any required permissions are missing, show the RBAC warning page.
            // Matches C# AddRbacPage() inserted after ExportAppliancePage.
            if (!this->m_blockingRbacMissing.isEmpty())
                return Page_Rbac;
            // XVA exports a single preselected VM — skip VM selection page
            return this->m_exportAsXVA ? Page_Options : Page_VMs;
        case Page_Rbac:
            // Continue after RBAC page (only reachable if permissions are present)
            return this->m_exportAsXVA ? Page_Options : Page_VMs;
        case Page_VMs:
            // OVF/OVA: after VM selection show the EULA page (C# ExportEulaPage)
            return Page_Eula;
        case Page_Eula:
            return Page_Options;
        case Page_Options:
            return Page_Finish;
        default:
            return -1;
    }
}

void ExportWizard::updateSummary()
{
    // Matches C# ExportFinishPage.SummaryRetriever: format, target path, VMs, options.
    QString summary;

    summary += tr("Export Format: %1\n").arg(this->m_exportAsXVA ? tr("XVA Package") : tr("OVF Package"));

    // Full resolved output path (matches C# finish page showing applianceDirectory + name).
    const QString dir  = this->m_exportDirectory.trimmed();
    const QString name = this->m_exportFileName.trimmed();
    if (!dir.isEmpty() && !name.isEmpty())
    {
        QString fullPath = QDir(dir).filePath(name);
        if (this->m_exportAsXVA)
        {
            if (!fullPath.toLower().endsWith(".xva"))
                fullPath += ".xva";
        } else
        {
            const bool isOva = this->ui->formatComboBox->currentData().toString().toLower().contains("ova");
            if (isOva)
            {
                // OVA: single archive file at dir/name.ova
                if (fullPath.toLower().endsWith(".ovf") || fullPath.toLower().endsWith(".ova"))
                    fullPath = fullPath.left(fullPath.length() - 4);
                fullPath += ".ova";
            } else
            {
                // OVF: package written into a sub-folder dir/name/ — show the folder path
                // so the user knows where to look.  Matches C# finish page showing
                // applianceDirectory (the parent dir) + name (the sub-folder).
                if (fullPath.toLower().endsWith(".ovf") || fullPath.toLower().endsWith(".ova"))
                    fullPath = fullPath.left(fullPath.length() - 4);
                fullPath = fullPath + QDir::separator();
            }
        }
        summary += tr("Target: %1\n\n").arg(fullPath);
    } else
    {
        summary += tr("Destination: %1\n").arg(dir);
        summary += tr("File Name: %1\n\n").arg(name);
    }

    // VM list with power state (matches C# ExportFinishPage VM rows).
    const QList<QSharedPointer<VM>> selectedVMs = this->GetSelectedVMs();
    if (!selectedVMs.isEmpty())
    {
        summary += tr("Virtual Machines:\n");
        for (const QSharedPointer<VM>& vm : selectedVMs)
        {
            if (vm && vm->IsValid())
            {
                const QString ps = vm->GetPowerState();
                summary += tr("  \u2022 %1 (%2)\n").arg(vm->GetName(), ps.isEmpty() ? tr("Unknown") : ps);
            }
        }
        summary += "\n";
    } else if (!this->m_selectedObjectName.isEmpty())
    {
        summary += tr("Virtual Machine: %1\n\n").arg(this->m_selectedObjectName);
    }

    // EULAs (OVF only, matches C# ExportFinishPage EULA summary row)
    if (!this->m_exportAsXVA && this->m_eulaListWidget)
    {
        const int eulaCount = this->m_eulaListWidget->count();
        if (eulaCount > 0)
        {
            summary += tr("EULAs:\n");
            for (int i = 0; i < eulaCount; ++i)
            {
                QListWidgetItem* item = this->m_eulaListWidget->item(i);
                if (item)
                    summary += tr("  \u2022 %1\n").arg(QFileInfo(item->text()).fileName());
            }
            summary += "\n";
        }
    }

    // Options (OVF only)
    if (!this->m_exportAsXVA)
    {
        summary += tr("Export Options:\n");
        summary += tr("  \u2022 Create manifest: %1\n").arg(this->ui->createManifestCheckBox->isChecked() ? tr("Yes") : tr("No"));

        if (this->ui->signApplianceCheckBox->isChecked())
        {
            const QString certName = QFileInfo(this->ui->certPathLineEdit->text().trimmed()).fileName();
            summary += tr("  \u2022 Sign appliance: Yes (%1)\n").arg(certName.isEmpty() ? tr("no certificate selected") : certName);
        } else
        {
            summary += tr("  \u2022 Sign appliance: No\n");
        }

        summary += tr("  \u2022 Create OVA package: %1\n").arg(this->ui->createOVACheckBox->isChecked() ? tr("Yes") : tr("No"));
        summary += tr("  \u2022 Compress files: %1\n").arg(this->ui->compressOVFCheckBox->isChecked() ? tr("Yes") : tr("No"));
        summary += "\n";
    }

    summary += tr("General Options:\n");
    summary += tr("  \u2022 Verify export: %1\n").arg(this->ui->verifyExportCheckBox->isChecked() ? tr("Yes") : tr("No"));

    this->ui->summaryTextEdit->setPlainText(summary);
}

bool ExportWizard::hasApiPermissions(const QStringList& methods, QStringList* missing) const
{
    if (missing)
        missing->clear();
    if (!this->m_connection || !this->m_connection->GetSession())
        return true;   // No session — can't check; let the action fail naturally

    XenAPI::Session* session = this->m_connection->GetSession();
    if (session->IsLocalSuperuser())
        return true;

    const QStringList permissions = session->GetPermissions();
    if (permissions.isEmpty())
        return true;   // Empty list means we couldn't fetch permissions; assume OK

    QSet<QString> permSet;
    for (const QString& p : permissions)
        permSet.insert(p.toLower());

    QStringList missingMethods;
    for (const QString& method : methods)
    {
        if (!permSet.contains(method.toLower()))
            missingMethods << method;
    }

    if (missing)
        *missing = missingMethods;
    return missingMethods.isEmpty();
}

void ExportWizard::updateRbacPage()
{
    if (!this->ui->rbacWarningLabel)
        return;

    QStringList lines;
    if (!this->m_blockingRbacMissing.isEmpty())
    {
        lines << tr("Your current role is missing permissions required to export:");
        for (const QString& method : this->m_blockingRbacMissing)
            lines << tr("  \u2022 %1").arg(method);
        lines << QString();
        lines << tr("The export cannot continue with the current role.");
    } else
    {
        // All permissions present — show confirmation
        const QStringList methods = { "task.create", "http/get_export" };
        lines << tr("The following permissions are required for this export:");
        for (const QString& method : methods)
            lines << tr("  \u2713 %1").arg(method);
    }

    this->ui->rbacWarningLabel->setText(lines.join("\n"));
}
