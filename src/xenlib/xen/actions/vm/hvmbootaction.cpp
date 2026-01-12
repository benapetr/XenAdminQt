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
#include "../../vm.h"
#include <QDebug>
#include <stdexcept>

HVMBootAction::HVMBootAction(QSharedPointer<VM> vm, QObject* parent)
    : AsyncOperation(tr("Booting VM in Recovery Mode"),
                     tr("Booting '%1' with temporary recovery boot settings...").arg(vm ? vm->GetName() : ""),
                     parent), 
      m_vm(vm)
{
    if (!vm)
        throw std::invalid_argument("VM cannot be null");
    this->m_connection = vm->GetConnection();

    // Register API methods for RBAC checks
    AddApiMethodToRoleCheck("VM.get_HVM_boot_policy");
    AddApiMethodToRoleCheck("VM.get_HVM_boot_params");
    AddApiMethodToRoleCheck("VM.set_HVM_boot_policy");
    AddApiMethodToRoleCheck("VM.set_HVM_boot_params");
    AddApiMethodToRoleCheck("VM.start");
}

void HVMBootAction::run()
{
    XenAPI::Session* session = this->GetSession();
    if (!session)
    {
        throw std::runtime_error("Not connected to XenServer");
    }

    try
    {
        // Step 1: Save current boot policy and boot order
        SetPercentComplete(10);

        m_oldBootPolicy = XenAPI::VM::get_HVM_boot_policy(session, this->m_vm->OpaqueRef());
        QVariantMap oldBootParams = XenAPI::VM::get_HVM_boot_params(session, this->m_vm->OpaqueRef());
        m_oldBootOrder = getBootOrder(oldBootParams);

        // Step 2: Set temporary boot policy and order
        SetPercentComplete(30);

        // Set boot policy to "BIOS order" for manual boot device selection
        XenAPI::VM::set_HVM_boot_policy(session, this->m_vm->OpaqueRef(), "BIOS order");

        // Set boot order to "DN" (DVD drive, then Network)
        QVariantMap recoveryBootParams = oldBootParams;
        setBootOrder(recoveryBootParams, "DN");
        XenAPI::VM::set_HVM_boot_params(session, this->m_vm->OpaqueRef(), recoveryBootParams);

        // Step 3: Start the VM
        SetPercentComplete(50);

        QString taskRef = XenAPI::VM::async_start(session, this->m_vm->OpaqueRef(), false, false);
        pollToCompletion(taskRef); // Wait for VM to start

        if (GetState() == Failed)
        {
            // If start failed, restore boot settings before returning
            restoreBootSettings(session);
            return;
        }

        // Step 4: Restore original boot policy and order
        SetPercentComplete(80);
        restoreBootSettings(session);

        SetPercentComplete(100);

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
    if (!session || !this->m_vm || !this->m_vm->IsValid())
    {
        return;
    }

    try
    {
        // Restore boot policy
        if (!m_oldBootPolicy.isEmpty())
        {
            XenAPI::VM::set_HVM_boot_policy(session, this->m_vm->OpaqueRef(), m_oldBootPolicy);
        }

        // Restore boot order
        QVariantMap bootParams = XenAPI::VM::get_HVM_boot_params(session, this->m_vm->OpaqueRef());
        setBootOrder(bootParams, m_oldBootOrder);
        XenAPI::VM::set_HVM_boot_params(session, this->m_vm->OpaqueRef(), bootParams);

    } catch (const std::exception& e)
    {
        // Log error but don't propagate (we're in cleanup)
        qWarning() << "Failed to restore boot settings for VM" << this->m_vm->OpaqueRef() << ":" << e.what();
    }
}
