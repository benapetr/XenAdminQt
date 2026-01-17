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

#include "crosspoolmigratewizard.h"
#include "../mainwindow.h"
#include "../operations/operationmanager.h"
#include "../commands/vm/vmoperationhelpers.h"
#include "xen/host.h"
#include "xen/network.h"
#include "xen/sr.h"
#include "xen/vdi.h"
#include "xen/vm.h"
#include "xen/friendlyerrornames.h"
#include "xen/failure.h"
#include "xen/actions/vm/vmcrosspoolmigrateaction.h"
#include "xen/actions/vm/vmmigrateaction.h"
#include "xen/actions/vm/vmmoveaction.h"
#include "xen/actions/vm/vmcopyaction.h"
#include "xen/actions/vm/vmcloneaction.h"
#include "../controls/srpicker.h"
#include "xen/network/connectionsmanager.h"
#include "xen/xenapi/xenapi_Host.h"
#include "xen/xenapi/xenapi_VM.h"
#include "xenlib/xencache.h"
#include "xen/session.h"
#include <QComboBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QRadioButton>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QStandardItemModel>
#include <QRegularExpression>

namespace
{
    class DestinationWizardPage : public QWizardPage
    {
        public:
            explicit DestinationWizardPage(CrossPoolMigrateWizard* wizard, QWidget* parent = nullptr)
                : QWizardPage(parent), wizard_(wizard)
            {
            }

            int nextId() const override
            {
                if (this->wizard_ && this->wizard_->requiresRbacWarning())
                    return CrossPoolMigrateWizard::Page_RbacWarning;
                return CrossPoolMigrateWizard::Page_Storage;
            }

        private:
            CrossPoolMigrateWizard* wizard_;
    };

    class CopyModeWizardPage : public QWizardPage
    {
        public:
            explicit CopyModeWizardPage(CrossPoolMigrateWizard* wizard, QWidget* parent = nullptr)
                : QWizardPage(parent), wizard_(wizard)
            {
            }

            int nextId() const override
            {
                if (wizard_ && wizard_->isIntraPoolCopySelected())
                    return CrossPoolMigrateWizard::Page_IntraPoolCopy;
                return CrossPoolMigrateWizard::Page_Destination;
            }

        private:
            CrossPoolMigrateWizard* wizard_;
    };

    class IntraPoolCopyWizardPage : public QWizardPage
    {
        public:
            explicit IntraPoolCopyWizardPage(QWidget* parent = nullptr)
                : QWizardPage(parent)
            {
                setFinalPage(true);
            }

            int nextId() const override
            {
                return CrossPoolMigrateWizard::Page_Finish;
            }
    };

    class StorageWizardPage : public QWizardPage
    {
        public:
            explicit StorageWizardPage(CrossPoolMigrateWizard* wizard, QWidget* parent = nullptr)
                : QWizardPage(parent), wizard_(wizard)
            {
            }

            int nextId() const override
            {
                if (!this->wizard_)
                    return CrossPoolMigrateWizard::Page_Finish;
                if (this->wizard_->shouldShowNetworkPage())
                    return CrossPoolMigrateWizard::Page_Network;
                if (this->wizard_->shouldShowTransferNetworkPage())
                    return CrossPoolMigrateWizard::Page_TransferNetwork;
                return CrossPoolMigrateWizard::Page_Finish;
            }

        private:
            CrossPoolMigrateWizard* wizard_;
    };

    class NetworkWizardPage : public QWizardPage
    {
        public:
            explicit NetworkWizardPage(CrossPoolMigrateWizard* wizard, QWidget* parent = nullptr)
                : QWizardPage(parent), wizard_(wizard)
            {
            }

            int nextId() const override
            {
                if (this->wizard_ && this->wizard_->shouldShowTransferNetworkPage())
                    return CrossPoolMigrateWizard::Page_TransferNetwork;
                return CrossPoolMigrateWizard::Page_Finish;
            }

        private:
            CrossPoolMigrateWizard* wizard_;
    };

    class TransferWizardPage : public QWizardPage
    {
        public:
            explicit TransferWizardPage(QWidget* parent = nullptr) : QWizardPage(parent) {}

            int nextId() const override
            {
                return CrossPoolMigrateWizard::Page_Finish;
            }
    };

    class RbacWizardPage : public QWizardPage
    {
        public:
            explicit RbacWizardPage(QWidget* parent = nullptr)
                : QWizardPage(parent)
            {
                setFinalPage(false);
            }

            int nextId() const override
            {
                return CrossPoolMigrateWizard::Page_Storage;
            }

            void setConfirmation(QCheckBox* box)
            {
                confirmBox_ = box;
                if (confirmBox_)
                {
                    connect(confirmBox_, &QCheckBox::toggled, this, &RbacWizardPage::onConfirmationToggled);
                }
            }

            bool isComplete() const override
            {
                return confirmBox_ ? confirmBox_->isChecked() : true;
            }

        private:
            void onConfirmationToggled(bool)
            {
                emit completeChanged();
            }

            QCheckBox* confirmBox_ = nullptr;
    };

    QList<int> parseVersionParts(const QString& version)
    {
        QList<int> parts;
        QStringList tokens = version.split(QRegularExpression("[^0-9]"), Qt::SkipEmptyParts);
        for (const QString& token : tokens)
        {
            bool ok = false;
            int value = token.toInt(&ok);
            if (ok)
                parts.append(value);
        }
        return parts;
    }

    int compareVersions(const QString& a, const QString& b)
    {
        QList<int> partsA = parseVersionParts(a);
        QList<int> partsB = parseVersionParts(b);
        int maxSize = qMax(partsA.size(), partsB.size());
        for (int i = 0; i < maxSize; ++i)
        {
            int va = i < partsA.size() ? partsA[i] : 0;
            int vb = i < partsB.size() ? partsB[i] : 0;
            if (va < vb)
                return -1;
            if (va > vb)
                return 1;
        }
        return 0;
    }

    QString poolRefForConnection(XenCache* cache)
    {
        if (!cache)
            return QString();

        QList<QVariantMap> pools = cache->GetAllData("pool");
        if (pools.isEmpty())
            return QString();

        return pools.first().value("opaque_ref").toString();
    }

    QString poolMasterRefForConnection(XenCache* cache)
    {
        if (!cache)
            return QString();

        QList<QVariantMap> pools = cache->GetAllData("pool");
        if (pools.isEmpty())
            return QString();

        return pools.first().value("master").toString();
    }

    bool hostCanSeeNetwork(XenCache* cache, const QString& hostRef, const QString& networkRef)
    {
        if (!cache || hostRef.isEmpty() || networkRef.isEmpty())
            return false;

        QList<QVariantMap> pifs = cache->GetAllData("pif");
        for (const QVariantMap& pifData : pifs)
        {
            if (pifData.value("host").toString() != hostRef)
                continue;
            if (pifData.value("network").toString() != networkRef)
                continue;
            return true;
        }

        return false;
    }

