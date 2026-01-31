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

#include <QComboBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QIcon>
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
#include <QVector>
#include "crosspoolmigratewizard.h"
#include "crosspoolmigratewizardpages.h"
#include "crosspoolmigratewizard_copymodepage.h"
#include "crosspoolmigratewizard_intrapoolcopypage.h"
#include "ui_crosspoolmigratewizard.h"
#include "../mainwindow.h"
#include "../operations/operationmanager.h"
#include "../commands/vm/vmoperationhelpers.h"
#include "../controls/srpicker.h"
#include "../widgets/wizardnavigationpane.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/network.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/vdi.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/friendlyerrornames.h"
#include "xenlib/xen/failure.h"
#include "xenlib/xen/actions/vm/vmcrosspoolmigrateaction.h"
#include "xenlib/xen/actions/vm/vmmigrateaction.h"
#include "xenlib/xen/actions/vm/vmmoveaction.h"
#include "xenlib/xen/actions/vm/vmcopyaction.h"
#include "xenlib/xen/actions/vm/vmcloneaction.h"
#include "xenlib/xen/actions/vm/resumeandstartvmsaction.h"
#include "xenlib/operations/multipleaction.h"
#include "xenlib/xen/network/connectionsmanager.h"
#include "xenlib/xen/xenapi/xenapi_Host.h"
#include "xenlib/xen/xenapi/xenapi_VM.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/session.h"

namespace
{
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

        QList<QVariantMap> pools = cache->GetAllData(XenObjectType::Pool);
        if (pools.isEmpty())
            return QString();

