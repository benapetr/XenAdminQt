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

#include "crosspoolmigratewizard_intrapoolcopypage.h"
#include "ui_crosspoolmigratewizard_intrapoolcopypage.h"
#include "crosspoolmigratewizard.h"
#include "../controls/srpicker.h"
#include "xen/network/connectionsmanager.h"
#include "xenlib/xencache.h"
#include "xen/vm.h"
#include "xen/vbd.h"
#include "xen/vdi.h"

IntraPoolCopyPage::IntraPoolCopyPage(const QList<QString>& selectedVMs, QWidget *parent)
    : QWizardPage(parent)
    , ui(new Ui::IntraPoolCopyPage)
    , vmsFromSelection_(selectedVMs)
    , fastCloneAvailable_(true)
    , m_vm()
    , cloneRadioButton_(nullptr)
    , fastCloneDescription_(nullptr)
{
    ui->setupUi(this);
    
    this->setTitle(tr("Copy Within Pool"));
    this->setSubTitle(tr("Configure the copy settings for the virtual machine."));
    
    // Create FastClonePanel and add it to ToolTipContainer programmatically
    this->setupFastClonePanel();
    
    this->setupConnections();
}

IntraPoolCopyPage::~IntraPoolCopyPage()
{
    delete ui;
}

void IntraPoolCopyPage::setupConnections()
{
    connect(ui->NameTextBox, &QLineEdit::textChanged,
            this, &IntraPoolCopyPage::onNameTextChanged);
    connect(ui->buttonRescan, &QPushButton::clicked,
            this, &IntraPoolCopyPage::onButtonRescanClicked);

    if (ui->srPicker1)
    {
        connect(ui->srPicker1, &SrPicker::selectedIndexChanged,
                this, &IntraPoolCopyPage::onSrPickerSelectionChanged);
        connect(ui->srPicker1, &SrPicker::canBeScannedChanged,
                this, &IntraPoolCopyPage::onSrPickerCanBeScannedChanged);
        connect(ui->srPicker1, &SrPicker::doubleClickOnRow,
                this, &IntraPoolCopyPage::onSrPickerSelectionChanged);
    }
}

