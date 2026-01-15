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

#include "deletesnapshotcommand.h"
#include <QDebug>
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/actions/vm/vmsnapshotdeleteaction.h"
#include "xenlib/xen/xenobject.h"
#include <QtWidgets>

DeleteSnapshotCommand::DeleteSnapshotCommand(QObject* parent) : Command(nullptr, parent)
{
    // qDebug() << "DeleteSnapshotCommand: Created default constructor";
}

DeleteSnapshotCommand::DeleteSnapshotCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
    // qDebug() << "DeleteSnapshotCommand: Created with MainWindow";
}

DeleteSnapshotCommand::DeleteSnapshotCommand(const QString& snapshotUuid, MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent), m_snapshotUuid(snapshotUuid)
{
    // qDebug() << "DeleteSnapshotCommand: Created with snapshot UUID:" << snapshotUuid;
}

void DeleteSnapshotCommand::Run()
{
    // qDebug() << "DeleteSnapshotCommand: Executing Delete Snapshot command";

    if (!this->CanRun())
    {
        qWarning() << "DeleteSnapshotCommand: Cannot execute - snapshot is not valid or cannot be deleted";
        QMessageBox::warning(nullptr, tr("Cannot Delete Snapshot"),
                             tr("The selected snapshot cannot be deleted.\n"
                                "Please ensure the snapshot is valid and not in use."));
        return;
    }

    if (this->showConfirmationDialog())
    {
        this->deleteSnapshot();
    }
}

bool DeleteSnapshotCommand::CanRun() const
{
    if (!this->mainWindow() || this->m_snapshotUuid.isEmpty())
    {
        return false;
    }

    return this->canDeleteSnapshot();
}

QString DeleteSnapshotCommand::MenuText() const
{
    return tr("Delete Snapshot");
}

bool DeleteSnapshotCommand::canDeleteSnapshot() const
{
    if (this->m_snapshotUuid.isEmpty() || !this->mainWindow())
        return false;

    QSharedPointer<XenObject> selectedObject = this->GetObject();
    if (!selectedObject)
        return false;

    // Get snapshot data from cache
    XenCache* cache = selectedObject->GetCache();

    QSharedPointer<VM> snapshot = cache->ResolveObject<VM>("vm", this->m_snapshotUuid);
    if (!snapshot)
    {
        qDebug() << "DeleteSnapshotCommand: Snapshot not found in cache:" << this->m_snapshotUuid;
        return false;
    }

    // Verify it's actually a snapshot
    if (!snapshot->IsSnapshot())
    {
        qDebug() << "DeleteSnapshotCommand: Object is not a snapshot:" << this->m_snapshotUuid;
        return false;
    }

    // Check if snapshot is being used by any operations
    if (!snapshot->CurrentOperations().isEmpty())
    {
        qDebug() << "DeleteSnapshotCommand: Snapshot has active operations:" << this->m_snapshotUuid;
        return false;
    }

    // Check allowed operations
    QStringList allowedOps = snapshot->GetAllowedOperations();
    if (!allowedOps.contains("destroy"))
    {
        qDebug() << "DeleteSnapshotCommand: Destroy operation not allowed for snapshot:" << this->m_snapshotUuid;
        return false;
    }

    return true;
}

bool DeleteSnapshotCommand::showConfirmationDialog()
{
    QString snapshotName = this->m_snapshotUuid;

    // Try to get snapshot name from cache
    QSharedPointer<VM> snapshot = this->GetObject()->GetCache()->ResolveObject<VM>("vm", this->m_snapshotUuid);
    if (!snapshot)
    {
        snapshotName = snapshot->GetName();
        if (snapshotName.isEmpty())
            snapshotName = this->m_snapshotUuid;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this->mainWindow(),
        tr("Delete Snapshot"),
        tr("Are you sure you want to delete this snapshot?\n\n"
           "This action cannot be undone.\n\n"
           "Snapshot: %1")
            .arg(snapshotName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    return reply == QMessageBox::Yes;
}

void DeleteSnapshotCommand::deleteSnapshot()
{
    qDebug() << "DeleteSnapshotCommand: Deleting snapshot:" << this->m_snapshotUuid;

    if (!this->mainWindow())
    {
        qWarning() << "DeleteSnapshotCommand: No main window available";
        return;
    }

    QSharedPointer<VM> snapshot = this->GetObject()->GetCache()->ResolveObject<VM>("vm", this->m_snapshotUuid);
    if (!snapshot || !snapshot->IsValid())
    {
        qWarning() << "DeleteSnapshotCommand: Failed to resolve snapshot VM";
        QMessageBox::critical(this->mainWindow(), tr("Delete Error"), tr("Failed to resolve snapshot VM."));
        return;
    }

    XenConnection* conn = snapshot->GetConnection();
    if (!conn || !conn->IsConnected())
    {
        qWarning() << "DeleteSnapshotCommand: Not connected";
        QMessageBox::critical(this->mainWindow(), tr("Delete Error"), tr("Not connected to XenServer."));
        return;
    }

    // Create VMSnapshotDeleteAction (matches C# VMSnapshotDeleteAction pattern)
    // Action handles task polling, history tracking, and automatic cache refresh
    VMSnapshotDeleteAction* action = new VMSnapshotDeleteAction(snapshot, this->mainWindow());

    // Register with OperationManager for history tracking (matches C# ConnectionsManager.History.Add)
    OperationManager::instance()->RegisterOperation(action);

    // Connect completion signal for cleanup and status update
    connect(action, &AsyncOperation::completed, this, [this, action]()
    {
        bool success = (action->GetState() == AsyncOperation::Completed && !action->IsFailed());
        if (success)
        {
            qDebug() << "DeleteSnapshotCommand: Snapshot deleted successfully:" << this->m_snapshotUuid;
            // Cache will be automatically refreshed via event polling
        } else
        {
            qWarning() << "DeleteSnapshotCommand: Failed to delete snapshot";
        }
        // Auto-delete when complete (matches C# GC behavior)
        action->deleteLater();
    });

    // Run action asynchronously (matches C# pattern - no modal dialog)
    // Progress shown in status bar via OperationManager signals
    action->RunAsync();
}
