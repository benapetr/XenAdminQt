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

#include "hawizard.h"
#include "ui_hawizard.h"
#include "../dialogs/actionprogressdialog.h"
#include "../widgets/wizardnavigationpane.h"
#include "../iconmanager.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/pbd.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/session.h"
#include "xenlib/xen/xenapi/xenapi_VM.h"
#include "xenlib/xen/xenapi/xenapi_Pool.h"
#include "xenlib/xen/actions/pool/enablehaaction.h"
#include "xenlib/xen/actions/pool/getheartbeatsrsaction.h"
#include <QMessageBox>
#include <QHeaderView>
#include <QIcon>
#include <QTimer>
#include <QApplication>
#include <QThread>
#include <QPointer>
#include <QSignalBlocker>
#include <QShowEvent>
#include <QStyle>

HAWizard::HAWizard(QSharedPointer<Pool> pool, QWidget* parent) : QWizard(parent), m_pool(pool), ui(new Ui::HAWizard)
{
    this->ui->setupUi(this);
    this->setWindowTitle(tr("Configure High Availability"));
    this->setWizardStyle(QWizard::ModernStyle);
    this->setOption(QWizard::HaveHelpButton, true);
    this->setOption(QWizard::HelpButtonOnRight, false);
    this->setMinimumSize(700, 500);

    // Get pool name for display
    this->m_poolName = this->m_pool ? this->m_pool->GetName() : QString("Pool");

    // Bind UI pages to explicit wizard IDs and populate page content.
    this->setPage(Page_Intro, this->ui->pageIntro);
    this->setPage(Page_RbacWarning, this->ui->pageRbac);
    this->setPage(Page_ChooseSR, this->ui->pageChooseSr);
    this->setPage(Page_AssignPriorities, this->ui->pageAssign);
    this->setPage(Page_Finish, this->ui->pageFinish);
    this->setStartId(Page_Intro);
    this->createIntroPage();
    this->createRbacWarningPage();
    this->createChooseSRPage();
    this->createAssignPrioritiesPage();
    this->createFinishPage();

    XenConnection* conn = this->m_pool ? this->m_pool->GetConnection() : nullptr;
    XenAPI::Session* session = conn ? conn->GetSession() : nullptr;
    this->m_rbacRequired = session && session->IsLoggedIn() && !session->IsLocalSuperuser() &&
                           session->ApiVersionMeets(APIVersion::API_1_7);

    const QStringList requiredMethods = {"vm.set_ha_restart_priority",
                                         "vm.set_order",
                                         "vm.set_start_delay",
                                         "pool.sync_database",
                                         "pool.ha_compute_hypothetical_max_host_failures_to_tolerate",
                                         "pool.set_ha_host_failures_to_tolerate",
                                         "pool.enable_ha",
                                         "sr.assert_can_host_ha_statefile"};
    if (this->m_rbacRequired && session)
    {
        const QStringList permissions = session->GetPermissions();
        if (!permissions.isEmpty())
        {
            QStringList missing;
            for (const QString& method : requiredMethods)
            {
                if (!permissions.contains(method, Qt::CaseInsensitive))
                    missing.append(method);
            }

            this->m_rbacBlockingFailure = !missing.isEmpty();
            this->m_rbacWarningLabel->setText(
                this->m_rbacBlockingFailure
                    ? tr("You do not have sufficient permissions to enable HA.\n\nMissing methods:\n%1")
                          .arg(missing.join("\n"))
                    : tr("Permission checks passed."));
        } else
        {
            this->m_rbacWarningLabel->setText(tr("Permission checks are unavailable for this session."));
        }
    } else
    {
        this->m_rbacWarningLabel->setText(tr("RBAC checks are not required for this connection."));
        this->removePage(Page_RbacWarning);
    }

    this->setButtonText(QWizard::FinishButton, tr("Enable HA"));
    connect(this, &QWizard::currentIdChanged, this, &HAWizard::onCurrentIdChanged);

    this->m_navigationPane = new WizardNavigationPane(this);
    QVector<WizardNavigationPane::Step> steps = {{tr("Prerequisites"), QIcon()}};
    if (this->m_rbacRequired)
        steps.append({tr("Permissions"), QIcon()});
    steps.append({tr("Heartbeat SR"), QIcon()});
    steps.append({tr("HA Plan"), QIcon()});
    steps.append({tr("Finish"), QIcon()});
    this->m_navigationPane->setSteps(steps);
    this->setSideWidget(this->m_navigationPane);
}

HAWizard::~HAWizard()
{
    delete this->ui;
}

void HAWizard::showEvent(QShowEvent* event)
{
    QWizard::showEvent(event);

    if (this->m_brokenSrWarningShown || !this->m_pool || !this->m_pool->GetCache())
        return;

    this->m_brokenSrWarningShown = true;
    QStringList broken;
    const QList<QSharedPointer<SR>> srs = this->m_pool->GetCache()->GetAll<SR>(XenObjectType::SR);
    for (const QSharedPointer<SR>& sr : srs)
    {
        if (!sr || !sr->IsValid())
            continue;
        if (sr->HasPBDs() && sr->IsBroken() && !sr->IsToolsSR() && sr->IsShared())
            broken.append(sr->GetName());
    }

    if (!broken.isEmpty())
    {
        QTimer::singleShot(0, this, [this, broken]() {
            QMessageBox::warning(this,
                                 tr("High Availability"),
                                 tr("Some shared storage repositories are broken:\n\n%1").arg(broken.join("\n")));
        });
    }
}