void IntraPoolCopyPage::setupFastClonePanel()
{
    // Create FastClonePanel container
    QWidget* fastClonePanel = new QWidget();
    fastClonePanel->setObjectName("FastClonePanel");
    
    QVBoxLayout* panelLayout = new QVBoxLayout(fastClonePanel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(6);
    
    // Create CloneRadioButton
    this->cloneRadioButton_ = new QRadioButton(tr("Fast &clone (uses Storage XenMotion)"));
    this->cloneRadioButton_->setObjectName("CloneRadioButton");
    this->cloneRadioButton_->setChecked(true);
    panelLayout->addWidget(this->cloneRadioButton_);
    
    // Create FastCloneDescription label
    this->fastCloneDescription_ = new QLabel(tr("Creates a fast clone of the VM on the same storage."));
    this->fastCloneDescription_->setObjectName("FastCloneDescription");
    this->fastCloneDescription_->setWordWrap(true);
    this->fastCloneDescription_->setIndent(20);
    panelLayout->addWidget(this->fastCloneDescription_);
    
    // Add FastClonePanel to ToolTipContainer
    // ToolTipContainer's childEvent() will handle adding it to the layout
    fastClonePanel->setParent(ui->toolTipContainer1);
    
    // Connect radio button signals
    connect(this->cloneRadioButton_, &QRadioButton::toggled,
            this, &IntraPoolCopyPage::onCloneRadioButtonChanged);
    connect(ui->CopyRadioButton, &QRadioButton::toggled,
            this, &IntraPoolCopyPage::onCopyRadioButtonChanged);
}

void IntraPoolCopyPage::initializePage()
{
    this->populatePage();
}

bool IntraPoolCopyPage::validatePage()
{
    // Ensure name is not empty
    QString name = this->NewVmName().trimmed();
    if (name.isEmpty())
    {
        return false;
    }
    
    // If full copy mode, ensure SR is selected
    if (!this->CloneVM() && this->SelectedSR().isEmpty())
    {
        return false;
    }
    
    return true;
}

bool IntraPoolCopyPage::isComplete() const
{
    // Name must not be empty
    QString name = ui->NameTextBox->text().trimmed();
    if (name.isEmpty())
        return false;
    
    // If full copy mode, SR must be selected
    if (ui->CopyRadioButton->isChecked())
    {
        if (ui->srPicker1 && ui->srPicker1->GetSelectedSR().isEmpty())
            return false;
    }
    
    return true;
}

int IntraPoolCopyPage::nextId() const
{
    return CrossPoolMigrateWizard::Page_Finish;
}

void IntraPoolCopyPage::populatePage()
{
    if (vmsFromSelection_.isEmpty())
        return;

    if (!this->m_vm)
    {
        Xen::ConnectionsManager* mgr = Xen::ConnectionsManager::instance();
        if (mgr)
        {
            for (XenConnection* conn : mgr->GetAllConnections())
            {
                if (!conn || !conn->GetCache())
                    continue;
                QSharedPointer<VM> vm = conn->GetCache()->ResolveObject<VM>("vm", vmsFromSelection_.first());
                if (vm)
                {
                    this->m_vm = vm;
                    break;
                }
            }
        }
    }

    if (!this->m_vm)
        return;

    this->originalVmName_ = this->m_vm->GetName();
    this->originalVmDescription_ = this->m_vm->GetDescription();

    QString defaultName = this->originalVmName_;
    QString baseName = tr("Copy of %1").arg(this->originalVmName_);
    QStringList takenNames;
    XenCache* cache = this->m_vm->GetConnection() ? this->m_vm->GetConnection()->GetCache() : nullptr;
    if (cache)
    {
        QStringList vmRefs = cache->GetAllRefs("vm");
        for (const QString& vmRef : vmRefs)
        {
            QSharedPointer<VM> vm = cache->ResolveObject<VM>("vm", vmRef);
            if (vm)
                takenNames.append(vm->GetName());
        }
        defaultName = baseName;
        int counter = 1;
        while (takenNames.contains(defaultName))
        {
            defaultName = QString("%1 (%2)").arg(baseName).arg(counter);
            counter++;
        }
    }

    ui->NameTextBox->setText(defaultName);
    ui->DescriptionTextBox->setPlainText(this->originalVmDescription_);

    bool allowCopy = !this->m_vm->IsTemplate() || this->m_vm->GetAllowedOperations().contains("copy");
    bool anyDiskFastCloneable = this->m_vm->AnyDiskFastClonable();
    bool hasAtLeastOneDisk = this->m_vm->HasAtLeastOneDisk();

    ui->CopyRadioButton->setEnabled(allowCopy && hasAtLeastOneDisk);
    if (this->cloneRadioButton_)
        this->cloneRadioButton_->setEnabled(!allowCopy || anyDiskFastCloneable || !hasAtLeastOneDisk);

    if (this->cloneRadioButton_ && !this->cloneRadioButton_->isEnabled())
        this->cloneRadioButton_->setChecked(false);

    if (!ui->CopyRadioButton->isEnabled())
        ui->CopyRadioButton->setChecked(false);

    ui->labelRubric->setText(this->m_vm->IsTemplate()
                                 ? tr("Enter a name and description for the copy of the template:")
                                 : tr("Enter a name and description for the copy of the VM:"));
    ui->labelSrHint->setText(this->m_vm->IsTemplate()
                                 ? tr("Select the storage repository for the copied template:")
                                 : tr("Select the storage repository for the copied VM:"));

    this->fastCloneAvailable_ = anyDiskFastCloneable;
    if (!this->fastCloneAvailable_)
        this->enableFastClone(false, tr("Fast clone is not available for this VM."));

    this->updateCopyModeControls();
    this->updateSrPicker();
}

void IntraPoolCopyPage::updateCopyModeControls()
{
    bool cloneMode = this->cloneRadioButton_->isChecked();

    // SR picker is only visible/enabled in full copy mode
    ui->labelSrHint->setVisible(!cloneMode);
    ui->srPicker1->setVisible(!cloneMode);
    ui->buttonRescan->setVisible(!cloneMode);
    if (ui->srPicker1)
        ui->srPicker1->setEnabled(!cloneMode);
    ui->buttonRescan->setEnabled(!cloneMode && ui->srPicker1 && ui->srPicker1->CanBeScanned());

    if (cloneMode)
    {
        ui->labelSrHint->setText(tr("Fast clone will use the same storage as the source VM."));
    } else
    {
        if (this->m_vm && this->m_vm->IsTemplate())
            ui->labelSrHint->setText(tr("Select the storage repository for the copied template:"));
        else
            ui->labelSrHint->setText(tr("Select the storage repository for the full copy:"));
    }

    emit completeChanged();
}

void IntraPoolCopyPage::updateSrPicker()
{
    if (!this->m_vm || !ui->srPicker1)
        return;

    XenConnection* connection = this->m_vm->GetConnection();
    if (!connection)
        return;

    QStringList vdiRefs;
    XenCache* cache = connection->GetCache();
    if (cache)
    {
        QStringList vbdRefs = this->m_vm->GetVBDRefs();
        for (const QString& vbdRef : vbdRefs)
        {
            QSharedPointer<VBD> vbd = cache->ResolveObject<VBD>("vbd", vbdRef);
            if (!vbd || !vbd->IsValid())
                continue;
            if (vbd->GetType().compare("Disk", Qt::CaseInsensitive) != 0)
                continue;
            QString vdiRef = vbd->GetVDIRef();
            if (!vdiRef.isEmpty())
                vdiRefs.append(vdiRef);
        }
    }

    ui->srPicker1->Populate(SrPicker::Copy,
                            connection,
                            this->m_vm->GetHomeRef(),
                            QString(),
                            vdiRefs);
    ui->srPicker1->setEnabled(ui->CopyRadioButton->isChecked());
    ui->buttonRescan->setEnabled(ui->CopyRadioButton->isChecked() && ui->srPicker1->CanBeScanned());
}

void IntraPoolCopyPage::enableFastClone(bool enable, const QString& reason)
{
    this->fastCloneAvailable_ = enable;
    
    if (!enable)
    {
        // Disable fast clone radio and show tooltip
        ui->toolTipContainer1->SetToolTip(reason);
        this->cloneRadioButton_->setEnabled(false);
        this->fastCloneDescription_->setEnabled(false);
        
        // Force full copy mode
        ui->CopyRadioButton->setChecked(true);
    } else
    {
        // Enable fast clone
        ui->toolTipContainer1->RemoveAll();
        this->cloneRadioButton_->setEnabled(true);
        this->fastCloneDescription_->setEnabled(true);
    }
}

bool IntraPoolCopyPage::CloneVM() const
{
    return this->cloneRadioButton_->isChecked();
}

QString IntraPoolCopyPage::SelectedSR() const
{
    return ui->srPicker1 ? ui->srPicker1->GetSelectedSR() : QString();
}

QString IntraPoolCopyPage::NewVmName() const
{
    return ui->NameTextBox->text();
}

QString IntraPoolCopyPage::NewVmDescription() const
{
    return ui->DescriptionTextBox->toPlainText();
}

void IntraPoolCopyPage::onNameTextChanged()
{
    emit completeChanged();
}

void IntraPoolCopyPage::onCloneRadioButtonChanged()
{
    if (ui->CopyRadioButton)
        ui->CopyRadioButton->setChecked(!this->cloneRadioButton_->isChecked());
    this->updateCopyModeControls();
    emit completeChanged();
}

void IntraPoolCopyPage::onCopyRadioButtonChanged()
{
    if (this->cloneRadioButton_)
        this->cloneRadioButton_->setChecked(!ui->CopyRadioButton->isChecked());
    this->updateCopyModeControls();
    emit completeChanged();
}

void IntraPoolCopyPage::onSrPickerSelectionChanged()
{
    emit completeChanged();
}

void IntraPoolCopyPage::onSrPickerCanBeScannedChanged()
{
    // Update rescan button state
    if (ui->srPicker1)
        ui->buttonRescan->setEnabled(ui->srPicker1->CanBeScanned());
    emit completeChanged();
}

void IntraPoolCopyPage::onButtonRescanClicked()
{
    if (ui->srPicker1)
        ui->srPicker1->ScanSRs();
}
