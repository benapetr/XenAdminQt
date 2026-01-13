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
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_Host.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../../xencache.h"
#include <QDebug>

EnableHostAction::EnableHostAction(QSharedPointer<Host> host, bool resumeVMs, QObject* parent)
    : AsyncOperation(host->GetConnection(),
                     "Enabling host",
                     QString("Exiting maintenance mode for '%1'").arg(host ? host->GetName() : ""),
                     parent),
      m_resumeVMs(resumeVMs)
{
    this->m_host = host;
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
                    QVariantMap vmData = this->GetConnection()->GetCache()->ResolveObjectData("vm", vmRef);
                    QString vmName = vmData.value("name_label").toString();

                    qDebug() << "EnableHostAction: Migrating VM" << vmName << "back to host";

                    // Live migration back to this host
                    QVariantMap options;
                    options["live"] = true;

                    QString taskRef = XenAPI::VM::async_pool_migrate(this->GetSession(), vmRef,
                                                                     this->m_host->OpaqueRef(),
                                                                     options);
                    this->pollToCompletion(taskRef, start, start + each);
                    start += each;
                }

                // Start halted VMs on this host
                for (const QString& vmRef : haltedVMRefs)
                {
                    QVariantMap vmData = this->GetConnection()->GetCache()->ResolveObjectData("vm", vmRef);
                    QString vmName = vmData.value("name_label").toString();

                    qDebug() << "EnableHostAction: Starting VM" << vmName << "on host";

                    QString taskRef = XenAPI::VM::async_start_on(this->GetSession(), vmRef,
                                                                 this->m_host->OpaqueRef(),
                                                                 false, false);
                    this->pollToCompletion(taskRef, start, start + each);
                    start += each;
                }

                // Resume suspended VMs on this host
                for (const QString& vmRef : suspendedVMRefs)
                {
                    QVariantMap vmData = this->GetConnection()->GetCache()->ResolveObjectData("vm", vmRef);
                    QString vmName = vmData.value("name_label").toString();

                    qDebug() << "EnableHostAction: Resuming VM" << vmName << "on host";

                    QString taskRef = XenAPI::VM::async_resume_on(this->GetSession(), vmRef,
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

    // TODO: HA ntol increase query
    // In C#, this checks if HA is enabled, calculates max ntol, and asks user
    // if they want to increase ntol back up after enabling the host.
    // For now, we'll skip this (HA not fully implemented yet)
    if (queryNtolIncrease)
    {
        qDebug() << "EnableHostAction: TODO - HA ntol increase query not yet implemented";
    }
}
