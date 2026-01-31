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

#include "reverttosnapshotcommand.h"
#include <QDebug>
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/actions/vm/vmsnapshotrevertaction.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/vm.h"
#include <QtWidgets>

RevertToSnapshotCommand::RevertToSnapshotCommand(QObject* parent) : Command(nullptr, parent)
{
    // qDebug() << "RevertToSnapshotCommand: Created default constructor";
}

RevertToSnapshotCommand::RevertToSnapshotCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
    // qDebug() << "RevertToSnapshotCommand: Created with MainWindow";
}

RevertToSnapshotCommand::RevertToSnapshotCommand(const QString& snapshotUuid, MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent), m_snapshotUuid(snapshotUuid)
{
    // qDebug() << "RevertToSnapshotCommand: Created with snapshot UUID:" << snapshotUuid;
}

void RevertToSnapshotCommand::Run()
{
    // qDebug() << "RevertToSnapshotCommand: Executing Revert to Snapshot command";

    if (!this->CanRun())
    {
        qWarning() << "RevertToSnapshotCommand: Cannot execute - snapshot is not valid or cannot be reverted to";
        QMessageBox::warning(nullptr, tr("Cannot Revert to Snapshot"),
                             tr("Cannot revert to the selected snapshot.\n"
                                "Please ensure the snapshot is valid and the VM is not locked."));
        return;
    }

    if (this->showConfirmationDialog())
    {
        this->revertToSnapshot();
    }
}

bool RevertToSnapshotCommand::CanRun() const
{
    if (!this->mainWindow() || this->m_snapshotUuid.isEmpty())
    {
        return false;
    }

    return this->canRevertToSnapshot();
}

QString RevertToSnapshotCommand::MenuText() const
{
    return tr("Revert to Snapshot");
}

bool RevertToSnapshotCommand::canRevertToSnapshot() const
{
    if (!this->mainWindow() || this->m_snapshotUuid.isEmpty())
        return false;

    QSharedPointer<VM> snapshot = this->GetObject()->GetCache()->ResolveObject<VM>(XenObjectType::VM, this->m_snapshotUuid);

    if (!snapshot)
    {
        qDebug() << "RevertToSnapshotCommand: Snapshot not found in cache:" << this->m_snapshotUuid;
        return false;
    }

    // Verify it's actually a snapshot
    if (!snapshot->IsSnapshot())
    {
        qDebug() << "RevertToSnapshotCommand: Object is not a snapshot:" << this->m_snapshotUuid;
        return false;
    }

    // Check if snapshot is being used by any operations
    if (!snapshot->CurrentOperations().isEmpty())
    {
        qDebug() << "RevertToSnapshotCommand: Snapshot has active operations:" << this->m_snapshotUuid;
        return false;
    }

    // Check allowed operations
    QStringList allowed_ops = snapshot->GetAllowedOperations();
    if (!allowed_ops.contains("revert"))
    {
        qDebug() << "RevertToSnapshotCommand: Revert operation not allowed for snapshot:" << this->m_snapshotUuid;
        return false;
    }

    // Get parent VM and check if it's locked
    QSharedPointer<VM> snapshot_of = snapshot->SnapshotOf();
    if (!snapshot_of.isNull())
    {
        QVariantMap vmCurrentOps = snapshot_of->CurrentOperations();
        // Allow revert if VM has no critical operations
        // (Some operations like snapshot creation are allowed during revert)
        for (const QVariant& op : vmCurrentOps)
        {
            QString operation = op.toString();
            if (operation == "changing_VCPUs" ||
                operation == "changing_memory" ||
                operation == "migrating" ||
                operation == "pool_migrate")
            {
                qDebug() << "RevertToSnapshotCommand: Parent VM has critical operation:" << operation;
                return false;
            }
        }
    }

    return true;
}

bool RevertToSnapshotCommand::showConfirmationDialog()
{
    QString snapshotName = this->m_snapshotUuid;
    QString snapshotTime;

    QSharedPointer<VM> snapshot = this->GetObject()->GetCache()->ResolveObject<VM>(XenObjectType::VM, this->m_snapshotUuid);

    if (!snapshot)
        return false;

    snapshotName = snapshot->GetName();
    if (snapshotName.isEmpty())
        snapshotName = this->m_snapshotUuid;

    snapshotTime = snapshot->SnapshotTime().toString("yyyy-MM-dd HH:mm:ss");

    QString message = tr("Are you sure you want to revert to this snapshot?\n\n"
                         "This will undo all changes made to the VM since this snapshot was created.\n"
                         "The VM will be stopped if it is currently running.\n\n"
                         "Snapshot: %1")
                          .arg(snapshotName);

    if (!snapshotTime.isEmpty())
        message += tr("\nCreated: %1").arg(snapshotTime);

    message += tr("\n\nThis action cannot be undone.");

    QMessageBox::StandardButton reply = QMessageBox::question(
        this->mainWindow(),
        tr("Revert to Snapshot"),
        message,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    return reply == QMessageBox::Yes;
}

void RevertToSnapshotCommand::revertToSnapshot()
{
    qDebug() << "RevertToSnapshotCommand: Reverting to snapshot:" << this->m_snapshotUuid;

    if (!this->mainWindow())
    {
        qWarning() << "RevertToSnapshotCommand: No main window available";
        return;
    }

    QSharedPointer<VM> snapshot = this->GetObject()->GetCache()->ResolveObject<VM>(XenObjectType::VM, this->m_snapshotUuid);

    XenConnection* conn = snapshot->GetConnection();
    if (!conn || !conn->IsConnected())
    {
        qWarning() << "RevertToSnapshotCommand: Not connected";
        QMessageBox::critical(this->mainWindow(), tr("Revert Error"), tr("Not connected to XenServer."));
        return;
    }

    // Resolve snapshot VM from cache
    if (!snapshot || !snapshot->IsValid())
    {
        qWarning() << "RevertToSnapshotCommand: Failed to resolve snapshot VM";
        QMessageBox::critical(this->mainWindow(), tr("Revert Error"), tr("Failed to resolve snapshot VM."));
        return;
    }

    // Create VMSnapshotRevertAction (matches C# VMSnapshotRevertAction pattern)
    // Action handles VM power cycle tracking and is cancellable
    VMSnapshotRevertAction* action = new VMSnapshotRevertAction(snapshot, this->mainWindow());

    // Register with OperationManager for history tracking (matches C# ConnectionsManager.History.Add)
    OperationManager::instance()->RegisterOperation(action);

    // Connect completion signal for cleanup and status update
    QString snapshotUuid = this->m_snapshotUuid;
    connect(action, &AsyncOperation::completed, action, [snapshotUuid, action]()
    {
        bool success = (action->GetState() == AsyncOperation::Completed && !action->IsFailed());
        if (success)
        {
            qDebug() << "RevertToSnapshotCommand: VM reverted to snapshot successfully:" << snapshotUuid;
            // Cache will be automatically refreshed via event polling
            // No message box - matches C# behavior (success shown in history/status bar)
        } else
        {
            qWarning() << "RevertToSnapshotCommand: Failed to revert to snapshot";
        }
        // Auto-delete when complete (matches C# GC behavior)
        action->deleteLater();
    });

    // Run action asynchronously (matches C# pattern - no modal dialog)
    // Progress shown in status bar via OperationManager signals
    action->RunAsync();
}
