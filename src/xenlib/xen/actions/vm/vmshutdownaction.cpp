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
#include "../../../xencache.h"
#include "../../session.h"
#include "../../xenapi/xenapi_VM.h"
#include <QtCore/QDebug>


namespace
{
    // C#: vm.Home() == null ? Helpers.GetName(vm.Connection) : vm.Home().Name()
    QString homeOrConnectionName(const QSharedPointer<VM>& vm)
    {
        if (!vm)
            return QString();
        QSharedPointer<Host> home = vm->GetHome();
        if (home)
            return home->GetName();
        XenConnection* connection = vm->GetConnection();
        return connection ? connection->GetName() : QString();
    }
} // namespace

// VMShutdownAction implementation

VMShutdownAction::VMShutdownAction(QSharedPointer<VM> vm, const QString& title, QObject* parent)
    : AsyncOperation(vm ? vm->GetConnection() : nullptr, title, tr("Preparing..."), parent)
{
    SetVM(vm);

    if (vm)
    {
        // C#: Host = vm.Home(); Pool = Helpers.GetPool(vm.Connection);
        this->SetHost(vm->GetHome());

        XenConnection* conn = vm->GetConnection();
        XenCache* cache = conn ? conn->GetCache() : nullptr;
        if (cache)
            this->SetPool(cache->GetPool());
    }
}

VMShutdownAction::~VMShutdownAction()
{
}

// VMCleanShutdown implementation

VMCleanShutdown::VMCleanShutdown(QSharedPointer<VM> vm, QObject* parent)
    : VMShutdownAction(vm,
                       tr("Shutting down VM '%1' on '%2'").arg(vm ? vm->GetName() : QString(), homeOrConnectionName(vm)),
                       parent)
{
    AddApiMethodToRoleCheck("VM.async_clean_shutdown");
}

void VMCleanShutdown::run()
{
    SetDescription(tr("Shutting down VM"));

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
    QString taskRef = XenAPI::VM::async_clean_shutdown(sess, vmObj->OpaqueRef());

    if (taskRef.isEmpty())
    {
        setError("Failed to shutdown VM - no task returned");
        return;
    }

    SetRelatedTaskRef(taskRef);
    pollToCompletion(taskRef, 0, 100);

    SetDescription(tr("Shut down"));
}

// VMHardShutdown implementation

VMHardShutdown::VMHardShutdown(QSharedPointer<VM> vm, QObject* parent)
    : VMShutdownAction(vm,
                       tr("Shutting down VM '%1' on '%2'").arg(vm ? vm->GetName() : QString(), homeOrConnectionName(vm)),
                       parent)
{
    AddApiMethodToRoleCheck("VM.async_hard_shutdown");
}

void VMHardShutdown::run()
{
    SetDescription(tr("Shutting down VM"));

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
    QString taskRef = XenAPI::VM::async_hard_shutdown(sess, vmObj->OpaqueRef());

    if (taskRef.isEmpty())
    {
        setError("Failed to shutdown VM - no task returned");
        return;
    }

    SetRelatedTaskRef(taskRef);
    pollToCompletion(taskRef, 0, 100);

    SetDescription(tr("Shut down"));
}

// VMSuspendAction implementation

VMSuspendAction::VMSuspendAction(QSharedPointer<VM> vm, QObject* parent)
    : VMShutdownAction(vm,
                       tr("Suspending VM '%1'").arg(vm ? vm->GetName() : QString()),
                       parent)
{
    AddApiMethodToRoleCheck("VM.async_suspend");
}

void VMSuspendAction::run()
{
    SetDescription(tr("Suspending VM"));

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
    QString taskRef = XenAPI::VM::async_suspend(sess, vmObj->OpaqueRef());

    if (taskRef.isEmpty())
    {
        setError("Failed to suspend VM - no task returned");
        return;
    }

    SetRelatedTaskRef(taskRef);
    pollToCompletion(taskRef, 0, 100);

    SetDescription(tr("Suspended"));
}
