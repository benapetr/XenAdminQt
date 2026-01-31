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

#include "shutdownhostaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../pool.h"
#include "../../vm.h"
#include "../host/hahelpers.h"
#include "../../../xencache.h"
#include "../../xenapi/xenapi_Host.h"
#include "../../xenapi/xenapi_Pool.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../xenapi/xenapi_Task.h"
#include <QDebug>

ShutdownHostAction::ShutdownHostAction(QSharedPointer<Host> host,
                                       std::function<bool(QSharedPointer<Pool>, qint64, qint64)> acceptNtolChanges,
                                       QObject* parent)
    : AsyncOperation(QString("Shutting down %1").arg(host->GetName()),
                     "Waiting...",
                     parent),
      m_host(host),
      m_wasEnabled(false),
      m_acceptNtolChanges(std::move(acceptNtolChanges))
{
    this->m_connection = host->GetConnection();
    this->setAppliesToFromObject(host);
    this->AddApiMethodToRoleCheck("host.disable");
    this->AddApiMethodToRoleCheck("host.enable");
    this->AddApiMethodToRoleCheck("host.shutdown");
    this->AddApiMethodToRoleCheck("vm.clean_shutdown");
    this->AddApiMethodToRoleCheck("vm.hard_shutdown");
    this->AddApiMethodToRoleCheck("pool.ha_compute_hypothetical_max_host_failures_to_tolerate");
    this->AddApiMethodToRoleCheck("pool.set_ha_host_failures_to_tolerate");
    this->AddApiMethodToRoleCheck("task.add_to_other_config");
}

void ShutdownHostAction::run()
{
    if (!this->m_host)
    {
        this->setError("No host specified for shutdown");
        return;
    }

    try
    {
        this->m_wasEnabled = this->m_host->IsEnabled();
        this->SetDescription(QString("Shutting down %1...").arg(this->m_host->GetName()));

        // Step 1: Maybe reduce ntol before operation (HA support)
        this->maybeReduceNtolBeforeOp();

        // Step 2: Shutdown all VMs on the host
        this->shutdownVMs(false); // false = for shutdown (not reboot) 

        // Step 3: Shutdown the host
        QString taskRef = XenAPI::Host::async_shutdown(this->GetSession(), this->m_host->OpaqueRef());

        // WLB task metadata (C# adds this when available)
        QSharedPointer<Pool> pool = this->m_host->GetPool();
        if (pool && pool->IsWLBEnabled() && !pool->WLBUrl().isEmpty())
        {
            QVariantMap otherConfig = this->m_host->GetOtherConfig();
            const QString wlbRecId = otherConfig.value("wlb_optimizing_pool").toString();
            if (!wlbRecId.isEmpty())
            {
                XenAPI::Task::add_to_other_config(this->GetSession(), taskRef, "wlb_advised", wlbRecId);
                XenAPI::Task::add_to_other_config(this->GetSession(), taskRef, "wlb_action", "host_poweroff");
                XenAPI::Task::add_to_other_config(this->GetSession(), taskRef, "wlb_action_obj_ref", this->m_host->OpaqueRef());
                XenAPI::Task::add_to_other_config(this->GetSession(), taskRef, "wlb_action_obj_type", "host");
            }
        }

        this->pollToCompletion(taskRef, 95, 100);

        // Step 4: End connection if this is the coordinator
        // Note: This will be handled by connection management in Qt
        // For now, just log it
        qDebug() << "ShutdownHostAction: Host shutdown successfully";

        this->SetDescription(QString("%1 shut down").arg(this->m_host->GetName()));

        if (this->m_host->IsMaster())
        {
            XenConnection* connection = this->GetConnection();
            if (connection)
                connection->EndConnect(true, false);
        }

    } catch (const std::exception& e)
    {
        QString error = e.what();
        qWarning() << "ShutdownHostAction: Exception shutting down host:" << error;

        // Try to re-enable the host on error
        if (this->m_wasEnabled)
        {
            try
            {
                XenAPI::Host::enable(this->GetSession(), this->m_host->OpaqueRef());
            } catch (const std::exception& e2)
            {
                qWarning() << "ShutdownHostAction: Exception trying to re-enable host after error:" << e2.what();
            }
        }

        this->setError(QString("Failed to shutdown host: %1").arg(error));
    }
}

