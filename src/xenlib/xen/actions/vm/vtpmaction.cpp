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

#include "vtpmaction.h"
#include "../../network/connection.h"
#include "../../xenapi/xenapi_VTPM.h"
#include "../../vm.h"
#include "../../vtpm.h"

NewVtpmAction::NewVtpmAction(XenConnection* connection, const QSharedPointer<VM>& vm, QObject* parent)
    : AsyncOperation(connection,
                     tr("Attaching a new vTPM to VM '%1'").arg(vm ? vm->GetName() : QString()),
                     tr("Attaching vTPM..."),
                     parent),
      m_vmRef(vm ? vm->OpaqueRef() : QString())
{
    this->AddApiMethodToRoleCheck("vtpm.create");
    if (vm)
        this->SetVM(vm);
}

void NewVtpmAction::run()
{
    try
    {
        const QString vtpmRef = XenAPI::VTPM::create(this->GetSession(), this->m_vmRef, false);
        // C# Connection.TryResolveWithTimeout: wait until the new vTPM is in the cache
        if (!vtpmRef.isEmpty() && this->GetConnection())
            this->GetConnection()->WaitForCacheObject<VTPM>("vtpm", vtpmRef, 60000, [this]() { return this->IsCancelled(); });
        this->SetResult(vtpmRef);
        this->SetDescription(tr("Completed"));
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}

RemoveVtpmAction::RemoveVtpmAction(XenConnection* connection, const QSharedPointer<VTPM>& vtpm,
                                   const QSharedPointer<VM>& vm, QObject* parent)
    : AsyncOperation(connection,
                     tr("Removing a vTPM from VM '%1'").arg(vm ? vm->GetName() : QString()),
                     tr("Removing vTPM..."),
                     parent),
      m_vtpmRef(vtpm ? vtpm->OpaqueRef() : QString())
{
    this->AddApiMethodToRoleCheck("vtpm.destroy");
    if (vm)
        this->SetVM(vm);
}

void RemoveVtpmAction::run()
{
    try
    {
        XenAPI::VTPM::destroy(this->GetSession(), this->m_vtpmRef);
        this->SetDescription(tr("Completed"));
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}
