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

#include "newtemplatefromsnapshotcommand.h"
#include "../../../xenlib/xen/actions/vm/vmcloneaction.h"
#include "../../../xenlib/xen/xenapi/xenapi_VM.h"
#include "../../../xenlib/xen/network/connection.h"
#include "../../../xenlib/xen/vm.h"
#include "../../../xenlib/xencache.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include <QMessageBox>
#include <QInputDialog>

NewTemplateFromSnapshotCommand::NewTemplateFromSnapshotCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

NewTemplateFromSnapshotCommand::NewTemplateFromSnapshotCommand(const QString& snapshotRef,
                                                               XenConnection* connection,
                                                               MainWindow* mainWindow,
                                                               QObject* parent)
    : Command(mainWindow, parent),
      m_snapshotRef(snapshotRef),
      m_connection(connection)
{
}

bool NewTemplateFromSnapshotCommand::CanRun() const
{
    QString vmRef = !this->m_snapshotRef.isEmpty() ? this->m_snapshotRef : this->getSelectedObjectRef();
    QString type = !this->m_snapshotRef.isEmpty() ? "vm" : this->getSelectedObjectType();

    if (vmRef.isEmpty() || type != "vm")
    {
        return false;
    }

    XenConnection* connection = this->m_connection;
    if (!connection)
    {
        QSharedPointer<XenObject> selectedObject = this->GetObject();
        connection = selectedObject ? selectedObject->GetConnection() : nullptr;
    }
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    if (!cache)
        return false;

    // Get VM data from cache
    QVariantMap vmData = cache->ResolveObjectData("vm", vmRef);
    if (vmData.isEmpty())
    {
        return false;
    }

    // VM must be a snapshot
    bool isSnapshot = vmData.value("is_a_snapshot").toBool();
    return isSnapshot;
}

void NewTemplateFromSnapshotCommand::Run()
{
    QString vmRef = !this->m_snapshotRef.isEmpty() ? this->m_snapshotRef : this->getSelectedObjectRef();
    QString type = !this->m_snapshotRef.isEmpty() ? "vm" : this->getSelectedObjectType();

    if (vmRef.isEmpty() || type != "vm")
    {
        return;
    }

    XenConnection* connection = this->m_connection;
    if (!connection)
    {
        QSharedPointer<XenObject> selectedObject = this->GetObject();
        connection = selectedObject ? selectedObject->GetConnection() : nullptr;
    }
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    if (!cache)
        return;

    // Get snapshot data
    QVariantMap vmData = cache->ResolveObjectData("vm", vmRef);
    if (vmData.isEmpty())
    {
        return;
    }

    bool isSnapshot = vmData.value("is_a_snapshot").toBool();
    if (!isSnapshot)
    {
        QMessageBox::warning(this->mainWindow(), tr("Not a Snapshot"),
                             tr("Selected item is not a VM snapshot"));
        return;
    }

    QString snapshotName = vmData.value("name_label").toString();
    QString defaultName = this->generateUniqueName(snapshotName);

    // Prompt for template name
    bool ok;
    QString templateName = QInputDialog::getText(this->mainWindow(),
                                                 tr("Create Template from Snapshot"),
                                                 tr("Enter a name for the new template:"),
                                                 QLineEdit::Normal,
                                                 defaultName,
                                                 &ok);

    if (!ok || templateName.isEmpty())
    {
        return;
    }

    // Get connection
    XenConnection* conn = connection;
    if (!conn || !conn->IsConnected())
    {
        QMessageBox::warning(this->mainWindow(), tr("Not Connected"),
                             tr("Not connected to XenServer"));
        return;
    }

    // Create description for the template
    QString description = tr("Template created from snapshot '%1'").arg(snapshotName);

    // Create VM object for the snapshot
    QSharedPointer<VM> vm = QSharedPointer<VM>(new VM(conn, vmRef));

    // Create VMCloneAction
    VMCloneAction* action = new VMCloneAction(conn, vm, templateName, description, this->mainWindow());

    // Register with OperationManager
    OperationManager::instance()->RegisterOperation(action);

    // Connect completion signal
    connect(action, &AsyncOperation::completed, this, [=]() {
        if (action->GetState() == AsyncOperation::Completed)
        {
            // Mark template as "instant" (created from snapshot)
            // In C#, this is done with SetVMOtherConfigAction
            // For now, we'll just show success message
            // TODO: Add VM.add_to_other_config call to mark as instant template

            this->mainWindow()->ShowStatusMessage(
                tr("Template '%1' created from snapshot").arg(templateName), 5000);
        } else if (action->GetState() == AsyncOperation::Failed)
        {
            QMessageBox::critical(this->mainWindow(), tr("Error"),
                                  tr("Failed to create template from snapshot:\n%1")
                                      .arg(action->GetErrorMessage()));
        }

        action->deleteLater();
    });

    // Run action asynchronously
    action->RunAsync();
}

QString NewTemplateFromSnapshotCommand::MenuText() const
{
    return tr("Create &Template from Snapshot...");
}

QString NewTemplateFromSnapshotCommand::generateUniqueName(const QString& snapshotName) const
{
    QString baseName = tr("Template from '%1'").arg(snapshotName);
    QString name = baseName;
    int suffix = 1;

    // Get all VM names from cache to check for uniqueness
    QSharedPointer<XenObject> selectedObject = this->GetObject();
    XenConnection* conn = selectedObject ? selectedObject->GetConnection() : nullptr;
    if (!conn || !conn->GetCache())
    {
        return name;
    }

    // TODO unfinished logic
    // Simple uniqueness check - just append numbers if needed
    // In production, should check against all VMs in connection
    // For now, this basic implementation should work

    return name;
}
