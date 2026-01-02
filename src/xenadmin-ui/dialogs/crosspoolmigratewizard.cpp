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
                if (wizard_ && wizard_->requiresRbacWarning())
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
                if (!wizard_)
                    return CrossPoolMigrateWizard::Page_Finish;
                if (wizard_->shouldShowNetworkPage())
                    return CrossPoolMigrateWizard::Page_Network;
                if (wizard_->shouldShowTransferNetworkPage())
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
                if (wizard_ && wizard_->shouldShowTransferNetworkPage())
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
                    connect(confirmBox_, &QCheckBox::toggled, this, [this]() {
                        emit completeChanged();
                    });
                }
            }

            bool isComplete() const override
            {
                return confirmBox_ ? confirmBox_->isChecked() : true;
            }

        private:
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
    if (m_mode == WizardMode::Copy)
        setWindowTitle(tr("Copy VM Wizard"));
    else if (m_mode == WizardMode::Move)
        setWindowTitle(tr("Move VM Wizard"));
    else
        setWindowTitle(tr("Cross Pool Migrate Wizard"));
    setWizardStyle(QWizard::ModernStyle);
    setOption(QWizard::HaveHelpButton, true);
    setOption(QWizard::HelpButtonOnRight, false);
    if (m_mode == WizardMode::Copy)
        setStartId(Page_CopyMode);
    else
        setStartId(Page_Destination);

    for (const QSharedPointer<VM>& vmItem : m_vms)
        ensureMappingForVm(vmItem);

    setupWizardPages();
}

void CrossPoolMigrateWizard::setupWizardPages()
{
    setPage(Page_Destination, createDestinationPage());
    setPage(Page_RbacWarning, createRbacWarningPage());
    setPage(Page_Storage, createStoragePage());
    setPage(Page_Network, createNetworkPage());
    setPage(Page_TransferNetwork, createTransferNetworkPage());
    setPage(Page_Finish, createFinishPage());
    setPage(Page_CopyMode, createCopyModePage());
    setPage(Page_IntraPoolCopy, createIntraPoolCopyPage());
}

QWizardPage* CrossPoolMigrateWizard::createDestinationPage()
{
    QWizardPage* page = new DestinationWizardPage(this, this);
    page->setTitle(tr("Destination"));

    QLabel* info = new QLabel(tr("Select the destination host for the VM."));
    info->setWordWrap(true);

    m_hostCombo = new QComboBox(page);

    QFormLayout* layout = new QFormLayout(page);
    layout->addRow(info);
    layout->addRow(tr("Target host:"), m_hostCombo);

    return page;
}

QWizardPage* CrossPoolMigrateWizard::createStoragePage()
{
    QWizardPage* page = new StorageWizardPage(this, this);
    page->setTitle(tr("Storage"));

    QLabel* info = new QLabel(tr("Select storage repositories for each virtual disk."));
    info->setWordWrap(true);

    m_storageTable = new QTableWidget(page);
    m_storageTable->setColumnCount(3);
    m_storageTable->setHorizontalHeaderLabels({tr("VM"), tr("Virtual disk"), tr("Target SR")});
    m_storageTable->horizontalHeader()->setStretchLastSection(true);
    m_storageTable->verticalHeader()->setVisible(false);
    m_storageTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_storageTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->addWidget(info);
    layout->addWidget(m_storageTable);

    return page;
}

QWizardPage* CrossPoolMigrateWizard::createNetworkPage()
{
    QWizardPage* page = new NetworkWizardPage(this, this);
    page->setTitle(tr("Networking"));

    QLabel* info = new QLabel(tr("Select networks for each virtual interface."));
    info->setWordWrap(true);

    m_networkTable = new QTableWidget(page);
    m_networkTable->setColumnCount(3);
    m_networkTable->setHorizontalHeaderLabels({tr("VM"), tr("VIF (MAC)"), tr("Target Network")});
    m_networkTable->horizontalHeader()->setStretchLastSection(true);
    m_networkTable->verticalHeader()->setVisible(false);
    m_networkTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_networkTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->addWidget(info);
    layout->addWidget(m_networkTable);

    return page;
}

