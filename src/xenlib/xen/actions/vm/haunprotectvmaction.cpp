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

#include "haunprotectvmaction.h"
#include "../../vm.h"
#include "../../session.h"
#include "../../xenapi/xenapi_VM.h"
#include <stdexcept>

HAUnprotectVMAction::HAUnprotectVMAction(QSharedPointer<VM> vm, bool suppressHistory, QObject* parent)
    : AsyncOperation(vm ? vm->GetConnection() : nullptr,
                     tr("Setting HA restart priority on '%1' to 'Do not restart'").arg(vm ? vm->GetName() : QString()),
                     tr("Setting HA restart priority"), suppressHistory, parent)
{
    this->SetVM(vm);
    this->AddApiMethodToRoleCheck("VM.set_ha_restart_priority");
}

void HAUnprotectVMAction::run()
{
    const auto vm = this->GetVM();
    if (!vm || !vm->IsValid())
        throw std::runtime_error("Invalid VM object");
    XenAPI::Session* session = this->GetSession();
    if (!session || !session->IsLoggedIn())
        throw std::runtime_error("No valid session");

    // C# VM.RESTART_PRIORITY_DO_NOT_RESTART is the empty string.
    XenAPI::VM::set_ha_restart_priority(session, vm->OpaqueRef(), QString());
    this->SetDescription(tr("Completed"));
}