void HAWizard::onCurrentIdChanged(int id)
{
    if (this->m_navigationPane)
        this->m_navigationPane->setCurrentStep(this->wizardStepIndexForPage(id));
}

QWizardPage* HAWizard::createIntroPage()
{
    QWizardPage* page = this->ui->pageIntro;
    return page;
}

QWizardPage* HAWizard::createChooseSRPage()
{
    QWizardPage* page = this->ui->pageChooseSr;
    this->m_scanProgress = this->ui->scanProgress;
    this->m_scanProgress->setRange(0, 0);
    this->m_scanProgress->setVisible(false);
    this->m_noSRsLabel = this->ui->noSrLabel;
    this->m_noSRsLabel->setStyleSheet("color: red; font-weight: bold;");
    this->m_srTable = this->ui->srTable;
    this->m_srTable->setColumnCount(4);
    this->m_srTable->setHorizontalHeaderLabels({tr(""), tr("Name"), tr("Description"), tr("Comment")});
    this->m_srTable->horizontalHeader()->setStretchLastSection(true);
    this->m_srTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    this->m_srTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    this->m_srTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    this->m_srTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    this->m_srTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->m_srTable->setSelectionMode(QAbstractItemView::SingleSelection);
    this->m_srTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->m_srTable->verticalHeader()->setVisible(false);

    connect(this->m_srTable, &QTableWidget::itemSelectionChanged, this, &HAWizard::onHeartbeatSRSelectionChanged);
    this->m_rescanButton = this->ui->rescanButton;
    connect(this->m_rescanButton, &QPushButton::clicked, this, &HAWizard::scanForHeartbeatSRs);

    return page;
}

QWizardPage* HAWizard::createRbacWarningPage()
{
    QWizardPage* page = this->ui->pageRbac;
    this->m_rbacWarningLabel = this->ui->rbacWarningLabel;
    return page;
}

QWizardPage* HAWizard::createAssignPrioritiesPage()
{
    QWizardPage* page = this->ui->pageAssign;
    this->m_ntolSpinBox = this->ui->ntolSpinBox;
    // C# numeric control is not capped to computed ntolMax; over-max is shown as overcommit.
    this->m_ntolSpinBox->setRange(0, 9999);
    this->m_ntolSpinBox->setValue(0);
    connect(this->m_ntolSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &HAWizard::onNtolChanged);
    this->m_maxNtolLabel = this->ui->maxNtolLabel;
    this->m_maxNtolLabel->setStyleSheet("color: gray;");
    this->m_ntolStatusLabel = this->ui->ntolStatusLabel;
    this->m_vmTable = this->ui->vmTable;
    this->m_vmTable->setColumnCount(6);
    this->m_vmTable->setHorizontalHeaderLabels({tr(""), tr("VM"), tr("Restart Priority"), tr("Start Order"), tr("Start Delay (s)"), tr("Agility")});
    this->m_vmTable->horizontalHeader()->setStretchLastSection(true);
    this->m_vmTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    this->m_vmTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    this->m_vmTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    this->m_vmTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    this->m_vmTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    this->m_vmTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    this->m_vmTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->m_vmTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    this->m_vmTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->m_vmTable->verticalHeader()->setVisible(false);

    this->m_selectedPriorityCombo = this->ui->selectedPriorityCombo;
    this->m_selectedOrderSpin = this->ui->selectedOrderSpin;
    this->m_selectedDelaySpin = this->ui->selectedDelaySpin;

    this->m_selectedPriorityCombo->clear();
    this->m_selectedPriorityCombo->addItem(tr("Mixed"), "__mixed__");
    this->m_selectedPriorityCombo->addItem(tr("Restart"), "restart");
    this->m_selectedPriorityCombo->addItem(tr("Restart if possible"), "best-effort");
    this->m_selectedPriorityCombo->addItem(tr("Do not restart"), "");

    this->m_selectedOrderSpin->setRange(0, 9999);
    this->m_selectedDelaySpin->setRange(0, 600);
    this->ui->haStatusIconLabel->setText(QString::fromUtf8("\xE2\x9C\x93"));

    connect(this->m_vmTable, &QTableWidget::itemSelectionChanged, this, &HAWizard::onVmSelectionChanged);
    connect(this->m_selectedPriorityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &HAWizard::onSelectedPriorityChanged);
    connect(this->m_selectedOrderSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &HAWizard::onSelectedOrderChanged);
    connect(this->m_selectedDelaySpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &HAWizard::onSelectedDelayChanged);

    return page;
}

QWizardPage* HAWizard::createFinishPage()
{
    QWizardPage* page = this->ui->pageFinish;
    this->m_finishSRLabel = this->ui->finishSRLabel;
    this->m_finishNtolLabel = this->ui->finishNtolLabel;
    this->m_finishRestartLabel = this->ui->finishRestartLabel;
    this->m_finishBestEffortLabel = this->ui->finishBestEffortLabel;
    this->m_finishDoNotRestartLabel = this->ui->finishDoNotRestartLabel;
    this->ui->labelRestartHigh->setVisible(false);
    this->ui->finishRestartHighLabel->setVisible(false);
    this->m_finishWarningIcon = this->ui->finishWarningIcon;
    this->m_finishWarningIcon->setPixmap(QIcon::fromTheme("dialog-warning").pixmap(24, 24));
    this->m_finishWarningIcon->setVisible(false);
    this->m_finishWarningLabel = this->ui->finishWarningLabel;
    this->m_finishWarningLabel->setStyleSheet("color: #b8860b;"); // Dark goldenrod
    this->m_finishWarningLabel->setVisible(false);
    return page;
}