QWizardPage* CrossPoolMigrateWizard::createTransferNetworkPage()
{
    QWizardPage* page = new TransferWizardPage(this);
    page->setTitle(tr("Transfer Network"));

    QLabel* info = new QLabel(tr("Select the network used for transferring VM data."));
    info->setWordWrap(true);

    m_transferNetworkCombo = new QComboBox(page);

    QFormLayout* layout = new QFormLayout(page);
    layout->addRow(info);
    layout->addRow(tr("Transfer network:"), m_transferNetworkCombo);

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

    m_rbacPage = page;
    page->setConfirmation(confirm);
    return page;
}

QWizardPage* CrossPoolMigrateWizard::createFinishPage()
{
    QWizardPage* page = new QWizardPage(this);
    page->setTitle(tr("Finish"));

    m_summaryText = new QTextEdit(page);
    m_summaryText->setReadOnly(true);

    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->addWidget(m_summaryText);

    return page;
}

QWizardPage* CrossPoolMigrateWizard::createCopyModePage()
{
    QWizardPage* page = new CopyModeWizardPage(this, this);
    page->setTitle(tr("Copy Mode"));

    QLabel* info = new QLabel(tr("Choose how to copy the VM."), page);
    info->setWordWrap(true);

    m_copyIntraRadio = new QRadioButton(tr("Copy within the current pool"), page);
    m_copyCrossRadio = new QRadioButton(tr("Copy to another pool"), page);

    bool canUseIntra = (m_vms.size() == 1);
    m_copyIntraRadio->setEnabled(canUseIntra);
    if (canUseIntra)
        m_copyIntraRadio->setChecked(true);
    else
        m_copyCrossRadio->setChecked(true);
    m_intraPoolCopySelected = canUseIntra;

    connect(m_copyIntraRadio, &QRadioButton::toggled, this, [this](bool checked) {
        this->m_intraPoolCopySelected = checked;
    });
    connect(m_copyCrossRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked)
            this->m_intraPoolCopySelected = false;
    });

    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->addWidget(info);
    layout->addWidget(m_copyIntraRadio);
    layout->addWidget(m_copyCrossRadio);

    m_copyModePage = page;
    return page;
}

QWizardPage* CrossPoolMigrateWizard::createIntraPoolCopyPage()
{
    QWizardPage* page = new IntraPoolCopyWizardPage(this);
    page->setTitle(tr("Intra-pool Copy"));

    QLabel* info = new QLabel(tr("Select copy type and target storage."), page);
    info->setWordWrap(true);

    m_copyCloneRadio = new QRadioButton(tr("Fast clone (no storage selection)"), page);
    m_copyFullRadio = new QRadioButton(tr("Full copy to selected SR"), page);
    m_copyCloneRadio->setChecked(true);

    m_copyNameEdit = new QLineEdit(page);
    m_copyDescriptionEdit = new QTextEdit(page);
    m_copyDescriptionEdit->setFixedHeight(80);

    m_copySrCombo = new QComboBox(page);
    m_copySrCombo->setEnabled(false);

    connect(m_copyCloneRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (this->m_copySrCombo)
            this->m_copySrCombo->setEnabled(!checked);
    });

    QFormLayout* formLayout = new QFormLayout();
    formLayout->addRow(tr("Name:"), m_copyNameEdit);
    formLayout->addRow(tr("Description:"), m_copyDescriptionEdit);
    formLayout->addRow(tr("Target SR:"), m_copySrCombo);

    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->addWidget(info);
    layout->addWidget(m_copyCloneRadio);
    layout->addWidget(m_copyFullRadio);
    layout->addLayout(formLayout);

    m_intraPoolCopyPage = page;
    return page;
}

