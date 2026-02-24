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

#include "setvmotherconfigaction.h"
#include "../../vm.h"
#include "../../xenapi/xenapi_VM.h"

SetVMOtherConfigAction::SetVMOtherConfigAction(QSharedPointer<VM> vm,
                                               const QString& key,
                                               const QString& value,
                                               QObject* parent)
    : AsyncOperation(vm ? vm->GetConnection() : nullptr,
                     QObject::tr("Updating VM configuration"),
                     QObject::tr("Updating VM configuration..."),
                     parent),
      m_vm(vm),
      m_key(key),
      m_value(value)
{
    this->AddApiMethodToRoleCheck("VM.set_other_config");
}

void SetVMOtherConfigAction::run()
{
    if (!m_vm || !m_vm->IsValid())
    {
        setError("Invalid VM object");
        return;
    }

    try
    {
        QVariantMap otherConfig = m_vm->GetOtherConfig();
        otherConfig.insert(m_key, m_value);
        XenAPI::VM::set_other_config(GetSession(), m_vm->OpaqueRef(), otherConfig);
        SetDescription(QObject::tr("VM configuration updated"));
    }
    catch (const std::exception& e)
    {
        setError(QString("Failed to update VM configuration: %1").arg(e.what()));
    }
}
