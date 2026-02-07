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

#include "vmoperationmenu.h"
#include "../mainwindow.h"
#include "../commands/vm/vmoperationhelpers.h"
#include "../commands/vm/crosspoolmigratecommand.h"
#include "../dialogs/commanderrordialog.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/actions/vm/vmstartonaction.h"
#include "xenlib/xen/actions/vm/vmresumeonaction.h"
#include "xenlib/xen/actions/vm/vmmigrateaction.h"
#include "xenlib/xen/actions/vm/vmstartabstractaction.h"
#include "xenlib/xen/actions/wlb/wlbretrievevmrecommendationsaction.h"
#include "xenlib/xen/actions/wlb/wlbrecommendations.h"
#include "xenlib/operations/producerconsumerqueue.h"
#include <QMutexLocker>
#include <QSet>

namespace
{
    bool isIntraPoolMigrationRestricted(XenConnection* connection)
    {
        if (!connection)
            return false;

        XenCache* cache = connection->GetCache();

        QList<QSharedPointer<Host>> hosts = cache->GetAll<Host>();
        for (const QSharedPointer<Host>& host : hosts)
        {
            if (host && host->RestrictIntraPoolMigrate())
                return true;
        }

        return false;
    }

    QString joinUniqueReasons(const QSet<QString>& reasons)
    {
        if (reasons.size() != 1)
            return QString();
        return *reasons.begin();
    }
}

VMOperationMenu::VMOperationMenu(MainWindow* main_window, const QList<QSharedPointer<VM>>& vms, Operation operation, QWidget* parent) : QMenu(parent),
      m_mainWindow(main_window), m_vms(vms), m_operation(operation)
{
    this->m_operationName = this->getOperationName();
    this->setTitle(this->getMenuText());
    
    // Populate menu when it's about to be shown
    connect(this, &QMenu::aboutToShow, this, &VMOperationMenu::aboutToShowMenu);
    connect(this, &QMenu::aboutToHide, this, &VMOperationMenu::stop);
}

VMOperationMenu::~VMOperationMenu()
{
    this->stop();
    
    qDeleteAll(this->m_hostMenuItems);
    this->m_hostMenuItems.clear();
    
    this->clearWorkerQueue();
}

QString VMOperationMenu::getOperationName() const
{
    switch (this->m_operation)
    {
        case Operation::StartOn:
            return "start_on";
        case Operation::ResumeOn:
            return "resume_on";
        case Operation::Migrate:
            return "pool_migrate";
    }
    return "pool_migrate";
}

QString VMOperationMenu::getMenuText() const
{
    switch (this->m_operation)
    {
        case Operation::StartOn:
            return tr("Start on Server");
        case Operation::ResumeOn:
            return tr("Resume on Server");
        case Operation::Migrate:
            return tr("Migrate to Server");
    }
    return tr("VM Operation");
}

void VMOperationMenu::aboutToShowMenu()
{
    this->populate();
}

void VMOperationMenu::stop()
{
    this->setStopped(true);
    
    if (this->m_workerQueue)
    {
        this->m_workerQueue->CancelWorkers(false);
    }
}

bool VMOperationMenu::isStopped() const
{
    QMutexLocker locker(&this->m_stopMutex);
    return this->m_stopped;
}

void VMOperationMenu::setStopped(bool stopped)
{
    QMutexLocker locker(&this->m_stopMutex);
    this->m_stopped = stopped;
}

void VMOperationMenu::clearWorkerQueue()
{
    if (this->m_workerQueue)
    {
        this->m_workerQueue->CancelWorkers(false);
        delete this->m_workerQueue;
        this->m_workerQueue = nullptr;
    }
}

XenConnection* VMOperationMenu::getConnection() const
{
    if (this->m_vms.isEmpty())
        return nullptr;
    return this->m_vms.first()->GetConnection();
}

QStringList VMOperationMenu::getSelectionRefs() const
{
    QStringList refs;
    for (const QSharedPointer<VM>& vm : this->m_vms)
    {
        refs.append(vm->OpaqueRef());
    }
    return refs;
}