void CrossPoolMigrateWizard::initializePage(int id)
{
    if (id == Page_CopyMode)
    {
        m_intraPoolCopySelected = m_copyIntraRadio ? m_copyIntraRadio->isChecked() : false;
    }
    else if (id == Page_IntraPoolCopy)
    {
        if (m_copyNameEdit && !m_vms.isEmpty() && m_vms.first())
            m_copyNameEdit->setText(tr("Copy of %1").arg(m_vms.first()->GetName()));

        if (m_copySrCombo)
        {
            m_copySrCombo->clear();
            XenConnection* conn = m_sourceConnection;
            XenCache* cache = conn ? conn->GetCache() : nullptr;
            if (cache)
            {
                QStringList srRefs = cache->GetAllRefs("sr");
                for (const QString& srRef : srRefs)
                {
                    QVariantMap srData = cache->ResolveObjectData("sr", srRef);
                    QString srName = srData.value("name_label", "SR").toString();
                    m_copySrCombo->addItem(srName, srRef);
                }
            }
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
    if (currentId() == Page_CopyMode)
    {
        if (m_mode == WizardMode::Copy && m_copyIntraRadio && m_copyIntraRadio->isChecked() && m_vms.size() != 1)
        {
            QMessageBox::warning(this, tr("Copy VM"), tr("Intra-pool copy supports a single VM selection."));
            return false;
        }
    }
    else if (currentId() == Page_IntraPoolCopy)
    {
        if (!m_copyNameEdit || m_copyNameEdit->text().trimmed().isEmpty())
        {
            QMessageBox::warning(this, tr("Copy VM"), tr("Please enter a name for the copied VM."));
            return false;
        }
        if (m_copyFullRadio && m_copyFullRadio->isChecked())
        {
            if (!m_copySrCombo || m_copySrCombo->currentIndex() < 0)
            {
                QMessageBox::warning(this, tr("Copy VM"), tr("Please select a target SR."));
                return false;
            }
        }
    }
    else if (currentId() == Page_Destination)
    {
        if (!m_hostCombo || m_hostCombo->currentIndex() < 0)
        {
            QMessageBox::warning(this, tr("Cross Pool Migrate"), tr("Please select a destination host."));
            return false;
        }

        auto* model = qobject_cast<QStandardItemModel*>(m_hostCombo->model());
        if (model)
        {
            QStandardItem* item = model->item(m_hostCombo->currentIndex());
            if (item && !item->isEnabled())
            {
                QMessageBox::warning(this, tr("Cross Pool Migrate"),
                                     tr("Selected host is not eligible for migration."));
                return false;
            }
        }
    }
    else if (currentId() == Page_TransferNetwork && requiresTransferNetwork())
    {
        if (!m_transferNetworkCombo || m_transferNetworkCombo->currentIndex() < 0)
        {
            QMessageBox::warning(this, tr("Cross Pool Migrate"), tr("Please select a transfer network."));
            return false;
        }
    }

    return QWizard::validateCurrentPage();
}

void CrossPoolMigrateWizard::accept()
{
    if (m_vms.isEmpty() || !m_sourceConnection)
    {
        QWizard::accept();
        return;
    }

    if (m_mode == WizardMode::Copy && isIntraPoolCopySelected())
    {
        QSharedPointer<VM> vmItem = m_vms.first();
        if (vmItem)
        {
            if (isCopyCloneSelected())
            {
                VMCloneAction* action = new VMCloneAction(m_sourceConnection,
                                                          vmItem.data(),
                                                          copyName(),
                                                          copyDescription(),
                                                          this);
                OperationManager::instance()->registerOperation(action);
                action->runAsync();
            }
            else
            {
                XenCache* cache = m_sourceConnection->GetCache();
                QSharedPointer<SR> sr = cache ? cache->ResolveObject<SR>("sr", copyTargetSrRef()) : QSharedPointer<SR>();
                if (sr && sr->IsValid())
                {
                    VMCopyAction* action = new VMCopyAction(m_sourceConnection,
                                                            vmItem.data(),
                                                            nullptr,
                                                            sr.data(),
                                                            copyName(),
                                                            copyDescription(),
                                                            this);
                    OperationManager::instance()->registerOperation(action);
                    action->runAsync();
                }
            }
        }

        QWizard::accept();
        return;
    }

    if (m_storageTable)
    {
        for (int row = 0; row < m_storageTable->rowCount(); ++row)
        {
            QTableWidgetItem* vmItem = m_storageTable->item(row, 0);
            QTableWidgetItem* vdiItem = m_storageTable->item(row, 1);
            QString vmRef = vmItem ? vmItem->data(Qt::UserRole).toString() : QString();
            QString vdiRef = vdiItem ? vdiItem->data(Qt::UserRole).toString() : QString();
            QComboBox* combo = qobject_cast<QComboBox*>(m_storageTable->cellWidget(row, 2));
            if (!combo || vmRef.isEmpty() || vdiRef.isEmpty())
                continue;

            QString srRef = combo->currentData().toString();
            if (!srRef.isEmpty())
                m_vmMappings[vmRef].storage.insert(vdiRef, srRef);
        }
    }

    if (m_networkTable)
    {
        for (int row = 0; row < m_networkTable->rowCount(); ++row)
        {
            QTableWidgetItem* vmItem = m_networkTable->item(row, 0);
            QTableWidgetItem* vifItem = m_networkTable->item(row, 1);
            QString vmRef = vmItem ? vmItem->data(Qt::UserRole).toString() : QString();
            QString vifRef = vifItem ? vifItem->data(Qt::UserRole).toString() : QString();
            QComboBox* combo = qobject_cast<QComboBox*>(m_networkTable->cellWidget(row, 2));
            if (!combo || vmRef.isEmpty() || vifRef.isEmpty())
                continue;

            QString netRef = combo->currentData().toString();
            if (!netRef.isEmpty())
                m_vmMappings[vmRef].vifs.insert(vifRef, netRef);
        }
    }

    if (m_transferNetworkCombo)
        m_transferNetworkRef = m_transferNetworkCombo->currentData().toString();

    if (!m_targetConnection || m_targetHostRef.isEmpty() ||
        (requiresTransferNetwork() && m_transferNetworkRef.isEmpty()))
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

    for (const QSharedPointer<VM>& vmItem : m_vms)
    {
        if (!vmItem)
            continue;

        VmMapping mapping = m_vmMappings.value(vmItem->OpaqueRef());
        mapping.targetRef = m_targetHostRef;

        XenCache* sourceCache = m_sourceConnection ? m_sourceConnection->GetCache() : nullptr;
        bool sameConnection = (m_sourceConnection == m_targetConnection);
        bool hasStorageMotion = isStorageMotion(vmItem, mapping, sourceCache);

        if (m_mode == WizardMode::Move && sameConnection && vmItem->CanBeMoved())
        {
            if (hasStorageMotion && sourceCache)
            {
                QMap<QString, SR*> storageMap;
                for (auto it = mapping.storage.cbegin(); it != mapping.storage.cend(); ++it)
                {
                    QString vdiRef = it.key();
                    QString srRef = it.value();
                    if (vdiRef.isEmpty() || srRef.isEmpty())
                        continue;

                    QSharedPointer<SR> srObj = sourceCache->ResolveObject<SR>("sr", srRef);
                    if (srObj && srObj->IsValid())
                        storageMap.insert(vdiRef, srObj.data());
                }

                QSharedPointer<Host> hostObj = sourceCache->ResolveObject<Host>("host", m_targetHostRef);
                Host* hostPtr = hostObj && hostObj->IsValid() ? hostObj.data() : nullptr;

                VMMoveAction* action = new VMMoveAction(m_sourceConnection,
                                                        vmItem.data(),
                                                        storageMap,
                                                        hostPtr,
                                                        this);
                OperationManager::instance()->registerOperation(action);
                action->runAsync();
            }
        }
        else
        {
            bool useCrossPool = (m_mode == WizardMode::Copy) || hasStorageMotion || !sameConnection;
            if (useCrossPool)
            {
                VMCrossPoolMigrateAction* action = new VMCrossPoolMigrateAction(
                    m_sourceConnection,
                    m_targetConnection,
                    vmItem->OpaqueRef(),
                    m_targetHostRef,
                    m_transferNetworkRef,
                    mapping,
                    m_mode == WizardMode::Copy,
                    this);

                OperationManager::instance()->registerOperation(action);
                action->runAsync();
            }
            else
            {
                VMMigrateAction* action = new VMMigrateAction(m_sourceConnection,
                                                             vmItem->OpaqueRef(),
                                                             m_targetHostRef,
                                                             this);
                OperationManager::instance()->registerOperation(action);
                action->runAsync();
            }
        }
    }

    QWizard::accept();
}

void CrossPoolMigrateWizard::populateDestinationHosts()
{
    if (!m_hostCombo)
        return;

    m_hostCombo->clear();

    Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
    if (!connMgr)
        return;

    const QList<XenConnection*> connections = connMgr->getAllConnections();
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

            for (const QSharedPointer<VM>& vmItem : m_vms)
            {
                if (!vmItem)
                    continue;

                if (!canMigrateVmToHost(vmItem, conn, hostRef, &failureReason))
                {
                    eligible = false;
                    break;
                }
            }

            QString label = eligible ? hostName : QString("%1 (%2)").arg(hostName, failureReason);
            m_hostCombo->addItem(label, hostRef);
            int index = m_hostCombo->count() - 1;
            if (!eligible)
            {
                auto* model = qobject_cast<QStandardItemModel*>(m_hostCombo->model());
                if (model)
                {
                    QStandardItem* item = model->item(index);
                    if (item)
                        item->setEnabled(false);
                }
            }
        }
    }

    connect(m_hostCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        this->m_targetHostRef = this->m_hostCombo->currentData().toString();
        this->m_targetConnection = this->resolveTargetConnection(this->m_targetHostRef);
        this->updateRbacRequirement();

        QString targetName = this->m_hostCombo->currentText();
        for (auto it = this->m_vmMappings.begin(); it != this->m_vmMappings.end(); ++it)
            it.value().targetName = targetName;
    }, Qt::UniqueConnection);

    if (m_hostCombo->count() > 0)
    {
        int firstEnabled = 0;
        auto* model = qobject_cast<QStandardItemModel*>(m_hostCombo->model());
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

        m_hostCombo->setCurrentIndex(firstEnabled);
        m_targetHostRef = m_hostCombo->currentData().toString();
        m_targetConnection = resolveTargetConnection(m_targetHostRef);
        updateRbacRequirement();

        QString targetName = m_hostCombo->currentText();
        for (auto it = m_vmMappings.begin(); it != m_vmMappings.end(); ++it)
            it.value().targetName = targetName;
    }
}

