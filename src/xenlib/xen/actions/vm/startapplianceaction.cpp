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

#include "startapplianceaction.h"
#include "../../xenapi/vm_appliance.h"
#include "../../session.h"
#include "../../network/connection.h"
#include <stdexcept>

StartApplianceAction::StartApplianceAction(XenConnection* connection, const QString& applianceRef,
                                           bool suspend, QObject* parent)
    : AsyncOperation(connection, suspend ? tr("Start VM appliance (paused)") : tr("Start VM appliance"),
                     QString(), parent),
      m_applianceRef(applianceRef), m_suspend(suspend)
{
    // Add RBAC permission check
    addApiMethodToRoleCheck("VM_appliance.start");
}

void StartApplianceAction::run()
{
    if (!session() || !session()->IsLoggedIn())
    {
        setError(tr("Not connected to XenServer"));
        return;
    }

    try
    {
        // Get appliance name for progress messages
        QString applianceName = this->m_applianceRef;
        try
        {
            QVariantMap record = XenAPI::VM_appliance::get_record(session(), m_applianceRef);
            if (record.contains("name_label"))
            {
                applianceName = record["name_label"].toString();
            }
        } catch (...)
        {
            // Name lookup failed, use ref instead
        }

        // Update description
        if (this->m_suspend)
        {
            setDescription(tr("Starting appliance '%1' in paused state...").arg(applianceName));
        } else
        {
            setDescription(tr("Starting appliance '%1'...").arg(applianceName));
        }

        // Start the VM appliance (async operation returns task ref)
        QString taskRef = XenAPI::VM_appliance::async_start(session(), this->m_applianceRef, this->m_suspend);

        // Poll task to completion with progress tracking
        pollToCompletion(taskRef, 0, 100);

        // Success
        if (m_suspend)
        {
            setDescription(tr("Successfully started appliance '%1' (paused)").arg(applianceName));
        } else
        {
            setDescription(tr("Successfully started appliance '%1'").arg(applianceName));
        }

    } catch (const std::exception& e)
    {
        setError(tr("Failed to start VM appliance: %1").arg(e.what()));
    } catch (...)
    {
        setError(tr("Failed to start VM appliance: Unknown error"));
    }
}