void HAWizard::initializePage(int id)
{
    switch (id)
    {
        case Page_Intro:
            break;
        case Page_RbacWarning:
            break;
        case Page_ChooseSR:
            break;

        case Page_AssignPriorities:
            populateVMTable();
            updateNtolCalculation();
            break;

        case Page_Finish:
            updateFinishPage();
            break;

        default:
            break;
    }

    QWizard::initializePage(id);
}

bool HAWizard::validateCurrentPage()
{
    int id = currentId();

    switch (id)
    {
        case Page_Intro:
            if (!this->m_rbacRequired)
            {
                if (!this->performHeartbeatSRScan())
                    return false;
                break;
            }
            [[fallthrough]];
        case Page_RbacWarning:
            if (this->m_rbacBlockingFailure)
            {
                QMessageBox::warning(this, tr("Insufficient permissions"), tr("You do not have the required permissions to configure HA on this pool."));
                return false;
            }
            if (!this->performHeartbeatSRScan())
                return false;
            break;
        case Page_ChooseSR:
            if (this->m_selectedHeartbeatSR.isEmpty())
            {
                QMessageBox::warning(this, tr("No SR Selected"), tr("Please select a storage repository for the HA heartbeat."));
                return false;
            }
            break;

        case Page_AssignPriorities:
            if (this->m_ntolUpdateInProgress)
            {
                QMessageBox::information(this, tr("Please wait"), tr("Still calculating host failure tolerance. Please wait."));
                return false;
            }
            // Validate NTOL is reasonable
            if (this->m_maxNtol >= 0 && this->m_ntol > this->m_maxNtol)
            {
                QMessageBox::warning(this, tr("Invalid NTOL"), tr("The number of failures to tolerate (%1) exceeds the maximum (%2) based on current VM priorities and available resources.").arg(this->m_ntol).arg(this->m_maxNtol));
                return false;
            }
            break;

        default:
            break;
    }

    return QWizard::validateCurrentPage();
}

