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

#include "clonevmcommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/actions/vm/vmcloneaction.h"
#include "xenlib/xencache.h"
#include <QMessageBox>
#include <QInputDialog>

CloneVMCommand::CloneVMCommand(MainWindow* mainWindow, QObject* parent) : VMCommand(mainWindow, parent)
{
}

bool CloneVMCommand::CanRun() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    // Only enable if VM can be cloned (halted and not a template)
    return this->isVMCloneable();
}

void CloneVMCommand::Run()
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return;

    QString vmRef = vm->OpaqueRef();
    QString vmName = this->getSelectedVMName();

    if (vmRef.isEmpty() || vmName.isEmpty())
        return;

    // Use cache instead of async API call
    QString powerState = vm->GetPowerState();

    // Check if VM is in a cloneable state
    if (powerState != "Halted")
    {
        QMessageBox::warning(this->mainWindow(), "Clone VM",
                             QString("VM '%1' must be shut down before it can be cloned.\n\n"
                                     "Current state: %2")
                                 .arg(vmName, powerState));
        return;
    }

    // Get new name for the clone
    bool ok;
    QString cloneName = QInputDialog::getText(this->mainWindow(), "Clone VM",
                                              QString("Enter a name for the cloned VM:"),
                                              QLineEdit::Normal,
                                              QString("Copy of %1").arg(vmName), &ok);

    if (!ok || cloneName.isEmpty())
        return;

    // Show confirmation dialog
    int ret = QMessageBox::question(this->mainWindow(), "Clone VM",
                                    QString("Are you sure you want to clone VM '%1' as '%2'?\n\n"
                                            "This will create a full copy of the VM including all disks.")
                                        .arg(vmName, cloneName),
                                    QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes)
    {
        // Get XenConnection from XenLib
        XenConnection* conn = vm->GetConnection();
        if (!conn || !conn->IsConnected())
        {
            QMessageBox::warning(this->mainWindow(), "Not Connected",
                                 "Not connected to XenServer");
            return;
        }

        // Create VMCloneAction (matches C# VMCloneAction pattern)
        VMCloneAction* action = new VMCloneAction(vm, cloneName, "", this->mainWindow());

        // Register with OperationManager for history tracking (matches C# ConnectionsManager.History.Add)
        OperationManager::instance()->RegisterOperation(action);

        // Connect completion signal for cleanup and status update
        connect(action, &AsyncOperation::completed, action, [this, vmName, cloneName, action]()
        {
            if (action->GetState() == AsyncOperation::Completed && !action->IsFailed())
            {
                this->mainWindow()->ShowStatusMessage(QString("VM '%1' cloned successfully as '%2'").arg(vmName, cloneName), 5000);
                // Cache will be automatically refreshed via event polling
            } else
            {
                this->mainWindow()->ShowStatusMessage(QString("Failed to clone VM '%1'").arg(vmName), 5000);
            }
            // Auto-delete when complete (matches C# GC behavior)
            action->deleteLater();
        });

        // Run action asynchronously (matches C# pattern - no modal dialog)
        // Progress shown in status bar via OperationManager signals
        action->RunAsync();
    }
}

QString CloneVMCommand::MenuText() const
{
    return "Clone VM";
}

bool CloneVMCommand::isVMCloneable() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    // Check if it's not a template
    if (vm->IsTemplate())
        return false;

    // Check if VM is halted (required for clone)
    return (vm->GetPowerState() == "Halted");
}