void CrossPoolMigrateWizard::populateStorageMappings()
{
    if (!m_storageTable || m_vms.isEmpty() || !m_sourceConnection)
        return;

    m_storageTable->setRowCount(0);

    XenCache* sourceCache = m_sourceConnection->GetCache();
    XenCache* targetCache = m_targetConnection ? m_targetConnection->GetCache() : nullptr;
    if (!sourceCache || !targetCache)
        return;

    QStringList srRefs = targetCache->GetAllRefs("sr");
    QString defaultSrRef;
    QList<QVariantMap> pools = targetCache->GetAllData("pool");
    if (!pools.isEmpty())
        defaultSrRef = pools.first().value("default_SR").toString();

    int row = 0;
    for (const QSharedPointer<VM>& vmItem : m_vms)
    {
        if (!vmItem)
            continue;

        QStringList vbdRefs = vmItem->VBDRefs();
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

            m_storageTable->insertRow(row);

            QTableWidgetItem* vmNameItem = new QTableWidgetItem(vmItem->GetName());
            vmNameItem->setData(Qt::UserRole, vmItem->OpaqueRef());
            m_storageTable->setItem(row, 0, vmNameItem);

            QTableWidgetItem* vdiItem = new QTableWidgetItem(vdiName);
            vdiItem->setData(Qt::UserRole, vdiRef);
            m_storageTable->setItem(row, 1, vdiItem);

            QComboBox* srCombo = new QComboBox(m_storageTable);
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

            m_storageTable->setCellWidget(row, 2, srCombo);
            row++;
        }
    }

    m_storageTable->resizeRowsToContents();
}

