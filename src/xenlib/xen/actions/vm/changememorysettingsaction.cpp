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

#include "changememorysettingsaction.h"
#include "../../../xen/network/connection.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../../xencache.h"
#include <stdexcept>

ChangeMemorySettingsAction::ChangeMemorySettingsAction(QSharedPointer<VM> vm,
                                                       qint64 staticMin,
                                                       qint64 dynamicMin,
                                                       qint64 dynamicMax,
                                                       qint64 staticMax,
                                                       QObject* parent)
    : AsyncOperation(vm->GetConnection(),
                     QString("Changing memory settings"),
                     QString("Changing memory settings for '%1'").arg(vm ? vm->GetName() : ""),
                     parent),
      m_vm(vm),
      m_staticMin(staticMin),
      m_dynamicMin(dynamicMin),
      m_dynamicMax(dynamicMax),
      m_staticMax(staticMax),
      m_staticChanged(false),
      m_needReboot(false)
{
}

void ChangeMemorySettingsAction::run()
{
    try
    {
        SetPercentComplete(0);
        SetDescription("Checking VM state...");

        // Check if static memory changed
        qint64 currentStaticMin = this->m_vm->MemoryStaticMin();
        qint64 currentStaticMax = this->m_vm->MemoryStaticMax();
        this->m_staticChanged = (this->m_staticMin != currentStaticMin || this->m_staticMax != currentStaticMax);

        // Get current power state
        QString powerState = this->m_vm->GetPowerState();

        // Determine if reboot is needed
        if (this->m_staticChanged)
        {
            this->m_needReboot = (powerState != "Halted");
        } else
        {
            // Dynamic-only changes require VM to be running or halted
            this->m_needReboot = (powerState != "Halted" && powerState != "Running");
        }

        // Save host affinity for restart
        QString residentOn = this->m_vm->ResidentOnRef();
        if (!residentOn.isEmpty() && residentOn != "OpaqueRef:NULL")
        {
            this->m_vmHost = residentOn;
        }

        SetPercentComplete(10);

        // Shutdown VM if needed
        if (this->m_needReboot)
        {
            SetDescription("Shutting down VM...");

            // Check allowed operations to determine clean vs hard shutdown
            QStringList allowedOps = this->m_vm->GetAllowedOperations();
            bool canCleanShutdown = allowedOps.contains("clean_shutdown");

            QString taskRef;
            if (canCleanShutdown)
            {
                taskRef = XenAPI::VM::async_clean_shutdown(GetSession(), this->m_vm->OpaqueRef());
            } else
            {
                taskRef = XenAPI::VM::async_hard_shutdown(GetSession(), this->m_vm->OpaqueRef());
            }

            pollToCompletion(taskRef, 10, 40);

            // Wait for VM to reach Halted state
            SetDescription("Waiting for VM to halt...");
            bool halted = false;
            for (int i = 0; i < 60; ++i)
            { // Wait up to 60 seconds
                if (this->m_vm->GetPowerState() == "Halted")
                {
                    halted = true;
                    break;
                }
                QThread::sleep(1);
            }

            if (!halted)
            {
                throw std::runtime_error("VM failed to halt in time");
            }
        }

        SetPercentComplete(40);

        // Apply memory changes
        try
        {
            SetDescription("Changing memory settings...");

            if (this->m_staticChanged)
            {
                // Change all memory limits
                XenAPI::VM::set_memory_limits(GetSession(), this->m_vm->OpaqueRef(),
                                              this->m_staticMin, this->m_staticMax,
                                              this->m_dynamicMin, this->m_dynamicMax);
            } else
            {
                // Change only dynamic range
                XenAPI::VM::set_memory_dynamic_range(GetSession(), this->m_vm->OpaqueRef(),
                                                     this->m_dynamicMin, this->m_dynamicMax);
            }

            SetPercentComplete(70);

        } catch (const std::exception& e)
        {
            // Ensure VM restart even if memory change fails
            if (this->m_needReboot)
            {
                SetDescription("Restarting VM after error...");
                try
                {
                    QString taskRef;
                    if (!this->m_vmHost.isEmpty())
                    {
                        taskRef = XenAPI::VM::async_start_on(GetSession(), this->m_vm->OpaqueRef(), this->m_vmHost, false, false);
                    } else
                    {
                        taskRef = XenAPI::VM::async_start(GetSession(), this->m_vm->OpaqueRef(), false, false);
                    }
                    this->pollToCompletion(taskRef, 70, 100);
                } catch (...)
                {
                    // Ignore restart errors - report original error
                }
            }
            throw; // Re-throw original error
        }

        // Restart VM if we shut it down
        if (this->m_needReboot)
        {
            SetDescription("Restarting VM...");

            QString taskRef;
            if (!this->m_vmHost.isEmpty())
            {
                taskRef = XenAPI::VM::async_start_on(GetSession(), this->m_vm->OpaqueRef(), this->m_vmHost, false, false);
            } else
            {
                taskRef = XenAPI::VM::async_start(GetSession(), this->m_vm->OpaqueRef(), false, false);
            }

            this->pollToCompletion(taskRef, 70, 100);
        }

        SetPercentComplete(100);
        SetDescription("Memory settings changed successfully");

    } catch (const std::exception& e)
    {
        setError(QString("Failed to change memory settings: %1").arg(e.what()));
    }
}
