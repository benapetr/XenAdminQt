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

#include "convertvmtotemplatecommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/actions/vm/vmtotemplateaction.h"
#include <QMessageBox>

ConvertVMToTemplateCommand::ConvertVMToTemplateCommand(MainWindow* mainWindow, QObject* parent) : VMCommand(mainWindow, parent)
{
}

ConvertVMToTemplateCommand::ConvertVMToTemplateCommand(const QList<QSharedPointer<VM>>& selectedVms, MainWindow* mainWindow, QObject* parent) : VMCommand(selectedVms, mainWindow, parent)
{
}

bool ConvertVMToTemplateCommand::CanRun() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    return this->canConvertToTemplate();
}

void ConvertVMToTemplateCommand::Run()
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return;

    QString vmName = this->getSelectedVMName();
    if (vmName.isEmpty())
        return;

    // Show confirmation dialog
    QMessageBox::StandardButton reply = QMessageBox::question(
        this->mainWindow(),
        tr("Convert to Template"),
        tr("Are you sure you want to convert VM '%1' to a template?\n\n"
           "The VM will be shut down and converted to a template. "
           "Templates cannot be started directly but can be used to create new VMs.")
            .arg(vmName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        // Get XenConnection from VM
        XenConnection* conn = vm->GetConnection();
        if (!conn || !conn->IsConnected())
        {
            QMessageBox::warning(this->mainWindow(), "Not Connected",
                                 "Not connected to XenServer");
            return;
        }

        // Create VMToTemplateAction (matches C# VMToTemplateAction pattern)
        // Action automatically sets other_config["instant"] = "true"
        VMToTemplateAction* action = new VMToTemplateAction(vm, this->mainWindow());

        // Register with OperationManager for history tracking (matches C# ConnectionsManager.History.Add)
        OperationManager::instance()->RegisterOperation(action);

        // Connect completion signal for cleanup and status update
        connect(action, &AsyncOperation::completed, this, [this, vmName, action]() {
            if (action->GetState() == AsyncOperation::Completed && !action->IsFailed())
            {
                this->mainWindow()->ShowStatusMessage(QString("VM '%1' converted to template successfully").arg(vmName), 5000);
                QMessageBox::information(this->mainWindow(), tr("Conversion Complete"),
                                         tr("VM '%1' has been successfully converted to a template.").arg(vmName));
                // Cache will be automatically refreshed via event polling
            } else
            {
                this->mainWindow()->ShowStatusMessage(QString("Failed to convert VM '%1'").arg(vmName), 5000);
            }
            // Auto-delete when complete (matches C# GC behavior)
            action->deleteLater();
        });

        // Run action asynchronously (matches C# pattern - no modal dialog)
        // Progress shown in status bar via OperationManager signals
        action->RunAsync();
    }
}

QString ConvertVMToTemplateCommand::MenuText() const
{
    return "Convert to Template";
}

bool ConvertVMToTemplateCommand::canConvertToTemplate() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    // Check if it's already a template
    if (vm->IsTemplate())
        return false;

    // Check if it's a snapshot
    if (vm->IsSnapshot())
        return false;

    // Check power state - must be halted
    if (vm->GetPowerState() != "Halted")
        return false;

    // Check if the operation is allowed
    return vm->GetAllowedOperations().contains("make_into_template");
}