QString VMOperationMenu::getErrorDialogTitle() const
{
    switch (this->m_operation)
    {
        case Operation::StartOn:
            return tr("Error Starting VM on Server");
        case Operation::ResumeOn:
            return tr("Error Resuming VM on Server");
        case Operation::Migrate:
            return tr("Error Migrating VM to Server");
    }

    return tr("Error Performing VM Operation");
}

QString VMOperationMenu::getErrorDialogText() const
{
    switch (this->m_operation)
    {
        case Operation::StartOn:
            return tr("The following VMs could not be started on the selected server:");
        case Operation::ResumeOn:
            return tr("The following VMs could not be resumed on the selected server:");
        case Operation::Migrate:
            return tr("The following VMs could not be migrated to the selected server:");
    }

    return tr("The following VMs could not be processed:");
}

bool VMOperationMenu::showCantRunDialog(const QHash<QSharedPointer<VM>, QString>& cantRunReasons, bool allowProceed)
{
    if (cantRunReasons.isEmpty())
        return false;

    QHash<QSharedPointer<XenObject>, QString> dialogReasons;
    for (auto it = cantRunReasons.begin(); it != cantRunReasons.end(); ++it)
    {
        if (!it.key())
            continue;
        dialogReasons.insert(qSharedPointerCast<XenObject>(it.key()), it.value());
    }

    CommandErrorDialog::DialogMode mode = allowProceed
        ? CommandErrorDialog::DialogMode::OKCancel
        : CommandErrorDialog::DialogMode::Close;

    CommandErrorDialog dialog(this->getErrorDialogTitle(), this->getErrorDialogText(), dialogReasons, mode, this->m_mainWindow);
    int result = dialog.exec();
    if (!allowProceed)
        return false;

    return result == QDialog::Accepted;
}

void VMOperationMenu::addDisabledReason(const QString& reason)
{
    QAction* action = this->addAction(reason);
    action->setEnabled(false);
}

void VMOperationMenu::populate()
{
    // Clear existing items
    this->clear();
    qDeleteAll(this->m_hostMenuItems);
    this->m_hostMenuItems.clear();
    this->m_additionalActions.clear();
    this->m_wlbRecommendations.clear();
    this->setStopped(false);
    this->clearWorkerQueue();
    this->menuAction()->setEnabled(true);
    
    if (this->m_vms.isEmpty())
    {
        this->addDisabledReason(tr("No VM selected."));
        this->menuAction()->setEnabled(false);
        return;
    }

    XenConnection* connection = this->getConnection();

    if (!connection || !connection->IsConnected())
    {
        this->addDisabledReason(tr("Not connected to server."));
        this->menuAction()->setEnabled(false);
        return;
    }

    XenCache* cache = connection->GetCache();

    for (const QSharedPointer<VM>& vm : this->m_vms)
    {
        if (vm->GetConnection() != connection)
        {
            this->addDisabledReason(tr("Selected VMs must be on the same server."));
            this->menuAction()->setEnabled(false);
            return;
        }
    }

    // Check if operation is allowed
    bool atLeastOneAllowed = false;
    for (const QSharedPointer<VM>& vm : this->m_vms)
    {
        if (vm->GetAllowedOperations().contains(this->m_operationName))
        {
            atLeastOneAllowed = true;
            break;
        }
    }

    if (!atLeastOneAllowed)
    {
        QString message;
        switch (this->m_operation)
        {
            case Operation::StartOn:
                message = tr("VM does not allow start operation.");
                break;
            case Operation::ResumeOn:
                message = tr("VM does not allow resume operation.");
                break;
            case Operation::Migrate:
                message = tr("VM does not allow migration.");
                break;
        }
        this->addDisabledReason(message);
        this->menuAction()->setEnabled(false);
        return;
    }

    if (this->m_operation == Operation::Migrate && isIntraPoolMigrationRestricted(connection))
    {
        // TODO verify if this licensing is even applicable to XCP-ng, it sounds like some historic stuff
        this->addDisabledReason(tr("Migration is restricted by licensing."));
        this->menuAction()->setEnabled(false);
        return;
    }

    // Check for WLB (Workload Balancing) support
    bool wlbEnabled = false;
    QSharedPointer<Pool> pool = cache->GetPoolOfOne();
    if (pool && pool->IsValid())
    {
        QString wlbUrl = pool->WLBUrl();
        wlbEnabled = pool->IsWLBEnabled() && !wlbUrl.isEmpty();
    }

    // Add home/optimal server item first
    HostMenuItem* firstItem = new HostMenuItem();
    firstItem->isHomeServer = !wlbEnabled;
    firstItem->isOptimalServer = wlbEnabled;
    firstItem->starRating = 0.0;
    firstItem->canRunAny = false;
    firstItem->action = this->addAction(wlbEnabled ? tr("Optimal Server") : tr("Home Server"));
    firstItem->action->setEnabled(false);  // Will be enabled in updateHostList
    this->m_hostMenuItems.append(firstItem);

    if (wlbEnabled)
    {
        connect(firstItem->action, &QAction::triggered, this, &VMOperationMenu::runOptimalServerOperation);
    } else
    {
        connect(firstItem->action, &QAction::triggered, this, &VMOperationMenu::runHomeServerOperation);
    }

    // Add host menu items (will be populated in updateHostList)
    QList<QSharedPointer<Host>> hosts = cache->GetAll<Host>();
    std::sort(hosts.begin(), hosts.end(), [](const QSharedPointer<Host>& a, const QSharedPointer<Host>& b) {
        return a->GetName() < b->GetName();
    });

    for (const QSharedPointer<Host>& host : hosts)
    {
        if (!host || !host->IsValid())
            continue;

        HostMenuItem* hostItem = new HostMenuItem();
        hostItem->host = host;
        hostItem->isHomeServer = false;
        hostItem->isOptimalServer = false;
        hostItem->starRating = 0.0;
        hostItem->canRunAny = false;
        hostItem->action = this->addAction(tr("Updating %1...").arg(host->GetName()));
        hostItem->action->setEnabled(false);
        this->m_hostMenuItems.append(hostItem);
        
        connect(hostItem->action, &QAction::triggered, this, [this, host]() {
            this->runOperationOnHost(host);
        });
    }

    // Add additional menu items (for subclasses to override)
    this->AddAdditionalMenuItems();

    // Update host list asynchronously
    this->updateHostList();
}

