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

#include "changevcpusettingsaction.h"
#include "../../xenapi/xenapi_VM.h"
#include "xen/vm.h"
#include <stdexcept>

ChangeVCPUSettingsAction::ChangeVCPUSettingsAction(QSharedPointer<VM> vm,
                                                   qint64 vcpusMax,
                                                   qint64 vcpusAtStartup,
                                                   QObject* parent)
    : AsyncOperation(QString("Changing VCPU settings"),
                     QString("Changing VCPU settings for '%1'").arg(vm ? vm->GetName() : ""),
                     parent),
      m_vm(vm),
      m_vcpusMax(vcpusMax),
      m_vcpusAtStartup(vcpusAtStartup)
{
    if (!vm)
        throw std::invalid_argument("VM cannot be null");
    this->m_connection = vm->GetConnection();
}

void ChangeVCPUSettingsAction::run()
{
    try
    {
        SetPercentComplete(0);
        SetDescription("Checking VM state...");

        // Check if VM is still valid
        if (!this->m_vm || !this->m_vm->IsValid())
        {
            // VM disappeared - nothing to do
            SetDescription("VM no longer exists");
            SetPercentComplete(100);
            return;
        }

        QString powerState = this->m_vm->GetPowerState();
        qint64 currentVCPUsAtStartup = this->m_vm->VCPUsAtStartup();

        SetPercentComplete(20);

        if (powerState == "Running")
        {
            // Running VM: can only hot-plug (increase) VCPUs
            SetDescription("Hot-plugging VCPUs...");

            if (currentVCPUsAtStartup > this->m_vcpusAtStartup)
            {
                // Trying to reduce VCPUs on running VM
                throw std::runtime_error("Cannot reduce VCPUs on a running VM. "
                                         "Please shut down the VM first.");
            }

            // Hot-plug VCPUs
            XenAPI::VM::set_VCPUs_number_live(GetSession(), this->m_vm->OpaqueRef(), this->m_vcpusAtStartup);

            SetPercentComplete(100);
            SetDescription("VCPUs hot-plugged successfully");

        } else
        {
            // Halted VM: can set both max and startup with proper ordering
            SetDescription("Changing VCPU configuration...");

            // Order matters: must satisfy constraint VCPUs_at_startup <= VCPUs_max
            if (currentVCPUsAtStartup > this->m_vcpusAtStartup)
            {
                // Reducing VCPUs: lower at_startup first, then max
                XenAPI::VM::set_VCPUs_at_startup(GetSession(), this->m_vm->OpaqueRef(), this->m_vcpusAtStartup);
                SetPercentComplete(50);
                XenAPI::VM::set_VCPUs_max(GetSession(), this->m_vm->OpaqueRef(), this->m_vcpusMax);
                SetPercentComplete(100);
            } else
            {
                // Increasing VCPUs: raise max first, then at_startup
                XenAPI::VM::set_VCPUs_max(GetSession(), this->m_vm->OpaqueRef(), this->m_vcpusMax);
                SetPercentComplete(50);
                XenAPI::VM::set_VCPUs_at_startup(GetSession(), this->m_vm->OpaqueRef(), this->m_vcpusAtStartup);
                SetPercentComplete(100);
            }

            SetDescription("VCPU configuration changed successfully");
        }

    } catch (const std::exception& e)
    {
        setError(QString("Failed to change VCPU settings: %1").arg(e.what()));
    }
}
