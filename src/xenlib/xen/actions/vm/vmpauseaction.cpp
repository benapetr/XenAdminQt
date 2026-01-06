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

#include "vmpauseaction.h"
#include "../../vm.h"
#include "../../host.h"
#include "../../pool.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_VM.h"
#include "xencache.h"
#include <QtCore/QDebug>

// VMPauseAction implementation

VMPauseAction::VMPauseAction(QSharedPointer<VM> vm, const QString& title, QObject* parent)
    : AsyncOperation(vm ? vm->GetConnection() : nullptr, title, tr("Preparing..."), parent)
{
    this->SetVM(vm);

    if (vm)
    {
        // Set host to VM's home
        this->SetHost(vm->GetHost());

        // Set pool
        XenConnection* conn = vm->GetConnection();
        if (conn)
        {
            this->SetPool(conn->GetCache()->GetPool());
        }
    }
}

VMPauseAction::~VMPauseAction()
{
}

// VMPause implementation

VMPause::VMPause(QSharedPointer<VM> vm, QObject* parent)
    : VMPauseAction(vm,
                    tr("Pausing '%1'...").arg(vm ? vm->GetName() : "VM"),
                    parent)
{
    AddApiMethodToRoleCheck("VM.async_pause");
}

void VMPause::run()
{
    SetDescription(tr("Pausing..."));

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
    QString taskRef = XenAPI::VM::async_pause(sess, vmObj->OpaqueRef());

    if (taskRef.isEmpty())
    {
        setError("Failed to pause VM - no task returned");
        return;
    }

    SetRelatedTaskRef(taskRef);
    pollToCompletion(taskRef, 0, 100);

    SetDescription(tr("Paused"));
}

// VMUnpause implementation

VMUnpause::VMUnpause(QSharedPointer<VM> vm, QObject* parent)
    : VMPauseAction(vm,
                    tr("Unpausing '%1'...").arg(vm ? vm->GetName() : "VM"),
                    parent)
{
    AddApiMethodToRoleCheck("VM.async_unpause");
}

void VMUnpause::run()
{
    SetDescription(tr("Unpausing..."));

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
    QString taskRef = XenAPI::VM::async_unpause(sess, vmObj->OpaqueRef());

    if (taskRef.isEmpty())
    {
        setError("Failed to unpause VM - no task returned");
        return;
    }

    SetRelatedTaskRef(taskRef);
    pollToCompletion(taskRef, 0, 100);

    SetDescription(tr("Unpaused"));
}
