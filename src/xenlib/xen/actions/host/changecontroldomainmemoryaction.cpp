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

#include "changecontroldomainmemoryaction.h"
#include "../../host.h"
#include "../../vm.h"
#include "../../failure.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../../utils/misc.h"

ChangeControlDomainMemoryAction::ChangeControlDomainMemoryAction(QSharedPointer<Host> host, qint64 memory, bool suppressHistory, QObject* parent)
    : AsyncOperation(host ? host->GetConnection() : nullptr,
                     QString("Changing control domain memory for %1").arg(host ? host->GetName() : "host"),
                     "Waiting...",
                     parent),
      m_host(host),
      m_memory(memory)
{
    this->SetSuppressHistory(suppressHistory);
    this->setAppliesToFromObject(this->m_host);
    this->AddApiMethodToRoleCheck("vm.set_memory");
}

void ChangeControlDomainMemoryAction::run()
{
    if (!this->m_host)
    {
        this->setError("No host selected.");
        return;
    }

    QSharedPointer<VM> dom0 = this->m_host->ControlDomainZero();
    if (!dom0 || !dom0->IsValid())
    {
        this->setError("Failed to locate control domain VM.");
        return;
    }

    try
    {
        XenAPI::VM::set_memory(this->GetSession(), dom0->OpaqueRef(), this->m_memory);
    } catch (const Failure& failure)
    {
        const QStringList errorDescription = failure.errorDescription();
        if (!errorDescription.isEmpty()
            && errorDescription.first() == Failure::MEMORY_CONSTRAINT_VIOLATION
            && this->m_memory < dom0->GetMemoryStaticMin())
        {
            this->setError(QString("Control domain memory value %1 is too low. Minimum is %2.")
                               .arg(Misc::FormatSize(this->m_memory))
                               .arg(Misc::FormatSize(dom0->GetMemoryStaticMin())));
            return;
        }

        throw;
    }

    this->SetDescription("Completed");
}
