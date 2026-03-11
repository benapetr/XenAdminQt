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

#include "vmmigrateaction.h"
#include "xen/network/connection.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../xenapi/xenapi_Host.h"
#include "xencache.h"
#include "../../failure.h"
#include "xen/host.h"
#include "xen/pif.h"
#include "xen/pool.h"
#include "xen/vm.h"
#include <stdexcept>

VMMigrateAction::VMMigrateAction(QSharedPointer<VM> vm, QSharedPointer<Host> host, QObject* parent)
    : VMMigrateAction(vm, host, QString(), parent)
{
}

VMMigrateAction::VMMigrateAction(QSharedPointer<VM> vm, QSharedPointer<Host> host, const QString& migrationNetworkRef, QObject* parent)
    : AsyncOperation(QString("Migrating VM"), QString("Migrating VM to another host"), parent),
      m_vm(vm),
      m_host(host),
      m_migrationNetworkRef(migrationNetworkRef)
{
    if (!vm)
        throw std::invalid_argument("VM cannot be null");

    this->m_connection = vm->GetConnection();

    this->AddApiMethodToRoleCheck("VM.async_pool_migrate");
    this->AddApiMethodToRoleCheck("Host.migrate_receive");
    this->AddApiMethodToRoleCheck("VM.async_migrate_send");
}

void VMMigrateAction::run()
{
    try
    {
        this->SetPercentComplete(0);
        this->SetDescription("Preparing migration...");

        QString vmName = this->m_vm->GetName();
        QString hostName = this->m_host->GetName();

        // Check if VM is resident on a host
        QString residentOnRef = this->m_vm->GetResidentOnRef();
        QString sourceHostName;

        if (!residentOnRef.isEmpty() && residentOnRef != XENOBJECT_NULL)
        {
            QVariantMap residentHostData = GetConnection()->GetCache()->ResolveObjectData(XenObjectType::Host, residentOnRef);
            sourceHostName = residentHostData.value("name_label").toString();

            this->SetTitle(QString("Migrating %1 from %2 to %3").arg(vmName).arg(sourceHostName).arg(hostName));
        } else
        {
            this->SetTitle(QString("Migrating %1 to %2").arg(vmName).arg(hostName));
        }

        this->SetPercentComplete(10);
        this->SetDescription(QString("Migrating %1 to %2...").arg(vmName).arg(hostName));

        QString taskRef;
        const QString migrationNetworkRef = this->resolveMigrationNetworkRef();
        const bool useTransferNetwork = !migrationNetworkRef.isEmpty() && hostHasUsableMigrationPif(migrationNetworkRef);

        if (useTransferNetwork)
        {
            QVariantMap receiveData = XenAPI::Host::migrate_receive(GetSession(),
                                                                    this->m_host->OpaqueRef(),
                                                                    migrationNetworkRef,
                                                                    QVariantMap());

            taskRef = XenAPI::VM::async_migrate_send(GetSession(),
                                                     this->m_vm->OpaqueRef(),
                                                     receiveData,
                                                     true,
                                                     QVariantMap(),
                                                     QVariantMap(),
                                                     QVariantMap());
        } else
        {
            // Start migration with live migration enabled
            QVariantMap options;
            options["live"] = "true";
            taskRef = XenAPI::VM::async_pool_migrate(GetSession(), this->m_vm->OpaqueRef(), this->m_host->OpaqueRef(), options);
        }

        // Poll the task to completion
        this->pollToCompletion(taskRef, 10, 100);

        this->SetDescription(QString("VM migrated successfully to %1").arg(hostName));

    } catch (const Failure& failure)
    {
        QStringList params = failure.errorDescription();
        if (params.size() >= 5 && params[0] == "VM_MIGRATE_FAILED" && params[4].contains("VDI_MISSING"))
        {
            this->setError("Migration failed: Please eject any mounted ISOs (especially XenServer Tools) and try again");
        } else
        {
            this->setError(QString("Failed to migrate VM: %1").arg(failure.message()));
        }
    } catch (const std::exception& e)
    {
        QString errorMsg = QString::fromUtf8(e.what());

        // Check for specific error about VDI_MISSING (tools ISO issue)
        if (errorMsg.contains("VM_MIGRATE_FAILED") && errorMsg.contains("VDI_MISSING"))
        {
            this->setError("Migration failed: Please eject any mounted ISOs (especially XenServer Tools) and try again");
        } else
        {
            this->setError(QString("Failed to migrate VM: %1").arg(errorMsg));
        }
    }
}

QString VMMigrateAction::resolveMigrationNetworkRef() const
{
    if (!this->m_migrationNetworkRef.isEmpty())
        return this->m_migrationNetworkRef;

    if (!this->m_host)
        return QString();

    QSharedPointer<Pool> pool = this->m_host->GetPool();
    if (!pool || !pool->IsValid())
        return QString();

    return pool->GetOtherConfig().value("xo:migrationNetwork").toString();
}

bool VMMigrateAction::hostHasUsableMigrationPif(const QString& networkRef) const
{
    if (networkRef.isEmpty() || !m_host || !GetConnection() || !GetConnection()->GetCache())
        return false;

    XenCache* cache = GetConnection()->GetCache();
    QList<QSharedPointer<PIF>> pifs = cache->GetAll<PIF>(XenObjectType::PIF);

    for (const QSharedPointer<PIF>& pif : pifs)
    {
        if (!pif || !pif->IsValid())
            continue;
        if (pif->GetHostRef() != m_host->OpaqueRef())
            continue;
        if (pif->GetNetworkRef() != networkRef)
            continue;
        if (pif->IP().isEmpty())
            continue;

        return true;
    }

    return false;
}
