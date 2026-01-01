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

#include "hvmbootaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_VM.h"
#include <QDebug>
#include <stdexcept>

HVMBootAction::HVMBootAction(XenConnection* connection, const QString& vmRef, QObject* parent)
    : AsyncOperation(connection, tr("Booting VM in Recovery Mode"), QString(), parent), m_vmRef(vmRef)
{
    // Get VM name for display purposes
    try
    {
        QVariantMap vmRecord = XenAPI::VM::get_record(connection->GetSession(), vmRef);
        m_vmName = vmRecord.value("name_label").toString();
        if (m_vmName.isEmpty())
        {
            m_vmName = vmRef; // Fallback to ref if name not available
        }
    } catch (...)
    {
        m_vmName = vmRef; // Use ref on error
    }

    // Update description with VM name
    setDescription(tr("Booting '%1' with temporary recovery boot settings...").arg(m_vmName));

    // Register API methods for RBAC checks
    addApiMethodToRoleCheck("VM.get_HVM_boot_policy");
    addApiMethodToRoleCheck("VM.get_HVM_boot_params");
    addApiMethodToRoleCheck("VM.set_HVM_boot_policy");
    addApiMethodToRoleCheck("VM.set_HVM_boot_params");
    addApiMethodToRoleCheck("VM.start");
}

void HVMBootAction::run()
{
    XenAPI::Session* session = this->session();
    if (!session)
    {
        throw std::runtime_error("Not connected to XenServer");
    }

    try
    {
        // Step 1: Save current boot policy and boot order
        setPercentComplete(10);

        m_oldBootPolicy = XenAPI::VM::get_HVM_boot_policy(session, m_vmRef);
        QVariantMap oldBootParams = XenAPI::VM::get_HVM_boot_params(session, m_vmRef);
        m_oldBootOrder = getBootOrder(oldBootParams);

        // Step 2: Set temporary boot policy and order
        setPercentComplete(30);

        // Set boot policy to "BIOS order" for manual boot device selection
        XenAPI::VM::set_HVM_boot_policy(session, m_vmRef, "BIOS order");

        // Set boot order to "DN" (DVD drive, then Network)
        QVariantMap recoveryBootParams = oldBootParams;
        setBootOrder(recoveryBootParams, "DN");
        XenAPI::VM::set_HVM_boot_params(session, m_vmRef, recoveryBootParams);

        // Step 3: Start the VM
        setPercentComplete(50);

        QString taskRef = XenAPI::VM::async_start(session, m_vmRef, false, false);
        pollToCompletion(taskRef); // Wait for VM to start

        if (state() == Failed)
        {
            // If start failed, restore boot settings before returning
            restoreBootSettings(session);
            return;
        }

        // Step 4: Restore original boot policy and order
        setPercentComplete(80);
        restoreBootSettings(session);

        setPercentComplete(100);

    } catch (const std::exception& e)
    {
        // On any error, attempt to restore boot settings
        try
        {
            restoreBootSettings(session);
        } catch (...)
        {
            // Ignore errors during cleanup
        }
        throw; // Re-throw for AsyncOperation error handling
    }
}

QString HVMBootAction::getBootOrder(const QVariantMap& bootParams) const
{
    return bootParams.value("order").toString();
}

void HVMBootAction::setBootOrder(QVariantMap& bootParams, const QString& order)
{
    if (order.isEmpty())
    {
        bootParams.remove("order");
    } else
    {
        bootParams["order"] = order;
    }
}

void HVMBootAction::restoreBootSettings(XenAPI::Session* session)
{
    if (!session || m_vmRef.isEmpty())
    {
        return;
    }

    try
    {
        // Restore boot policy
        if (!m_oldBootPolicy.isEmpty())
        {
            XenAPI::VM::set_HVM_boot_policy(session, m_vmRef, m_oldBootPolicy);
        }

        // Restore boot order
        QVariantMap bootParams = XenAPI::VM::get_HVM_boot_params(session, m_vmRef);
        setBootOrder(bootParams, m_oldBootOrder);
        XenAPI::VM::set_HVM_boot_params(session, m_vmRef, bootParams);

    } catch (const std::exception& e)
    {
        // Log error but don't propagate (we're in cleanup)
        qWarning() << "Failed to restore boot settings for VM" << m_vmRef << ":" << e.what();
    }
}