        return pools.first().value("opaque_ref").toString();
    }

    QString poolMasterRefForConnection(XenCache* cache)
    {
        if (!cache)
            return QString();

        QList<QVariantMap> pools = cache->GetAllData(XenObjectType::Pool);
        if (pools.isEmpty())
            return QString();

        return pools.first().value("master").toString();
    }

    bool hostCanSeeNetwork(XenCache* cache, const QString& hostRef, const QString& networkRef)
    {
        if (!cache || hostRef.isEmpty() || networkRef.isEmpty())
            return false;

        QList<QVariantMap> pifs = cache->GetAllData(XenObjectType::PIF);
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

        QList<QVariantMap> pifs = cache->GetAllData(XenObjectType::PIF);
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

CrossPoolMigrateWizard::CrossPoolMigrateWizard(MainWindow* mainWindow, const QSharedPointer<VM>& vm, WizardMode mode, bool resumeAfterMigrate, QWidget* parent)
    : CrossPoolMigrateWizard(mainWindow,
                             vm ? QList<QSharedPointer<VM>>{vm} : QList<QSharedPointer<VM>>(),
                             mode,
                             resumeAfterMigrate,
                             parent)
{
}

CrossPoolMigrateWizard::CrossPoolMigrateWizard(MainWindow* mainWindow, const QList<QSharedPointer<VM>>& vms, WizardMode mode, bool resumeAfterMigrate, QWidget* parent)
    : QWizard(parent),
      ui(new Ui::CrossPoolMigrateWizard),
      m_mainWindow(mainWindow),
      m_vms(vms),
      m_sourceConnection(vms.isEmpty() ? nullptr : vms.first()->GetConnection()),
      m_mode(mode),
      m_resumeAfterMigrate(resumeAfterMigrate)
{
    this->ui->setupUi(this);
    if (this->m_mode == WizardMode::Copy)
        this->setWindowTitle(tr("Copy VM Wizard"));
    else if (this->m_mode == WizardMode::Move)
        this->setWindowTitle(tr("Move VM Wizard"));
    else
        this->setWindowTitle(tr("Cross Pool Migrate Wizard"));
    this->setWizardStyle(QWizard::ModernStyle);
    this->setOption(QWizard::HaveHelpButton, true);
    this->setOption(QWizard::HelpButtonOnRight, false);

    for (const QSharedPointer<VM>& vmItem : this->m_vms)
        this->ensureMappingForVm(vmItem);

    this->setupWizardPages();
    if (this->m_mode == WizardMode::Copy)
        this->setStartId(Page_CopyMode);
    else
        this->setStartId(Page_Destination);
    this->updateWizardPages();
    this->setupNavigationPane();
    this->updateRbacRequirement();

    connect(this, &QWizard::currentIdChanged, this, [this](int) {
        this->updateNavigationPane();
        this->updateNextButtonVisibility();
    });
}

CrossPoolMigrateWizard::~CrossPoolMigrateWizard()
{
    delete this->ui;
}

void CrossPoolMigrateWizard::setupWizardPages()
{
    this->m_destinationPage = this->createDestinationPage();
    this->m_rbacPage = this->createRbacWarningPage();
    this->m_storagePage = this->createStoragePage();
    this->m_networkPage = this->createNetworkPage();
    this->m_transferPage = this->createTransferNetworkPage();
    this->m_finishPage = this->createFinishPage();
    this->m_copyModePage = this->createCopyModePage();
    this->m_intraPoolCopyPage = this->createIntraPoolCopyPage();

    this->setPage(Page_Destination, this->m_destinationPage);
    this->setPage(Page_RbacWarning, this->m_rbacPage);
    this->setPage(Page_Storage, this->m_storagePage);
    this->setPage(Page_Network, this->m_networkPage);
    this->setPage(Page_TransferNetwork, this->m_transferPage);
    this->setPage(Page_Finish, this->m_finishPage);
    this->setPage(Page_CopyMode, this->m_copyModePage);
    this->setPage(Page_IntraPoolCopy, this->m_intraPoolCopyPage);

    for (QWizardPage* page : {this->m_destinationPage, this->m_rbacPage, this->m_storagePage,
                              this->m_networkPage, this->m_transferPage, this->m_finishPage,
                              this->m_copyModePage, this->m_intraPoolCopyPage})
    {
        if (page)
            connect(page, &QWizardPage::completeChanged, this, &CrossPoolMigrateWizard::updateNextButtonVisibility);
    }
}

void CrossPoolMigrateWizard::setupNavigationPane()
{
    this->m_navigationPane = new WizardNavigationPane(this);
    this->setSideWidget(this->m_navigationPane);
    this->updateNavigationPane();
}

void CrossPoolMigrateWizard::updateNavigationPane()
{
    if (!this->m_navigationPane)
        return;

    QVector<WizardNavigationPane::Step> steps;
    QVector<int> stepIds;
    auto addStep = [&](int id, const QString& title)
    {
        steps.push_back({title, QIcon()});
        stepIds.push_back(id);
    };

    if (this->m_mode == WizardMode::Copy)
    {
        addStep(Page_CopyMode, tr("Copy Mode"));
        if (this->isIntraPoolCopySelected())
        {
            if (this->requiresRbacWarning())
                addStep(Page_RbacWarning, tr("Permissions"));
            addStep(Page_IntraPoolCopy, tr("Copy Within Pool"));
        }
        else
        {
            addStep(Page_Destination, tr("Destination"));
            if (this->requiresRbacWarning())
                addStep(Page_RbacWarning, tr("Permissions"));
            addStep(Page_Storage, tr("Storage"));
            if (this->shouldShowNetworkPage())
                addStep(Page_Network, tr("Networking"));
            if (this->shouldShowTransferNetworkPage())
                addStep(Page_TransferNetwork, tr("Transfer Network"));
            addStep(Page_Finish, tr("Finish"));
        }
    }
    else
    {
        addStep(Page_Destination, tr("Destination"));
        if (this->requiresRbacWarning())
            addStep(Page_RbacWarning, tr("Permissions"));
        addStep(Page_Storage, tr("Storage"));
        if (this->shouldShowNetworkPage())
            addStep(Page_Network, tr("Networking"));
        if (this->shouldShowTransferNetworkPage())
            addStep(Page_TransferNetwork, tr("Transfer Network"));
        addStep(Page_Finish, tr("Finish"));
    }

    if (stepIds != this->m_navigationSteps)
    {
        this->m_navigationSteps = stepIds;
        this->m_navigationPane->setSteps(steps);
    }

    this->updateNavigationSelection();
}

void CrossPoolMigrateWizard::updateNavigationSelection()
{
    if (!this->m_navigationPane)
        return;

    int idx = this->m_navigationSteps.indexOf(this->currentId());
    if (idx < 0)
        idx = 0;
    this->m_navigationPane->setCurrentStep(idx);
}

void CrossPoolMigrateWizard::updateNextButtonVisibility()
{
    if (QAbstractButton* nextButton = this->button(QWizard::NextButton))
    {
        bool isFinal = this->currentPage() && this->currentPage()->isFinalPage();
        nextButton->setVisible(!isFinal);
    }
}

void CrossPoolMigrateWizard::updateWizardPages()
{
    bool copyMode = (this->m_mode == WizardMode::Copy);
    bool intraCopy = copyMode && this->isIntraPoolCopySelected();
    bool needsRbac = this->requiresRbacWarning();
    bool needsStorage = !intraCopy;
    bool needsNetwork = !intraCopy && this->shouldShowNetworkPage();
    bool needsTransfer = !intraCopy && this->shouldShowTransferNetworkPage();
    bool needsDestination = !copyMode || !intraCopy;

    auto ensurePage = [this](int id, QWizardPage* page)
    {
        if (page && this->page(id) != page)
            this->setPage(id, page);
    };

    auto removePageIf = [this](int id, bool remove)
    {
        if (remove && this->page(id) != nullptr)
            this->removePage(id);
    };

    if (copyMode)
        ensurePage(Page_CopyMode, this->m_copyModePage);
    else
        removePageIf(Page_CopyMode, true);

    if (intraCopy)
        ensurePage(Page_IntraPoolCopy, this->m_intraPoolCopyPage);
    else
        removePageIf(Page_IntraPoolCopy, true);

    if (needsRbac)
        ensurePage(Page_RbacWarning, this->m_rbacPage);
    else
        removePageIf(Page_RbacWarning, true);

    if (needsDestination)
        ensurePage(Page_Destination, this->m_destinationPage);
    else
        removePageIf(Page_Destination, true);

    if (needsStorage)
        ensurePage(Page_Storage, this->m_storagePage);
    else
        removePageIf(Page_Storage, true);

    if (needsNetwork)
        ensurePage(Page_Network, this->m_networkPage);
    else
        removePageIf(Page_Network, true);

    if (needsTransfer)
        ensurePage(Page_TransferNetwork, this->m_transferPage);
    else
        removePageIf(Page_TransferNetwork, true);

    if (!intraCopy)
        ensurePage(Page_Finish, this->m_finishPage);
    else
        removePageIf(Page_Finish, true);

    int desiredStartId = copyMode ? Page_CopyMode : Page_Destination;
    if (this->page(desiredStartId) != nullptr)
        this->setStartId(desiredStartId);

    if (this->m_intraPoolCopyPage)
        this->m_intraPoolCopyPage->setFinalPage(intraCopy);

    this->updateNextButtonVisibility();
}

QWizardPage* CrossPoolMigrateWizard::createDestinationPage()
{
    QWizardPage* page = this->ui->pageDestination;
    if (!page)
        page = new DestinationWizardPage(this);
    page->setTitle(tr("Destination"));

    if (auto* destPage = qobject_cast<DestinationWizardPage*>(page))
        destPage->setWizard(this);

    this->m_poolCombo = this->ui->poolComboBox;
    this->m_hostCombo = this->ui->hostComboBox;

    if (this->ui->destinationIntroLabel)
    {
        this->ui->destinationIntroLabel->setText(tr("Select the destination pool and host for the VM."));
        this->ui->destinationIntroLabel->setWordWrap(true);
    }

    if (this->m_poolCombo)
    {
        connect(this->m_poolCombo,
                static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                this,
                [this](int) {
            QVariantMap data = this->m_poolCombo->currentData().toMap();
            QString poolRef = data.value("poolRef").toString();
            QString hostRef = data.value("hostRef").toString();
            QString connHost = data.value("connectionHost").toString();
            int connPort = data.value("connectionPort").toInt();
            XenConnection* conn = Xen::ConnectionsManager::instance()
                ? Xen::ConnectionsManager::instance()->FindConnectionByHostname(connHost, connPort)
                : nullptr;
            this->m_targetPoolRef = poolRef;

            this->populateHostsForPool(poolRef, conn, hostRef);
            if (!hostRef.isEmpty() && this->m_hostCombo)
            {
                int idx = this->m_hostCombo->findData(hostRef);
                if (idx >= 0)
                    this->m_hostCombo->setCurrentIndex(idx);
            }
            this->updateDestinationMapping();
            this->updateNavigationPane();
        });
    }

    if (this->m_hostCombo)
    {
        connect(this->m_hostCombo,
                static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                this,
                [this](int) {
            this->updateDestinationMapping();
            this->updateNavigationPane();
        });
    }

    return page;
}

QWizardPage* CrossPoolMigrateWizard::createStoragePage()
{
    QWizardPage* page = this->ui->pageStorage;
    if (!page)
        page = new StorageWizardPage(this);
    page->setTitle(tr("Storage"));

    if (auto* storagePage = qobject_cast<StorageWizardPage*>(page))
        storagePage->setWizard(this);

    if (this->ui->storageIntroLabel)
    {
        this->ui->storageIntroLabel->setText(tr("Select storage repositories for each virtual disk."));
        this->ui->storageIntroLabel->setWordWrap(true);
    }

    this->m_storageTable = this->ui->storageTable;
    if (this->m_storageTable)
    {
        this->m_storageTable->setColumnCount(3);
        this->m_storageTable->setHorizontalHeaderLabels({tr("VM"), tr("Virtual disk"), tr("Target SR")});
        this->m_storageTable->horizontalHeader()->setStretchLastSection(true);
        this->m_storageTable->verticalHeader()->setVisible(false);
        this->m_storageTable->setSelectionMode(QAbstractItemView::NoSelection);
        this->m_storageTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    }

    return page;
}

QWizardPage* CrossPoolMigrateWizard::createNetworkPage()
{
    QWizardPage* page = this->ui->pageNetwork;
    if (!page)
        page = new NetworkWizardPage(this);
    page->setTitle(tr("Networking"));

    if (auto* networkPage = qobject_cast<NetworkWizardPage*>(page))
        networkPage->setWizard(this);

    if (this->ui->networkIntroLabel)
    {
        this->ui->networkIntroLabel->setText(tr("Select networks for each virtual interface."));
        this->ui->networkIntroLabel->setWordWrap(true);
    }

    this->m_networkTable = this->ui->networkTable;
    if (this->m_networkTable)
    {
        this->m_networkTable->setColumnCount(3);
        this->m_networkTable->setHorizontalHeaderLabels({tr("VM"), tr("VIF (MAC)"), tr("Target Network")});
        this->m_networkTable->horizontalHeader()->setStretchLastSection(true);
        this->m_networkTable->verticalHeader()->setVisible(false);
        this->m_networkTable->setSelectionMode(QAbstractItemView::NoSelection);
        this->m_networkTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    }

    return page;
}

QWizardPage* CrossPoolMigrateWizard::createTransferNetworkPage()
{
    QWizardPage* page = this->ui->pageTransfer;
    if (!page)
        page = new TransferWizardPage(this);
    page->setTitle(tr("Transfer Network"));

    if (auto* transferPage = qobject_cast<TransferWizardPage*>(page))
        transferPage->setWizard(this);

    if (this->ui->transferIntroLabel)
    {
        this->ui->transferIntroLabel->setText(tr("Select the network used for transferring VM data."));
        this->ui->transferIntroLabel->setWordWrap(true);
    }

    this->m_transferNetworkCombo = this->ui->transferNetworkComboBox;

    return page;
}

QWizardPage* CrossPoolMigrateWizard::createRbacWarningPage()
{
    QWizardPage* page = this->ui->pageRbac;
    if (!page)
        page = new RbacWizardPage(this);
    page->setTitle(tr("Permissions"));

    this->m_rbacInfoLabel = this->ui->rbacInfoLabel;
    this->m_rbacConfirm = this->ui->rbacConfirmCheckBox;

    if (!this->m_rbacInfoLabel)
    {
        this->m_rbacInfoLabel = new QLabel(tr("The target connection may require permissions "
                                              "to perform cross-pool migration. If you do not "
                                              "have the required role, the operation will fail."), page);
        this->m_rbacInfoLabel->setWordWrap(true);
    }

    if (!this->m_rbacConfirm)
    {
        this->m_rbacConfirm = new QCheckBox(tr("I have the required permissions to continue."), page);
        this->m_rbacConfirm->setChecked(false);
    }

    if (!this->ui->pageRbac)
    {
        QVBoxLayout* layout = new QVBoxLayout(page);
        layout->addWidget(this->m_rbacInfoLabel);
        layout->addWidget(this->m_rbacConfirm);
    }

    if (auto* rbacPage = qobject_cast<RbacWizardPage*>(page))
    {
        rbacPage->setWizard(this);
        rbacPage->setConfirmation(this->m_rbacConfirm);
    }

    this->m_rbacPage = page;
    return page;
}

QWizardPage* CrossPoolMigrateWizard::createFinishPage()
{
    QWizardPage* page = this->ui->pageFinish;
    if (!page)
        page = new QWizardPage(this);
    page->setTitle(tr("Finish"));

    this->m_summaryText = this->ui->summaryTextEdit;
    if (this->m_summaryText)
        this->m_summaryText->setReadOnly(true);

    return page;
}

QWizardPage* CrossPoolMigrateWizard::createCopyModePage()
{
    // Create list of VM refs for the page
    QList<QString> vmRefs;
    for (const QSharedPointer<VM>& vm : this->m_vms)
    {
        if (vm)
            vmRefs.append(vm->OpaqueRef());
    }

    CrossPoolMigrateCopyModePage* page = new CrossPoolMigrateCopyModePage(vmRefs, this);
    connect(page, &QWizardPage::completeChanged, this, &CrossPoolMigrateWizard::updateRbacRequirement);
    connect(page, &QWizardPage::completeChanged, this, &CrossPoolMigrateWizard::updateWizardPages);
    connect(page, &QWizardPage::completeChanged, this, &CrossPoolMigrateWizard::updateNavigationPane);
    this->m_copyModePage = page;
    return page;
}

QWizardPage* CrossPoolMigrateWizard::createIntraPoolCopyPage()
{
    // Create list of VM refs for the page
    QList<QString> vmRefs;
    for (const QSharedPointer<VM>& vm : this->m_vms)
    {
        if (vm)
            vmRefs.append(vm->OpaqueRef());
    }

    IntraPoolCopyPage* page = new IntraPoolCopyPage(vmRefs, this);
    this->m_intraPoolCopyPage = page;
    return page;
}

void CrossPoolMigrateWizard::initializePage(int id)
{
    if (id == Page_Destination)
    {
        this->populateDestinationHosts();
    }
    else if (id == Page_RbacWarning)
    {
        this->updateRbacRequirement();
    }
    else if (id == Page_Storage)
    {
        this->populateStorageMappings();
    }
    else if (id == Page_Network)
    {
        this->populateNetworkMappings();
    }
    else if (id == Page_TransferNetwork)
    {
        this->populateTransferNetworks();
    }
    else if (id == Page_Finish)
    {
        this->updateSummary();
    }
    // Note: Page_CopyMode and Page_IntraPoolCopy handle their own initialization

    QWizard::initializePage(id);
    this->updateNavigationPane();
}

bool CrossPoolMigrateWizard::validateCurrentPage()
{
    if (!this->allVMsAvailable())
    {
        QMessageBox::warning(this, tr("Cross Pool Migrate"), tr("One or more selected VMs are no longer available."));
        return false;
    }

    if (this->currentId() == Page_IntraPoolCopy)
    {
        IntraPoolCopyPage* intraPage = qobject_cast<IntraPoolCopyPage*>(this->m_intraPoolCopyPage);
        if (!intraPage)
            return false;
        
        // Name validation
        if (intraPage->NewVmName().trimmed().isEmpty())
        {
            QMessageBox::warning(this, tr("Copy VM"), tr("Please enter a name for the copied VM."));
            return false;
        }
        
        // Full copy mode requires SR selection
        if (!intraPage->CloneVM() && intraPage->SelectedSR().isEmpty())
        {
            QMessageBox::warning(this, tr("Copy VM"), tr("Please select a target SR for full copy mode."));
            return false;
        }
    }
    else if (this->currentId() == Page_Destination)
    {
        if (!this->m_poolCombo || this->m_poolCombo->currentIndex() < 0)
        {
            QMessageBox::warning(this, tr("Cross Pool Migrate"), tr("Please select a destination pool or host."));
            return false;
        }

        auto* poolModel = qobject_cast<QStandardItemModel*>(this->m_poolCombo->model());
        if (poolModel)
        {
            QStandardItem* item = poolModel->item(this->m_poolCombo->currentIndex());
            if (item && !item->isEnabled())
            {
                QMessageBox::warning(this, tr("Cross Pool Migrate"), tr("Selected host is not eligible for migration."));
                return false;
            }
        }

        if (this->m_hostCombo && this->m_hostCombo->isEnabled())
        {
            auto* hostModel = qobject_cast<QStandardItemModel*>(this->m_hostCombo->model());
            if (hostModel)
            {
                QStandardItem* item = hostModel->item(this->m_hostCombo->currentIndex());
                if (item && !item->isEnabled())
                {
                    QMessageBox::warning(this, tr("Cross Pool Migrate"), tr("Selected host is not eligible for migration."));
                    return false;
                }
            }
        }

        this->updateDestinationMapping();
        this->updateWizardPages();
        this->updateNavigationPane();
    }
    else if (this->currentId() == Page_TransferNetwork && this->requiresTransferNetwork())
    {
        if (!this->m_transferNetworkCombo || this->m_transferNetworkCombo->currentIndex() < 0)
        {
            QMessageBox::warning(this, tr("Cross Pool Migrate"), tr("Please select a transfer network."));
            return false;
        }
    }
    else if (this->currentId() == Page_Storage)
    {
        this->updateStorageMapping();
    }
    else if (this->currentId() == Page_Network)
    {
        this->updateNetworkMapping();
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
                VMCloneAction* action = new VMCloneAction(vmItem, this->copyName(), this->copyDescription(), nullptr);
                OperationManager::instance()->RegisterOperation(action);
                action->RunAsync(true);
            }
            else
            {
                XenCache* cache = this->m_sourceConnection->GetCache();
                QSharedPointer<SR> sr = cache ? cache->ResolveObject<SR>(XenObjectType::SR, this->copyTargetSrRef()) : QSharedPointer<SR>();
                if (sr && sr->IsValid())
                {
                    VMCopyAction* action = new VMCopyAction(vmItem, QSharedPointer<Host>(), sr, this->copyName(), this->copyDescription(), nullptr);
                    OperationManager::instance()->RegisterOperation(action);
                    action->RunAsync(true);
                }
            }
        }

        QWizard::accept();
        return;
    }

    this->updateDestinationMapping();
    this->updateStorageMapping();
    this->updateNetworkMapping();

    if (this->m_transferNetworkCombo)
        this->m_transferNetworkRef = this->m_transferNetworkCombo->currentData().toString();

    if (this->m_targetHostRef.isEmpty() && !this->m_targetPoolRef.isEmpty() && this->m_targetConnection)
    {
        XenCache* targetCache = this->m_targetConnection->GetCache();
        if (targetCache)
        {
            QVariantMap poolData = targetCache->ResolveObjectData(XenObjectType::Pool, this->m_targetPoolRef);
            this->m_targetHostRef = poolData.value("master").toString();
        }
    }

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

            QVariantMap vdiData = cache->ResolveObjectData(XenObjectType::VDI, vdiRef);
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

                    QSharedPointer<SR> srObj = sourceCache->ResolveObject<SR>(XenObjectType::SR, srRef);
                    if (srObj && srObj->IsValid())
                        storageMap.insert(vdiRef, srObj);
                }

                QSharedPointer<Host> hostObj = sourceCache->ResolveObject<Host>(XenObjectType::Host, this->m_targetHostRef);

                VMMoveAction* action = new VMMoveAction(vm, storageMap, hostObj, nullptr);
                OperationManager::instance()->RegisterOperation(action);
                action->RunAsync(true);
            }
        } else
        {
            bool useCrossPool = (this->m_mode == WizardMode::Copy) || hasStorageMotion || !sameConnection;
            AsyncOperation* migrateAction = nullptr;
            if (useCrossPool)
            {
                migrateAction = new VMCrossPoolMigrateAction(
                    this->m_sourceConnection,
                    this->m_targetConnection,
                    vm->OpaqueRef(),
                    this->m_targetHostRef,
                    this->m_transferNetworkRef,
                    mapping,
                    this->m_mode == WizardMode::Copy,
                    nullptr);
            } else
            {
                QSharedPointer<Host> host = vm->GetCache()->ResolveObject<Host>(XenObjectType::Host, this->m_targetHostRef);
                migrateAction = new VMMigrateAction(vm, host, nullptr);
            }

            if (this->m_resumeAfterMigrate && this->m_mode == WizardMode::Migrate && migrateAction)
            {
                QSharedPointer<Host> host = vm->GetCache()->ResolveObject<Host>(XenObjectType::Host, this->m_targetHostRef);
                QList<QSharedPointer<VM>> resumeList;
                resumeList.append(vm);
                ResumeAndStartVMsAction* resumeAction = new ResumeAndStartVMsAction(
                    vm->GetConnection(),
                    host,
                    resumeList,
                    QList<QSharedPointer<VM>>(),
                    nullptr,
                    nullptr,
                    nullptr);

                QList<AsyncOperation*> actions;
                actions.append(migrateAction);
                actions.append(resumeAction);

                MultipleAction* multi = new MultipleAction(
                    vm->GetConnection(),
                    migrateAction->GetTitle(),
                    tr("Migrating VM..."),
                    tr("VM migrated"),
                    actions,
                    true,
                    false,
                    true,
                    nullptr);

                OperationManager::instance()->RegisterOperation(multi);
                multi->RunAsync(true);
            } else if (migrateAction)
            {
                OperationManager::instance()->RegisterOperation(migrateAction);
                migrateAction->RunAsync(true);
            }
        }
    }
    QWizard::accept();
}

