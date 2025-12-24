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

#include "deletevmcommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xen/network/connection.h"
#include "xen/vm.h"
#include "xen/actions/vm/vmdestroyaction.h"
#include <QMessageBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QDialog>
#include <QPushButton>
#include <QDialogButtonBox>

DeleteVMCommand::DeleteVMCommand(MainWindow* mainWindow, QObject* parent)
    : VMCommand(mainWindow, parent)
{
}

bool DeleteVMCommand::CanRun() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    // Only enable if VM can be deleted
    return this->isVMDeletable();
}

void DeleteVMCommand::Run()
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return;

    QString vmName = this->getSelectedVMName();
    if (vmName.isEmpty())
        return;

    // Use VM GetPowerState method
    QString powerState = vm->GetPowerState();

    // Check if VM is in a deletable state
    if (powerState != "Halted")
    {
        QMessageBox::warning(this->mainWindow(), "Delete VM",
                             QString("VM '%1' must be shut down before it can be deleted.\n\n"
                                     "Current state: %2")
                                 .arg(vmName, powerState));
        return;
    }

    // Create custom confirmation dialog following original XenAdmin pattern
    QDialog confirmDialog(this->mainWindow());
    confirmDialog.setWindowTitle("Confirm VM Deletion");
    confirmDialog.setModal(true);
    confirmDialog.resize(400, 200);

    QVBoxLayout* layout = new QVBoxLayout(&confirmDialog);

    // Warning message
    QLabel* warningLabel = new QLabel(QString("Are you sure you want to delete VM '%1'?\n\n"
                                              "This action cannot be undone.")
                                          .arg(vmName));
    warningLabel->setWordWrap(true);
    layout->addWidget(warningLabel);

    // Option to delete disks (following original XenAdmin pattern)
    QCheckBox* deleteDisksCheckbox = new QCheckBox("Delete associated virtual disk files");
    deleteDisksCheckbox->setChecked(true); // Default to checked like original
    deleteDisksCheckbox->setToolTip("Remove the VM's virtual disk files from storage.\n"
                                    "Uncheck to keep disks for potential reuse.");
    layout->addWidget(deleteDisksCheckbox);

    // Buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Yes | QDialogButtonBox::No);
    connect(buttonBox, &QDialogButtonBox::accepted, &confirmDialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &confirmDialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    if (confirmDialog.exec() == QDialog::Accepted)
    {
        bool deleteDisks = deleteDisksCheckbox->isChecked();

        // Get XenConnection from VM
        XenConnection* conn = vm->GetConnection();
        if (!conn || !conn->IsConnected())
        {
            QMessageBox::warning(this->mainWindow(), "Not Connected",
                                 "Not connected to XenServer");
            return;
        }

        // Create VM object for action (action will own and delete it)
        VM* vmForAction = new VM(conn, vm->OpaqueRef());

        // Create VMDestroyAction (matches C# VMDestroyAction pattern)
        // Action handles VDI cleanup, task polling, and error aggregation
        VMDestroyAction* action = new VMDestroyAction(conn, vmForAction, deleteDisks, this->mainWindow());

        // Register with OperationManager for history tracking (matches C# ConnectionsManager.History.Add)
        OperationManager::instance()->registerOperation(action);

        // Connect completion signal for cleanup and status update
        connect(action, &AsyncOperation::completed, this, [this, vmName, action]() {
            if (action->state() == AsyncOperation::Completed && !action->isFailed())
            {
                this->mainWindow()->showStatusMessage(QString("VM '%1' deleted successfully").arg(vmName), 5000);
                // Cache will be automatically refreshed via event polling
            } else
            {
                this->mainWindow()->showStatusMessage(QString("Failed to delete VM '%1'").arg(vmName), 5000);
            }
            // Auto-delete when complete (matches C# GC behavior)
            action->deleteLater();
        });

        // Run action asynchronously (matches C# pattern - no modal dialog)
        // Progress shown in status bar via OperationManager signals
        action->runAsync();
    }
}

QString DeleteVMCommand::MenuText() const
{
    return "Delete VM";
}

bool DeleteVMCommand::isVMDeletable() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    QVariantMap vmData = vm->GetData();

    // Check if it's not a template (templates handled separately)
    bool isTemplate = vmData.value("is_a_template", false).toBool();
    if (isTemplate)
        return false;

    // Check if VM is halted (required for deletion in most cases)
    return (vm->GetPowerState() == "Halted");
}