void HAWizard::accept()
{
    if (this->m_ntolUpdateInProgress)
        return;

    if (this->m_ntol == 0)
    {
        const int rc = QMessageBox::warning(this, tr("High Availability"), tr("Failures to tolerate is set to 0. Continue anyway?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (rc != QMessageBox::Yes)
            return;
    }

    // Resolve Pool object from cache
    if (!this->m_pool || !this->m_pool->IsValid())
    {
        QMessageBox::critical(this, tr("Error"), tr("Failed to resolve pool object"));
        return;
    }

    // Create and run EnableHAAction
    EnableHAAction* action = new EnableHAAction(this->m_pool, QStringList{this->m_selectedHeartbeatSR}, this->m_ntol, this->m_vmStartupOptions, this);

    ActionProgressDialog progressDialog(action, this);
    progressDialog.setShowCancel(true);

    if (progressDialog.exec() == QDialog::Accepted && !action->HasError() && !action->IsCancelled())
    {
        QMessageBox::information(this, tr("HA Enabled"), tr("High Availability has been successfully enabled on pool '%1'.").arg(this->m_poolName));
        QWizard::accept();
    }
}

void HAWizard::scanForHeartbeatSRs()
{
    this->performHeartbeatSRScan();
}

bool HAWizard::performHeartbeatSRScan()
{
    this->m_rescanButton->setEnabled(false);
    this->m_srTable->setEnabled(false);
    this->m_noSRsLabel->setVisible(false);

    this->m_heartbeatSRs.clear();
    this->m_srTable->setRowCount(0);

    GetHeartbeatSRsAction* action = new GetHeartbeatSRsAction(this->m_pool, this);
    ActionProgressDialog progressDialog(action, this);
    progressDialog.setShowCancel(true);
    progressDialog.exec();

    bool cancelled = action->IsCancelled();
    if (action->HasError())
    {
        QMessageBox::warning(this, tr("Scan Failed"), tr("Failed to scan for heartbeat SRs: %1").arg(action->GetErrorMessage()));
    } else if (!cancelled)
    {
        const QList<SRWrapper>& scanned = action->GetSRs();
        for (const SRWrapper& wrapper : scanned)
        {
            if (!wrapper.sr || !wrapper.sr->IsValid())
                continue;

            const int row = this->m_srTable->rowCount();
            this->m_srTable->insertRow(row);

            QTableWidgetItem* iconItem = new QTableWidgetItem();
            iconItem->setIcon(IconManager::instance().GetIconForObject(wrapper.sr.data()));
            iconItem->setFlags(iconItem->flags() & ~Qt::ItemIsEditable);
            this->m_srTable->setItem(row, 0, iconItem);

            QTableWidgetItem* nameItem = new QTableWidgetItem(wrapper.sr->GetName());
            nameItem->setData(Qt::UserRole, wrapper.sr->OpaqueRef());
            this->m_srTable->setItem(row, 1, nameItem);

            this->m_srTable->setItem(row, 2, new QTableWidgetItem(wrapper.sr->GetDescription()));
            this->m_srTable->setItem(row, 3, new QTableWidgetItem(wrapper.enabled ? tr("") : wrapper.reasonUnsuitable));

            if (wrapper.enabled)
            {
                this->m_heartbeatSRs.append({wrapper.sr->OpaqueRef(), wrapper.sr->GetName()});
            } else
            {
                for (int col = 0; col < 4; ++col)
                {
                    QTableWidgetItem* item = this->m_srTable->item(row, col);
                    if (!item)
                        continue;
                    item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
                    item->setForeground(Qt::gray);
                }
            }
        }
    }

    this->m_rescanButton->setEnabled(true);
    this->m_srTable->setEnabled(true);

    if (this->m_srTable->rowCount() == 0)
    {
        this->m_noSRsLabel->setVisible(true);
    }

    // Re-select previous selection if still valid
    if (!this->m_selectedHeartbeatSR.isEmpty())
    {
        for (int row = 0; row < this->m_srTable->rowCount(); ++row)
        {
            QTableWidgetItem* item = this->m_srTable->item(row, 1);
            if (item && item->data(Qt::UserRole).toString() == this->m_selectedHeartbeatSR)
            {
                if (item->flags() & Qt::ItemIsEnabled)
                {
                    this->m_srTable->selectRow(row);
                }
                break;
            }
        }
    }

    return !cancelled;
}

void HAWizard::onHeartbeatSRSelectionChanged()
{
    QList<QTableWidgetItem*> selected = this->m_srTable->selectedItems();
    if (!selected.isEmpty())
    {
        QTableWidgetItem* item = this->m_srTable->item(selected.first()->row(), 1);
        if (item && (item->flags() & Qt::ItemIsEnabled))
        {
            this->m_selectedHeartbeatSR = item->data(Qt::UserRole).toString();
            this->m_selectedHeartbeatSRName = item->text();
        } else
        {
            this->m_selectedHeartbeatSR.clear();
            this->m_selectedHeartbeatSRName.clear();
        }
    } else
    {
        this->m_selectedHeartbeatSR.clear();
        this->m_selectedHeartbeatSRName.clear();
    }
}

void HAWizard::populateVMTable()
{
    this->m_vmTable->blockSignals(true);
    this->m_vmTable->setRowCount(0);
    this->m_vmStartupOptions.clear();
    this->m_vmAgilityKnown.clear();
    this->m_vmIsAgile.clear();
    this->m_pendingPriorityInitialization.clear();

    // Get all VMs from cache
    XenCache* cache = this->cache();
    if (!cache)
        return;

    const QList<QSharedPointer<VM>> vms = cache->GetAll<VM>(XenObjectType::VM);
    bool firstTime = this->m_protectVmsByDefault;
    QList<QSharedPointer<VM>> protectableVms;

    static constexpr int ColIcon = 0;
    static constexpr int ColVm = 1;
    static constexpr int ColPriority = 2;
    static constexpr int ColOrder = 3;
    static constexpr int ColDelay = 4;
    static constexpr int ColAgility = 5;

    for (const QSharedPointer<VM>& vm : vms)
    {
        if (!isVmProtectable(vm))
            continue;

        protectableVms.append(vm);
        if (!this->normalizePriority(vm->HARestartPriority()).isEmpty())
            firstTime = false;
    }

    for (const QSharedPointer<VM>& vm : protectableVms)
    {
        const QString vmRef = vm->OpaqueRef();
        const int row = this->m_vmTable->rowCount();
        this->m_vmTable->insertRow(row);

        auto* iconItem = new QTableWidgetItem();
        iconItem->setIcon(IconManager::instance().GetIconForObject(vm.data()));
        iconItem->setFlags(iconItem->flags() & ~Qt::ItemIsEditable);
        this->m_vmTable->setItem(row, ColIcon, iconItem);

        auto* nameItem = new QTableWidgetItem(vm->GetName());
        nameItem->setData(Qt::UserRole, vmRef);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        this->m_vmTable->setItem(row, ColVm, nameItem);

        QVariantMap options;
        options["order"] = vm->Order();
        options["start_delay"] = vm->StartDelay();

        QString priority = this->normalizePriority(vm->HARestartPriority());
        if (firstTime)
        {
            // C# first-time flow: assign after agility determination.
            this->m_pendingPriorityInitialization.insert(vmRef);
            priority.clear();
        }
        options["ha_restart_priority"] = priority;
        this->m_vmStartupOptions.insert(vmRef, options);

        auto* priorityItem = new QTableWidgetItem();
        priorityItem->setFlags(priorityItem->flags() & ~Qt::ItemIsEditable);
        this->m_vmTable->setItem(row, ColPriority, priorityItem);

        auto* orderItem = new QTableWidgetItem();
        orderItem->setFlags(orderItem->flags() & ~Qt::ItemIsEditable);
        this->m_vmTable->setItem(row, ColOrder, orderItem);

        auto* delayItem = new QTableWidgetItem();
        delayItem->setFlags(delayItem->flags() & ~Qt::ItemIsEditable);
        this->m_vmTable->setItem(row, ColDelay, delayItem);

        auto* agilityItem = new QTableWidgetItem(tr("Checking..."));
        agilityItem->setFlags(agilityItem->flags() & ~Qt::ItemIsEditable);
        this->m_vmTable->setItem(row, ColAgility, agilityItem);

        this->m_vmAgilityKnown.insert(vmRef, false);
        this->m_vmIsAgile.insert(vmRef, false);
        this->setVmRowValues(row, vmRef);
    }

    this->m_protectVmsByDefault = firstTime;
    this->m_vmTable->blockSignals(false);

    this->updateAgilityForRows();
    this->refreshSelectionEditors();
}

void HAWizard::onNtolChanged(int value)
{
    this->m_ntol = value;
    this->updateNtolCalculation();
}

void HAWizard::onVmSelectionChanged()
{
    this->refreshSelectionEditors();
}

void HAWizard::onSelectedPriorityChanged(int index)
{
    if (this->m_updatingSelectionEditors || index < 0)
        return;

    const QString target = this->m_selectedPriorityCombo->currentData().toString();
    if (target == "__mixed__")
        return;

    const QList<QTableWidgetItem*> selectedItems = this->m_vmTable->selectedItems();
    QSet<int> rows;
    for (QTableWidgetItem* item : selectedItems)
        rows.insert(item->row());

    bool changed = false;
    for (int row : rows)
    {
        QTableWidgetItem* vmItem = this->m_vmTable->item(row, 1);
        if (!vmItem)
            continue;
        const QString vmRef = vmItem->data(Qt::UserRole).toString();
        QVariantMap options = this->m_vmStartupOptions.value(vmRef);
        QString priority = target;
        if (priority == "restart" && this->m_vmAgilityKnown.value(vmRef, false) && !this->m_vmIsAgile.value(vmRef, false))
            priority = "best-effort";
        if (this->normalizePriority(options.value("ha_restart_priority").toString()) == priority)
            continue;
        options["ha_restart_priority"] = priority;
        this->m_vmStartupOptions.insert(vmRef, options);
        this->setVmRowValues(row, vmRef);
        changed = true;
    }

    if (changed)
        this->updateNtolCalculation();
    this->refreshSelectionEditors();
}

void HAWizard::onSelectedOrderChanged(int value)
{
    if (this->m_updatingSelectionEditors)
        return;

    const QList<QTableWidgetItem*> selectedItems = this->m_vmTable->selectedItems();
    QSet<int> rows;
    for (QTableWidgetItem* item : selectedItems)
        rows.insert(item->row());

    bool changed = false;
    for (int row : rows)
    {
        QTableWidgetItem* vmItem = this->m_vmTable->item(row, 1);
        if (!vmItem)
            continue;
        const QString vmRef = vmItem->data(Qt::UserRole).toString();
        QVariantMap options = this->m_vmStartupOptions.value(vmRef);
        if (options.value("order", 0).toLongLong() == value)
            continue;
        options["order"] = value;
        this->m_vmStartupOptions.insert(vmRef, options);
        this->setVmRowValues(row, vmRef);
        changed = true;
    }

    if (changed)
        this->updateNtolCalculation();
}

void HAWizard::onSelectedDelayChanged(int value)
{
    if (this->m_updatingSelectionEditors)
        return;

    const QList<QTableWidgetItem*> selectedItems = this->m_vmTable->selectedItems();
    QSet<int> rows;
    for (QTableWidgetItem* item : selectedItems)
        rows.insert(item->row());

    bool changed = false;
    for (int row : rows)
    {
        QTableWidgetItem* vmItem = this->m_vmTable->item(row, 1);
        if (!vmItem)
            continue;
        const QString vmRef = vmItem->data(Qt::UserRole).toString();
        QVariantMap options = this->m_vmStartupOptions.value(vmRef);
        if (options.value("start_delay", 0).toLongLong() == value)
            continue;
        options["start_delay"] = value;
        this->m_vmStartupOptions.insert(vmRef, options);
        this->setVmRowValues(row, vmRef);
        changed = true;
    }

    if (changed)
        this->updateNtolCalculation();
}

void HAWizard::updateNtolCalculation()
{
    this->m_ntol = this->m_ntolSpinBox->value();
    if (!this->m_pool || !this->m_pool->GetConnection())
        return;

    setNtolUpdateInProgress(true);
    const int requestId = ++this->m_ntolRequestId;
    const QVariantMap ntolConfig = buildNtolConfig();
    const QString poolRef = this->m_pool->OpaqueRef();
    XenConnection* connection = this->m_pool->GetConnection();
    QPointer<HAWizard> self(this);

    QThread* thread = QThread::create([self, requestId, ntolConfig, poolRef, connection]()
    {
        if (!self || !connection || !connection->GetSession())
            return;

        bool ok = false;
        qint64 ntolMax = -1;
        XenAPI::Session* session = XenAPI::Session::DuplicateSession(connection->GetSession(), nullptr);
        if (session)
        {
            try
            {
                ntolMax = XenAPI::Pool::ha_compute_hypothetical_max_host_failures_to_tolerate(session, ntolConfig);
                ok = true;
            }
            catch (const std::exception&)
            {
                ok = false;
            }
            delete session;
        }

        QMetaObject::invokeMethod(self, [self, requestId, ok, ntolMax, poolRef, ntolConfig, connection]() {
            if (!self)
                return;
            self->applyNtolCalculationResult(requestId, ok, ntolMax, ntolConfig, poolRef, connection);
        }, Qt::QueuedConnection);
    });

    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

QVariantMap HAWizard::buildNtolConfig() const
{
    QVariantMap config;
    for (int row = 0; row < this->m_vmTable->rowCount(); ++row)
    {
        QTableWidgetItem* vmItem = this->m_vmTable->item(row, 1);
        if (!vmItem)
            continue;

        const QString vmRef = vmItem->data(Qt::UserRole).toString();
        const QString priority = this->normalizePriority(this->m_vmStartupOptions.value(vmRef).value("ha_restart_priority").toString());
        if (vmRef.isEmpty() || !isRestartPriority(priority))
            continue;

        config.insert(vmRef, "restart");
    }
    return config;
}

void HAWizard::updateAgilityForRows()
{
    if (!this->m_pool || !this->m_pool->GetConnection() || !this->m_pool->GetConnection()->GetSession())
        return;

    QStringList vmRefs;
    for (int row = 0; row < this->m_vmTable->rowCount(); ++row)
    {
        QTableWidgetItem* vmItem = this->m_vmTable->item(row, 1);
        if (!vmItem)
            continue;
        vmRefs.append(vmItem->data(Qt::UserRole).toString());
    }

    const int requestId = ++this->m_agilityRequestId;
    XenConnection* connection = this->m_pool->GetConnection();
    QPointer<HAWizard> self(this);

    QThread* thread = QThread::create([self, requestId, vmRefs, connection]() {
        if (!self || !connection || !connection->GetSession())
            return;

        QMap<QString, bool> agileMap;
        QMap<QString, QString> reasonMap;
        XenAPI::Session* session = XenAPI::Session::DuplicateSession(connection->GetSession(), nullptr);
        if (session)
        {
            for (const QString& vmRef : vmRefs)
            {
                bool agile = false;
                QString reason;
                try
                {
                    XenAPI::VM::assert_agile(session, vmRef);
                    agile = true;
                }
                catch (const std::exception& ex)
                {
                    agile = false;
                    reason = QString::fromUtf8(ex.what());
                }
                agileMap.insert(vmRef, agile);
                reasonMap.insert(vmRef, reason);
            }
            delete session;
        }

        QMetaObject::invokeMethod(self, [self, requestId, agileMap, reasonMap]() {
            if (!self)
                return;
            self->applyAgilityResults(requestId, agileMap, reasonMap);
        }, Qt::QueuedConnection);
    });

    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void HAWizard::applyNtolCalculationResult(int requestId, bool ok, qint64 ntolMax, const QVariantMap& ntolConfig, const QString& poolRef, XenConnection* connection)
{
    if (requestId != this->m_ntolRequestId)
        return;

    this->setNtolUpdateInProgress(false);
    if (!ok)
    {
        this->m_maxNtol = -1;
        this->ui->haStatusIconLabel->setText(QString::fromUtf8("\xE2\x9A\xA0"));
        this->m_ntolStatusLabel->setStyleSheet("color: #b8860b;");
        this->m_ntolStatusLabel->setText(QObject::tr("Unable to calculate failure tolerance for current priorities."));
        this->m_maxNtolLabel->setText(QObject::tr("Maximum: unavailable"));
        return;
    }

    this->m_maxNtol = qMax<qint64>(0, ntolMax);
    this->m_maxNtolLabel->setText(QObject::tr("(max = %1)").arg(this->m_maxNtol));

    if (connection && connection->GetCache())
    {
        QVariantMap poolData = connection->GetCache()->ResolveObjectData(XenObjectType::Pool, poolRef);
        const bool haEnabled = poolData.value("ha_enabled", false).toBool();
        if (!this->m_ntolInitializedFromServer)
        {
            QSignalBlocker blocker(this->m_ntolSpinBox);
            const qint64 initialNtol = haEnabled ? poolData.value("ha_host_failures_to_tolerate", 0).toLongLong() : this->m_maxNtol;
            const qint64 clampedInitial = qBound<qint64>(0LL, initialNtol, 9999LL);
            this->m_ntolSpinBox->setValue(static_cast<int>(clampedInitial));
            this->m_ntol = this->m_ntolSpinBox->value();
            this->m_ntolInitializedFromServer = true;
        }
    }

    const int protectedVMs = static_cast<int>(ntolConfig.size());
    if (this->m_ntol > 0 && protectedVMs > 0 && this->m_ntol <= this->m_maxNtol)
    {
        this->ui->haStatusIconLabel->setText(QString::fromUtf8("\xE2\x9C\x93"));
        this->m_ntolStatusLabel->setStyleSheet("color: green;");
        this->m_ntolStatusLabel->setText(QObject::tr("Pool can tolerate %1 host failure(s) with %2 protected VM(s)")
                                             .arg(this->m_ntol)
                                             .arg(protectedVMs));
    } else if (this->m_ntol == 0)
    {
        this->ui->haStatusIconLabel->setText(QString::fromUtf8("\xE2\x9A\xA0"));
        this->m_ntolStatusLabel->setStyleSheet("color: #b8860b;");
        this->m_ntolStatusLabel->setText(QObject::tr("NTOL is 0 - HA will not automatically restart VMs on host failure"));
    } else if (protectedVMs == 0)
    {
        this->ui->haStatusIconLabel->setText(QString::fromUtf8("\xE2\x9A\xA0"));
        this->m_ntolStatusLabel->setStyleSheet("color: #b8860b;");
        this->m_ntolStatusLabel->setText(QObject::tr("No VMs set to Restart priority"));
    } else
    {
        this->ui->haStatusIconLabel->setText(QString::fromUtf8("\xE2\x9A\xA0"));
        this->m_ntolStatusLabel->setStyleSheet("color: #b8860b;");
        this->m_ntolStatusLabel->setText(QObject::tr("Configured NTOL exceeds current maximum."));
    }
}

void HAWizard::applyAgilityResults(int requestId,
                                   const QMap<QString, bool>& agileMap,
                                   const QMap<QString, QString>& reasonMap)
{
    if (requestId != this->m_agilityRequestId)
        return;

    for (int row = 0; row < this->m_vmTable->rowCount(); ++row)
    {
        QTableWidgetItem* vmItem = this->m_vmTable->item(row, 1);
        QTableWidgetItem* agilityItem = this->m_vmTable->item(row, 5);
        if (!vmItem)
            continue;

        const QString vmRef = vmItem->data(Qt::UserRole).toString();
        if (!agileMap.contains(vmRef))
            continue;

        const bool isAgile = agileMap.value(vmRef);
        this->m_vmAgilityKnown[vmRef] = true;
        this->m_vmIsAgile[vmRef] = isAgile;

        QVariantMap options = this->m_vmStartupOptions.value(vmRef);
        QString priority = this->normalizePriority(options.value("ha_restart_priority").toString());

        if (this->m_pendingPriorityInitialization.contains(vmRef))
        {
            priority = isAgile ? QStringLiteral("restart") : QStringLiteral("best-effort");
            this->m_pendingPriorityInitialization.remove(vmRef);
        } else if (!isAgile && this->isRestartPriority(priority))
        {
            priority = QStringLiteral("best-effort");
        }

        options["ha_restart_priority"] = priority;
        this->m_vmStartupOptions.insert(vmRef, options);
        vmItem->setToolTip(isAgile ? QString() : reasonMap.value(vmRef));
        if (agilityItem)
            agilityItem->setText(isAgile ? tr("Yes") : tr("No"));

        this->setVmRowValues(row, vmRef);
    }

    this->updateNtolCalculation();
    this->refreshSelectionEditors();
}

void HAWizard::setNtolUpdateInProgress(bool inProgress)
{
    this->m_ntolUpdateInProgress = inProgress;
    if (this->button(QWizard::NextButton))
        this->button(QWizard::NextButton)->setEnabled(!inProgress);
    if (this->button(QWizard::FinishButton))
        this->button(QWizard::FinishButton)->setEnabled(!inProgress);
    if (inProgress)
    {
        this->ui->haStatusIconLabel->setText("...");
        this->m_ntolStatusLabel->setStyleSheet("color: gray;");
        this->m_ntolStatusLabel->setText(tr("Calculating host failure tolerance..."));
    }
}

bool HAWizard::isRestartPriority(const QString& priority) const
{
    return this->normalizePriority(priority) == "restart";
}

bool HAWizard::isVmProtectable(const QSharedPointer<VM>& vm) const
{
    return vm && vm->IsValid() && !vm->IsTemplate() && !vm->IsControlDomain() && !vm->IsSnapshot() && vm->Show(false);
}

QString HAWizard::normalizePriority(const QString& priority) const
{
    const QString normalized = priority.trimmed().toLower();
    if (normalized == "always_restart_high_priority" || normalized == "always_restart" || normalized == "restart")
        return "restart";
    if (normalized == "best_effort" || normalized == "best-effort")
        return "best-effort";
    return "";
}

QString HAWizard::priorityDisplayText(const QString& priority) const
{
    const QString normalized = this->normalizePriority(priority);
    if (normalized == "restart")
        return tr("Restart");
    if (normalized == "best-effort")
        return tr("Restart if possible");
    return tr("Do not restart");
}

void HAWizard::setVmRowValues(int row, const QString& vmRef)
{
    if (row < 0 || row >= this->m_vmTable->rowCount())
        return;

    const QVariantMap options = this->m_vmStartupOptions.value(vmRef);
    const QString priority = this->normalizePriority(options.value("ha_restart_priority").toString());
    const qint64 order = options.value("order", 0).toLongLong();
    const qint64 delay = options.value("start_delay", 0).toLongLong();

    if (QTableWidgetItem* priorityItem = this->m_vmTable->item(row, 2))
        priorityItem->setText(this->priorityDisplayText(priority));
    if (QTableWidgetItem* orderItem = this->m_vmTable->item(row, 3))
        orderItem->setText(QString::number(order));
    if (QTableWidgetItem* delayItem = this->m_vmTable->item(row, 4))
        delayItem->setText(tr("%1 seconds").arg(delay));
}

void HAWizard::refreshSelectionEditors()
{
    if (!this->m_selectedPriorityCombo || !this->m_selectedOrderSpin || !this->m_selectedDelaySpin)
        return;

    this->m_updatingSelectionEditors = true;
    const QSignalBlocker b1(this->m_selectedPriorityCombo);
    const QSignalBlocker b2(this->m_selectedOrderSpin);
    const QSignalBlocker b3(this->m_selectedDelaySpin);

    const QList<QTableWidgetItem*> selectedItems = this->m_vmTable->selectedItems();
    QSet<int> rows;
    for (QTableWidgetItem* item : selectedItems)
        rows.insert(item->row());

    if (rows.isEmpty())
    {
        this->m_selectedPriorityCombo->setCurrentIndex(0);
        this->m_selectedPriorityCombo->setEnabled(false);
        this->m_selectedOrderSpin->setValue(0);
        this->m_selectedOrderSpin->setEnabled(false);
        this->m_selectedDelaySpin->setValue(0);
        this->m_selectedDelaySpin->setEnabled(false);
        this->m_updatingSelectionEditors = false;
        return;
    }

    QString firstPriority;
    qint64 firstOrder = 0;
    qint64 firstDelay = 0;
    bool priorityMixed = false;
    bool orderMixed = false;
    bool delayMixed = false;
    bool anyAgilityUnknown = false;
    bool anyNonAgile = false;
    bool first = true;

    for (int row : rows)
    {
        QTableWidgetItem* vmItem = this->m_vmTable->item(row, 1);
        if (!vmItem)
            continue;
        const QString vmRef = vmItem->data(Qt::UserRole).toString();
        const QVariantMap options = this->m_vmStartupOptions.value(vmRef);
        const QString priority = this->normalizePriority(options.value("ha_restart_priority").toString());
        const qint64 order = options.value("order", 0).toLongLong();
        const qint64 delay = options.value("start_delay", 0).toLongLong();

        if (!this->m_vmAgilityKnown.value(vmRef, false))
            anyAgilityUnknown = true;
        else if (!this->m_vmIsAgile.value(vmRef, false))
            anyNonAgile = true;

        if (first)
        {
            firstPriority = priority;
            firstOrder = order;
            firstDelay = delay;
            first = false;
            continue;
        }

        if (firstPriority != priority)
            priorityMixed = true;
        if (firstOrder != order)
            orderMixed = true;
        if (firstDelay != delay)
            delayMixed = true;
    }

    this->m_selectedPriorityCombo->setEnabled(!anyAgilityUnknown);
    this->m_selectedOrderSpin->setEnabled(!anyAgilityUnknown);
    this->m_selectedDelaySpin->setEnabled(!anyAgilityUnknown);

    // Keep restart choice visible; non-agile rows will be coerced to Best Effort on apply.
    Q_UNUSED(anyNonAgile);

    if (priorityMixed)
    {
        this->m_selectedPriorityCombo->setCurrentIndex(0);
    } else
    {
        int idx = 3;
        if (firstPriority == "restart")
            idx = 1;
        else if (firstPriority == "best-effort")
            idx = 2;
        this->m_selectedPriorityCombo->setCurrentIndex(idx);
    }

    this->m_selectedOrderSpin->setToolTip(orderMixed ? tr("Selected VMs have mixed values.") : QString());
    this->m_selectedDelaySpin->setToolTip(delayMixed ? tr("Selected VMs have mixed values.") : QString());
    this->m_selectedOrderSpin->setValue(static_cast<int>(qBound<qint64>(0LL, firstOrder, 9999LL)));
    this->m_selectedDelaySpin->setValue(static_cast<int>(qBound<qint64>(0LL, firstDelay, 600LL)));
    this->m_updatingSelectionEditors = false;
}

void HAWizard::updateFinishPage()
{
    this->m_finishSRLabel->setText(this->m_selectedHeartbeatSRName);
    this->m_finishNtolLabel->setText(QString::number(this->m_ntol));

    // Count VMs by priority
    int restartCount = 0;
    int bestEffortCount = 0;
    int doNotRestartCount = 0;

    for (int row = 0; row < this->m_vmTable->rowCount(); ++row)
    {
        QTableWidgetItem* vmItem = this->m_vmTable->item(row, 1);
        if (!vmItem)
            continue;
        const QString vmRef = vmItem->data(Qt::UserRole).toString();
        const QString priority = this->normalizePriority(this->m_vmStartupOptions.value(vmRef).value("ha_restart_priority").toString());
        if (priority == "restart")
            restartCount++;
        else if (priority == "best-effort")
            bestEffortCount++;
        else
            doNotRestartCount++;
    }

    this->m_finishRestartLabel->setText(QString::number(restartCount));
    this->m_finishBestEffortLabel->setText(QString::number(bestEffortCount));
    this->m_finishDoNotRestartLabel->setText(QString::number(doNotRestartCount));

    // Show warnings if needed
    bool showWarning = false;
    QString warningText;

    if (restartCount + bestEffortCount == 0 && doNotRestartCount > 0)
    {
        showWarning = true;
        warningText = tr("No VMs are configured for restart. HA will be enabled but no VMs will be protected.");
    } else if (this->m_ntol == 0)
    {
        showWarning = true;
        warningText = tr("Host failures to tolerate is set to 0. HA monitoring will be enabled but VMs will not be automatically restarted.");
    }

    this->m_finishWarningIcon->setVisible(showWarning);
    this->m_finishWarningLabel->setVisible(showWarning);
    this->m_finishWarningLabel->setText(warningText);
}

XenCache* HAWizard::cache() const
{
    return this->m_pool ? this->m_pool->GetCache() : nullptr;
}

QString HAWizard::priorityToString(HaRestartPriority priority) const
{
    switch (priority)
    {
    case AlwaysRestartHighPriority:
        return "always_restart_high_priority";
    case AlwaysRestart:
        return "restart";
    case BestEffort:
        return "best-effort";
    case DoNotRestart:
    default:
        return "";
    }
}

HAWizard::HaRestartPriority HAWizard::stringToPriority(const QString& str) const
{
    if (str == "always_restart_high_priority")
        return AlwaysRestartHighPriority;
    if (str == "restart" || str == "always_restart")
        return AlwaysRestart;
    if (str == "best-effort" || str == "best_effort")
        return BestEffort;
    return DoNotRestart;
}

int HAWizard::countVMsByPriority(HaRestartPriority priority) const
{
    int count = 0;
    for (auto it = this->m_vmPriorities.constBegin(); it != this->m_vmPriorities.constEnd(); ++it)
    {
        if (it.value() == priority)
        {
            count++;
        }
    }
    return count;
}

int HAWizard::wizardStepIndexForPage(int pageId) const
{
    if (this->m_rbacRequired)
        return pageId;

    switch (pageId)
    {
        case Page_Intro:
            return 0;
        case Page_ChooseSR:
            return 1;
        case Page_AssignPriorities:
            return 2;
        case Page_Finish:
            return 3;
        default:
            return 0;
    }
}
