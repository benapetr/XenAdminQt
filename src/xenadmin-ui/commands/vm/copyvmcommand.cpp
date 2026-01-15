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
#include "xenlib/xen/vm.h"
#include "xenlib/xencache.h"
#include <QMessageBox>

CopyVMCommand::CopyVMCommand(MainWindow* mainWindow, QObject* parent)
    : VMCommand(mainWindow, parent)
{
}

bool CopyVMCommand::CanRun() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    // Can copy if VM is not locked and copy/clone operation is allowed
    return !this->isVMLocked() && this->canVMBeCopied();
}

void CopyVMCommand::Run()
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return;

    CrossPoolMigrateWizard wizard(this->mainWindow(), vm, CrossPoolMigrateWizard::WizardMode::Copy);
    wizard.exec();
}

QString CopyVMCommand::MenuText() const
{
    return "Copy VM to Shared Storage...";
}

bool CopyVMCommand::isVMLocked() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return true;

    // Check if VM has current operations that would lock it
    QVariantMap currentOps = vm->GetData().value("current_operations", QVariantMap()).toMap();
    return !currentOps.isEmpty();
}

bool CopyVMCommand::canVMBeCopied() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm || !vm->GetConnection())
        return false;

    XenCache* cache = vm->GetConnection()->GetCache();

    QString vmRef = vm->OpaqueRef();
    QVariantMap vmData = cache->ResolveObjectData("vm", vmRef);

    // Check if VM is a template or snapshot
    if (vmData.value("is_a_template", false).toBool())
        return false;
    if (vmData.value("is_a_snapshot", false).toBool())
        return false;

    // Check if clone operation is allowed
    QVariantList allowedOps = vmData.value("allowed_operations", QVariantList()).toList();
    return allowedOps.contains("clone") || allowedOps.contains("copy");
}