void VMOperationMenu::AddAdditionalMenuItems()
{
    if (this->m_operation != Operation::Migrate)
        return;

    QAction* separator = this->addSeparator();
    this->m_additionalActions.append(separator);

    CrossPoolMigrateCommand* cmd = new CrossPoolMigrateCommand(this->m_mainWindow, CrossPoolMigrateWizard::WizardMode::Migrate, false, this);
    cmd->SetSelection(this->getSelectionRefs());

    QAction* action = this->addAction(cmd->MenuText());
    QIcon icon = cmd->GetIcon();
    if (!icon.isNull())
        action->setIcon(icon);
    action->setEnabled(cmd->CanRun());
    this->m_additionalActions.append(action);

    connect(action, &QAction::triggered, this, [cmd]() {
        cmd->Run();
    });
}

void VMOperationMenu::updateHostList()
{
    this->setStopped(false);

    XenConnection* connection = this->getConnection();
    if (!connection)
        return;

    // Check for WLB and call enableAppropriateHostsWlb() if enabled
    bool wlbEnabled = false;
    QSharedPointer<Pool> pool = connection->GetCache()->GetPoolOfOne();
    if (pool && pool->IsValid())
    {
        QString wlbUrl = pool->WLBUrl();
        wlbEnabled = pool->IsWLBEnabled() && !wlbUrl.isEmpty();
    }

    if (wlbEnabled)
    {
        this->enableAppropriateHostsWlb();
    } else
    {
        this->enableAppropriateHostsNoWlb();
    }
}

void VMOperationMenu::enableAppropriateHostsNoWlb()
{
    if (this->isStopped() || this->m_hostMenuItems.isEmpty())
        return;

    // Create worker queue for parallel eligibility checks
    this->clearWorkerQueue();
    this->m_workerQueue = new ProducerConsumerQueue(25);

    // Get affinity host (home server)
    QSharedPointer<Host> affinityHost = this->m_vms.first()->GetAffinityHost();

    // Process home server item
    if (!this->m_hostMenuItems.isEmpty() && this->m_hostMenuItems.first()->isHomeServer)
    {
        this->enqueueHostMenuItem(affinityHost, this->m_hostMenuItems.first(), true);
    }

    if (this->isStopped())
        return;

    // Process remaining host items
    for (int i = 1; i < this->m_hostMenuItems.count(); ++i)
    {
        HostMenuItem* item = this->m_hostMenuItems[i];
        if (item->host)
        {
            this->enqueueHostMenuItem(item->host, item, false);
        }
    }
}

