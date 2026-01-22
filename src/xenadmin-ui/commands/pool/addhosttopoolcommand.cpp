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
#include "xenlib/xencache.h"
#include <QMessageBox>

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
        
        // TODO: Check Host::RestrictPooling when ported
        // if (Host::RestrictPooling(host))
        //     return false;
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
        }
        else
        {
            message = tr("The pool '%1' is disconnected. Cannot add hosts.").arg(poolName);
        }
        
        QMessageBox::critical(this->mainWindow(), tr("Pool Disconnected"), message);
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
        
        QMessageBox::critical(this->mainWindow(), tr("Cannot Add to Pool"), errorText);
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
        QMessageBox::critical(this->mainWindow(), tr("Error"), 
                            tr("Cannot find pool coordinator for '%1'.")
                            .arg(this->m_pool->GetName()));
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
    
    // TODO: Get permission for CPU feature levelling when HelpersGUI::GetPermissionForCpuFeatureLevelling is ported
    
    // Select pool in tree
    this->mainWindow()->SelectObjectInTree("pool", this->m_pool->OpaqueRef());
    
    // TODO: Create and run AddHostToPoolAction for each host
    // For now, show placeholder message
    QMessageBox::information(this->mainWindow(), tr("Add to Pool"),
                            tr("Adding %1 host(s) to pool '%2'.\n\n"
                               "TODO: Implement AddHostToPoolAction")
                            .arg(this->hosts_.count())
                            .arg(this->m_pool->GetName()));
}

QMap<QString, QString> AddHostToPoolCommand::checkPoolJoinRules() const
{
    QMap<QString, QString> reasons;
    
    // TODO: Implement PoolJoinRules::CanJoinPool when ported
    // For now, perform basic checks
    
    for (const auto& host : this->hosts_)
    {
        if (!host || !host->GetConnection() || !host->GetConnection()->IsConnected())
        {
            QString hostName = host ? host->GetName() : tr("Unknown Host");
            reasons[hostName] = tr("Host is not connected");
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
    
    QMessageBox::StandardButton reply = QMessageBox::question(this->mainWindow(), tr("Confirm Add to Pool"), message, QMessageBox::Yes | QMessageBox::No);
    return reply == QMessageBox::Yes;
}

bool AddHostToPoolCommand::checkSuppPacksAndWarn()
{
    // TODO: Implement PoolJoinRules::HomogeneousSuppPacksDiffering when ported
    // For now, return true (no warning)
    return true;
}

bool AddHostToPoolCommand::getPermissionForLicensing(QSharedPointer<Host> coordinator)
{
    // TODO: Implement PoolJoinRules::FreeHostPaidCoordinator check when ported
    // For now, return true (no licensing changes needed)
    return true;
}

bool AddHostToPoolCommand::getPermissionForCpuMasking(QSharedPointer<Host> coordinator)
{
    // TODO: Implement PoolJoinRules::CompatibleCPUs check when ported
    // For now, return true (CPUs are compatible)
    return true;
}

bool AddHostToPoolCommand::getPermissionForAdConfig(QSharedPointer<Host> coordinator)
{
    // TODO: Implement PoolJoinRules::CompatibleAdConfig check when ported
    // For now, return true (AD configs are compatible)
    return true;
}