void CrossPoolMigrateWizard::populateNetworkMappings()
{
    if (!m_networkTable || m_vms.isEmpty() || !m_sourceConnection)
        return;

    m_networkTable->setRowCount(0);

    XenCache* sourceCache = m_sourceConnection->GetCache();
    XenCache* targetCache = m_targetConnection ? m_targetConnection->GetCache() : nullptr;
    if (!sourceCache || !targetCache)
        return;

    QStringList networkRefs = targetCache->GetAllRefs("network");
    int row = 0;
    for (const QSharedPointer<VM>& vmItem : m_vms)
    {
        if (!vmItem)
            continue;

        QStringList vifRefs = vmItem->VIFRefs();
        QStringList snapVifRefs = collectSnapshotVifRefs(vmItem);
        for (const QString& vifRef : vifRefs + snapVifRefs)
        {
            bool isSnapshotVif = !vifRefs.contains(vifRef);
            QVariantMap vifData = sourceCache->ResolveObjectData("vif", vifRef);
            QString mac = vifData.value("MAC", "VIF").toString();
            if (isSnapshotVif)
                mac = QString("%1 (%2)").arg(mac, tr("snapshot"));

            m_networkTable->insertRow(row);
            QTableWidgetItem* vmNameItem = new QTableWidgetItem(vmItem->GetName());
            vmNameItem->setData(Qt::UserRole, vmItem->OpaqueRef());
            m_networkTable->setItem(row, 0, vmNameItem);

            QTableWidgetItem* vifItem = new QTableWidgetItem(mac);
            vifItem->setData(Qt::UserRole, vifRef);
            m_networkTable->setItem(row, 1, vifItem);

            QComboBox* netCombo = new QComboBox(m_networkTable);
            for (const QString& netRef : networkRefs)
            {
                QVariantMap netData = targetCache->ResolveObjectData("network", netRef);
                QString netName = netData.value("name_label", "Network").toString();
                netCombo->addItem(netName, netRef);
            }

            m_networkTable->setCellWidget(row, 2, netCombo);
            row++;
        }
    }

    m_networkTable->resizeRowsToContents();
}

