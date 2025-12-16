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
#include "../../../xen/connection.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../../xencache.h"
#include <stdexcept>

ChangeMemorySettingsAction::ChangeMemorySettingsAction(XenConnection* connection,
                                                       const QString& vmRef,
                                                       qint64 staticMin,
                                                       qint64 dynamicMin,
                                                       qint64 dynamicMax,
                                                       qint64 staticMax,
                                                       QObject* parent)
    : AsyncOperation(connection,
                     QString("Changing memory settings"),
                     QString("Changing memory settings for VM"),
                     parent),
      m_vmRef(vmRef),
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
        setPercentComplete(0);
        setDescription("Checking VM state...");

        // Get current VM data from cache
        QVariantMap vmData = connection()->getCache()->resolve("vm", m_vmRef);
        if (vmData.isEmpty())
        {
            throw std::runtime_error("VM not found in cache");
        }

        // Check if static memory changed
        qint64 currentStaticMin = vmData.value("memory_static_min").toLongLong();
        qint64 currentStaticMax = vmData.value("memory_static_max").toLongLong();
        m_staticChanged = (m_staticMin != currentStaticMin || m_staticMax != currentStaticMax);

        // Get current power state
        QString powerState = vmData.value("power_state").toString();

        // Determine if reboot is needed
        if (m_staticChanged)
        {
            m_needReboot = (powerState != "Halted");
        } else
        {
            // Dynamic-only changes require VM to be running or halted
            m_needReboot = (powerState != "Halted" && powerState != "Running");
        }

        // Save host affinity for restart
        QString residentOn = vmData.value("resident_on").toString();
        if (!residentOn.isEmpty() && residentOn != "OpaqueRef:NULL")
        {
            m_vmHost = residentOn;
        }

        setPercentComplete(10);

        // Shutdown VM if needed
        if (m_needReboot)
        {
            setDescription("Shutting down VM...");

            // Check allowed operations to determine clean vs hard shutdown
            QVariantList allowedOps = vmData.value("allowed_operations").toList();
            bool canCleanShutdown = allowedOps.contains("clean_shutdown");

            QString taskRef;
            if (canCleanShutdown)
            {
                taskRef = XenAPI::VM::async_clean_shutdown(session(), m_vmRef);
            } else
            {
                taskRef = XenAPI::VM::async_hard_shutdown(session(), m_vmRef);
            }

            pollToCompletion(taskRef, 10, 40);

            // Wait for VM to reach Halted state
            setDescription("Waiting for VM to halt...");
            bool halted = false;
            for (int i = 0; i < 60; ++i)
            { // Wait up to 60 seconds
                vmData = connection()->getCache()->resolve("vm", m_vmRef);
                if (vmData.value("power_state").toString() == "Halted")
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

        setPercentComplete(40);

        // Apply memory changes
        try
        {
            setDescription("Changing memory settings...");

            if (m_staticChanged)
            {
                // Change all memory limits
                XenAPI::VM::set_memory_limits(session(), m_vmRef,
                                              m_staticMin, m_staticMax,
                                              m_dynamicMin, m_dynamicMax);
            } else
            {
                // Change only dynamic range
                XenAPI::VM::set_memory_dynamic_range(session(), m_vmRef,
                                                     m_dynamicMin, m_dynamicMax);
            }

            setPercentComplete(70);

        } catch (const std::exception& e)
        {
            // Ensure VM restart even if memory change fails
            if (m_needReboot)
            {
                setDescription("Restarting VM after error...");
                try
                {
                    QString taskRef;
                    if (!m_vmHost.isEmpty())
                    {
                        taskRef = XenAPI::VM::async_start_on(session(), m_vmRef, m_vmHost, false, false);
                    } else
                    {
                        taskRef = XenAPI::VM::async_start(session(), m_vmRef, false, false);
                    }
                    pollToCompletion(taskRef, 70, 100);
                } catch (...)
                {
                    // Ignore restart errors - report original error
                }
            }
            throw; // Re-throw original error
        }

        // Restart VM if we shut it down
        if (m_needReboot)
        {
            setDescription("Restarting VM...");

            QString taskRef;
            if (!m_vmHost.isEmpty())
            {
                taskRef = XenAPI::VM::async_start_on(session(), m_vmRef, m_vmHost, false, false);
            } else
            {
                taskRef = XenAPI::VM::async_start(session(), m_vmRef, false, false);
            }

            pollToCompletion(taskRef, 70, 100);
        }

        setPercentComplete(100);
        setDescription("Memory settings changed successfully");

    } catch (const std::exception& e)
    {
        setError(QString("Failed to change memory settings: %1").arg(e.what()));
    }
}
