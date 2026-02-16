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

#include "updateintegratedgpupassthroughaction.h"
#include "../../host.h"
#include "../../pgpu.h"
#include "../../xenapi/xenapi_Host.h"
#include "../../xenapi/xenapi_PGPU.h"
#include <stdexcept>

UpdateIntegratedGpuPassthroughAction::UpdateIntegratedGpuPassthroughAction(QSharedPointer<Host> host,
                                                                           bool enableOnNextReboot,
                                                                           bool suppressHistory,
                                                                           QObject* parent)
    : AsyncOperation(host ? host->GetConnection() : nullptr,
                     host ? QStringLiteral("Update integrated GPU passthrough - %1").arg(host->GetName())
                          : QStringLiteral("Update integrated GPU passthrough"),
                     QStringLiteral("Updating host GPU settings..."),
                     suppressHistory,
                     parent),
      m_host(host),
      m_enable(enableOnNextReboot)
{
    if (!this->m_host || !this->m_host->IsValid())
        throw std::invalid_argument("Invalid host object");

    if (this->m_enable)
    {
        this->AddApiMethodToRoleCheck("host.async_enable_display");
        this->AddApiMethodToRoleCheck("PGPU.async_enable_dom0_access");
    }
    else
    {
        this->AddApiMethodToRoleCheck("host.async_disable_display");
        this->AddApiMethodToRoleCheck("PGPU.async_disable_dom0_access");
    }
}

void UpdateIntegratedGpuPassthroughAction::run()
{
    this->SetDescription(QStringLiteral("Updating host display mode..."));

    QString taskRef;
    if (this->m_enable)
        taskRef = XenAPI::Host::async_enable_display(this->GetSession(), this->m_host->OpaqueRef());
    else
        taskRef = XenAPI::Host::async_disable_display(this->GetSession(), this->m_host->OpaqueRef());

    this->pollToCompletion(taskRef, 0, 50);

    QSharedPointer<PGPU> systemDisplayGpu = this->m_host->SystemDisplayDevice();
    if (systemDisplayGpu && systemDisplayGpu->IsValid())
    {
        this->SetDescription(QStringLiteral("Updating integrated GPU dom0 access..."));

        if (this->m_enable)
            taskRef = XenAPI::PGPU::async_enable_dom0_access(this->GetSession(), systemDisplayGpu->OpaqueRef());
        else
            taskRef = XenAPI::PGPU::async_disable_dom0_access(this->GetSession(), systemDisplayGpu->OpaqueRef());

        this->pollToCompletion(taskRef, 50, 100);
    }
    else
    {
        this->SetPercentComplete(100);
    }

    this->SetDescription(QStringLiteral("Integrated GPU passthrough updated"));
}