    QString firstNetworkForHost(XenCache* cache, const QString& hostRef)
    {
        if (!cache || hostRef.isEmpty())
            return QString();

        QList<QVariantMap> pifs = cache->GetAllData("pif");
        for (const QVariantMap& pifData : pifs)
        {
            if (pifData.value("host").toString() != hostRef)
                continue;

            QString networkRef = pifData.value("network").toString();
            if (!networkRef.isEmpty())
                return networkRef;
        }

        return QString();
    }
}

CrossPoolMigrateWizard::CrossPoolMigrateWizard(MainWindow* mainWindow,
                                               const QSharedPointer<VM>& vm,
                                               WizardMode mode,
                                               QWidget* parent)
    : CrossPoolMigrateWizard(mainWindow,
                             vm ? QList<QSharedPointer<VM>>{vm} : QList<QSharedPointer<VM>>(),
                             mode,
                             parent)
{
}

CrossPoolMigrateWizard::CrossPoolMigrateWizard(MainWindow* mainWindow,
                                               const QList<QSharedPointer<VM>>& vms,
                                               WizardMode mode,
                                               QWidget* parent)
    : QWizard(parent),
      m_mainWindow(mainWindow),
      m_vms(vms),
      m_sourceConnection(vms.isEmpty() ? nullptr : vms.first()->GetConnection()),
      m_mode(mode)
{
    if (this->m_mode == WizardMode::Copy)
        this->setWindowTitle(tr("Copy VM Wizard"));
    else if (this->m_mode == WizardMode::Move)
        this->setWindowTitle(tr("Move VM Wizard"));
    else
        this->setWindowTitle(tr("Cross Pool Migrate Wizard"));
    this->setWizardStyle(QWizard::ModernStyle);
    this->setOption(QWizard::HaveHelpButton, true);
    this->setOption(QWizard::HelpButtonOnRight, false);
    if (this->m_mode == WizardMode::Copy)
        this->setStartId(Page_CopyMode);
    else
        this->setStartId(Page_Destination);

    for (const QSharedPointer<VM>& vmItem : this->m_vms)
        this->ensureMappingForVm(vmItem);

    this->setupWizardPages();
}

void CrossPoolMigrateWizard::setupWizardPages()
{
    this->setPage(Page_Destination, this->createDestinationPage());
    this->setPage(Page_RbacWarning, this->createRbacWarningPage());
    this->setPage(Page_Storage, this->createStoragePage());
    this->setPage(Page_Network, this->createNetworkPage());
    this->setPage(Page_TransferNetwork, this->createTransferNetworkPage());
    this->setPage(Page_Finish, this->createFinishPage());
    this->setPage(Page_CopyMode, this->createCopyModePage());
    this->setPage(Page_IntraPoolCopy, this->createIntraPoolCopyPage());
}

QWizardPage* CrossPoolMigrateWizard::createDestinationPage()
{
    QWizardPage* page = new DestinationWizardPage(this, this);
    page->setTitle(tr("Destination"));

    QLabel* info = new QLabel(tr("Select the destination host for the VM."));
    info->setWordWrap(true);

    this->m_hostCombo = new QComboBox(page);
    QFormLayout* layout = new QFormLayout(page);
    layout->addRow(info);
    layout->addRow(tr("Target host:"), this->m_hostCombo);

    return page;
}

QWizardPage* CrossPoolMigrateWizard::createStoragePage()
{
    QWizardPage* page = new StorageWizardPage(this, this);
    page->setTitle(tr("Storage"));

    QLabel* info = new QLabel(tr("Select storage repositories for each virtual disk."));
    info->setWordWrap(true);

    this->m_storageTable = new QTableWidget(page);
    this->m_storageTable->setColumnCount(3);
    this->m_storageTable->setHorizontalHeaderLabels({tr("VM"), tr("Virtual disk"), tr("Target SR")});
    this->m_storageTable->horizontalHeader()->setStretchLastSection(true);
    this->m_storageTable->verticalHeader()->setVisible(false);
    this->m_storageTable->setSelectionMode(QAbstractItemView::NoSelection);
    this->m_storageTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->addWidget(info);
    layout->addWidget(this->m_storageTable);

    return page;
}

QWizardPage* CrossPoolMigrateWizard::createNetworkPage()
{
    QWizardPage* page = new NetworkWizardPage(this, this);
    page->setTitle(tr("Networking"));

    QLabel* info = new QLabel(tr("Select networks for each virtual interface."));
    info->setWordWrap(true);

    this->m_networkTable = new QTableWidget(page);
    this->m_networkTable->setColumnCount(3);
    this->m_networkTable->setHorizontalHeaderLabels({tr("VM"), tr("VIF (MAC)"), tr("Target Network")});
    this->m_networkTable->horizontalHeader()->setStretchLastSection(true);
    this->m_networkTable->verticalHeader()->setVisible(false);
    this->m_networkTable->setSelectionMode(QAbstractItemView::NoSelection);
    this->m_networkTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->addWidget(info);
    layout->addWidget(this->m_networkTable);

    return page;
}

QWizardPage* CrossPoolMigrateWizard::createTransferNetworkPage()
{
    QWizardPage* page = new TransferWizardPage(this);
    page->setTitle(tr("Transfer Network"));

    QLabel* info = new QLabel(tr("Select the network used for transferring VM data."));
    info->setWordWrap(true);

    this->m_transferNetworkCombo = new QComboBox(page);

    QFormLayout* layout = new QFormLayout(page);
    layout->addRow(info);
    layout->addRow(tr("Transfer network:"), this->m_transferNetworkCombo);

    return page;
}

QWizardPage* CrossPoolMigrateWizard::createRbacWarningPage()
{
    RbacWizardPage* page = new RbacWizardPage(this);
    page->setTitle(tr("Permissions"));

    QLabel* info = new QLabel(tr("The target connection may require permissions "
                                 "to perform cross-pool migration. If you do not "
                                 "have the required role, the operation will fail."));
    info->setWordWrap(true);

    QCheckBox* confirm = new QCheckBox(tr("I have the required permissions to continue."), page);
    confirm->setChecked(false);

    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->addWidget(info);
    layout->addWidget(confirm);

    this->m_rbacPage = page;
    page->setConfirmation(confirm);
    return page;
}

QWizardPage* CrossPoolMigrateWizard::createFinishPage()
{
    QWizardPage* page = new QWizardPage(this);
    page->setTitle(tr("Finish"));

    this->m_summaryText = new QTextEdit(page);
    this->m_summaryText->setReadOnly(true);

    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->addWidget(this->m_summaryText);

    return page;
}

