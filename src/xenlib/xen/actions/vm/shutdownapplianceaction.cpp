/* Copyright (c) Cloud Software Group, Inc.
 *
 * Redistribution and use in source and binary forms,
 * with or without modification, are permitted provided
 * that the following conditions are met:
 *
 * *   Redistributions of source code must retain the above
 *     copyright notice, this list of conditions and the
 *     following disclaimer.
 * *   Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the
 *     following disclaimer in the documentation and/or other
 *     materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "shutdownapplianceaction.h"
#include "../../xenapi/vm_appliance.h"
#include "../../session.h"
#include "../../network/connection.h"
#include <stdexcept>

// ============================================================================
// ShutDownApplianceAction (try clean, fall back to hard)
// ============================================================================

ShutDownApplianceAction::ShutDownApplianceAction(XenConnection* connection, const QString& applianceRef,
                                                 QObject* parent)
    : AsyncOperation(connection, tr("Shut down VM appliance"), QString(), parent), m_applianceRef(applianceRef)
{
    addApiMethodToRoleCheck("VM_appliance.shutdown");
}

void ShutDownApplianceAction::run()
{
    if (!session() || !session()->isLoggedIn())
    {
        setError(tr("Not connected to XenServer"));
        return;
    }

    try
    {
        // Get appliance name
        QString applianceName = m_applianceRef;
        try
        {
            QVariantMap record = XenAPI::VM_appliance::get_record(session(), m_applianceRef);
            if (record.contains("name_label"))
            {
                applianceName = record["name_label"].toString();
            }
        } catch (...)
        {
            // Name lookup failed
        }

        setDescription(tr("Shutting down appliance '%1'...").arg(applianceName));

        // Use shutdown method (tries clean, falls back to hard)
        QString taskRef = XenAPI::VM_appliance::async_shutdown(session(), m_applianceRef);

        // Poll task to completion
        pollToCompletion(taskRef, 0, 100);

        setDescription(tr("Successfully shut down appliance '%1'").arg(applianceName));

    } catch (const std::exception& e)
    {
        setError(tr("Failed to shut down VM appliance: %1").arg(e.what()));
    } catch (...)
    {
        setError(tr("Failed to shut down VM appliance: Unknown error"));
    }
}

// ============================================================================
// CleanShutDownApplianceAction (graceful shutdown via guest OS)
// ============================================================================

CleanShutDownApplianceAction::CleanShutDownApplianceAction(XenConnection* connection,
                                                           const QString& applianceRef,
                                                           QObject* parent)
    : AsyncOperation(connection, tr("Clean shut down VM appliance"), QString(), parent), m_applianceRef(applianceRef)
{
    addApiMethodToRoleCheck("VM_appliance.clean_shutdown");
}

void CleanShutDownApplianceAction::run()
{
    if (!session() || !session()->isLoggedIn())
    {
        setError(tr("Not connected to XenServer"));
        return;
    }

    try
    {
        // Get appliance name
        QString applianceName = m_applianceRef;
        try
        {
            QVariantMap record = XenAPI::VM_appliance::get_record(session(), m_applianceRef);
            if (record.contains("name_label"))
            {
                applianceName = record["name_label"].toString();
            }
        } catch (...)
        {
            // Name lookup failed
        }

        setDescription(tr("Performing clean shutdown of appliance '%1'...").arg(applianceName));

        // Clean shutdown
        QString taskRef = XenAPI::VM_appliance::async_clean_shutdown(session(), m_applianceRef);

        // Poll task to completion
        pollToCompletion(taskRef, 0, 100);

        setDescription(tr("Successfully performed clean shutdown of appliance '%1'").arg(applianceName));

    } catch (const std::exception& e)
    {
        setError(tr("Failed to clean shutdown VM appliance: %1").arg(e.what()));
    } catch (...)
    {
        setError(tr("Failed to clean shutdown VM appliance: Unknown error"));
    }
}

// ============================================================================
// HardShutDownApplianceAction (immediate power off)
// ============================================================================

HardShutDownApplianceAction::HardShutDownApplianceAction(XenConnection* connection,
                                                         const QString& applianceRef,
                                                         QObject* parent)
    : AsyncOperation(connection, tr("Force shut down VM appliance"), QString(), parent), m_applianceRef(applianceRef)
{
    addApiMethodToRoleCheck("VM_appliance.hard_shutdown");
}

void HardShutDownApplianceAction::run()
{
    if (!session() || !session()->isLoggedIn())
    {
        setError(tr("Not connected to XenServer"));
        return;
    }

    try
    {
        // Get appliance name
        QString applianceName = m_applianceRef;
        try
        {
            QVariantMap record = XenAPI::VM_appliance::get_record(session(), m_applianceRef);
            if (record.contains("name_label"))
            {
                applianceName = record["name_label"].toString();
            }
        } catch (...)
        {
            // Name lookup failed
        }

        setDescription(tr("Forcing shutdown of appliance '%1'...").arg(applianceName));

        // Hard shutdown
        QString taskRef = XenAPI::VM_appliance::async_hard_shutdown(session(), m_applianceRef);

        // Poll task to completion
        pollToCompletion(taskRef, 0, 100);

        setDescription(tr("Successfully forced shutdown of appliance '%1'").arg(applianceName));

    } catch (const std::exception& e)
    {
        setError(tr("Failed to force shutdown VM appliance: %1").arg(e.what()));
    } catch (...)
    {
        setError(tr("Failed to force shutdown VM appliance: Unknown error"));
    }
}
