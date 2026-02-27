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

#include "addhosttopoolcommand.h"
#include "../../mainwindow.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/pooljoinrules.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/actions/pool/addhosttopoolaction.h"
#include "xenlib/xencache.h"
#include "../../dialogs/warningdialogs/warningdialog.h"
#include <QMessageBox>

namespace
{
QString buildHostNameList(const QList<QSharedPointer<Host>>& hosts)
{
    QStringList names;
    for (const QSharedPointer<Host>& host : hosts)
    {
        if (host && host->IsValid())
            names.append(host->GetName());
    }
    return names.join("\n");
}
}

AddHostToPoolCommand::AddHostToPoolCommand(MainWindow* mainWindow, const QList<QSharedPointer<Host>>& hosts, QSharedPointer<Pool> pool, bool confirm)
    : Command(mainWindow), hosts_(hosts), m_pool(pool), confirm_(confirm)
{
}

bool AddHostToPoolCommand::CanRun() const
{
    if (this->hosts_.isEmpty() || !this->m_pool)
        return false;
    
    // Only allowed to add standalone hosts
    for (const auto& host : this->hosts_)
    {
        if (!host)
            return false;
            
        // Check if host is already in a pool
        QSharedPointer<Pool> hostPool = host->GetCache()->GetPool();
        if (hostPool)
            return false;
        
        if (host->RestrictPooling())
            return false;
    }
    
    return true;
}

void AddHostToPoolCommand::Run()
{
    if (!this->m_pool || !this->m_pool->GetConnection() || !this->m_pool->GetConnection()->IsConnected())
    {
        QString poolName = this->m_pool->GetName();
        QString message;
        
        if (this->hosts_.count() == 1)
        {
            QString hostName = this->hosts_[0]->GetName();
            message = tr("The pool '%1' is disconnected. Cannot add host '%2'.").arg(poolName).arg(hostName);
        } else
        {
            message = tr("The pool '%1' is disconnected. Cannot add hosts.").arg(poolName);
        }
        
        QMessageBox::critical(MainWindow::instance(), tr("Pool Disconnected"), message);
        return;
    }
    
    // Check pool join rules
    QMap<QString, QString> errors = this->checkPoolJoinRules();
    if (!errors.isEmpty())
    {
        QString errorText = tr("Cannot add the following hosts to pool '%1':\n\n").arg(this->m_pool->GetName());
        
        for (auto it = errors.constBegin(); it != errors.constEnd(); ++it)
        {
            errorText += tr("• %1: %2\n").arg(it.key()).arg(it.value());
        }
        
        QMessageBox::critical(MainWindow::instance(), tr("Cannot Add to Pool"), errorText);
        return;
    }
    
    // Show confirmation dialog if requested
    if (this->confirm_ && !this->showConfirmationDialog())
        return;
    
    // Check supplemental packs and warn
    if (!this->checkSuppPacksAndWarn())
        return;
    
    // Get pool coordinator
    QSharedPointer<Host> coordinator = this->m_pool->GetMasterHost();
    if (!coordinator)
    {
        QMessageBox::critical(MainWindow::instance(), tr("Error"), tr("Cannot find pool coordinator for '%1'.").arg(this->m_pool->GetName()));
        return;
    }
    
    // Get permission for licensing changes
    if (!this->getPermissionForLicensing(coordinator))
        return;
    
    // Get permission for CPU masking
    if (!this->getPermissionForCpuMasking(coordinator))
        return;
    
    // Get permission for AD configuration changes
    if (!this->getPermissionForAdConfig(coordinator))
        return;
    
    if (!this->getPermissionForCpuFeatureLevelling(this->m_pool))
        return;
    
    // Select pool in tree
    MainWindow::instance()->SelectObjectInTree("pool", this->m_pool->OpaqueRef());

    QList<AsyncOperation*> actions;
    actions.reserve(this->hosts_.size());

    XenConnection* poolConnection = this->m_pool->GetConnection();
    for (const QSharedPointer<Host>& host : this->hosts_)
    {
        if (!host || !host->GetConnection())
            continue;

        actions.append(new AddHostToPoolAction(poolConnection, host->GetConnection(), host, true, nullptr));
    }

    if (actions.isEmpty())
        return;

    this->RunMultipleActions(actions,
                             tr("Adding Servers to Pool"),
                             tr("Adding Servers to Pool"),
                             tr("Added"),
                             false);
}

QMap<QString, QString> AddHostToPoolCommand::checkPoolJoinRules() const
{
    QMap<QString, QString> reasons;
    
    XenConnection* poolConnection = this->m_pool ? this->m_pool->GetConnection() : nullptr;
    const int poolSizeIncrement = this->hosts_.size();

    for (const auto& host : this->hosts_)
    {
        if (!host || !host->GetConnection() || !host->GetConnection()->IsConnected())
        {
            QString hostName = host ? host->GetName() : tr("Unknown Host");
            reasons[hostName] = tr("Host is not connected");
            continue;
        }

        PoolJoinRules::Reason reason = PoolJoinRules::CanJoinPool(host->GetConnection(),
                                                                  poolConnection,
                                                                  true,
                                                                  true,
                                                                  poolSizeIncrement);
        if (reason != PoolJoinRules::Reason::Allowed)
        {
            QString hostName = host->GetName();
            QString message = PoolJoinRules::ReasonMessage(reason);
            if (message.isEmpty())
                message = tr("Host cannot join the pool");
            reasons[hostName] = message;
        }
    }
    
    return reasons;
}

