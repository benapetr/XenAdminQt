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

#include "resumevmcommand.h"
#include "vmoperationhelpers.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib.h"
#include "xen/connection.h"
#include "xen/vm.h"
#include "xen/actions/vm/vmresumeaction.h"
#include "xen/failure.h"
#include "xencache.h"
#include <QMessageBox>
#include <QTimer>
#include <QMetaObject>
#include <QPointer>

ResumeVMCommand::ResumeVMCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool ResumeVMCommand::canRun() const
{
    QString vmRef = this->getSelectedVMRef();
    if (vmRef.isEmpty())
        return false;

    // Check if VM is suspended AND resume is allowed (matches C# ResumeVMCommand.CanRun)
    if (!this->isVMSuspended(vmRef))
        return false;

    QVariantMap vmData = this->mainWindow()->xenLib()->getCache()->ResolveObjectData("vm", vmRef);
    QVariantList allowedOperations = vmData.value("allowed_operations").toList();

    return allowedOperations.contains("resume");
}

void ResumeVMCommand::run()
{
    QString vmRef = this->getSelectedVMRef();
    QString vmName = this->getSelectedVMName();

    if (vmRef.isEmpty())
        return;

    runForVm(vmRef, vmName, true);
}

bool ResumeVMCommand::runForVm(const QString& vmRef, const QString& vmName, bool promptUser)
{
    if (vmRef.isEmpty())
        return false;

    QString displayName = vmName;
    if (displayName.isEmpty())
    {
        QVariantMap vmData = this->mainWindow()->xenLib()->getCache()->ResolveObjectData("vm", vmRef);
        displayName = vmData.value("name_label").toString();
    }

    if (promptUser)
    {
        int ret = QMessageBox::question(this->mainWindow(), tr("Resume VM"),
                                        tr("Are you sure you want to resume VM '%1'?").arg(displayName),
                                        QMessageBox::Yes | QMessageBox::No);

        if (ret != QMessageBox::Yes)
            return false;
    }

    // Get XenConnection from XenLib
    XenConnection* conn = this->mainWindow()->xenLib()->getConnection();
    if (!conn || !conn->isConnected())
    {
        QMessageBox::warning(this->mainWindow(), "Not Connected",
                             "Not connected to XenServer");
        return false;
    }

    // Create VM object (lightweight wrapper)
    VM* vm = new VM(conn, vmRef);

    // Create VMResumeAction with diagnosis callbacks (matches C# pattern)
    // Note: VMResumeAction is for resuming from Suspended state (from disk)
    // For unpausing from Paused state (in memory), use VMUnpause instead
    QPointer<MainWindow> mainWindow = this->mainWindow();

    VMResumeAction* action = new VMResumeAction(
        vm,
        nullptr,  // WarningDialogHAInvalidConfig callback (TODO: implement if needed)
        [conn, vmRef, displayName, mainWindow](VMStartAbstractAction* abstractAction, const Failure& failure) {
            Q_UNUSED(abstractAction)
            if (!mainWindow)
                return;

            Failure failureCopy = failure;
            QMetaObject::invokeMethod(mainWindow, [mainWindow, conn, vmRef, displayName, failureCopy]() {
                if (!mainWindow)
                    return;

                XenLib* lib = mainWindow->xenLib();
                if (!lib)
                    return;

                VMOperationHelpers::startDiagnosisForm(lib,
                                                       conn, vmRef, displayName,
                                                       false,
                                                       failureCopy,
                                                       mainWindow);
            }, Qt::QueuedConnection);
        },
        this->mainWindow());

    // Register with OperationManager for history tracking
    OperationManager::instance()->registerOperation(action);

    // Connect completion signal for cleanup
    connect(action, &AsyncOperation::completed, this, [action]() {
        // Auto-delete when complete
        action->deleteLater();
    });

    // Run action asynchronously
    action->runAsync();
    return true;
}

QString ResumeVMCommand::menuText() const
{
    return "Resume VM";
}

QString ResumeVMCommand::getSelectedVMRef() const
{
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    QString objectType = this->getSelectedObjectType();
    if (objectType != "vm")
        return QString();

    return this->getSelectedObjectRef();
}

QString ResumeVMCommand::getSelectedVMName() const
{
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    QString objectType = this->getSelectedObjectType();
    if (objectType != "vm")
        return QString();

    return item->text(0);
}

bool ResumeVMCommand::isVMSuspended(const QString& vmRef) const
{
    // Use cache instead of async API call
    QVariantMap vmData = this->mainWindow()->xenLib()->getCache()->ResolveObjectData("vm", vmRef);
    QString powerState = vmData.value("power_state", "Halted").toString();

    return (powerState == "Suspended");
}