void CrossPoolMigrateWizard::populateTransferNetworks()
{
    if (!m_transferNetworkCombo || !m_targetConnection)
        return;

    m_transferNetworkCombo->clear();

    XenCache* targetCache = m_targetConnection->GetCache();
    if (!targetCache)
        return;

    QStringList networkRefs = targetCache->GetAllRefs("network");
    for (const QString& netRef : networkRefs)
    {
        QVariantMap netData = targetCache->ResolveObjectData("network", netRef);
        QString netName = netData.value("name_label", "Network").toString();
        m_transferNetworkCombo->addItem(netName, netRef);
    }

    if (m_transferNetworkCombo->count() > 0)
        m_transferNetworkCombo->setCurrentIndex(0);
}

void CrossPoolMigrateWizard::updateSummary()
{
    if (!m_summaryText)
        return;

    QString summary;
    if (!m_vms.isEmpty())
    {
        summary += tr("VMs:\n");
        for (const QSharedPointer<VM>& vmItem : m_vms)
        {
            if (vmItem)
                summary += QString("  %1\n").arg(vmItem->GetName());
        }
        summary += "\n";
    }
    summary += tr("Target host: %1\n").arg(m_hostCombo ? m_hostCombo->currentText() : tr("Unknown"));
    if (requiresTransferNetwork())
        summary += tr("Transfer network: %1\n").arg(m_transferNetworkCombo ? m_transferNetworkCombo->currentText() : tr("Unknown"));
    summary += "\n";

    if (m_storageTable)
    {
        summary += tr("Storage mappings:\n");
        for (int row = 0; row < m_storageTable->rowCount(); ++row)
        {
            QString vmName = m_storageTable->item(row, 0)->text();
            QString vdiName = m_storageTable->item(row, 1)->text();
            QComboBox* combo = qobject_cast<QComboBox*>(m_storageTable->cellWidget(row, 2));
            QString srName = combo ? combo->currentText() : tr("Unknown");
            summary += QString("  %1: %2 -> %3\n").arg(vmName, vdiName, srName);
        }
        summary += "\n";
    }

    if (m_networkTable)
    {
        summary += tr("Network mappings:\n");
        for (int row = 0; row < m_networkTable->rowCount(); ++row)
        {
            QString vmName = m_networkTable->item(row, 0)->text();
            QString vifName = m_networkTable->item(row, 1)->text();
            QComboBox* combo = qobject_cast<QComboBox*>(m_networkTable->cellWidget(row, 2));
            QString netName = combo ? combo->currentText() : tr("Unknown");
            summary += QString("  %1: %2 -> %3\n").arg(vmName, vifName, netName);
        }
    }

    m_summaryText->setPlainText(summary);
}

