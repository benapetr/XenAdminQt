/*
 * Copyright (c) 2026, Petr Bena <petr@bena.rocks>
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

#include "vmssactions.h"
#include "../../vmss.h"
#include "../../vm.h"
#include "../../host.h"
#include "../../pool.h"
#include "../../network/connection.h"
#include "../../xenapi/xenapi_VMSS.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../xenapi/xenapi_Host.h"
#include "../../../xencache.h"
#include "../../../utils/misc.h"
#include <QDebug>

namespace
{
    QSharedPointer<Pool> poolOf(XenConnection* connection)
    {
        return connection && connection->GetCache() ? connection->GetCache()->GetPoolOfOne() : QSharedPointer<Pool>();
    }
}

// CreateVMPolicyAction

CreateVMPolicyAction::CreateVMPolicyAction(XenConnection* connection, const QVariantMap& record, const QStringList& vmRefs, bool runNow, QObject* parent)
    : AsyncOperation(connection, tr("Create policy"), QString(), parent), m_record(record), m_vmRefs(vmRefs), m_runNow(runNow)
{
    this->SetPool(poolOf(connection));
    this->AddApiMethodToRoleCheck("VMSS.create");
    this->AddApiMethodToRoleCheck("VM.set_snapshot_schedule");
    this->AddApiMethodToRoleCheck("VMSS.snapshot_now");
}

void CreateVMPolicyAction::run()
{
    const QString name = this->m_record.value("name_label").toString();
    try
    {
        this->SetDescription(tr("Creating snapshot schedule '%1'...").arg(name));
        const QString vmssRef = XenAPI::VMSS::create(this->GetSession(), this->m_record);
        this->GetConnection()->WaitForCacheObject("vmss", vmssRef);

        for (const QString& vmRef : this->m_vmRefs)
            XenAPI::VM::set_snapshot_schedule(this->GetSession(), vmRef, vmssRef);

        this->SetPercentComplete(60);
        this->SetDescription(tr("Snapshot schedule '%1' created.").arg(name));

        if (this->m_runNow)
            XenAPI::VMSS::snapshot_now(this->GetSession(), vmssRef);

        this->SetResult(vmssRef);
        this->SetPercentComplete(100);
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}

// DestroyPolicyAction

DestroyPolicyAction::DestroyPolicyAction(XenConnection* connection, const QList<QSharedPointer<VMSS>>& policies, QObject* parent)
    : AsyncOperation(connection, tr("Delete policies"), QString(), parent)
{
    for (const QSharedPointer<VMSS>& policy : policies)
    {
        if (policy)
            this->m_items.append({policy->OpaqueRef(), policy->GetName(), policy->GetVMRefs()});
    }
    this->SetPool(poolOf(connection));
    this->AddApiMethodToRoleCheck("VM.set_snapshot_schedule");
    this->AddApiMethodToRoleCheck("VMSS.destroy");
}

void DestroyPolicyAction::run()
{
    try
    {
        for (const Item& item : this->m_items)
        {
            this->SetDescription(tr("Deleting snapshot schedule '%1'").arg(item.name));
            for (const QString& vmRef : item.vmRefs)
                XenAPI::VM::set_snapshot_schedule(this->GetSession(), vmRef, QString());

            try
            {
                XenAPI::VMSS::destroy(this->GetSession(), item.ref);
            } catch (const std::exception& e)
            {
                // C#: someone else deleted it meanwhile
                if (!QString::fromUtf8(e.what()).startsWith("Object has been deleted"))
                    throw;
            }
        }
        this->SetDescription(tr("Snapshot schedules deleted."));
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}

// AssignVMsToPolicyAction

AssignVMsToPolicyAction::AssignVMsToPolicyAction(QSharedPointer<VMSS> policy, const QStringList& vmRefs, bool suppressHistory, QObject* parent)
    : AsyncOperation(policy->GetConnection(), tr("Assign VM to snapshot schedule..."), QString(), suppressHistory, parent),
      m_policyRef(policy->OpaqueRef()), m_currentVmRefs(policy->GetVMRefs()), m_vmRefs(vmRefs)
{
    this->SetPool(poolOf(policy->GetConnection()));
    this->AddApiMethodToRoleCheck("VM.set_snapshot_schedule");
}

void AssignVMsToPolicyAction::run()
{
    try
    {
        this->SetDescription(tr("Assigning to a snapshot schedule..."));
        for (const QString& vmRef : this->m_currentVmRefs)
        {
            if (!this->m_vmRefs.contains(vmRef))
                XenAPI::VM::set_snapshot_schedule(this->GetSession(), vmRef, QString());
        }

        XenCache* cache = this->GetConnection()->GetCache();
        for (const QString& vmRef : this->m_vmRefs)
        {
            QSharedPointer<VM> vm = cache->ResolveObject<VM>(XenObjectType::VM, vmRef);
            if (vm && vm->SnapshotScheduleRef() != this->m_policyRef)
                XenAPI::VM::set_snapshot_schedule(this->GetSession(), vmRef, this->m_policyRef);
        }
        this->SetDescription(tr("Snapshot schedule assigned."));
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}

// RemoveVMsFromPolicyAction

RemoveVMsFromPolicyAction::RemoveVMsFromPolicyAction(QSharedPointer<VMSS> policy, const QStringList& vmRefs, QObject* parent)
    : AsyncOperation(policy->GetConnection(), QString(), QString(), parent), m_vmRefs(vmRefs)
{
    QString title;
    if (vmRefs.size() == 1)
    {
        QSharedPointer<VM> vm = policy->GetConnection()->GetCache()->ResolveObject<VM>(XenObjectType::VM, vmRefs.first());
        title = tr("Remove VM '%1' from snapshot schedule '%2'").arg(vm ? vm->GetName() : vmRefs.first(), policy->GetName());
    } else
    {
        // C#'s REMOVE_VMS_FROM_VMSS refers to {1} but gets one argument, which throws a
        // FormatException there; the schedule name is what's meant
        title = tr("Remove VMs from snapshot schedule '%1'").arg(policy->GetName());
    }
    this->SetTitle(title);
    this->SetPool(poolOf(policy->GetConnection()));
    this->AddApiMethodToRoleCheck("VM.set_snapshot_schedule");
}

void RemoveVMsFromPolicyAction::run()
{
    try
    {
        this->SetDescription(tr("Removing from snapshot schedule"));
        for (const QString& vmRef : this->m_vmRefs)
            XenAPI::VM::set_snapshot_schedule(this->GetSession(), vmRef, QString());
        this->SetDescription(tr("Removed from snapshot schedule"));
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}

// ChangePolicyEnabledAction

ChangePolicyEnabledAction::ChangePolicyEnabledAction(QSharedPointer<VMSS> policy, QObject* parent)
    : AsyncOperation(policy->GetConnection(), tr("Change policy '%1' status").arg(policy->GetName()), QString(), parent),
      m_policyRef(policy->OpaqueRef()), m_name(policy->GetName()), m_enable(!policy->Enabled())
{
    this->SetPool(poolOf(policy->GetConnection()));
    this->AddApiMethodToRoleCheck("VMSS.set_enabled");
}

void ChangePolicyEnabledAction::run()
{
    try
    {
        this->SetDescription(this->m_enable ? tr("Enabling snapshot schedule '%1'...").arg(this->m_name)
                                            : tr("Disabling snapshot schedule '%1'...").arg(this->m_name));
        XenAPI::VMSS::set_enabled(this->GetSession(), this->m_policyRef, this->m_enable);
        this->SetDescription(this->m_enable ? tr("Snapshot schedule '%1' enabled.").arg(this->m_name)
                                            : tr("Snapshot schedule '%1' disabled.").arg(this->m_name));
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}

// RunPolicyNowAction

RunPolicyNowAction::RunPolicyNowAction(QSharedPointer<VMSS> policy, QObject* parent)
    : AsyncOperation(policy->GetConnection(), tr("Running policy '%1'").arg(policy->GetName()), QString(), parent),
      m_policyRef(policy->OpaqueRef()), m_name(policy->GetName())
{
    this->SetPool(poolOf(policy->GetConnection()));
    this->AddApiMethodToRoleCheck("VMSS.snapshot_now");
}

void RunPolicyNowAction::run()
{
    try
    {
        this->SetResult(XenAPI::VMSS::snapshot_now(this->GetSession(), this->m_policyRef));
        this->SetDescription(tr("Policy started"));
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}

// GetServerLocalTimeAction

GetServerLocalTimeAction::GetServerLocalTimeAction(QSharedPointer<Host> host, QObject* parent)
    : AsyncOperation(host->GetConnection(), QString(), QString(), true, parent), m_hostRef(host->OpaqueRef())
{
    this->AddApiMethodToRoleCheck("host.get_server_localtime");
}

QDateTime GetServerLocalTimeAction::ClientLocalNow()
{
    const QDateTime now = QDateTime::currentDateTime();
    return QDateTime(now.date(), now.time(), Qt::UTC);
}

void GetServerLocalTimeAction::run()
{
    try
    {
        const QVariant value = XenAPI::Host::get_server_localtime(this->GetSession(), this->m_hostRef);
        const QDateTime parsed = Misc::ParseXenDateTime(value.toString());
        if (!parsed.isValid())
        {
            qWarning() << "GetServerLocalTimeAction: can't parse server local time" << value;
            return;
        }

        // The value is the server's wall clock; keep its fields as they are
        this->m_serverLocalTime = QDateTime(parsed.date(), parsed.time(), Qt::UTC);

        // C#: DateTime.Now - Connection.ServerTimeOffset - serverLocalTime
        const qint64 offset = this->GetConnection() ? this->GetConnection()->GetServerTimeOffsetSeconds() : 0;
        this->m_diffSeconds = this->m_serverLocalTime.secsTo(ClientLocalNow()) - offset;
        this->m_hasTime = true;
    } catch (const std::exception& e)
    {
        // C# only logs this; the callers show "Pool coordinator unreachable"
        qWarning() << "GetServerLocalTimeAction: An error occurred while obtaining the server local time:" << e.what();
    }
}
