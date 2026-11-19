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

#include "instantvmfromtemplatecommand.h"
#include "../../mainwindow.h"
#include "../vm/vmoperationhelpers.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/actions/vm/createvmfastaction.h"
#include "xenlib/xen/actions/vm/vmstartaction.h"
#include "xenlib/xencache.h"
#include <QPointer>

InstantVMFromTemplateCommand::InstantVMFromTemplateCommand(MainWindow* mainWindow, QObject* parent) : TemplateCommand(mainWindow, parent)
{
}

bool InstantVMFromTemplateCommand::CanRun() const
{
    return this->canRunTemplate(this->getTemplate());
}

void InstantVMFromTemplateCommand::Run()
{
    // C# InstantVMFromTemplateCommand.RunCore: create the VM quickly, then start it
    QSharedPointer<VM> templateVm = this->getTemplate();
    if (!this->canRunTemplate(templateVm))
        return;

    XenConnection* connection = templateVm->GetConnection();
    MainWindow* mainWindow = MainWindow::instance();

    CreateVMFastAction* action = new CreateVMFastAction(connection, templateVm, mainWindow);
    QPointer<CreateVMFastAction> actionPtr = action;
    QPointer<MainWindow> windowPtr = mainWindow;

    // C# createAction_Completed: start the new VM, with the HA warning and boot diagnosis
    // like Start VM (VMOperationCommand.WarningDialogHAInvalidConfig / StartDiagnosisForm).
    // completed() is only emitted on success.
    connect(action, &AsyncOperation::completed, mainWindow, [actionPtr, windowPtr, connection]()
    {
        if (!actionPtr || !windowPtr || !connection || !connection->GetCache())
            return;

        QSharedPointer<VM> vm = connection->GetCache()->ResolveObject<VM>(actionPtr->GetResult());
        if (vm && vm->IsValid())
            VMOperationHelpers::CreateVMStartAction(windowPtr, vm)->RunAsync(true);
    });

    action->RunAsync(true);
}

QString InstantVMFromTemplateCommand::MenuText() const
{
    // C# Messages.MAINWINDOW_INSTANT_VM_FROM_TEMPLATE
    return tr("&Quick Create");
}

bool InstantVMFromTemplateCommand::canRunTemplate(const QSharedPointer<VM>& templateVm) const
{
    if (!TemplateCommand::canRunTemplate(templateVm))
        return false;

    // C# CanRun: is_a_template && !Locked && !is_a_snapshot (checked by TemplateCommand)
    // && InstantTemplate()
    if (!templateVm->IsInstantTemplate())
        return false;

    return true;
}