bool AddHostToPoolCommand::showConfirmationDialog()
{
    QString message;
    
    if (this->hosts_.count() > 1)
    {
        message = tr("Are you sure you want to add these %1 hosts to pool '%2'?")
                    .arg(this->hosts_.count())
                    .arg(this->m_pool->GetName());
    } else
    {
        message = tr("Are you sure you want to add host '%1' to pool '%2'?")
                    .arg(this->hosts_[0]->GetName())
                    .arg(this->m_pool->GetName());
    }
    
    QMessageBox::StandardButton reply = QMessageBox::question(MainWindow::instance(), tr("Confirm Add to Pool"), message, QMessageBox::Yes | QMessageBox::No);
    return reply == QMessageBox::Yes;
}

bool AddHostToPoolCommand::checkSuppPacksAndWarn()
{
    const QList<QString> badSuppPacks = PoolJoinRules::HomogeneousSuppPacksDiffering(
        this->hosts_, qSharedPointerCast<XenObject>(this->m_pool));
    if (badSuppPacks.isEmpty())
        return true;

    const QString message = tr("Some supplemental packs differ across hosts:\n%1")
        .arg(badSuppPacks.join("\n"));
    WarningDialog dialog(message, tr("Supplemental Packs"),
                         { {tr("Proceed"), WarningDialog::Result::Yes},
                           {tr("Cancel"), WarningDialog::Result::Cancel} },
                         MainWindow::instance());
    dialog.exec();
    return dialog.GetResult() == WarningDialog::Result::Yes;
}

bool AddHostToPoolCommand::getPermissionForLicensing(QSharedPointer<Host> coordinator)
{
    QList<QSharedPointer<Host>> affectedHosts;
    for (const QSharedPointer<Host>& host : this->hosts_)
    {
        if (PoolJoinRules::FreeHostPaidCoordinator(host, coordinator, false))
            affectedHosts.append(host);
    }

    if (affectedHosts.isEmpty())
        return true;

    const QString message = tr("The following hosts will be relicensed to match the coordinator:\n%1")
        .arg(buildHostNameList(affectedHosts));
    return WarningDialog::ShowYesNo(message, tr("License Warning"), MainWindow::instance()) == WarningDialog::Result::Yes;
}

bool AddHostToPoolCommand::getPermissionForCpuMasking(QSharedPointer<Host> coordinator)
{
    QList<QSharedPointer<Host>> affectedHosts;
    for (const QSharedPointer<Host>& host : this->hosts_)
    {
        if (!PoolJoinRules::CompatibleCPUs(host, coordinator))
            affectedHosts.append(host);
    }

    if (affectedHosts.isEmpty())
        return true;

    const QString message = tr("CPU masking will be required for:\n%1")
        .arg(buildHostNameList(affectedHosts));
    return WarningDialog::ShowYesNo(message, tr("CPU Masking"), MainWindow::instance()) == WarningDialog::Result::Yes;
}

bool AddHostToPoolCommand::getPermissionForAdConfig(QSharedPointer<Host> coordinator)
{
    QList<QSharedPointer<Host>> affectedHosts;
    for (const QSharedPointer<Host>& host : this->hosts_)
    {
        if (!PoolJoinRules::CompatibleAdConfig(host, coordinator, false))
            affectedHosts.append(host);
    }

    if (affectedHosts.isEmpty())
        return true;

    const QString message = tr("Active Directory configuration will be updated for:\n%1")
        .arg(buildHostNameList(affectedHosts));
    return WarningDialog::ShowYesNo(message, tr("Active Directory"), MainWindow::instance()) == WarningDialog::Result::Yes;
}

bool AddHostToPoolCommand::getPermissionForCpuFeatureLevelling(QSharedPointer<Pool> coordinatorPool)
{
    if (!coordinatorPool)
        return true;

    QList<QSharedPointer<Host>> hostsWithFewerFeatures;
    QList<QSharedPointer<Host>> hostsWithMoreFeatures;

    for (const QSharedPointer<Host>& host : this->hosts_)
    {
        if (!host || !host->IsValid())
            continue;

        if (PoolJoinRules::HostHasFewerFeatures(host, coordinatorPool))
            hostsWithFewerFeatures.append(host);
        if (PoolJoinRules::HostHasMoreFeatures(host, coordinatorPool))
            hostsWithMoreFeatures.append(host);
    }

    if (!hostsWithFewerFeatures.isEmpty() && !hostsWithMoreFeatures.isEmpty())
    {
        const QString message = tr("CPU feature levelling will down-level both pool and host CPUs for:\n%1")
            .arg(buildHostNameList(hostsWithFewerFeatures + hostsWithMoreFeatures));
        return WarningDialog::ShowYesNo(message, tr("CPU Feature Levelling"), MainWindow::instance()) == WarningDialog::Result::Yes;
    }

    if (!hostsWithFewerFeatures.isEmpty())
    {
        const QString message = tr("CPU feature levelling will down-level the pool CPUs for:\n%1")
            .arg(buildHostNameList(hostsWithFewerFeatures));
        return WarningDialog::ShowYesNo(message, tr("CPU Feature Levelling"), MainWindow::instance()) == WarningDialog::Result::Yes;
    }

    if (!hostsWithMoreFeatures.isEmpty())
    {
        const QString message = tr("CPU feature levelling will down-level host CPUs for:\n%1")
            .arg(buildHostNameList(hostsWithMoreFeatures));
        return WarningDialog::ShowYesNo(message, tr("CPU Feature Levelling"), MainWindow::instance()) == WarningDialog::Result::Yes;
    }

    return true;
}
