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

#include "movevmcommand.h"
#include "../../mainwindow.h"
#include "../../dialogs/crosspoolmigratewizard.h"
#include "../../dialogs/movevmdialog.h"
#include "../vm/crosspoolmovevmcommand.h"
#include "xen/vbd.h"
#include "xen/vdi.h"
#include "xenlib/xen/vm.h"

MoveVMCommand::MoveVMCommand(MainWindow* mainWindow, QObject* parent) : VMCommand(mainWindow, parent)
{
}

bool MoveVMCommand::CanRun() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    if (!this->isCBTDisabled())
        return false;

    if (this->canLaunchCrossPoolWizard())
        return true;

    return vm->CanBeMoved();
}

void MoveVMCommand::Run()
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return;

    if (this->canLaunchCrossPoolWizard())
    {
        CrossPoolMigrateWizard::WizardMode mode = CrossPoolMoveVMCommand::GetWizardMode(vm);
        CrossPoolMigrateWizard wizard(this->mainWindow(), vm, mode);
        wizard.exec();
        return;
    }

    MoveVMDialog dialog(vm, this->mainWindow());
    dialog.exec();
}

QString MoveVMCommand::MenuText() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (this->canLaunchCrossPoolWizard() && CrossPoolMoveVMCommand::GetWizardMode(vm) == CrossPoolMigrateWizard::WizardMode::Migrate)
        return "Migrate VM...";

    return "Move VM...";
}

bool MoveVMCommand::isCBTDisabled() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    QList<QSharedPointer<VBD>> vbds = vm->GetVBDs();
    for (const QSharedPointer<VBD>& vbd : vbds)
    {
        QSharedPointer<VDI> vdi = vbd ? vbd->GetVDI() : QSharedPointer<VDI>();
        if (vdi && vdi->IsCBTEnabled())
            return false;
    }

    return true;
}

bool MoveVMCommand::canLaunchCrossPoolWizard() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    CrossPoolMoveVMCommand crossPoolCmd(this->mainWindow(), this->mainWindow());
    return crossPoolCmd.CanRun();
}