QWizardPage* CrossPoolMigrateWizard::createCopyModePage()
{
    QWizardPage* page = new CopyModeWizardPage(this, this);
    page->setTitle(tr("Copy Mode"));

    QLabel* info = new QLabel(tr("Choose how to copy the VM."), page);
    info->setWordWrap(true);

    this->m_copyIntraRadio = new QRadioButton(tr("Copy within the current pool"), page);
    this->m_copyCrossRadio = new QRadioButton(tr("Copy to another pool"), page);

    bool canUseIntra = (this->m_vms.size() == 1);
    this->m_copyIntraRadio->setEnabled(canUseIntra);
    if (canUseIntra)
        this->m_copyIntraRadio->setChecked(true);
    else
        this->m_copyCrossRadio->setChecked(true);
    this->m_intraPoolCopySelected = canUseIntra;

    connect(this->m_copyIntraRadio, &QRadioButton::toggled, this, &CrossPoolMigrateWizard::onCopyIntraToggled);
    connect(this->m_copyCrossRadio, &QRadioButton::toggled, this, &CrossPoolMigrateWizard::onCopyCrossToggled);

    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->addWidget(info);
    layout->addWidget(this->m_copyIntraRadio);
    layout->addWidget(this->m_copyCrossRadio);

    this->m_copyModePage = page;
    return page;
}

QWizardPage* CrossPoolMigrateWizard::createIntraPoolCopyPage()
{
    QWizardPage* page = new IntraPoolCopyWizardPage(this);
    page->setTitle(tr("Intra-pool Copy"));

    QLabel* info = new QLabel(tr("Select copy type and target storage."), page);
    info->setWordWrap(true);

    this->m_copyCloneRadio = new QRadioButton(tr("Fast clone (no storage selection)"), page);
    this->m_copyFullRadio = new QRadioButton(tr("Full copy to selected SR"), page);
    this->m_copyCloneRadio->setChecked(true);

    this->m_copyNameEdit = new QLineEdit(page);
    this->m_copyDescriptionEdit = new QTextEdit(page);
    this->m_copyDescriptionEdit->setFixedHeight(80);

    this->m_copySrPicker = new SrPicker(page);
    this->m_copySrPicker->setEnabled(false);
    this->m_copyRescanButton = new QPushButton(tr("Rescan"), page);
    this->m_copyRescanButton->setEnabled(false);

    connect(this->m_copyCloneRadio, &QRadioButton::toggled, this, &CrossPoolMigrateWizard::onCopyCloneToggled);
    connect(this->m_copySrPicker, &SrPicker::canBeScannedChanged, this, &CrossPoolMigrateWizard::onCopySrPickerCanBeScannedChanged);
    connect(this->m_copySrPicker, &SrPicker::selectedIndexChanged, this, &CrossPoolMigrateWizard::onCopySrPickerSelectionChanged);
    connect(this->m_copyRescanButton, &QPushButton::clicked, this, &CrossPoolMigrateWizard::onCopyRescanClicked);

    QFormLayout* formLayout = new QFormLayout();
    formLayout->addRow(tr("Name:"), this->m_copyNameEdit);
    formLayout->addRow(tr("Description:"), this->m_copyDescriptionEdit);
    formLayout->addRow(tr("Target SR:"), this->m_copySrPicker);
    formLayout->addRow(QString(), this->m_copyRescanButton);

    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->addWidget(info);
    layout->addWidget(this->m_copyCloneRadio);
    layout->addWidget(this->m_copyFullRadio);
    layout->addLayout(formLayout);

    this->m_intraPoolCopyPage = page;
    return page;
}

void CrossPoolMigrateWizard::initializePage(int id)
{
    if (id == Page_CopyMode)
    {
        this->m_intraPoolCopySelected = this->m_copyIntraRadio ? this->m_copyIntraRadio->isChecked() : false;
    }
    else if (id == Page_IntraPoolCopy)
    {
        QSharedPointer<VM> vmItem = this->m_vms.isEmpty() ? QSharedPointer<VM>() : this->m_vms.first();
        if (this->m_copyNameEdit && vmItem)
            this->m_copyNameEdit->setText(tr("Copy of %1").arg(vmItem->GetName()));

        if (this->m_copySrPicker && vmItem)
        {
            XenConnection* conn = this->m_sourceConnection;
            XenCache* cache = conn ? conn->GetCache() : nullptr;
            if (cache)
            {
                QStringList vdiRefs;
                QStringList vbdRefs = vmItem->GetVBDRefs();
                for (const QString& vbdRef : vbdRefs)
                {
                    QVariantMap vbdData = cache->ResolveObjectData("vbd", vbdRef);
                    QString vdiRef = vbdData.value("VDI").toString();
                    if (vdiRef.isEmpty() || vdiRef == "OpaqueRef:NULL")
                        continue;
                    QVariantMap vdiData = cache->ResolveObjectData("vdi", vdiRef);
                    QString vdiType = vdiData.value("type").toString();
                    if (vdiType == "iso")
                        continue;
                    vdiRefs.append(vdiRef);
                }
                this->m_copySrPicker->Populate(SrPicker::Copy,
                                               conn,
                                               vmItem->GetHomeRef(),
                                               QString(),
                                               vdiRefs);
            }
        }

        if (vmItem)
        {
            bool allowCopy = !vmItem->IsTemplate() || vmItem->GetAllowedOperations().contains("copy");
            bool anyFastClone = vmItem->AnyDiskFastClonable();
            bool hasDisk = vmItem->HasAtLeastOneDisk();
            bool copyEnabled = allowCopy && hasDisk;
            bool cloneEnabled = (!allowCopy) || anyFastClone || !hasDisk;

            if (this->m_copyFullRadio)
                this->m_copyFullRadio->setEnabled(copyEnabled);
            if (this->m_copyCloneRadio)
                this->m_copyCloneRadio->setEnabled(cloneEnabled);

            if (!cloneEnabled && this->m_copyFullRadio)
                this->m_copyFullRadio->setChecked(true);
            if (!copyEnabled && this->m_copyCloneRadio)
                this->m_copyCloneRadio->setChecked(true);

            if (this->m_copySrPicker)
                this->m_copySrPicker->setEnabled(this->m_copyFullRadio && this->m_copyFullRadio->isChecked());
        }
    }
    else if (id == Page_Destination)
    {
        populateDestinationHosts();
    }
    else if (id == Page_RbacWarning)
    {
        updateRbacRequirement();
    }
    else if (id == Page_Storage)
    {
        populateStorageMappings();
    }
    else if (id == Page_Network)
    {
        populateNetworkMappings();
    }
    else if (id == Page_TransferNetwork)
    {
        populateTransferNetworks();
    }
    else if (id == Page_Finish)
    {
        updateSummary();
    }

    QWizard::initializePage(id);
}