void ShutdownHostAction::shutdownVMs(bool isForReboot)
{
    /*
     * C# logic:
     * 1. Host.disable() - Disable the host
     * 2. Count resident VMs that are not halted
     * 3. For each VM:
     *    - If allowed_operations contains clean_shutdown: use async_clean_shutdown
     *    - Otherwise: use async_hard_shutdown
     *    - Poll to completion with progress updates
     */

    try
    {
        // Step 1: Disable the host
        QString disableTaskRef = XenAPI::Host::async_disable(this->GetSession(), this->m_host->OpaqueRef());
        this->pollToCompletion(disableTaskRef, 0, 1);

        this->SetPercentComplete(1);

        // Step 2: Get all resident VMs
        const QList<QSharedPointer<VM>> residentVMs = this->m_host->GetResidentVMs();

        // Count VMs that need shutdown (running, non-control-domain)
        QList<QSharedPointer<VM>> toShutdown;

        for (const QSharedPointer<VM>& vm : residentVMs)
        {
            if (!vm || !vm->IsValid())
                continue;

            if (vm->IsRunning() && !vm->IsControlDomain())
                toShutdown.append(vm);
        }

        int n = toShutdown.count();
        if (n == 0)
        {
            return; // No VMs to shutdown
        }

        // Step 3: Shutdown each VM
        int step = 94 / n; // Progress from 1% to 95%

        for (int i = 0; i < n; ++i)
        {
            const QSharedPointer<VM>& vm = toShutdown[i];

            this->SetDescription(QString(isForReboot
                                       ? "Rebooting: Shutting down VM %1 (%2/%3)"
                                       : "Shutting down VM %1 (%2/%3)")
                               .arg(vm->GetName())
                               .arg(i + 1)
                               .arg(n));

            // Check if clean shutdown is allowed
            QStringList allowedOps = vm->GetAllowedOperations();
            bool canCleanShutdown = allowedOps.contains("clean_shutdown");

            QString taskRef;
            if (canCleanShutdown)
            {
                taskRef = XenAPI::VM::async_clean_shutdown(this->GetSession(), vm->OpaqueRef());
            } else
            {
                taskRef = XenAPI::VM::async_hard_shutdown(this->GetSession(), vm->OpaqueRef());
            }

            int progressStart = this->GetPercentComplete();
            this->pollToCompletion(taskRef, progressStart, progressStart + step);
        }

    } catch (const std::exception& e)
    {
        qWarning() << "ShutdownHostAction: Exception shutting down VMs:" << e.what();

        // Try to re-enable the host so user can manually shutdown VMs
        try
        {
            XenAPI::Host::enable(this->GetSession(), this->m_host->OpaqueRef());
        } catch (const std::exception& e2)
        {
            qWarning() << "ShutdownHostAction: Exception trying to re-enable host after VM shutdown error:" << e2.what();
        }

        throw; // Re-throw to caller
    }
}

void ShutdownHostAction::maybeReduceNtolBeforeOp()
{
    /*
     * C# logic:
     * 1. Get pool
     * 2. If HA enabled:
     *    - Get VM HA restart priorities
     *    - Calculate maximum tolerable host failures
     *    - Calculate target ntol = max(0, max - 1)
     *    - If current ntol > target ntol:
     *      - Ask user via acceptNTolChanges callback
     *      - If user accepts: call Pool.set_ha_host_failures_to_tolerate()
     *      - If user declines: throw CancelledException
     *
     * Note: Full HA logic not yet implemented in Qt version.
     * This is a placeholder that can be expanded later.
     */

    QSharedPointer<Pool> pool = this->m_host ? this->m_host->GetPool() : QSharedPointer<Pool>();
    if (!pool || !pool->HAEnabled())
        return;

    XenConnection* connection = this->GetConnection();
    if (!connection || !connection->GetCache())
        return;

    const QVariantMap configuration = HostHaHelpers::BuildHaConfiguration(connection);
    const qint64 maxFailures =
        XenAPI::Pool::ha_compute_hypothetical_max_host_failures_to_tolerate(this->GetSession(), configuration);
    const qint64 currentNtol = pool->HAHostFailuresToTolerate();
    const qint64 targetNtol = qMax<qint64>(0, maxFailures - 1);

    if (currentNtol > targetNtol)
    {
        bool cancel = false;
        if (this->m_acceptNtolChanges)
            cancel = this->m_acceptNtolChanges(pool, currentNtol, targetNtol);

        if (cancel)
        {
            this->setError("Cancelled");
            this->setState(Cancelled);
            return;
        }

        XenAPI::Pool::set_ha_host_failures_to_tolerate(this->GetSession(), pool->OpaqueRef(), targetNtol);
    }
}