void VMOperationMenu::enableAppropriateHostsWlb()
{
    if (this->isStopped() || this->m_hostMenuItems.isEmpty())
        return;

    XenConnection* connection = this->getConnection();
    if (!connection)
        return;

    // Retrieve WLB recommendations for this VM
    WlbRetrieveVmRecommendationsAction* wlbAction = new WlbRetrieveVmRecommendationsAction(connection, this->m_vms, this);

    connect(wlbAction, &AsyncOperation::completed, wlbAction, [this, wlbAction]()
    {
        if (this->isStopped())
        {
            wlbAction->deleteLater();
            return;
        }

        if (wlbAction->IsFailed())
        {
            this->enableAppropriateHostsNoWlb();
            wlbAction->deleteLater();
            return;
        }

        // Get recommendations
        QHash<QSharedPointer<VM>, QHash<QSharedPointer<Host>, QStringList>> recommendations = wlbAction->GetRecommendations();
        WlbRecommendations wlbRecs(this->m_vms, recommendations);

        if (wlbRecs.IsError())
        {
            qDebug() << "WLB recommendations returned error, falling back to non-WLB";
            this->enableAppropriateHostsNoWlb();
            wlbAction->deleteLater();
            return;
        }

        this->m_wlbRecommendations = QSharedPointer<WlbRecommendations>::create(wlbRecs);

        // Update home server item (optimal server)
        if (!this->m_hostMenuItems.isEmpty() && this->m_hostMenuItems.first()->isOptimalServer)
        {
            HostMenuItem* homeItem = this->m_hostMenuItems.first();

            bool anyOptimal = false;
            for (const QSharedPointer<VM>& vm : this->m_vms)
            {
                if (wlbRecs.GetOptimalServer(vm))
                {
                    anyOptimal = true;
                    break;
                }
            }

            homeItem->action->setText(tr("Optimal Server"));
            homeItem->action->setEnabled(anyOptimal);
            homeItem->canRunAny = anyOptimal;
        }

        // Update remaining host items with WLB star ratings
        for (int i = 1; i < this->m_hostMenuItems.count(); ++i)
        {
            HostMenuItem* item = this->m_hostMenuItems[i];
            if (!item->host)
                continue;

            WlbRecommendation rec = wlbRecs.GetStarRating(item->host);
            
            QString label = item->host->GetName();
            bool canRunAny = false;
            QSet<QString> reasons;
            item->cantRunReasons.clear();
            for (const QSharedPointer<VM>& vm : this->m_vms)
            {
                bool canRun = rec.CanRunByVM.value(vm, false);
                if (canRun)
                {
                    canRunAny = true;
                } else
                {
                    QString reason = rec.CantRunReasons.value(vm, QString());
                    if (!reason.isEmpty())
                        reasons.insert(reason);
                    item->cantRunReasons.insert(vm, reason);
                }
            }
            
            // Format star rating (0-5 stars)
            if (rec.StarRating > 0 && canRunAny)
            {
                int stars = qRound(rec.StarRating);
                QString starStr = QString(stars, QChar(0x2605)); // Unicode star
                label += QString(" [%1]").arg(starStr);
            }

            QString uniqueReason = joinUniqueReasons(reasons);
            if (!canRunAny && !uniqueReason.isEmpty())
                label += QString(" - %1").arg(uniqueReason);

            item->action->setText(label);
            item->action->setEnabled(canRunAny);
            item->starRating = rec.StarRating;
            item->canRunAny = canRunAny;
        }

        // Sort hosts by star rating and enabled status (matches C# ordering)
        QList<HostMenuItem*> hostItems = this->m_hostMenuItems.mid(1);
        std::sort(hostItems.begin(), hostItems.end(), [](HostMenuItem* a, HostMenuItem* b) {
            if (a->action->isEnabled() && b->action->isEnabled())
            {
                if (!qFuzzyCompare(a->starRating, b->starRating))
                    return a->starRating > b->starRating;
            } else if (!a->action->isEnabled() && !b->action->isEnabled())
            {
                return a->host && b->host ? a->host->GetName() < b->host->GetName() : false;
            } else if (!a->action->isEnabled())
            {
                return false;
            } else if (!b->action->isEnabled())
            {
                return true;
            }

            return a->host && b->host ? a->host->GetName() < b->host->GetName() : false;
        });

        QAction* insertBefore = this->m_additionalActions.isEmpty() ? nullptr : this->m_additionalActions.first();
        for (HostMenuItem* item : hostItems)
            this->removeAction(item->action);
        for (HostMenuItem* item : hostItems)
            this->insertAction(insertBefore, item->action);

        wlbAction->deleteLater();
    });

    wlbAction->RunAsync();
}