bool CrossPoolMigrateWizard::validateCurrentPage()
{
    if (this->currentId() == Page_CopyMode)
    {
        if (this->m_mode == WizardMode::Copy && this->m_copyIntraRadio && this->m_copyIntraRadio->isChecked() && this->m_vms.size() != 1)
        {
            QMessageBox::warning(this, tr("Copy VM"), tr("Intra-pool copy supports a single VM selection."));
            return false;
        }
    }
    else if (this->currentId() == Page_IntraPoolCopy)
    {
        if (!this->m_copyNameEdit || this->m_copyNameEdit->text().trimmed().isEmpty())
        {
            QMessageBox::warning(this, tr("Copy VM"), tr("Please enter a name for the copied VM."));
            return false;
        }
        if (this->m_copyFullRadio && this->m_copyFullRadio->isChecked())
        {
            if (!this->m_copySrPicker || this->m_copySrPicker->GetSelectedSR().isEmpty())
            {
                QMessageBox::warning(this, tr("Copy VM"), tr("Please select a target SR."));
                return false;
            }
        }
    }
    else if (this->currentId() == Page_Destination)
    {
        if (!this->m_hostCombo || this->m_hostCombo->currentIndex() < 0)
        {
            QMessageBox::warning(this, tr("Cross Pool Migrate"), tr("Please select a destination host."));
            return false;
        }

        auto* model = qobject_cast<QStandardItemModel*>(this->m_hostCombo->model());
        if (model)
        {
            QStandardItem* item = model->item(this->m_hostCombo->currentIndex());
            if (item && !item->isEnabled())
            {
                QMessageBox::warning(this, tr("Cross Pool Migrate"),
                                     tr("Selected host is not eligible for migration."));
                return false;
            }
        }

        this->m_targetHostRef = this->m_hostCombo->currentData().toString();
        this->m_targetConnection = this->resolveTargetConnection(this->m_targetHostRef);
        this->updateRbacRequirement();

        QString targetName = this->m_hostCombo->currentText();
        for (auto it = this->m_vmMappings.begin(); it != this->m_vmMappings.end(); ++it)
            it.value().targetName = targetName;
    }
    else if (this->currentId() == Page_TransferNetwork && this->requiresTransferNetwork())
    {
        if (!this->m_transferNetworkCombo || this->m_transferNetworkCombo->currentIndex() < 0)
        {
            QMessageBox::warning(this, tr("Cross Pool Migrate"), tr("Please select a transfer network."));
            return false;
        }
    }

    return QWizard::validateCurrentPage();
}

void CrossPoolMigrateWizard::accept()
{
    if (this->m_vms.isEmpty() || !this->m_sourceConnection)
    {
        QWizard::accept();
        return;
    }

    if (this->m_mode == WizardMode::Copy && this->isIntraPoolCopySelected())
    {
        QSharedPointer<VM> vmItem = this->m_vms.first();
        if (vmItem)
        {
            if (this->isCopyCloneSelected())
            {
                VMCloneAction* action = new VMCloneAction(vmItem, this->copyName(), this->copyDescription(), this);
                OperationManager::instance()->RegisterOperation(action);
                action->RunAsync();
            }
            else
            {
                XenCache* cache = this->m_sourceConnection->GetCache();
                QSharedPointer<SR> sr = cache ? cache->ResolveObject<SR>("sr", this->copyTargetSrRef()) : QSharedPointer<SR>();
                if (sr && sr->IsValid())
                {
                    VMCopyAction* action = new VMCopyAction(vmItem, QSharedPointer<Host>(), sr, this->copyName(), this->copyDescription(), this);
                    OperationManager::instance()->RegisterOperation(action);
                    action->RunAsync();
                }
            }
        }

        QWizard::accept();
        return;
    }

    if (this->m_storageTable)
    {
        for (int row = 0; row < this->m_storageTable->rowCount(); ++row)
        {
            QTableWidgetItem* vmItem = this->m_storageTable->item(row, 0);
            QTableWidgetItem* vdiItem = this->m_storageTable->item(row, 1);
            QString vmRef = vmItem ? vmItem->data(Qt::UserRole).toString() : QString();
            QString vdiRef = vdiItem ? vdiItem->data(Qt::UserRole).toString() : QString();
            QComboBox* combo = qobject_cast<QComboBox*>(this->m_storageTable->cellWidget(row, 2));
            if (!combo || vmRef.isEmpty() || vdiRef.isEmpty())
                continue;

            QString srRef = combo->currentData().toString();
            if (!srRef.isEmpty())
                this->m_vmMappings[vmRef].storage.insert(vdiRef, srRef);
        }
    }

    if (this->m_networkTable)
    {
        for (int row = 0; row < this->m_networkTable->rowCount(); ++row)
        {
            QTableWidgetItem* vmItem = this->m_networkTable->item(row, 0);
            QTableWidgetItem* vifItem = this->m_networkTable->item(row, 1);
            QString vmRef = vmItem ? vmItem->data(Qt::UserRole).toString() : QString();
            QString vifRef = vifItem ? vifItem->data(Qt::UserRole).toString() : QString();
            QComboBox* combo = qobject_cast<QComboBox*>(this->m_networkTable->cellWidget(row, 2));
            if (!combo || vmRef.isEmpty() || vifRef.isEmpty())
                continue;

            QString netRef = combo->currentData().toString();
            if (!netRef.isEmpty())
                this->m_vmMappings[vmRef].vifs.insert(vifRef, netRef);
        }
    }

    if (this->m_transferNetworkCombo)
        this->m_transferNetworkRef = this->m_transferNetworkCombo->currentData().toString();

    if (!this->m_targetConnection || this->m_targetHostRef.isEmpty() ||
        (this->requiresTransferNetwork() && this->m_transferNetworkRef.isEmpty()))
    {
        QMessageBox::warning(this, tr("Cross Pool Migrate"), tr("Missing destination or transfer network."));
        return;
    }

    auto isStorageMotion = [](const QSharedPointer<VM>& vm,
                              const VmMapping& mapping,
                              XenCache* cache) -> bool
    {
        if (!vm || !cache)
            return false;

        for (auto it = mapping.storage.cbegin(); it != mapping.storage.cend(); ++it)
        {
            QString vdiRef = it.key();
            QString targetSrRef = it.value();
            if (vdiRef.isEmpty() || targetSrRef.isEmpty())
                continue;

            QVariantMap vdiData = cache->ResolveObjectData("vdi", vdiRef);
            QString currentSrRef = vdiData.value("SR").toString();
            if (!currentSrRef.isEmpty() && currentSrRef != targetSrRef)
                return true;
        }

        return false;
    };

    for (const QSharedPointer<VM>& vm : this->m_vms)
    {
        if (!vm)
            continue;

        VmMapping mapping = this->m_vmMappings.value(vm->OpaqueRef());
        mapping.targetRef = this->m_targetHostRef;

        XenCache* sourceCache = this->m_sourceConnection ? this->m_sourceConnection->GetCache() : nullptr;
        bool sameConnection = (this->m_sourceConnection == this->m_targetConnection);
        bool hasStorageMotion = isStorageMotion(vm, mapping, sourceCache);

        if (this->m_mode == WizardMode::Move && sameConnection && vm->CanBeMoved())
        {
            if (hasStorageMotion && sourceCache)
            {
                QMap<QString, QSharedPointer<SR>> storageMap;
                for (auto it = mapping.storage.cbegin(); it != mapping.storage.cend(); ++it)
                {
                    QString vdiRef = it.key();
                    QString srRef = it.value();
                    if (vdiRef.isEmpty() || srRef.isEmpty())
                        continue;

                    QSharedPointer<SR> srObj = sourceCache->ResolveObject<SR>("sr", srRef);
                    if (srObj && srObj->IsValid())
                        storageMap.insert(vdiRef, srObj);
                }

                QSharedPointer<Host> hostObj = sourceCache->ResolveObject<Host>("host", this->m_targetHostRef);

                VMMoveAction* action = new VMMoveAction(vm, storageMap, hostObj, this);
                OperationManager::instance()->RegisterOperation(action);
                action->RunAsync();
            }
        } else
        {
            bool useCrossPool = (this->m_mode == WizardMode::Copy) || hasStorageMotion || !sameConnection;
            if (useCrossPool)
            {
                VMCrossPoolMigrateAction* action = new VMCrossPoolMigrateAction(
                    this->m_sourceConnection,
                    this->m_targetConnection,
                    vm->OpaqueRef(),
                    this->m_targetHostRef,
                    this->m_transferNetworkRef,
                    mapping,
                    this->m_mode == WizardMode::Copy,
                    this);

                OperationManager::instance()->RegisterOperation(action);
                action->RunAsync();
            } else
            {
                QSharedPointer<Host> host = vm->GetCache()->ResolveObject<Host>("host", this->m_targetHostRef);
                VMMigrateAction* action = new VMMigrateAction(vm, host, this);
                OperationManager::instance()->RegisterOperation(action);
                action->RunAsync();
            }
        }
    }

    QWizard::accept();
}

