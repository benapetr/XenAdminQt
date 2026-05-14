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
#include "xenlib/xencache.h"
#include "xenlib/xen/vm.h"
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
{
    QString defaultPath = SettingsManager::instance().GetDefaultExportPath();
    if (defaultPath.isEmpty())
        defaultPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (defaultPath.isEmpty())
        defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    this->m_exportDirectory = defaultPath;

    this->ui->setupUi(this);
    this->setupWizardPages();

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

ExportWizard::~ExportWizard()
{
    delete this->ui;
}

void ExportWizard::setupWizardPages()
{
    this->setPage(Page_Format, this->ui->pageFormat);
    this->setPage(Page_VMs,    this->ui->pageVMs);
    this->setPage(Page_Options, this->ui->pageOptions);
    this->setPage(Page_Finish,  this->ui->pageFinish);

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

void ExportWizard::initializePage(int id)
{
    if (id == Page_VMs)
        this->populateVmList();

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
            if (vm->IsControlDomain())
                continue;
            if (vm->IsTemplate() || vm->IsSnapshot())
                continue;
            if (!vm->GetAllowedOperations().contains("export"))
                continue;
            // OVF export requires the VM to be halted or suspended (matches C# IsVmExportable).
            // XVA export can target any power state selected via command context.
            if (!this->m_exportAsXVA)
            {
                const QString ps = vm->GetPowerState();
                if (ps != "Halted" && ps != "Suspended")
                    continue;
            }

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
    return QWizard::validateCurrentPage();
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
    if (!this->m_exportAsXVA)
    {
        summary += tr("Export Options:\n");
        summary += tr("  \u2022 Create manifest: %1\n").arg(this->ui->createManifestCheckBox->isChecked() ? tr("Yes") : tr("No"));
        summary += tr("  \u2022 Sign appliance: %1\n").arg(this->ui->signApplianceCheckBox->isChecked() ? tr("Yes") : tr("No"));
        summary += tr("  \u2022 Encrypt files: %1\n").arg(this->ui->encryptFilesCheckBox->isChecked() ? tr("Yes") : tr("No"));
        summary += tr("  \u2022 Create OVA package: %1\n").arg(this->ui->createOVACheckBox->isChecked() ? tr("Yes") : tr("No"));
        summary += tr("  \u2022 Compress files: %1\n").arg(this->ui->compressOVFCheckBox->isChecked() ? tr("Yes") : tr("No"));
        summary += "\n";
    }

    summary += tr("General Options:\n");
    summary += tr("  \u2022 Verify export: %1\n").arg(this->ui->verifyExportCheckBox->isChecked() ? tr("Yes") : tr("No"));

    this->ui->summaryTextEdit->setPlainText(summary);
}
