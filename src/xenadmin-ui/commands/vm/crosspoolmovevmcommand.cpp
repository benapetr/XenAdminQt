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

#include "crosspoolmovevmcommand.h"
#include "mainwindow.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xencache.h"

CrossPoolMoveVMCommand::CrossPoolMoveVMCommand(MainWindow* mainWindow, QObject* parent)
    : CrossPoolMigrateCommand(mainWindow, CrossPoolMigrateWizard::WizardMode::Move, false, parent)
{
}

bool CrossPoolMoveVMCommand::CanRun() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    // CrossPoolMoveVM only works on halted or suspended VMs (not running)
    if (!CrossPoolMoveVMCommand::CanRunOnVM(vm))
        return false;

    // Use parent's CanRun for additional checks (migrate_send permission, SR checks, etc.)
    return CrossPoolMigrateCommand::CanRun();
}

QString CrossPoolMoveVMCommand::MenuText() const
{
    return tr("Move VM...");
}

void CrossPoolMoveVMCommand::Run()
{
    QList<QSharedPointer<VM>> vms;
    QStringList selection = this->GetSelection();
    QSharedPointer<VM> baseVm = this->getVM();
    if (selection.isEmpty())
    {
        if (baseVm)
            vms.append(baseVm);
    } else
    {
        XenConnection* conn = baseVm ? baseVm->GetConnection() : nullptr;
        XenCache* cache = conn ? conn->GetCache() : nullptr;
        if (cache)
        {
            for (const QString& ref : selection)
            {
                QSharedPointer<VM> vm = cache->ResolveObject<VM>(XenObjectType::VM, ref);
                if (vm)
                    vms.append(vm);
            }
        }
    }

    if (vms.isEmpty())
        return;

    CrossPoolMigrateWizard::WizardMode mode = GetWizardMode(baseVm);
    CrossPoolMigrateWizard wizard(MainWindow::instance(), vms, mode);
    wizard.exec();
}

bool CrossPoolMoveVMCommand::CanRunOnVM(QSharedPointer<VM> vm)
{
    if (!vm)
        return false;

    // Can't move templates
    if (vm->IsTemplate())
        return false;

    // Can't move locked VMs
    if (vm->IsLocked())
        return false;

    // Can't move running VMs (must be halted or suspended)
    QString powerState = vm->GetPowerState();
    if (powerState == "Running")
        return false;

    return true;
}

CrossPoolMigrateWizard::WizardMode CrossPoolMoveVMCommand::GetWizardMode(QSharedPointer<VM> vm)
{
    if (!vm)
        return CrossPoolMigrateWizard::WizardMode::Move;

    // If VM is suspended, use Migrate mode; otherwise use Move mode
    QString powerState = vm->GetPowerState();
    if (powerState == "Suspended")
        return CrossPoolMigrateWizard::WizardMode::Migrate;

    return CrossPoolMigrateWizard::WizardMode::Move;
}