void CrossPoolMigrateWizard::populateDestinationHosts()
{
    if (!this->m_hostCombo)
        return;

    this->m_hostCombo->clear();

    Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
    if (!connMgr)
        return;

    const QList<XenConnection*> connections = connMgr->GetAllConnections();
    for (XenConnection* conn : connections)
    {
        if (!conn || !conn->IsConnected())
            continue;

        XenCache* cache = conn->GetCache();
        if (!cache)
            continue;

        QStringList hostRefs = cache->GetAllRefs("host");
        for (const QString& hostRef : hostRefs)
        {
            QVariantMap hostData = cache->ResolveObjectData("host", hostRef);
            QString hostName = hostData.value("name_label", "Host").toString();
            QString failureReason;
            bool eligible = true;

            for (const QSharedPointer<VM>& vmItem : this->m_vms)
            {
                if (!vmItem)
                    continue;

                if (!this->canMigrateVmToHost(vmItem, conn, hostRef, &failureReason))
                {
                    eligible = false;
                    break;
                }
            }

            QString label = eligible ? hostName : QString("%1 (%2)").arg(hostName, failureReason);
            this->m_hostCombo->addItem(label, hostRef);
            int index = this->m_hostCombo->count() - 1;
            if (!eligible)
            {
                auto* model = qobject_cast<QStandardItemModel*>(this->m_hostCombo->model());
                if (model)
                {
                    QStandardItem* item = model->item(index);
                    if (item)
                        item->setEnabled(false);
                }
            }
        }
    }

    if (this->m_hostCombo->count() > 0)
    {
        int firstEnabled = 0;
        auto* model = qobject_cast<QStandardItemModel*>(this->m_hostCombo->model());
        if (model)
        {
            for (int i = 0; i < model->rowCount(); ++i)
            {
                QStandardItem* item = model->item(i);
                if (item && item->isEnabled())
                {
                    firstEnabled = i;
                    break;
                }
            }
        }

        this->m_hostCombo->setCurrentIndex(firstEnabled);
        this->m_targetHostRef = this->m_hostCombo->currentData().toString();
        this->m_targetConnection = this->resolveTargetConnection(this->m_targetHostRef);
        this->updateRbacRequirement();

        QString targetName = this->m_hostCombo->currentText();
        for (auto it = this->m_vmMappings.begin(); it != this->m_vmMappings.end(); ++it)
            it.value().targetName = targetName;
    }
} 

void CrossPoolMigrateWizard::populateStorageMappings()
{
    if (!this->m_storageTable || this->m_vms.isEmpty() || !this->m_sourceConnection)
        return;

    this->m_storageTable->setRowCount(0);

    XenCache* sourceCache = this->m_sourceConnection->GetCache();
    XenCache* targetCache = this->m_targetConnection ? this->m_targetConnection->GetCache() : nullptr;
    if (!sourceCache || !targetCache)
        return;

    QStringList srRefs = targetCache->GetAllRefs("sr");
    QString defaultSrRef;
    QList<QVariantMap> pools = targetCache->GetAllData("pool");
    if (!pools.isEmpty())
        defaultSrRef = pools.first().value("default_SR").toString();

    int row = 0;
    for (const QSharedPointer<VM>& vmItem : this->m_vms)
    {
        if (!vmItem)
            continue;

        QStringList vbdRefs = vmItem->GetVBDRefs();
        for (const QString& vbdRef : vbdRefs)
        {
            QVariantMap vbdData = sourceCache->ResolveObjectData("vbd", vbdRef);
            QString vdiRef = vbdData.value("VDI").toString();
            if (vdiRef.isEmpty() || vdiRef == "OpaqueRef:NULL")
                continue;

            QVariantMap vdiData = sourceCache->ResolveObjectData("vdi", vdiRef);
            QString vdiName = vdiData.value("name_label", "VDI").toString();
            QString vdiType = vdiData.value("type").toString();
            if (vdiType == "iso")
                continue;

            this->m_storageTable->insertRow(row);

            QTableWidgetItem* vmNameItem = new QTableWidgetItem(vmItem->GetName());
            vmNameItem->setData(Qt::UserRole, vmItem->OpaqueRef());
            this->m_storageTable->setItem(row, 0, vmNameItem);

            QTableWidgetItem* vdiItem = new QTableWidgetItem(vdiName);
            vdiItem->setData(Qt::UserRole, vdiRef);
            this->m_storageTable->setItem(row, 1, vdiItem);

            QComboBox* srCombo = new QComboBox(this->m_storageTable);
            for (const QString& srRef : srRefs)
            {
                QVariantMap srData = targetCache->ResolveObjectData("sr", srRef);
                QString srName = srData.value("name_label", "SR").toString();
                srCombo->addItem(srName, srRef);
            }

            if (!defaultSrRef.isEmpty())
            {
                int idx = srCombo->findData(defaultSrRef);
                if (idx >= 0)
                    srCombo->setCurrentIndex(idx);
            }

            this->m_storageTable->setCellWidget(row, 2, srCombo);
            row++;
        }
    }

    this->m_storageTable->resizeRowsToContents();
} 