void CrossPoolMigrateWizard::updateRbacRequirement()
{
    if (!m_sourceConnection || !m_targetConnection)
    {
        m_requiresRbacWarning = false;
        return;
    }

    m_requiresRbacWarning = (m_sourceConnection != m_targetConnection);
}

bool CrossPoolMigrateWizard::requiresRbacWarning() const
{
    return m_requiresRbacWarning;
}

bool CrossPoolMigrateWizard::isIntraPoolMigration() const
{
    if (!m_sourceConnection || !m_targetConnection)
        return false;

    XenCache* sourceCache = m_sourceConnection->GetCache();
    XenCache* targetCache = m_targetConnection->GetCache();
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
    if (m_mode != WizardMode::Move)
        return false;
    if (!isIntraPoolMigration())
        return false;

    for (const QSharedPointer<VM>& vmItem : m_vms)
    {
        if (!vmItem || !vmItem->CanBeMoved())
            return false;
    }

    return true;
}

bool CrossPoolMigrateWizard::shouldShowNetworkPage() const
{
    return !isIntraPoolMigration();
}

bool CrossPoolMigrateWizard::shouldShowTransferNetworkPage() const
{
    return !isIntraPoolMove();
}

bool CrossPoolMigrateWizard::requiresTransferNetwork() const
{
    return shouldShowTransferNetworkPage();
}

bool CrossPoolMigrateWizard::isIntraPoolCopySelected() const
{
    return m_mode == WizardMode::Copy && m_intraPoolCopySelected;
}

bool CrossPoolMigrateWizard::isCopyCloneSelected() const
{
    return m_copyCloneRadio && m_copyCloneRadio->isChecked();
}

QString CrossPoolMigrateWizard::copyName() const
{
    return m_copyNameEdit ? m_copyNameEdit->text().trimmed() : QString();
}

QString CrossPoolMigrateWizard::copyDescription() const
{
    return m_copyDescriptionEdit ? m_copyDescriptionEdit->toPlainText().trimmed() : QString();
}

QString CrossPoolMigrateWizard::copyTargetSrRef() const
{
    return m_copySrCombo ? m_copySrCombo->currentData().toString() : QString();
}

void CrossPoolMigrateWizard::ensureMappingForVm(const QSharedPointer<VM>& vm)
{
    if (!vm)
        return;

    if (m_vmMappings.contains(vm->OpaqueRef()))
        return;

    VmMapping mapping(vm->OpaqueRef());
    mapping.vmNameLabel = vm->GetName();
    m_vmMappings.insert(vm->OpaqueRef(), mapping);
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

    if (m_mode == WizardMode::Move)
    {
        QString sourcePoolRef = poolRefForConnection(sourceCache);
        QString targetPoolRef = poolRefForConnection(targetCache);
        if (!sourcePoolRef.isEmpty() && sourcePoolRef == targetPoolRef)
            return true;
    }

    QString homeRef = vm->HomeRef();
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
            sourceHostRef = vm->ResidentOnRef();
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

    if (canDoStorageMigration(vm, targetConnection, hostRef, reason))
        return true;

    // Allow intra-pool live migration when pool_migrate is available
    if (m_mode == WizardMode::Migrate && vm->GetConnection() == targetConnection)
    {
        QString ignored;
        if (VMOperationHelpers::VmCanBootOnHost(vm->GetConnection(), vm, hostRef, "pool_migrate", &ignored))
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
    QStringList vbdRefs = vm->VBDRefs();
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
        QStringList vifRefs = vm->VIFRefs();
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

    QStringList vmVifs = vm->VIFRefs();
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

    QStringList snapshots = vm->SnapshotRefs();
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

    const QList<XenConnection*> connections = connMgr->getAllConnections();
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
