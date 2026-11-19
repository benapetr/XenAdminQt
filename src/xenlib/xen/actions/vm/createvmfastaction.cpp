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

#include "createvmfastaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../../xencache.h"
#include "../../../vmhelpers.h"
#include "../../hiddenobjects.h"
#include <QDebug>

CreateVMFastAction::CreateVMFastAction(XenConnection* connection,
                                       QSharedPointer<VM> templateVM,
                                       QObject* parent)
    // CA-339370: the VM's name is only worked out once the clone exists, so that VMs created
    // in quick succession don't get the same name; the title therefore names the template
    : AsyncOperation(connection,
                     QObject::tr("Creating VM quickly from template '%1'").arg(templateVM ? templateVM->GetName() : QString()),
                     QString(),
                     parent),
      m_template(templateVM)
{
    if (!m_template)
        throw std::invalid_argument("Template VM cannot be null");

    // RBAC dependencies (matches C# CreateVMFastAction)
    this->AddApiMethodToRoleCheck("vm.clone");
    this->AddApiMethodToRoleCheck("vm.provision");
    this->AddApiMethodToRoleCheck("vm.start");
    this->AddApiMethodToRoleCheck("vm.set_name_label");
}

void CreateVMFastAction::run()
{
    // C# CreateVMFastAction.Run
    QString newVmRef;
    try
    {
        const QString originalName = m_template->GetName();

        SetDescription(QObject::tr("Cloning template '%1'").arg(originalName));
        QString taskRef = XenAPI::VM::async_clone(GetSession(), m_template->OpaqueRef(), VMHelpers::MakeHiddenName(originalName));
        pollToCompletion(taskRef, 0, 80);

        newVmRef = GetResult();
        if (newVmRef.isEmpty())
            throw std::runtime_error("Clone returned empty VM ref");

        // C# Connection.WaitForCache: the VM must be known before it is renamed and shown
        GetConnection()->WaitForCacheObject<VM>("vm", newVmRef, 60000, [this]() { return this->IsCancelled(); });

        // Keep it out of the tree until it is provisioned and named (C# HideObject)
        HiddenObjects::Hide(newVmRef);

        SetDescription(QObject::tr("Provisioning VM"));
        taskRef = XenAPI::VM::async_provision(GetSession(), newVmRef);
        pollToCompletion(taskRef, 80, 90);

        SetDescription(QObject::tr("Saving VM properties"));
        const QString newName = VMHelpers::DefaultVMName(originalName, GetConnection());
        XenAPI::VM::set_name_label(GetSession(), newVmRef, newName);
        HiddenObjects::Show(newVmRef);

        SetResult(newVmRef);
        SetDescription(QObject::tr("Created VM '%1'").arg(newName));
    } catch (const std::exception& e)
    {
        // Don't leave the VM out of sight; C# leaves it hidden until restart, but it is a
        // real VM on the server (named "__gui__...", which VM::IsVisible still filters)
        if (!newVmRef.isEmpty())
            HiddenObjects::Show(newVmRef);
        setError(QString("Failed to create VM: %1").arg(e.what()));
    }
}