void CrossPoolMigrateWizard::populateNetworkMappings()
{
    if (!this->m_networkTable || this->m_vms.isEmpty() || !this->m_sourceConnection)
        return;

    this->m_networkTable->setRowCount(0);

    XenCache* sourceCache = this->m_sourceConnection->GetCache();
    XenCache* targetCache = this->m_targetConnection ? this->m_targetConnection->GetCache() : nullptr;
    if (!sourceCache || !targetCache)
        return;

    QStringList networkRefs = targetCache->GetAllRefs("network");
    int row = 0;
    for (const QSharedPointer<VM>& vmItem : this->m_vms)
    {
        if (!vmItem)
            continue;

        QStringList vifRefs = vmItem->GetVIFRefs();
        QStringList snapVifRefs = this->collectSnapshotVifRefs(vmItem);
        for (const QString& vifRef : vifRefs + snapVifRefs)
        {
            bool isSnapshotVif = !vifRefs.contains(vifRef);
            QVariantMap vifData = sourceCache->ResolveObjectData("vif", vifRef);
            QString mac = vifData.value("MAC", "VIF").toString();
            if (isSnapshotVif)
                mac = QString("%1 (%2)").arg(mac, tr("snapshot"));

            this->m_networkTable->insertRow(row);
            QTableWidgetItem* vmNameItem = new QTableWidgetItem(vmItem->GetName());
            vmNameItem->setData(Qt::UserRole, vmItem->OpaqueRef());
            this->m_networkTable->setItem(row, 0, vmNameItem);

            QTableWidgetItem* vifItem = new QTableWidgetItem(mac);
            vifItem->setData(Qt::UserRole, vifRef);
            this->m_networkTable->setItem(row, 1, vifItem);

            QComboBox* netCombo = new QComboBox(this->m_networkTable);
            for (const QString& netRef : networkRefs)
            {
                QVariantMap netData = targetCache->ResolveObjectData("network", netRef);
                QString netName = netData.value("name_label", "Network").toString();
                netCombo->addItem(netName, netRef);
            }

            this->m_networkTable->setCellWidget(row, 2, netCombo);
            row++;
        }
    }

    this->m_networkTable->resizeRowsToContents();
} 

void CrossPoolMigrateWizard::populateTransferNetworks()
{
    if (!this->m_transferNetworkCombo || !this->m_targetConnection)
        return;

    this->m_transferNetworkCombo->clear();

    XenCache* targetCache = this->m_targetConnection->GetCache();
    if (!targetCache)
        return;

    QStringList networkRefs = targetCache->GetAllRefs("network");
    for (const QString& netRef : networkRefs)
    {
        QVariantMap netData = targetCache->ResolveObjectData("network", netRef);
        QString netName = netData.value("name_label", "Network").toString();
        this->m_transferNetworkCombo->addItem(netName, netRef);
    }

    if (this->m_transferNetworkCombo->count() > 0)
        this->m_transferNetworkCombo->setCurrentIndex(0);
} 

void CrossPoolMigrateWizard::updateSummary()
{
    if (!this->m_summaryText)
        return;

    QString summary;
    if (!this->m_vms.isEmpty())
    {
        summary += tr("VMs:\n");
        for (const QSharedPointer<VM>& vmItem : this->m_vms)
        {
            if (vmItem)
                summary += QString("  %1\n").arg(vmItem->GetName());
        }
        summary += "\n";
    }
    summary += tr("Target host: %1\n").arg(this->m_hostCombo ? this->m_hostCombo->currentText() : tr("Unknown"));
    if (this->requiresTransferNetwork())
        summary += tr("Transfer network: %1\n").arg(this->m_transferNetworkCombo ? this->m_transferNetworkCombo->currentText() : tr("Unknown"));
    summary += "\n";

    if (this->m_storageTable)
    {
        summary += tr("Storage mappings:\n");
        for (int row = 0; row < this->m_storageTable->rowCount(); ++row)
        {
            QString vmName = this->m_storageTable->item(row, 0)->text();
            QString vdiName = this->m_storageTable->item(row, 1)->text();
            QComboBox* combo = qobject_cast<QComboBox*>(this->m_storageTable->cellWidget(row, 2));
            QString srName = combo ? combo->currentText() : tr("Unknown");
            summary += QString("  %1: %2 -> %3\n").arg(vmName, vdiName, srName);
        }
        summary += "\n";
    }

    if (this->m_networkTable)
    {
        summary += tr("Network mappings:\n");
        for (int row = 0; row < this->m_networkTable->rowCount(); ++row)
        {
            QString vmName = this->m_networkTable->item(row, 0)->text();
            QString vifName = this->m_networkTable->item(row, 1)->text();
            QComboBox* combo = qobject_cast<QComboBox*>(this->m_networkTable->cellWidget(row, 2));
            QString netName = combo ? combo->currentText() : tr("Unknown");
            summary += QString("  %1: %2 -> %3\n").arg(vmName, vifName, netName);
        }
    }

    this->m_summaryText->setPlainText(summary);
} 

void CrossPoolMigrateWizard::updateRbacRequirement()
{
    if (!this->m_sourceConnection || !this->m_targetConnection)
    {
        this->m_requiresRbacWarning = false;
        return;
    }

    this->m_requiresRbacWarning = (this->m_sourceConnection != this->m_targetConnection);
}

void CrossPoolMigrateWizard::onCopyIntraToggled(bool checked)
{
    this->m_intraPoolCopySelected = checked;
}

void CrossPoolMigrateWizard::onCopyCrossToggled(bool checked)
{
    if (checked)
        this->m_intraPoolCopySelected = false;
}

void CrossPoolMigrateWizard::onCopyCloneToggled(bool checked)
{
    if (this->m_copySrPicker)
        this->m_copySrPicker->setEnabled(!checked);
    if (this->m_copyRescanButton && this->m_copySrPicker)
        this->m_copyRescanButton->setEnabled(!checked && this->m_copySrPicker->CanBeScanned());
}

