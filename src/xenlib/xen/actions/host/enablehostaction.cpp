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

#include "enablehostaction.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/session.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/xenapi/xenapi_Host.h"
#include "xenlib/xen/xenapi/xenapi_Pool.h"
#include "xenlib/xen/xenapi/xenapi_VM.h"
#include "xenlib/xencache.h"
#include "../host/hahelpers.h"
#include <QDebug>

EnableHostAction::EnableHostAction(QSharedPointer<Host> host, bool resumeVMs, std::function<bool(QSharedPointer<Pool>, QSharedPointer<Host>, qint64, qint64)> acceptNtolChangesOnEnable, QObject* parent)
    : AsyncOperation(host->GetConnection(), "Enabling host", QString("Exiting maintenance mode for '%1'").arg(host ? host->GetName() : ""), parent),
      m_resumeVMs(resumeVMs), m_acceptNtolChangesOnEnable(std::move(acceptNtolChangesOnEnable))
{
    this->m_host = host;
    this->AddApiMethodToRoleCheck("host.remove_from_other_config");
    this->AddApiMethodToRoleCheck("host.enable");
    this->AddApiMethodToRoleCheck("vm.pool_migrate");
    this->AddApiMethodToRoleCheck("vm.start_on");
    this->AddApiMethodToRoleCheck("vm.resume_on");
    this->AddApiMethodToRoleCheck("pool.ha_compute_hypothetical_max_host_failures_to_tolerate");
    this->AddApiMethodToRoleCheck("pool.set_ha_host_failures_to_tolerate");
}

void EnableHostAction::run()
{
    try
    {
        this->SetDescription(QString("Exiting maintenance mode for '%1'").arg(this->m_host->GetName()));

        // Enable host (0-10% or 0-100% depending on whether we resume VMs)
        this->enable(0, this->m_resumeVMs ? 10 : 100, true);

        if (this->m_resumeVMs)
        {
            // Get evacuated VMs from host's other_config
            // C# uses extension methods: GetMigratedEvacuatedVMs, GetHaltedEvacuatedVMs, GetSuspendedEvacuatedVMs
            QVariantMap otherConfig = this->m_host->GetOtherConfig();

            QStringList migratedVMRefs = otherConfig.value("MAINTENANCE_MODE_MIGRATED_VMS", "").toString().split(',', Qt::SkipEmptyParts);
            QStringList haltedVMRefs = otherConfig.value("MAINTENANCE_MODE_HALTED_VMS", "").toString().split(',', Qt::SkipEmptyParts);
            QStringList suspendedVMRefs = otherConfig.value("MAINTENANCE_MODE_SUSPENDED_VMS", "").toString().split(',', Qt::SkipEmptyParts);

            // Clear evacuated VM lists from host's other_config
            if (!migratedVMRefs.isEmpty())
            {
                XenAPI::Host::remove_from_other_config(this->GetSession(), this->m_host->OpaqueRef(), "MAINTENANCE_MODE_MIGRATED_VMS");
            }
            if (!haltedVMRefs.isEmpty())
            {
                XenAPI::Host::remove_from_other_config(this->GetSession(), this->m_host->OpaqueRef(), "MAINTENANCE_MODE_HALTED_VMS");
            }
            if (!suspendedVMRefs.isEmpty())
            {
                XenAPI::Host::remove_from_other_config(this->GetSession(), this->m_host->OpaqueRef(), "MAINTENANCE_MODE_SUSPENDED_VMS");
            }

            int totalVMs = migratedVMRefs.size() + haltedVMRefs.size() + suspendedVMRefs.size();

            if (totalVMs > 0)
            {
                int start = 10;
                int each = 90 / totalVMs;

                // Migrate VMs back to this host
                for (const QString& vmRef : migratedVMRefs)
                {
                    QSharedPointer<VM> vm = this->GetConnection()->GetCache()->ResolveObject<VM>(vmRef);
                    if (!vm || !vm->IsValid())
                        continue;
                    QString vmName = vm->GetName();

                    qDebug() << "EnableHostAction: Migrating VM" << vmName << "back to host";

                    // Live migration back to this host
                    QVariantMap options;
                    options["live"] = true;

                    QString taskRef = XenAPI::VM::async_pool_migrate(this->GetSession(), vm->OpaqueRef(),
                                                                     this->m_host->OpaqueRef(),
                                                                     options);
                    this->pollToCompletion(taskRef, start, start + each);
                    start += each;
                }

                // Start halted VMs on this host
                for (const QString& vmRef : haltedVMRefs)
                {
                    QSharedPointer<VM> vm = this->GetConnection()->GetCache()->ResolveObject<VM>(vmRef);
                    if (!vm || !vm->IsValid())
                        continue;
                    QString vmName = vm->GetName();

                    qDebug() << "EnableHostAction: Starting VM" << vmName << "on host";

                    QString taskRef = XenAPI::VM::async_start_on(this->GetSession(), vm->OpaqueRef(),
                                                                 this->m_host->OpaqueRef(),
                                                                 false, false);
                    this->pollToCompletion(taskRef, start, start + each);
                    start += each;
                }

                // Resume suspended VMs on this host
                for (const QString& vmRef : suspendedVMRefs)
                {
                    QSharedPointer<VM> vm = this->GetConnection()->GetCache()->ResolveObject<VM>(vmRef);
                    if (!vm || !vm->IsValid())
                        continue;
                    QString vmName = vm->GetName();

                    qDebug() << "EnableHostAction: Resuming VM" << vmName << "on host";

                    QString taskRef = XenAPI::VM::async_resume_on(this->GetSession(), vm->OpaqueRef(),
                                                                  this->m_host->OpaqueRef(),
                                                                  false, false);
                    this->pollToCompletion(taskRef, start, start + each);
                    start += each;
                }
            }
        }

        this->SetDescription(QString("Exited maintenance mode for '%1'").arg(this->m_host->GetName()));

    } catch (const std::exception& e)
    {
        this->setError(QString("Failed to enable host: %1").arg(e.what()));
    }
}

void EnableHostAction::enable(int start, int finish, bool queryNtolIncrease)
{
    // Remove MAINTENANCE_MODE flag from other_config
    XenAPI::Host::remove_from_other_config(this->GetSession(), this->m_host->OpaqueRef(), "MAINTENANCE_MODE");

    // Enable the host
    QString taskRef = XenAPI::Host::async_enable(this->GetSession(), this->m_host->OpaqueRef());
    this->pollToCompletion(taskRef, start, finish);

    if (queryNtolIncrease)
    {
        QSharedPointer<Pool> pool = this->m_host ? this->m_host->GetPool() : QSharedPointer<Pool>();
        if (pool && pool->HAEnabled() && this->m_acceptNtolChangesOnEnable)
        {
            const QVariantMap configuration = HostHaHelpers::BuildHaConfiguration(this->GetConnection());
            const qint64 maxFailures =
                XenAPI::Pool::ha_compute_hypothetical_max_host_failures_to_tolerate(this->GetSession(), configuration);
            const qint64 currentNtol = pool->HAHostFailuresToTolerate();

            if (currentNtol < maxFailures &&
                this->m_acceptNtolChangesOnEnable(pool, this->m_host, currentNtol, maxFailures))
            {
                XenAPI::Pool::set_ha_host_failures_to_tolerate(this->GetSession(), pool->OpaqueRef(), maxFailures);
            }
        }
    }
}
