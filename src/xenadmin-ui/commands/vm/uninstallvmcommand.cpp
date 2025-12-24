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

#include "uninstallvmcommand.h"
#include "../../mainwindow.h"
#include "xen/vm.h"
#include <QMessageBox>

UninstallVMCommand::UninstallVMCommand(MainWindow* mainWindow, QObject* parent)
    : VMCommand(mainWindow, parent)
{
}

bool UninstallVMCommand::CanRun() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    return this->canVMBeUninstalled();
}

void UninstallVMCommand::Run()
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return;

    QString vmName = vm->GetName();

    // Show warning dialog
    int ret = QMessageBox::warning(this->mainWindow(), "Uninstall VM",
                                   QString("Are you sure you want to uninstall VM '%1'?\n\n"
                                           "This will PERMANENTLY DELETE the VM and ALL its virtual disks.\n\n"
                                           "This action CANNOT be undone!")
                                       .arg(vmName),
                                   QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes)
    {
        this->mainWindow()->showStatusMessage(QString("Uninstalling VM '%1'...").arg(vmName));

        // TODO: Delete the VM and its VDIs using XenAPI
        // bool success = XenAPI::VM::destroy(session, vmRef);
        QMessageBox::information(this->mainWindow(), "Not Implemented",
                                 "VM deletion will be implemented using XenAPI::VM::destroy.");
    }
}

QString UninstallVMCommand::MenuText() const
{
    return "Uninstall VM";
}

bool UninstallVMCommand::canVMBeUninstalled() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    QVariantMap vmData = vm->GetData();

    // Cannot uninstall templates or snapshots
    if (vmData.value("is_a_template", false).toBool())
        return false;
    if (vmData.value("is_a_snapshot", false).toBool())
        return false;

    // Cannot uninstall if VM is running
    if (vm->GetPowerState() == "Running")
        return false;

    // Check if VM has any current operations
    QVariantMap currentOps = vmData.value("current_operations", QVariantMap()).toMap();
    if (!currentOps.isEmpty())
        return false;

    return true;
}