void CrossPoolMigrateWizard::populateDestinationHosts()
{
    if (!this->m_poolCombo || !this->m_hostCombo)
        return;

    this->populateDestinationPools();
} 

void CrossPoolMigrateWizard::populateDestinationPools()
{
    this->m_poolCombo->clear();
    this->m_hostCombo->clear();
    this->m_targetPoolRef.clear();
    this->m_targetHostRef.clear();

    QSet<XenConnection*> ignoredConnections;
    if (this->m_mode == WizardMode::Copy)
    {
        for (const QSharedPointer<VM>& vmItem : this->m_vms)
        {
            if (vmItem)
                ignoredConnections.insert(vmItem->GetConnection());
        }
    }

    const QList<XenConnection*> connections = Xen::ConnectionsManager::instance()->GetAllConnections();
    for (XenConnection* conn : connections)
    {
        if (!conn || !conn->IsConnected())
            continue;
        if (ignoredConnections.contains(conn))
            continue;

        XenCache* cache = conn->GetCache();
        if (!cache)
            continue;

        QStringList poolRefs = cache->GetAllRefs(XenObjectType::Pool);
        if (!poolRefs.isEmpty())
        {
            QString poolRef = poolRefs.first();
            QVariantMap poolData = cache->ResolveObjectData(XenObjectType::Pool, poolRef);
            QString poolName = poolData.value("name_label", tr("Pool")).toString();

            QString failureReason;
            bool eligible = false;
            QStringList hostRefs = cache->GetAllRefs(XenObjectType::Host);
            for (const QString& hostRef : hostRefs)
            {
                bool hostEligible = true;
                for (const QSharedPointer<VM>& vmItem : this->m_vms)
                {
                    if (!vmItem)
                        continue;
                    if (!this->canMigrateVmToHost(vmItem, conn, hostRef, &failureReason))
                    {
                        hostEligible = false;
                        break;
                    }
                }
                if (hostEligible)
                {
                    eligible = true;
                    break;
                }
            }

            QString label = eligible ? poolName : QString("%1 (%2)").arg(poolName, failureReason);
            QVariantMap data;
            data.insert("poolRef", poolRef);
            data.insert("connectionHost", conn->GetHostname());
            data.insert("connectionPort", conn->GetPort());
            this->m_poolCombo->addItem(label, data);
            int index = this->m_poolCombo->count() - 1;
            if (!eligible)
            {
                auto* model = qobject_cast<QStandardItemModel*>(this->m_poolCombo->model());
                if (model)
                {
                    QStandardItem* item = model->item(index);
                    if (item)
                        item->setEnabled(false);
                }
            }
        } else
        {
            QStringList hostRefs = cache->GetAllRefs(XenObjectType::Host);
            for (const QString& hostRef : hostRefs)
            {
                QVariantMap hostData = cache->ResolveObjectData(XenObjectType::Host, hostRef);
                QString hostName = hostData.value("name_label", tr("Host")).toString();

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
                QVariantMap data;
                data.insert("hostRef", hostRef);
                data.insert("connectionHost", conn->GetHostname());
                data.insert("connectionPort", conn->GetPort());
                this->m_poolCombo->addItem(label, data);
                int index = this->m_poolCombo->count() - 1;
                if (!eligible)
                {
                    auto* model = qobject_cast<QStandardItemModel*>(this->m_poolCombo->model());
                    if (model)
                    {
                        QStandardItem* item = model->item(index);
                        if (item)
                            item->setEnabled(false);
                    }
                }
            }
        }
    }

    if (this->m_poolCombo->count() > 0)
    {
        int firstEnabled = 0;
        auto* model = qobject_cast<QStandardItemModel*>(this->m_poolCombo->model());
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

        this->m_poolCombo->setCurrentIndex(firstEnabled);
    }
}

