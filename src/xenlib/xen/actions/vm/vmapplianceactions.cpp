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

#include "vmapplianceactions.h"
#include "../../vmappliance.h"
#include "../../vm.h"
#include "../../pool.h"
#include "../../network/connection.h"
#include "../../xenapi/vm_appliance.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../../xencache.h"

namespace
{
    QSharedPointer<Pool> poolOf(XenConnection* connection)
    {
        return connection && connection->GetCache() ? connection->GetCache()->GetPoolOfOne() : QSharedPointer<Pool>();
    }
}

CreateVMApplianceAction::CreateVMApplianceAction(XenConnection* connection, const QVariantMap& record, const QStringList& vmRefs, QObject* parent)
    : AsyncOperation(connection, tr("Create vApp"), QString(), parent), m_record(record), m_vmRefs(vmRefs)
{
    this->SetPool(poolOf(connection));
    this->AddApiMethodToRoleCheck("VM_appliance.create");
    this->AddApiMethodToRoleCheck("VM.set_appliance");
}

void CreateVMApplianceAction::run()
{
    const QString name = this->m_record.value("name_label").toString();
    try
    {
        this->SetDescription(tr("Creating vApp '%1'...").arg(name));
        const QString applianceRef = XenAPI::VM_appliance::create(this->GetSession(), this->m_record);
        this->GetConnection()->WaitForCacheObject("vm_appliance", applianceRef);

        for (const QString& vmRef : this->m_vmRefs)
            XenAPI::VM::set_appliance(this->GetSession(), vmRef, applianceRef);

        this->SetResult(applianceRef);
        this->SetPercentComplete(100);
        this->SetDescription(tr("vApp '%1' created.").arg(name));
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}

DestroyVMApplianceAction::DestroyVMApplianceAction(XenConnection* connection, const QList<QSharedPointer<VMAppliance>>& appliances, QObject* parent)
    : AsyncOperation(connection, tr("Delete vApps"), QString(), parent)
{
    for (const QSharedPointer<VMAppliance>& appliance : appliances)
    {
        if (appliance)
            this->m_items.append({appliance->OpaqueRef(), appliance->GetName(), appliance->VMRefs()});
    }
    this->SetPool(poolOf(connection));
    this->AddApiMethodToRoleCheck("VM.set_appliance");
    this->AddApiMethodToRoleCheck("VM_appliance.destroy");
}

void DestroyVMApplianceAction::run()
{
    try
    {
        for (const Item& item : this->m_items)
        {
            this->SetDescription(tr("Deleting vApp '%1'").arg(item.name));
            for (const QString& vmRef : item.vmRefs)
                XenAPI::VM::set_appliance(this->GetSession(), vmRef, QString());

            try
            {
                XenAPI::VM_appliance::destroy(this->GetSession(), item.ref);
            } catch (const std::exception& e)
            {
                // C#: someone else deleted it meanwhile
                if (!QString::fromUtf8(e.what()).startsWith("Object has been deleted"))
                    throw;
            }
        }
        this->SetDescription(tr("vApps deleted."));
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}

AssignVMsToVMApplianceAction::AssignVMsToVMApplianceAction(QSharedPointer<VMAppliance> appliance, const QStringList& vmRefs, bool suppressHistory, QObject* parent)
    : AsyncOperation(appliance->GetConnection(), QString(), QString(), suppressHistory, parent),
      m_applianceRef(appliance->OpaqueRef()), m_currentVmRefs(appliance->VMRefs()), m_vmRefs(vmRefs)
{
    QString title;
    if (vmRefs.size() == 1)
    {
        QSharedPointer<VM> vm = appliance->GetConnection()->GetCache()->ResolveObject<VM>(XenObjectType::VM, vmRefs.first());
        title = tr("Assign VM '%1' to vApp '%2'").arg(vm ? vm->GetName() : vmRefs.first(), appliance->GetName());
    } else
    {
        title = tr("Assign VMs to vApp '%1'").arg(appliance->GetName());
    }
    this->SetTitle(title);
    this->SetPool(poolOf(appliance->GetConnection()));
    this->AddApiMethodToRoleCheck("VM.set_appliance");
}

void AssignVMsToVMApplianceAction::run()
{
    try
    {
        this->SetDescription(tr("Assigning vApp..."));
        for (const QString& vmRef : this->m_currentVmRefs)
        {
            if (!this->m_vmRefs.contains(vmRef))
                XenAPI::VM::set_appliance(this->GetSession(), vmRef, QString());
        }

        XenCache* cache = this->GetConnection()->GetCache();
        for (const QString& vmRef : this->m_vmRefs)
        {
            QSharedPointer<VM> vm = cache->ResolveObject<VM>(XenObjectType::VM, vmRef);
            if (vm && vm->ApplianceRef() != this->m_applianceRef)
                XenAPI::VM::set_appliance(this->GetSession(), vmRef, this->m_applianceRef);
        }
        this->SetDescription(tr("vApp assigned."));
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}

RemoveVMsFromVMApplianceAction::RemoveVMsFromVMApplianceAction(QSharedPointer<VMAppliance> appliance, const QStringList& vmRefs, QObject* parent)
    : AsyncOperation(appliance->GetConnection(), QString(), QString(), parent), m_vmRefs(vmRefs)
{
    QString title;
    if (vmRefs.size() == 1)
    {
        QSharedPointer<VM> vm = appliance->GetConnection()->GetCache()->ResolveObject<VM>(XenObjectType::VM, vmRefs.first());
        title = tr("Remove VM '%1' from vApp '%2'").arg(vm ? vm->GetName() : vmRefs.first(), appliance->GetName());
    } else
    {
        title = tr("Remove VMs from vApp '%1'").arg(appliance->GetName());
    }
    this->SetTitle(title);
    this->SetPool(poolOf(appliance->GetConnection()));
    this->AddApiMethodToRoleCheck("VM.set_appliance");
}

void RemoveVMsFromVMApplianceAction::run()
{
    try
    {
        this->SetDescription(tr("Removing from vApp"));
        for (const QString& vmRef : this->m_vmRefs)
            XenAPI::VM::set_appliance(this->GetSession(), vmRef, QString());
        this->SetDescription(tr("Removed from vApp"));
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}
