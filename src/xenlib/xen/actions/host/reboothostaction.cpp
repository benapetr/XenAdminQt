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

#include "reboothostaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../vm.h"
#include "../../../xencache.h"
#include "../../xenapi/xenapi_Host.h"
#include "../../xenapi/xenapi_VM.h"
#include <QDebug>

RebootHostAction::RebootHostAction(XenConnection* connection,
                                   Host* host,
                                   QObject* parent)
    : AsyncOperation(connection,
                     QString("Rebooting %1").arg(host->GetName()),
                     "Waiting...",
                     parent),
      m_host(host),
      m_wasEnabled(false)
{
    setAppliesToFromObject(host);
}

void RebootHostAction::run()
{
    if (!m_host)
    {
        setError("No host specified for reboot");
        return;
    }

    try
    {
        m_wasEnabled = m_host->IsEnabled();
        setDescription(QString("Rebooting %1...").arg(m_host->GetName()));

        // Step 1: Maybe reduce ntol before operation (HA support)
        maybeReduceNtolBeforeOp();

        // Step 2: Shutdown all VMs on the host
        shutdownVMs(true); // true = for reboot

        // Step 3: Reboot the host
        QString taskRef = XenAPI::Host::async_reboot(session(), m_host->OpaqueRef());
        pollToCompletion(taskRef, 95, 100);

        // Step 4: Interrupt connection if this is the coordinator
        // Note: This will be handled by connection management in Qt
        // For now, just log it
        qDebug() << "RebootHostAction: Host rebooted successfully";

        setDescription(QString("%1 rebooted").arg(m_host->GetName()));

    } catch (const std::exception& e)
    {
        QString error = e.what();
        qWarning() << "RebootHostAction: Exception rebooting host:" << error;

        // Try to re-enable the host on error
        if (m_wasEnabled)
        {
            try
            {
                XenAPI::Host::enable(session(), m_host->OpaqueRef());
            } catch (const std::exception& e2)
            {
                qWarning() << "RebootHostAction: Exception trying to re-enable host after error:" << e2.what();
            }
        }

        // Check for specific VM shutdown acknowledgment failure
        if (error.contains("VM_FAILED_SHUTDOWN_ACKNOWLEDGMENT"))
        {
            setError("A VM did not acknowledge the need to shut down. Please shut down VMs manually and try again.");
        } else
        {
            setError(QString("Failed to reboot host: %1").arg(error));
        }
    }
}

void RebootHostAction::shutdownVMs(bool isForReboot)
{
    /*
     * C# logic:
     * 1. Host.disable() - Disable the host
     * 2. Count resident VMs that are not halted
     * 3. For each VM:
     *    - If allowed_operations contains clean_shutdown: use async_clean_shutdown
     *    - Otherwise: use async_hard_shutdown
     *    - Poll to completion with progress updates
     */

    try
    {
        // Step 1: Disable the host
        QString disableTaskRef = XenAPI::Host::async_disable(session(), m_host->OpaqueRef());
        pollToCompletion(disableTaskRef, 0, 1);

        setPercentComplete(1);

        // Step 2: Get all resident VMs
        QStringList residentVMs = m_host->ResidentVMRefs();

        // Count VMs that need shutdown (running, non-control-domain)
        QList<VM*> toShutdown;
        XenCache* cache = connection()->getCache();

        for (const QString& vmRef : residentVMs)
        {
            QVariantMap vmData = cache->ResolveObjectData("vm", vmRef);
            if (vmData.isEmpty())
                continue;

            QString powerState = vmData.value("power_state").toString();
            bool isControlDomain = vmData.value("is_control_domain", false).toBool();

            if (powerState == "Running" && !isControlDomain)
            {
                VM* vm = new VM(connection(), vmRef, this);
                toShutdown.append(vm);
            }
        }

        int n = toShutdown.count();
        if (n == 0)
        {
            return; // No VMs to shutdown
        }

        // Step 3: Shutdown each VM
        int step = 94 / n; // Progress from 1% to 95%

        for (int i = 0; i < n; ++i)
        {
            VM* vm = toShutdown[i];

            setDescription(QString(isForReboot
                                       ? "Rebooting: Shutting down VM %1 (%2/%3)"
                                       : "Shutting down VM %1 (%2/%3)")
                               .arg(vm->GetName())
                               .arg(i + 1)
                               .arg(n));

            // Check if clean shutdown is allowed
            QStringList allowedOps = vm->AllowedOperations();
            bool canCleanShutdown = allowedOps.contains("clean_shutdown");

            QString taskRef;
            if (canCleanShutdown)
            {
                taskRef = XenAPI::VM::async_clean_shutdown(session(), vm->OpaqueRef());
            } else
            {
                taskRef = XenAPI::VM::async_hard_shutdown(session(), vm->OpaqueRef());
            }

            int progressStart = percentComplete();
            pollToCompletion(taskRef, progressStart, progressStart + step);
        }

        // Clean up VM objects
        qDeleteAll(toShutdown);

    } catch (const std::exception& e)
    {
        qWarning() << "RebootHostAction: Exception shutting down VMs:" << e.what();

        // Try to re-enable the host so user can manually shutdown VMs
        try
        {
            XenAPI::Host::enable(session(), m_host->OpaqueRef());
        } catch (const std::exception& e2)
        {
            qWarning() << "RebootHostAction: Exception trying to re-enable host after VM shutdown error:" << e2.what();
        }

        throw; // Re-throw to caller
    }
}

void RebootHostAction::maybeReduceNtolBeforeOp()
{
    /*
     * C# logic:
     * 1. Get pool
     * 2. If HA enabled:
     *    - Get VM HA restart priorities
     *    - Calculate maximum tolerable host failures
     *    - Calculate target ntol = max(0, max - 1)
     *    - If current ntol > target ntol:
     *      - Ask user via acceptNTolChanges callback
     *      - If user accepts: call Pool.set_ha_host_failures_to_tolerate()
     *      - If user declines: throw CancelledException
     *
     * Note: Full HA logic not yet implemented in Qt version.
     * This is a placeholder that can be expanded later.
     */

    // TODO: Implement full HA ntol logic when HA support is added
    // For now, this is a no-op
    qDebug() << "RebootHostAction: HA ntol adjustment not yet implemented";
}
