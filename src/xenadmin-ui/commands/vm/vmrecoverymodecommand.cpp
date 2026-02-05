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

#include "vmrecoverymodecommand.h"
#include "xenlib/xen/actions/vm/hvmbootaction.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/vm.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"

VMRecoveryModeCommand::VMRecoveryModeCommand(MainWindow* mainWindow, QObject* parent)
    : VMCommand(mainWindow, parent)
{
}

bool VMRecoveryModeCommand::CanRun() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    // Check if is_a_template is false (templates can't be started)
    if (vm->IsTemplate())
        return false;

    // C# allows recovery mode when start is allowed
    return vm->GetAllowedOperations().contains("start");
}

void VMRecoveryModeCommand::Run()
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return;

    // Create HVMBootAction
    HVMBootAction* action = new HVMBootAction(vm, MainWindow::instance());

    // Register with OperationManager for history tracking
    OperationManager::instance()->RegisterOperation(action);

    // Run action asynchronously
    action->RunAsync();
}

QString VMRecoveryModeCommand::MenuText() const
{
    return tr("Boot in &Recovery Mode");
}