void CrossPoolMigrateWizard::populateHostsForPool(const QString& poolRef, XenConnection* connection, const QString& standaloneHostRef)
{
    this->m_hostCombo->clear();
    this->m_targetHostRef.clear();
    this->m_targetConnection = connection;

    if (!connection)
        return;

    XenCache* cache = connection->GetCache();
    if (!cache)
        return;

    QStringList hostRefs;
    if (!standaloneHostRef.isEmpty())
    {
        hostRefs.append(standaloneHostRef);
        this->m_hostCombo->setEnabled(false);
    }
    else
    {
        hostRefs = cache->GetAllRefs(XenObjectType::Host);
        this->m_hostCombo->setEnabled(true);
    }

    for (const QString& hostRef : hostRefs)
    {
        QVariantMap hostData = cache->ResolveObjectData(XenObjectType::Host, hostRef);
        QString hostName = hostData.value("name_label", tr("Host")).toString();

        QString failureReason;
        bool eligible = true;
        for (const QSharedPointer<VM>& vmItem : this->m_vms)
        {
            if (!vmItem)
                continue;
            if (!this->canMigrateVmToHost(vmItem, connection, hostRef, &failureReason))
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
    }

    this->updateDestinationMapping();
}

void CrossPoolMigrateWizard::updateDestinationMapping()
{
    if (this->m_poolCombo)
    {
        QVariantMap data = this->m_poolCombo->currentData().toMap();
        this->m_targetPoolRef = data.value("poolRef").toString();
        if (this->m_targetConnection == nullptr)
        {
            QString connHost = data.value("connectionHost").toString();
            int connPort = data.value("connectionPort").toInt();
            this->m_targetConnection = Xen::ConnectionsManager::instance()
                ? Xen::ConnectionsManager::instance()->FindConnectionByHostname(connHost, connPort)
                : nullptr;
        }
    }

    if (this->m_hostCombo && this->m_hostCombo->currentIndex() >= 0)
        this->m_targetHostRef = this->m_hostCombo->currentData().toString();

    this->updateRbacRequirement();

    QString targetName;
    if (!this->m_targetPoolRef.isEmpty() && this->m_poolCombo)
        targetName = this->m_poolCombo->currentText();
    else if (this->m_hostCombo)
        targetName = this->m_hostCombo->currentText();

    for (auto it = this->m_vmMappings.begin(); it != this->m_vmMappings.end(); ++it)
    {
        it.value().targetName = targetName;
        it.value().targetRef = !this->m_targetPoolRef.isEmpty() ? this->m_targetPoolRef : this->m_targetHostRef;
    }

    this->updateWizardPages();
}

void CrossPoolMigrateWizard::updateStorageMapping()
{
    if (!this->m_storageTable)
        return;

    for (auto it = this->m_vmMappings.begin(); it != this->m_vmMappings.end(); ++it)
        it.value().storage.clear();

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

void CrossPoolMigrateWizard::updateNetworkMapping()
{
    if (!this->m_networkTable)
        return;

    for (auto it = this->m_vmMappings.begin(); it != this->m_vmMappings.end(); ++it)
        it.value().vifs.clear();

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

void CrossPoolMigrateWizard::populateStorageMappings()
{
    if (!this->m_storageTable || this->m_vms.isEmpty() || !this->m_sourceConnection)
        return;

    this->m_storageTable->setRowCount(0);

    XenCache* sourceCache = this->m_sourceConnection->GetCache();
    XenCache* targetCache = this->m_targetConnection ? this->m_targetConnection->GetCache() : nullptr;
    if (!sourceCache || !targetCache)
        return;

    QStringList srRefs = targetCache->GetAllRefs(XenObjectType::SR);
    QString defaultSrRef;
    QList<QVariantMap> pools = targetCache->GetAllData(XenObjectType::Pool);
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
            QVariantMap vbdData = sourceCache->ResolveObjectData(XenObjectType::VBD, vbdRef);
            QString vdiRef = vbdData.value("VDI").toString();
            if (vdiRef.isEmpty() || vdiRef == "OpaqueRef:NULL")
                continue;

            QVariantMap vdiData = sourceCache->ResolveObjectData(XenObjectType::VDI, vdiRef);
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
                QVariantMap srData = targetCache->ResolveObjectData(XenObjectType::SR, srRef);
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

    QStringList networkRefs = targetCache->GetAllRefs(XenObjectType::Network);
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
            QVariantMap vifData = sourceCache->ResolveObjectData(XenObjectType::VIF, vifRef);
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
                QVariantMap netData = targetCache->ResolveObjectData(XenObjectType::Network, netRef);
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

    QStringList networkRefs = targetCache->GetAllRefs(XenObjectType::Network);
    for (const QString& netRef : networkRefs)
    {
        QVariantMap netData = targetCache->ResolveObjectData(XenObjectType::Network, netRef);
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

    this->updateDestinationMapping();
    this->updateStorageMapping();
    this->updateNetworkMapping();

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
    QString targetLabel = tr("Unknown");
    if (!this->m_targetPoolRef.isEmpty() && this->m_poolCombo)
        targetLabel = this->m_poolCombo->currentText();
    else if (this->m_hostCombo)
        targetLabel = this->m_hostCombo->currentText();
    summary += tr("Target: %1\n").arg(targetLabel);
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
    if (!this->m_sourceConnection)
    {
        this->m_requiresRbacWarning = false;
        return;
    }

    QStringList required = this->requiredRbacMethods();
    bool sourceRequires = this->m_sourceConnection && !this->hasRbacPermissions(this->m_sourceConnection, required);
    bool targetRequires = this->m_targetConnection && !this->hasRbacPermissions(this->m_targetConnection, required);
    this->m_requiresRbacWarning = sourceRequires || targetRequires;

    if (this->m_rbacInfoLabel)
    {
        bool templatesOnly = true;
        for (const QSharedPointer<VM>& vmItem : this->m_vms)
        {
            if (!vmItem || !vmItem->IsTemplate())
            {
                templatesOnly = false;
                break;
            }
        }

        QString message;
        if (this->m_mode == WizardMode::Copy)
        {
            message = templatesOnly
                ? tr("Copying a template requires appropriate permissions on the target server.")
                : tr("Copying a VM requires appropriate permissions on the target server.");
        }
        else if (this->m_mode == WizardMode::Move)
        {
            message = tr("Moving a VM may require elevated permissions on the target server.");
        }
        else
        {
            message = tr("Migrating a VM may require elevated permissions on the target server.");
        }

        this->m_rbacInfoLabel->setText(message);
    }

    if (this->m_rbacConfirm)
        this->m_rbacConfirm->setChecked(false);

    this->updateWizardPages();
}

QStringList CrossPoolMigrateWizard::requiredRbacMethods() const
{
    if (this->m_mode == WizardMode::Copy && this->isIntraPoolCopySelected())
        return { "VM.copy", "VM.clone", "VM.set_name_description", "SR.scan" };

    return { "Host.migrate_receive",
             "VM.migrate_send",
             "VM.async_migrate_send",
             "VM.assert_can_migrate" };
}

bool CrossPoolMigrateWizard::hasRbacPermissions(XenConnection* connection, const QStringList& methods) const
{
    if (!connection)
        return true;

    XenAPI::Session* session = connection->GetSession();
    if (!session)
        return true;

    if (session->IsLocalSuperuser())
        return true;

    QStringList permissions = session->GetPermissions();
    if (permissions.isEmpty())
        return true;

    for (const QString& method : methods)
    {
        if (!permissions.contains(method))
            return false;
    }

    return true;
}

bool CrossPoolMigrateWizard::allVMsAvailable() const
{
    if (this->m_vms.isEmpty())
        return false;

    XenConnection* connection = this->m_vms.first() ? this->m_vms.first()->GetConnection() : nullptr;
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    if (!cache)
        return false;

    for (const QSharedPointer<VM>& vmItem : this->m_vms)
    {
        if (!vmItem)
            return false;
        if (!cache->ResolveObject<VM>(XenObjectType::VM, vmItem->OpaqueRef()))
            return false;
    }

    return true;
}

bool CrossPoolMigrateWizard::requiresRbacWarning() const
{
    return this->m_requiresRbacWarning;
}

bool CrossPoolMigrateWizard::isIntraPoolMigration() const
{
    if (this->m_vms.isEmpty())
        return false;

    for (const QSharedPointer<VM>& vmItem : this->m_vms)
    {
        if (!vmItem)
            return false;

        VmMapping mapping = this->m_vmMappings.value(vmItem->OpaqueRef());
        if (mapping.targetRef.isEmpty())
            return false;

        XenConnection* targetConn = this->m_targetConnection;
        if (!targetConn)
            targetConn = this->resolveTargetConnection(mapping.targetRef);

        XenCache* sourceCache = vmItem->GetConnection() ? vmItem->GetConnection()->GetCache() : nullptr;
        XenCache* targetCache = targetConn ? targetConn->GetCache() : nullptr;
        if (!sourceCache || !targetCache)
            return false;

        QString sourcePoolRef = poolRefForConnection(sourceCache);
        QString targetPoolRef = poolRefForConnection(targetCache);
        if (!sourcePoolRef.isEmpty() && !targetPoolRef.isEmpty())
        {
            if (sourcePoolRef != targetPoolRef)
                return false;
            continue;
        }

        QString homeRef = vmItem->GetHomeRef();
        QString residentRef = vmItem->GetResidentOnRef();
        if (!mapping.targetRef.isEmpty() &&
            (mapping.targetRef == homeRef || mapping.targetRef == residentRef))
            continue;

        if (vmItem->GetConnection() != targetConn)
            return false;
    }

    return true;
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
    if (this->m_mode != WizardMode::Copy)
        return false;
    
    CrossPoolMigrateCopyModePage* copyModePage = qobject_cast<CrossPoolMigrateCopyModePage*>(this->m_copyModePage);
    return copyModePage ? copyModePage->IntraPoolCopySelected() : false;
}

bool CrossPoolMigrateWizard::isCopyCloneSelected() const
{
    IntraPoolCopyPage* intraPage = qobject_cast<IntraPoolCopyPage*>(this->m_intraPoolCopyPage);
    return intraPage ? intraPage->CloneVM() : false;
}

QString CrossPoolMigrateWizard::copyName() const
{
    IntraPoolCopyPage* intraPage = qobject_cast<IntraPoolCopyPage*>(this->m_intraPoolCopyPage);
    return intraPage ? intraPage->NewVmName() : QString();
}

QString CrossPoolMigrateWizard::copyDescription() const
{
    IntraPoolCopyPage* intraPage = qobject_cast<IntraPoolCopyPage*>(this->m_intraPoolCopyPage);
    return intraPage ? intraPage->NewVmDescription() : QString();
}

QString CrossPoolMigrateWizard::copyTargetSrRef() const
{
    IntraPoolCopyPage* intraPage = qobject_cast<IntraPoolCopyPage*>(this->m_intraPoolCopyPage);
    return intraPage ? intraPage->SelectedSR() : QString();
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

bool CrossPoolMigrateWizard::canMigrateVmToHost(const QSharedPointer<VM>& vm, XenConnection* targetConnection, const QString& hostRef, QString* reason) const
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

    QVariantMap hostData = targetCache->ResolveObjectData(XenObjectType::Host, hostRef);
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
        QVariantMap sourceHostData = sourceCache->ResolveObjectData(XenObjectType::Host, sourceHostRef);
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
        if (vm->GetMemoryStaticMin() > vm->GetMemoryDynamicMin() ||
            vm->GetMemoryDynamicMin() != vm->GetMemoryDynamicMax() ||
            vm->GetMemoryDynamicMax() != vm->GetMemoryStaticMax())
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

bool CrossPoolMigrateWizard::canDoStorageMigration(const QSharedPointer<VM>& vm, XenConnection* targetConnection, const QString& hostRef, QString* reason) const
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
    QList<QVariantMap> pifs = targetCache->GetAllData(XenObjectType::PIF);
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
    QStringList allTargetSrRefs = targetCache->GetAllRefs(XenObjectType::SR);
    for (const QString& targetSrRef : allTargetSrRefs)
    {
        QSharedPointer<SR> targetSr = targetCache->ResolveObject<SR>(XenObjectType::SR, targetSrRef);
        if (targetSr && targetSr->SupportsStorageMigration())
            targetSrRefs.append(targetSrRef);
    }
    QStringList vbdRefs = vm->GetVBDRefs();
    for (const QString& vbdRef : vbdRefs)
    {
        QVariantMap vbdData = sourceCache->ResolveObjectData(XenObjectType::VBD, vbdRef);
        QString vdiRef = vbdData.value("VDI").toString();
        if (vdiRef.isEmpty() || vdiRef == XENOBJECT_NULL)
            continue;

        QVariantMap vdiData = sourceCache->ResolveObjectData(XenObjectType::VDI, vdiRef);
        QString srRef = vdiData.value("SR").toString();
        if (srRef.isEmpty())
            continue;

        QSharedPointer<SR> srObj = sourceCache->ResolveObject<SR>(XenObjectType::SR, srRef);
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
    QStringList targetNetworks = targetCache->GetAllRefs(XenObjectType::Network);
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
        QVariantMap snapData = cache->ResolveObjectData(XenObjectType::VM, snapRef);
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

XenConnection* CrossPoolMigrateWizard::resolveTargetConnection(const QString& targetRef) const
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

        if (cache->Contains(XenObjectType::Host, targetRef) || cache->Contains(XenObjectType::Pool, targetRef))
            return conn;
    }

    return nullptr;
}
