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

#include "vmcloneaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_VM.h"
#include <QDebug>

VMCloneAction::VMCloneAction(XenConnection* connection,
                             QSharedPointer<VM> vm,
                             const QString& name,
                             const QString& description,
                             QObject* parent)
    : AsyncOperation(connection,
                     QString("Cloning '%1' to '%2'").arg(vm ? vm->GetName() : "").arg(name),
                     QString("Cloning '%1'").arg(vm ? vm->GetName() : ""),
                     parent),
      m_vm(vm),
      m_cloneName(name),
      m_cloneDescription(description)
{
    if (!m_vm)
        throw std::invalid_argument("VM cannot be null");
}

void VMCloneAction::run()
{
    try
    {
        // Clone the VM
        QString taskRef = XenAPI::VM::async_clone(GetSession(), m_vm->OpaqueRef(), m_cloneName);
        pollToCompletion(taskRef, 0, 90);

        QString createdVmRef = GetResult();

        // Wait for VM to appear in cache
        // TODO: Implement Connection.WaitForCache equivalent
        // For now, we'll just set the description directly

        qDebug() << "VMCloneAction: Cloned VM ref:" << createdVmRef;

        // Set the description
        XenAPI::VM::set_name_description(GetSession(), createdVmRef, m_cloneDescription);

        // Store the created VM ref as result
        SetResult(createdVmRef);

        SetDescription("VM cloned successfully");

    } catch (const std::exception& e)
    {
        setError(QString("Failed to clone VM: %1").arg(e.what()));
    }
}
