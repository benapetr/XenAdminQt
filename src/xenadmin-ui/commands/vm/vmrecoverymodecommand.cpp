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
#include "../../../xenlib/xen/actions/vm/hvmbootaction.h"
#include "../../../xenlib/xen/connection.h"
#include "../../../xenlib/xen/vm.h"
#include "../../../xenlib/xenlib.h"
#include "../../../xenlib/xencache.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include <QMessageBox>

VMRecoveryModeCommand::VMRecoveryModeCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool VMRecoveryModeCommand::canRun() const
{
    QString vmRef = this->getSelectedObjectRef();
    QString type = this->getSelectedObjectType();

    if (vmRef.isEmpty() || type != "vm")
    {
        return false;
    }

    // Get VM data from cache
    QVariantMap vmData = this->xenLib()->getCache()->ResolveObjectData("vm", vmRef);
    if (vmData.isEmpty())
    {
        return false;
    }

    // VM must be halted
    QString powerState = vmData.value("power_state").toString();
    if (powerState != "Halted")
    {
        return false;
    }

    // Check if is_a_template is false (templates can't be started)
    bool isTemplate = vmData.value("is_a_template").toBool();
    if (isTemplate)
    {
        return false;
    }

    // Check if VM is HVM (has HVM_boot_policy)
    QString bootPolicy = vmData.value("HVM_boot_policy").toString();
    // Empty boot policy means PV VM (paravirtualized), which doesn't support HVM boot
    if (bootPolicy.isEmpty())
    {
        return false;
    }

    return true;
}

void VMRecoveryModeCommand::run()
{
    QString vmRef = this->getSelectedObjectRef();
    QString vmName = this->getSelectedObjectName();

    if (vmRef.isEmpty())
    {
        return;
    }

    // Confirm action with user
    QMessageBox::StandardButton reply = QMessageBox::question(
        this->mainWindow(),
        tr("Boot in Recovery Mode"),
        tr("This will boot '%1' with temporary boot settings (DVD drive, then Network).\n\n"
           "The original boot settings will be restored after the VM starts.\n\n"
           "Do you want to continue?")
            .arg(vmName),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
    {
        return;
    }

    // Get XenConnection from XenLib
    XenConnection* conn = this->xenLib()->getConnection();
    if (!conn || !conn->isConnected())
    {
        QMessageBox::warning(this->mainWindow(), tr("Not Connected"),
                             tr("Not connected to XenServer"));
        return;
    }

    // Create HVMBootAction
    HVMBootAction* action = new HVMBootAction(conn, vmRef, this->mainWindow());

    // Register with OperationManager for history tracking
    OperationManager::instance()->registerOperation(action);

    // Connect completion signal for cleanup and user feedback
    connect(action, &AsyncOperation::completed, this, [=]() {
        if (action->state() == AsyncOperation::Completed)
        {
            this->mainWindow()->showStatusMessage(
                tr("VM '%1' has been started in recovery mode").arg(vmName), 5000);
        } else if (action->state() == AsyncOperation::Failed)
        {
            QMessageBox::critical(this->mainWindow(), tr("Error"),
                                  tr("Failed to boot VM in recovery mode:\n%1").arg(action->errorMessage()));
        }

        // Auto-delete when complete
        action->deleteLater();
    });

    // Run action asynchronously
    action->runAsync();
}

QString VMRecoveryModeCommand::menuText() const
{
    return tr("Boot in &Recovery Mode");
}
