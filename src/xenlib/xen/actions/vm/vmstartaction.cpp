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

#include "vmstartaction.h"
#include "../../vm.h"
#include "../../session.h"
#include "../../xenapi/xenapi_VM.h"
#include <QtCore/QDebug>

VMStartAction::VMStartAction(QSharedPointer<VM> vm,
                             WarningDialogHAInvalidConfig warningDialogHAInvalidConfig,
                             StartDiagnosisForm startDiagnosisForm,
                             QObject* parent)
    : VMStartAbstractAction(vm,
                            tr("Starting '%1'...").arg(vm ? vm->GetName() : "VM"),
                            warningDialogHAInvalidConfig,
                            startDiagnosisForm,
                            parent)
{
    AddApiMethodToRoleCheck("vm.start");
}

void VMStartAction::run()
{
    SetDescription(tr("Starting..."));
    startOrResumeVmWithHa(0, 100);
    SetDescription(tr("Started"));
}

void VMStartAction::doAction(int start, int end)
{
    // Matches C# VM.async_start(Session, VM.opaque_ref, false, false)
    QSharedPointer<VM> vmObj = GetVM();
    if (!vmObj)
    {
        setError("VM object is null");
        return;
    }

    XenAPI::Session* sess = GetSession();
    if (!sess || !sess->IsLoggedIn())
    {
        setError("Not connected to XenServer");
        return;
    }

    // Call XenAPI::VM static method
    QString taskRef = XenAPI::VM::async_start(sess, vmObj->OpaqueRef(), false, false);

    if (taskRef.isEmpty())
    {
        setError("Failed to start VM - no task returned");
        return;
    }

    SetRelatedTaskRef(taskRef);
    pollToCompletion(taskRef, start, end);
}

VMStartAbstractAction* VMStartAction::clone()
{
    return new VMStartAction(GetVM(), m_warningDialogHAInvalidConfig, m_startDiagnosisForm);
}