void CrossPoolMigrateWizard::onCopySrPickerCanBeScannedChanged()
{
    if (this->m_copyRescanButton && this->m_copySrPicker)
        this->m_copyRescanButton->setEnabled(this->m_copySrPicker->CanBeScanned() && this->m_copySrPicker->isEnabled());
}

void CrossPoolMigrateWizard::onCopySrPickerSelectionChanged()
{
    QAbstractButton* nextButton = this->button(QWizard::NextButton);
    if (nextButton)
        nextButton->setEnabled(true);
}

void CrossPoolMigrateWizard::onCopyRescanClicked()
{
    if (this->m_copySrPicker)
        this->m_copySrPicker->ScanSRs();
}

bool CrossPoolMigrateWizard::requiresRbacWarning() const
{
    return this->m_requiresRbacWarning;
}

bool CrossPoolMigrateWizard::isIntraPoolMigration() const
{
    if (!this->m_sourceConnection || !this->m_targetConnection)
        return false;

    XenCache* sourceCache = this->m_sourceConnection->GetCache();
    XenCache* targetCache = this->m_targetConnection->GetCache();
    if (!sourceCache || !targetCache)
        return false;

    QString sourcePoolRef = poolRefForConnection(sourceCache);
    QString targetPoolRef = poolRefForConnection(targetCache);
    if (sourcePoolRef.isEmpty() || targetPoolRef.isEmpty())
        return false;

    return sourcePoolRef == targetPoolRef;
}

bool CrossPoolMigrateWizard::isIntraPoolMove() const
{
    if (this->m_mode != WizardMode::Move)
        return false;
    if (!this->isIntraPoolMigration())
        return false;

    for (const QSharedPointer<VM>& vmItem : this->m_vms)
    {
        if (!vmItem || !vmItem->CanBeMoved())
            return false;
    }

    return true;
} 

bool CrossPoolMigrateWizard::shouldShowNetworkPage() const
{
    return !this->isIntraPoolMigration();
}

bool CrossPoolMigrateWizard::shouldShowTransferNetworkPage() const
{
    return !this->isIntraPoolMove();
}

bool CrossPoolMigrateWizard::requiresTransferNetwork() const
{
    return this->shouldShowTransferNetworkPage();
}

bool CrossPoolMigrateWizard::isIntraPoolCopySelected() const
{
    return this->m_mode == WizardMode::Copy && this->m_intraPoolCopySelected;
}

bool CrossPoolMigrateWizard::isCopyCloneSelected() const
{
    return this->m_copyCloneRadio && this->m_copyCloneRadio->isChecked();
}

QString CrossPoolMigrateWizard::copyName() const
{
    return this->m_copyNameEdit ? this->m_copyNameEdit->text().trimmed() : QString();
}

QString CrossPoolMigrateWizard::copyDescription() const
{
    return this->m_copyDescriptionEdit ? this->m_copyDescriptionEdit->toPlainText().trimmed() : QString();
}

QString CrossPoolMigrateWizard::copyTargetSrRef() const
{
    return this->m_copySrPicker ? this->m_copySrPicker->GetSelectedSR() : QString();
}

void CrossPoolMigrateWizard::ensureMappingForVm(const QSharedPointer<VM>& vm)
{
    if (!vm)
        return;

    if (this->m_vmMappings.contains(vm->OpaqueRef()))
        return;

    VmMapping mapping(vm->OpaqueRef());
    mapping.vmNameLabel = vm->GetName();
    this->m_vmMappings.insert(vm->OpaqueRef(), mapping);
}

bool CrossPoolMigrateWizard::canMigrateVmToHost(const QSharedPointer<VM>& vm,
                                                XenConnection* targetConnection,
                                                const QString& hostRef,
                                                QString* reason) const
{
    if (!vm || !targetConnection)
        return false;

    XenCache* sourceCache = vm->GetConnection() ? vm->GetConnection()->GetCache() : nullptr;
    XenCache* targetCache = targetConnection->GetCache();
    if (!sourceCache || !targetCache)
        return false;

    if (this->m_mode == WizardMode::Move)
    {
        QString sourcePoolRef = poolRefForConnection(sourceCache);
        QString targetPoolRef = poolRefForConnection(targetCache);
        if (!sourcePoolRef.isEmpty() && sourcePoolRef == targetPoolRef)
            return true;
    }

    QString homeRef = vm->GetHomeRef();
    if (!homeRef.isEmpty() && homeRef == hostRef)
    {
        if (reason)
            *reason = tr("The VM is already on the selected host.");
        return false;
    }

    QVariantMap hostData = targetCache->ResolveObjectData("host", hostRef);
    QString targetVersion = hostData.value("software_version").toMap().value("product_version").toString();

    QString sourceHostRef = homeRef;
    if (sourceHostRef.isEmpty())
    {
        QString poolMasterRef = poolMasterRefForConnection(sourceCache);
        if (!poolMasterRef.isEmpty())
            sourceHostRef = poolMasterRef;
        else
            sourceHostRef = vm->GetResidentOnRef();
    }
    if (!sourceHostRef.isEmpty())
    {
        QVariantMap sourceHostData = sourceCache->ResolveObjectData("host", sourceHostRef);
        QString sourceVersion = sourceHostData.value("software_version").toMap().value("product_version").toString();
        if (!targetVersion.isEmpty() && !sourceVersion.isEmpty() &&
            compareVersions(targetVersion, sourceVersion) < 0)
        {
            if (reason)
                *reason = tr("The destination host is older than the current host.");
            return false;
        }
    }

    bool restrictDmc = hostData.value("restrict_dmc", false).toBool();
    QString powerState = vm->GetPowerState();
    if (restrictDmc &&
        (powerState == "Running" || powerState == "Paused" || powerState == "Suspended"))
    {
        if (vm->MemoryStaticMin() > vm->MemoryDynamicMin() ||
            vm->MemoryDynamicMin() != vm->MemoryDynamicMax() ||
            vm->MemoryDynamicMax() != vm->MemoryStaticMax())
        {
            if (reason)
                *reason = FriendlyErrorNames::getString(Failure::DYNAMIC_MEMORY_CONTROL_UNAVAILABLE);
            return false;
        }
    }

    if (this->canDoStorageMigration(vm, targetConnection, hostRef, reason))
        return true;

    // Allow intra-pool live migration when pool_migrate is available
    if (this->m_mode == WizardMode::Migrate && vm->GetConnection() == targetConnection)
    {
        QString ignored;
        if (VMOperationHelpers::VMCanBootOnHost(vm->GetConnection(), vm, hostRef, "pool_migrate", &ignored))
            return true;
    }

    if (reason && reason->isEmpty())
        *reason = tr("Migration is not supported for this target.");

    return false;
}

