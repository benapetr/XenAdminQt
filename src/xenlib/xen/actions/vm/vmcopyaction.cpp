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

#include "vmcopyaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../vm.h"
#include "../../sr.h"
#include "../../host.h"
#include "../../pool.h"
#include "../../xenapi/xenapi_VM.h"
#include <QDebug>
#include <stdexcept>

VMCopyAction::VMCopyAction(QSharedPointer<VM> vm,
                           QSharedPointer<Host> host,
                           QSharedPointer<SR> sr,
                           const QString& nameLabel,
                           const QString& description,
                           QObject* parent)
    : AsyncOperation(QString("Copying '%1' to '%2' on '%3'")
                         .arg(vm ? vm->GetName() : "")
                         .arg(nameLabel)
                         .arg(sr ? sr->GetName() : ""),
                     QString("Copying VM '%1'").arg(vm ? vm->GetName() : ""),
                     parent),
      m_vm(vm), m_host(host), m_sr(sr), m_nameLabel(nameLabel), m_description(description)
{
    if (!m_vm)
        throw std::invalid_argument("VM cannot be null");
    if (!m_sr)
        throw std::invalid_argument("SR cannot be null");

    this->m_connection = vm->GetConnection();

    // Set context objects
    SetVM(m_vm);
    SetSR(m_sr);
    if (this->m_host)
        SetHost(this->m_host);

    AddApiMethodToRoleCheck("VM.copy");
    AddApiMethodToRoleCheck("VM.set_name_description");

    // If VM is a template, set template context
    if (this->m_vm->IsTemplate())
        SetTemplate(this->m_vm);
}

void VMCopyAction::run()
{
    try
    {
        // Call VM.async_copy
        QString taskRef = XenAPI::VM::async_copy(
            GetSession(),
            this->m_vm->OpaqueRef(),
            this->m_nameLabel,
            this->m_sr->OpaqueRef());

        // Poll task to completion
        pollToCompletion(taskRef, 0, 90);

        // Get the GetResult (new VM ref)
        QString newVmRef = GetResult();

        if (newVmRef.isEmpty())
        {
            setError("Failed to get copied VM reference");
            return;
        }

        qDebug() << "VMCopyAction: Copied VM ref:" << newVmRef;

        // Set the description on the new VM
        if (!this->m_description.isEmpty())
        {
            try
            {
                XenAPI::VM::set_name_description(GetSession(), newVmRef, this->m_description);
            } catch (const std::exception& e)
            {
                // Non-fatal - log but continue
                qWarning() << "Failed to set description on copied VM:" << e.what();
            }
        }

        SetDescription("VM copied successfully");

    } catch (const std::exception& e)
    {
        if (IsCancelled())
        {
            SetDescription("Copy cancelled");
        } else
        {
            setError(QString("Failed to copy VM: %1").arg(e.what()));
        }
    }
}
