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

#include "copyvmcommand.h"
#include "../../mainwindow.h"
#include "../../dialogs/crosspoolmigratewizard.h"
#include "../../dialogs/copyvmdialog.h"
#include "../vm/crosspoolmigratecommand.h"
#include "xenlib/xen/vm.h"
#include <QMessageBox>

CopyVMCommand::CopyVMCommand(MainWindow* mainWindow, QObject* parent) : VMCommand(mainWindow, parent)
{
}

bool CopyVMCommand::CanRun() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    if (vm->IsTemplate() || vm->IsSnapshot() || !vm->CurrentOperations().isEmpty())
        return false;

    if (this->canLaunchCrossPoolWizard())
        return true;

    const QStringList allowedOps = vm->GetAllowedOperations();
    if (!allowedOps.contains("export"))
        return false;

    return vm->GetPowerState() != "Suspended";
}

void CopyVMCommand::Run()
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return;

    if (this->canLaunchCrossPoolWizard())
    {
        CrossPoolMigrateWizard wizard(this->mainWindow(), vm, CrossPoolMigrateWizard::WizardMode::Copy);
        wizard.exec();
        return;
    }

    CopyVMDialog dialog(vm, this->mainWindow());
    dialog.exec();
}

QString CopyVMCommand::MenuText() const
{
    return "Copy VM...";
}

bool CopyVMCommand::canLaunchCrossPoolWizard() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    if (vm->GetPowerState() != "Halted")
        return false;

    CrossPoolMigrateCommand crossPoolCmd(this->mainWindow(), CrossPoolMigrateWizard::WizardMode::Copy, false, this->mainWindow());
    return crossPoolCmd.CanRun();
}