void VMOperationMenu::enqueueHostMenuItem(const QSharedPointer<Host>& host, HostMenuItem* menuItem, bool isHomeServer)
{
    if (!this->m_workerQueue)
        return;

    this->m_workerQueue->EnqueueTask([this, host, menuItem, isHomeServer]()
    {
        if (this->isStopped())
            return;

        XenConnection* connection = this->getConnection();
        if (!connection)
            return;

        bool canRunAny = false;
        QSet<QString> reasons;
        menuItem->cantRunReasons.clear();

        if (isHomeServer)
        {
            // Home server operation
            for (const QSharedPointer<VM>& vm : this->m_vms)
            {
                QString reason;
                bool canRun = false;
                if (host)
                {
                    canRun = VMOperationHelpers::VMCanBootOnHost(connection, vm, host->OpaqueRef(), this->m_operationName, &reason);
                } else
                {
                    reason = tr("No home server");
                }

                if (canRun)
                {
                    canRunAny = true;
                } else
                {
                    menuItem->cantRunReasons.insert(vm, reason);
                    if (!reason.isEmpty())
                        reasons.insert(reason);
                }
            }

            QMetaObject::invokeMethod(this, [this, menuItem, canRunAny, reasons, host]()
            {
                if (this->isStopped())
                    return;

                QString label = tr("Home Server");
                if (host)
                    label += QString(" (%1)").arg(host->GetName());

                QString uniqueReason = joinUniqueReasons(reasons);
                if (!canRunAny && !uniqueReason.isEmpty())
                    label += QString(" - %1").arg(uniqueReason);

                menuItem->action->setText(label);
                menuItem->action->setEnabled(canRunAny);
                menuItem->canRunAny = canRunAny;
            }, Qt::QueuedConnection);
        } else if (host)
        {
            // Regular host operation
            for (const QSharedPointer<VM>& vm : this->m_vms)
            {
                QString reason;
                bool canRun = VMOperationHelpers::VMCanBootOnHost(connection, vm,
                                                                 host->OpaqueRef(),
                                                                 this->m_operationName,
                                                                 &reason);
                if (canRun)
                {
                    canRunAny = true;
                } else
                {
                    menuItem->cantRunReasons.insert(vm, reason);
                    if (!reason.isEmpty())
                        reasons.insert(reason);
                }
            }

            QMetaObject::invokeMethod(this, [this, menuItem, canRunAny, reasons, host]()
            {
                if (this->isStopped())
                    return;

                QString label = host->GetName();
                QString uniqueReason = joinUniqueReasons(reasons);
                if (!canRunAny && !uniqueReason.isEmpty())
                    label += QString(" - %1").arg(uniqueReason);

                menuItem->action->setText(label);
                menuItem->action->setEnabled(canRunAny);
                menuItem->reason = uniqueReason;
                menuItem->canRunAny = canRunAny;
            }, Qt::QueuedConnection);
        }
    });
}

void VMOperationMenu::runHomeServerOperation()
{
    if (this->m_vms.isEmpty())
        return;

    XenConnection* connection = this->getConnection();
    if (!connection)
        return;

    QSharedPointer<Host> affinityHost = this->m_vms.first()->GetAffinityHost();

    if (!affinityHost)
    {
        QHash<QSharedPointer<VM>, QString> reasons;
        for (const QSharedPointer<VM>& vm : this->m_vms)
        {
            reasons.insert(vm, tr("Home server not found."));
        }
        this->showCantRunDialog(reasons, false);
        return;
    }

    this->runOperationOnHost(affinityHost);
}

