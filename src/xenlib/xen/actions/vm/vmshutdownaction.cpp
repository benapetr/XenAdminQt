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

#include "vmshutdownaction.h"
#include "../../vm.h"
#include "../../host.h"
#include "../../pool.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_VM.h"
#include <QtCore/QDebug>

// VMShutdownAction implementation

VMShutdownAction::VMShutdownAction(VM* vm, const QString& title, QObject* parent)
    : AsyncOperation(vm ? vm->GetConnection() : nullptr, title, tr("Preparing..."), parent)
{
    setVM(vm);

    if (vm)
    {
        // Set host to VM's home
        // TODO: Implement VM::home() method
        // setHost(vm->home());

        // Set pool
        XenConnection* conn = vm->GetConnection();
        if (conn)
        {
            // TODO: Get pool from connection
            // setPool(conn->pool());
        }
    }
}

VMShutdownAction::~VMShutdownAction()
{
}

// VMCleanShutdown implementation

VMCleanShutdown::VMCleanShutdown(VM* vm, QObject* parent)
    : VMShutdownAction(vm,
                       tr("Shutting down '%1'...").arg(vm ? vm->GetName() : "VM"),
                       parent)
{
    addApiMethodToRoleCheck("VM.async_clean_shutdown");
}

void VMCleanShutdown::run()
{
    setDescription(tr("Shutting down..."));

    VM* vmObj = vm();
    if (!vmObj)
    {
        setError("VM object is null");
        return;
    }

    XenSession* sess = session();
    if (!sess || !sess->isLoggedIn())
    {
        setError("Not connected to XenServer");
        return;
    }

    // Call XenAPI::VM static method
    QString taskRef = XenAPI::VM::async_clean_shutdown(sess, vmObj->OpaqueRef());

    if (taskRef.isEmpty())
    {
        setError("Failed to shutdown VM - no task returned");
        return;
    }

    setRelatedTaskRef(taskRef);
    pollToCompletion(taskRef, 0, 100);

    setDescription(tr("Shut down"));
}

// VMHardShutdown implementation

VMHardShutdown::VMHardShutdown(VM* vm, QObject* parent)
    : VMShutdownAction(vm,
                       tr("Shutting down '%1'...").arg(vm ? vm->GetName() : "VM"),
                       parent)
{
    addApiMethodToRoleCheck("VM.async_hard_shutdown");
}

void VMHardShutdown::run()
{
    setDescription(tr("Shutting down..."));

    VM* vmObj = vm();
    if (!vmObj)
    {
        setError("VM object is null");
        return;
    }

    XenSession* sess = session();
    if (!sess || !sess->isLoggedIn())
    {
        setError("Not connected to XenServer");
        return;
    }

    // Call XenAPI::VM static method
    QString taskRef = XenAPI::VM::async_hard_shutdown(sess, vmObj->OpaqueRef());

    if (taskRef.isEmpty())
    {
        setError("Failed to shutdown VM - no task returned");
        return;
    }

    setRelatedTaskRef(taskRef);
    pollToCompletion(taskRef, 0, 100);

    setDescription(tr("Shut down"));
}

// VMSuspendAction implementation

VMSuspendAction::VMSuspendAction(VM* vm, QObject* parent)
    : VMShutdownAction(vm,
                       tr("Suspending '%1'...").arg(vm ? vm->GetName() : "VM"),
                       parent)
{
    addApiMethodToRoleCheck("VM.async_suspend");
}

void VMSuspendAction::run()
{
    setDescription(tr("Suspending..."));

    VM* vmObj = vm();
    if (!vmObj)
    {
        setError("VM object is null");
        return;
    }

    XenSession* sess = session();
    if (!sess || !sess->isLoggedIn())
    {
        setError("Not connected to XenServer");
        return;
    }

    // Call XenAPI::VM static method
    QString taskRef = XenAPI::VM::async_suspend(sess, vmObj->OpaqueRef());

    if (taskRef.isEmpty())
    {
        setError("Failed to suspend VM - no task returned");
        return;
    }

    setRelatedTaskRef(taskRef);
    pollToCompletion(taskRef, 0, 100);

    setDescription(tr("Suspended"));
}