bool CrossPoolMigrateWizard::canDoStorageMigration(const QSharedPointer<VM>& vm,
                                                   XenConnection* targetConnection,
                                                   const QString& hostRef,
                                                   QString* reason) const
{
    if (!vm || !targetConnection)
        return false;

    XenConnection* sourceConnection = vm->GetConnection();
    XenCache* sourceCache = sourceConnection ? sourceConnection->GetCache() : nullptr;
    XenCache* targetCache = targetConnection->GetCache();
    if (!sourceCache || !targetCache)
        return false;

    // Find management network for target host
    QString managementNetworkRef;
    QList<QVariantMap> pifs = targetCache->GetAllData("pif");
    for (const QVariantMap& pifData : pifs)
    {
        if (!pifData.value("management", false).toBool())
            continue;
        if (pifData.value("host").toString() != hostRef)
            continue;
        managementNetworkRef = pifData.value("network").toString();
        if (!managementNetworkRef.isEmpty())
            break;
    }

    if (managementNetworkRef.isEmpty())
        managementNetworkRef = firstNetworkForHost(targetCache, hostRef);

    if (managementNetworkRef.isEmpty())
    {
        if (reason)
            *reason = tr("No transfer network available.");
        return false;
    }

    XenAPI::Session* destSession = XenAPI::Session::DuplicateSession(targetConnection->GetSession(), nullptr);
    if (!destSession || !destSession->IsLoggedIn())
    {
        if (reason)
            *reason = tr("Failed to create destination session.");
        return false;
    }

    QVariantMap receiveMapping = XenAPI::Host::migrate_receive(destSession, hostRef, managementNetworkRef, QVariantMap());

    // Build VDI map
    QVariantMap vdiMap;
    QStringList targetSrRefs;
    QStringList allTargetSrRefs = targetCache->GetAllRefs("sr");
    for (const QString& targetSrRef : allTargetSrRefs)
    {
        QSharedPointer<SR> targetSr = targetCache->ResolveObject<SR>("sr", targetSrRef);
        if (targetSr && targetSr->SupportsStorageMigration())
            targetSrRefs.append(targetSrRef);
    }
    QStringList vbdRefs = vm->GetVBDRefs();
    for (const QString& vbdRef : vbdRefs)
    {
        QVariantMap vbdData = sourceCache->ResolveObjectData("vbd", vbdRef);
        QString vdiRef = vbdData.value("VDI").toString();
        if (vdiRef.isEmpty() || vdiRef == "OpaqueRef:NULL")
            continue;

        QVariantMap vdiData = sourceCache->ResolveObjectData("vdi", vdiRef);
        QString srRef = vdiData.value("SR").toString();
        if (srRef.isEmpty())
            continue;

        QSharedPointer<SR> srObj = sourceCache->ResolveObject<SR>("sr", srRef);
        if (srObj && srObj->ContentType() == "iso")
            continue;

        for (const QString& targetSrRef : targetSrRefs)
        {
            if (targetSrRef != srRef)
            {
                vdiMap.insert(vdiRef, targetSrRef);
                break;
            }
        }
    }

    // Build VIF map
    QVariantMap vifMap;
    QString targetNetworkRef;
    QStringList targetNetworks = targetCache->GetAllRefs("network");
    for (const QString& networkRef : targetNetworks)
    {
        if (hostCanSeeNetwork(targetCache, hostRef, networkRef))
        {
            targetNetworkRef = networkRef;
            break;
        }
    }
    if (targetNetworkRef.isEmpty())
    {
        if (reason)
            *reason = tr("No compatible network available.");
        return false;
    }
    if (!targetNetworkRef.isEmpty())
    {
        QStringList vifRefs = vm->GetVIFRefs();
        for (const QString& vifRef : vifRefs)
            vifMap.insert(vifRef, targetNetworkRef);
    }

    try
    {
        XenAPI::VM::assert_can_migrate(sourceConnection->GetSession(),
                                       vm->OpaqueRef(),
                                       receiveMapping,
                                       true,
                                       vdiMap,
                                       targetConnection == sourceConnection ? QVariantMap() : vifMap,
                                       QVariantMap());
        return true;
    }
    catch (const Failure& failure)
    {
        QStringList params = failure.errorDescription();
        if (!params.isEmpty() && params[0] == Failure::VIF_NOT_IN_MAP && snapshotsContainExtraVifs(vm))
            return true;

        if (reason)
        {
            if (!params.isEmpty() && params[0] == Failure::RBAC_PERMISSION_DENIED)
                *reason = failure.message().split('\n').first().trimmed();
            else if (params.size() > 1 && params[1].contains(Failure::DYNAMIC_MEMORY_CONTROL_UNAVAILABLE))
                *reason = FriendlyErrorNames::getString(Failure::DYNAMIC_MEMORY_CONTROL_UNAVAILABLE);
            else
                *reason = failure.message();
        }
        return false;
    }
    catch (const std::exception&)
    {
        if (reason)
            *reason = tr("Unknown error checking this server");
        return false;
    }
}

bool CrossPoolMigrateWizard::snapshotsContainExtraVifs(const QSharedPointer<VM>& vm) const
{
    if (!vm)
        return false;

    QStringList vmVifs = vm->GetVIFRefs();
    QStringList snapVifs = collectSnapshotVifRefs(vm);
    for (const QString& vifRef : snapVifs)
    {
        if (!vmVifs.contains(vifRef))
            return true;
    }
    return false;
}

QStringList CrossPoolMigrateWizard::collectSnapshotVifRefs(const QSharedPointer<VM>& vm) const
{
    QStringList result;
    if (!vm || !vm->GetConnection())
        return result;

    XenCache* cache = vm->GetConnection()->GetCache();
    if (!cache)
        return result;

    QStringList snapshots = vm->GetSnapshotRefs();
    for (const QString& snapRef : snapshots)
    {
        QVariantMap snapData = cache->ResolveObjectData("vm", snapRef);
        QVariantList vifList = snapData.value("VIFs").toList();
        for (const QVariant& vifVar : vifList)
        {
            QString vifRef = vifVar.toString();
            if (!vifRef.isEmpty())
                result.append(vifRef);
        }
    }

    return result;
}

XenConnection* CrossPoolMigrateWizard::resolveTargetConnection(const QString& hostRef) const
{
    Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
    if (!connMgr)
        return nullptr;

    const QList<XenConnection*> connections = connMgr->GetAllConnections();
    for (XenConnection* conn : connections)
    {
        if (!conn || !conn->IsConnected())
            continue;

        XenCache* cache = conn->GetCache();
        if (!cache)
            continue;

        if (cache->Contains("host", hostRef))
            return conn;
    }

    return nullptr;
}