void VMOperationMenu::runOptimalServerOperation()
{
    if (this->m_vms.isEmpty() || !this->m_wlbRecommendations)
        return;

    QHash<QSharedPointer<VM>, QString> cantRun;
    QList<QPair<QSharedPointer<VM>, QSharedPointer<Host>>> targets;

    for (const QSharedPointer<VM>& vm : this->m_vms)
    {
        QSharedPointer<Host> optimalHost = this->m_wlbRecommendations->GetOptimalServer(vm);
        if (!optimalHost)
        {
            cantRun.insert(vm, tr("No optimal server available."));
            continue;
        }

        targets.append(qMakePair(vm, optimalHost));
    }

    if (!cantRun.isEmpty())
    {
        bool allowProceed = !targets.isEmpty();
        bool proceed = this->showCantRunDialog(cantRun, allowProceed);
        if (!allowProceed || !proceed)
            return;
    }

    for (const auto& target : targets)
        this->runOperationOnHostForVms(target.second, QList<QSharedPointer<VM>>{target.first});
}

void VMOperationMenu::runOperationOnHost(const QSharedPointer<Host>& host)
{
    this->runOperationOnHostForVms(host, this->m_vms);
}

void VMOperationMenu::runOperationOnHostForVms(const QSharedPointer<Host>& host, const QList<QSharedPointer<VM>>& vms)
{
    if (vms.isEmpty() || !host)
        return;

    XenConnection* connection = vms.first()->GetConnection();
    if (!connection || !connection->IsConnected())
    {
        QHash<QSharedPointer<VM>, QString> reasons;
        for (const QSharedPointer<VM>& vm : vms)
        {
            reasons.insert(vm, tr("Not connected to server."));
        }
        this->showCantRunDialog(reasons, false);
        return;
    }

    // Verify operation is still allowed
    QHash<QSharedPointer<VM>, QString> cantRun;
    QList<QSharedPointer<VM>> runnable;
    for (const QSharedPointer<VM>& vm : vms)
    {
        QString reason;
        if (VMOperationHelpers::VMCanBootOnHost(connection, vm, host->OpaqueRef(), this->m_operationName, &reason))
        {
            runnable.append(vm);
        } else
        {
            cantRun.insert(vm, reason);
        }
    }

    if (!cantRun.isEmpty())
    {
        bool allowProceed = !runnable.isEmpty();
        bool proceed = this->showCantRunDialog(cantRun, allowProceed);
        if (!allowProceed || !proceed)
            return;
    }

    // Create appropriate action
    for (const QSharedPointer<VM>& vm : runnable)
    {
        AsyncOperation* action = nullptr;

        switch (this->m_operation)
        {
            case Operation::StartOn:
                action = new VMStartOnAction(vm, host, 
                                             nullptr,  // WarningDialogHAInvalidConfig
                                             [this, vm](VMStartAbstractAction*, const Failure& failure) {
                                                 VMOperationHelpers::StartDiagnosisForm(vm->GetConnection(),
                                                                                        vm->OpaqueRef(),
                                                                                        vm->GetName(),
                                                                                        true,
                                                                                        failure,
                                                                                        this->m_mainWindow);
                                             },
                                             this->m_mainWindow);
                break;
            case Operation::ResumeOn:
                action = new VMResumeOnAction(vm, host,
                                              nullptr,  // WarningDialogHAInvalidConfig
                                              [this, vm](VMStartAbstractAction*, const Failure& failure) {
                                                  VMOperationHelpers::StartDiagnosisForm(vm->GetConnection(),
                                                                                         vm->OpaqueRef(),
                                                                                         vm->GetName(),
                                                                                         false,
                                                                                         failure,
                                                                                         this->m_mainWindow);
                                              },
                                              this->m_mainWindow);
                break;
            case Operation::Migrate:
                action = new VMMigrateAction(vm, host, this->m_mainWindow);
                break;
        }

        if (!action)
            continue;

        action->RunAsync(true);
    }
}
