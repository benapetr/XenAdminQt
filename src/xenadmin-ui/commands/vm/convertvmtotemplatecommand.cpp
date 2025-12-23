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
#include "xenlib.h"
#include "xen/api.h"
#include "xen/network/connection.h"
#include "xen/vm.h"
#include "xen/actions/vm/vmtotemplateaction.h"
#include "xencache.h"
#include <QMessageBox>

ConvertVMToTemplateCommand::ConvertVMToTemplateCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool ConvertVMToTemplateCommand::canRun() const
{
    QString vmRef = this->getSelectedVMRef();
    if (vmRef.isEmpty())
        return false;

    return this->canConvertToTemplate(vmRef);
}

void ConvertVMToTemplateCommand::run()
{
    QString vmRef = this->getSelectedVMRef();
    QString vmName = this->getSelectedVMName();

    if (vmRef.isEmpty() || vmName.isEmpty())
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
        // Get XenConnection from XenLib
        XenConnection* conn = this->mainWindow()->xenLib()->getConnection();
        if (!conn || !conn->isConnected())
        {
            QMessageBox::warning(this->mainWindow(), "Not Connected",
                                 "Not connected to XenServer");
            return;
        }

        // Create VM object (lightweight wrapper)
        VM* vm = new VM(conn, vmRef);

        // Create VMToTemplateAction (matches C# VMToTemplateAction pattern)
        // Action automatically sets other_config["instant"] = "true"
        VMToTemplateAction* action = new VMToTemplateAction(conn, vm, this->mainWindow());

        // Register with OperationManager for history tracking (matches C# ConnectionsManager.History.Add)
        OperationManager::instance()->registerOperation(action);

        // Connect completion signal for cleanup and status update
        connect(action, &AsyncOperation::completed, this, [this, vmName, action]() {
            if (action->state() == AsyncOperation::Completed && !action->isFailed())
            {
                this->mainWindow()->showStatusMessage(QString("VM '%1' converted to template successfully").arg(vmName), 5000);
                QMessageBox::information(this->mainWindow(), tr("Conversion Complete"),
                                         tr("VM '%1' has been successfully converted to a template.").arg(vmName));
                // Cache will be automatically refreshed via event polling
            } else
            {
                this->mainWindow()->showStatusMessage(QString("Failed to convert VM '%1'").arg(vmName), 5000);
            }
            // Auto-delete when complete (matches C# GC behavior)
            action->deleteLater();
        });

        // Run action asynchronously (matches C# pattern - no modal dialog)
        // Progress shown in status bar via OperationManager signals
        action->runAsync();
    }
}

QString ConvertVMToTemplateCommand::menuText() const
{
    return "Convert to Template";
}

QString ConvertVMToTemplateCommand::getSelectedVMRef() const
{
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    QString objectType = this->getSelectedObjectType();
    if (objectType != "vm")
        return QString();

    return this->getSelectedObjectRef();
}

QString ConvertVMToTemplateCommand::getSelectedVMName() const
{
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    QString objectType = this->getSelectedObjectType();
    if (objectType != "vm")
        return QString();

    return item->text(0);
}

bool ConvertVMToTemplateCommand::canConvertToTemplate(const QString& vmRef) const
{
    // Use cache instead of async API call
    QVariantMap vmData = this->mainWindow()->xenLib()->getCache()->ResolveObjectData("vm", vmRef);

    // Check if it's already a template
    bool isTemplate = vmData.value("is_a_template", false).toBool();
    if (isTemplate)
        return false;

    // Check if it's a snapshot
    bool isSnapshot = vmData.value("is_a_snapshot", false).toBool();
    if (isSnapshot)
        return false;

    // Check power state - must be halted
    QString powerState = vmData.value("power_state", "Halted").toString();
    if (powerState != "Halted")
        return false;

    // Check if the operation is allowed
    QVariantList allowedOps = vmData.value("allowed_operations", QVariantList()).toList();
    QStringList allowedOpsStrings;
    for (const QVariant& op : allowedOps)
    {
        allowedOpsStrings << op.toString();
    }

    return allowedOpsStrings.contains("make_into_template");
}
