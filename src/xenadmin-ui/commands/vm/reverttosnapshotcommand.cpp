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
#include "xencache.h"
#include "xen/network/connection.h"
#include "xen/actions/vm/vmsnapshotrevertaction.h"
#include "xen/xenobject.h"
#include <QtWidgets>

RevertToSnapshotCommand::RevertToSnapshotCommand(QObject* parent)
    : Command(nullptr, parent)
{
    // qDebug() << "RevertToSnapshotCommand: Created default constructor";
}

RevertToSnapshotCommand::RevertToSnapshotCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
    // qDebug() << "RevertToSnapshotCommand: Created with MainWindow";
}

RevertToSnapshotCommand::RevertToSnapshotCommand(const QString& snapshotUuid, MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent), m_snapshotUuid(snapshotUuid)
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
    if (this->m_snapshotUuid.isEmpty())
    {
        return false;
    }

    if (!this->mainWindow())
    {
        return false;
    }

    QSharedPointer<XenObject> selectedObject = this->GetObject();
    if (!selectedObject)
        return false;

    // Get snapshot data from cache
    XenConnection* connection = selectedObject->GetConnection();
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    if (!cache)
        return false;

    QVariantMap snapshotData = cache->ResolveObjectData("vm", this->m_snapshotUuid);
    if (snapshotData.isEmpty())
    {
        qDebug() << "RevertToSnapshotCommand: Snapshot not found in cache:" << this->m_snapshotUuid;
        return false;
    }

    // Verify it's actually a snapshot
    if (!snapshotData.value("is_a_snapshot").toBool())
    {
        qDebug() << "RevertToSnapshotCommand: Object is not a snapshot:" << this->m_snapshotUuid;
        return false;
    }

    // Check if snapshot is being used by any operations
    QVariantList currentOperations = snapshotData.value("current_operations").toList();
    if (!currentOperations.isEmpty())
    {
        qDebug() << "RevertToSnapshotCommand: Snapshot has active operations:" << this->m_snapshotUuid;
        return false;
    }

    // Check allowed operations
    QVariantList allowedOps = snapshotData.value("allowed_operations").toList();
    if (!allowedOps.contains("revert"))
    {
        qDebug() << "RevertToSnapshotCommand: Revert operation not allowed for snapshot:" << this->m_snapshotUuid;
        return false;
    }

    // Get parent VM and check if it's locked
    QString snapshotOf = snapshotData.value("snapshot_of").toString();
    if (!snapshotOf.isEmpty())
    {
        QVariantMap vmData = cache->ResolveObjectData("vm", snapshotOf);
        if (!vmData.isEmpty())
        {
            QVariantList vmCurrentOps = vmData.value("current_operations").toList();
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
    }

    return true;
}

bool RevertToSnapshotCommand::showConfirmationDialog()
{
    QString snapshotName = this->m_snapshotUuid;
    QString snapshotTime;

    // Try to get snapshot details from cache
    QSharedPointer<XenObject> selectedObject = this->GetObject();
    XenConnection* connection = selectedObject ? selectedObject->GetConnection() : nullptr;
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    if (cache)
    {
        QVariantMap snapshotData = cache->ResolveObjectData("vm", this->m_snapshotUuid);
        if (!snapshotData.isEmpty())
        {
            snapshotName = snapshotData.value("name_label").toString();
            if (snapshotName.isEmpty())
            {
                snapshotName = this->m_snapshotUuid;
            }

            QString timestamp = snapshotData.value("snapshot_time").toString();
            if (!timestamp.isEmpty())
            {
                QDateTime dt = QDateTime::fromString(timestamp, Qt::ISODate);
                if (dt.isValid())
                {
                    snapshotTime = dt.toString("yyyy-MM-dd HH:mm:ss");
                }
            }
        }
    }

    QString message = tr("Are you sure you want to revert to this snapshot?\n\n"
                         "This will undo all changes made to the VM since this snapshot was created.\n"
                         "The VM will be stopped if it is currently running.\n\n"
                         "Snapshot: %1")
                          .arg(snapshotName);

    if (!snapshotTime.isEmpty())
    {
        message += tr("\nCreated: %1").arg(snapshotTime);
    }

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

    QSharedPointer<XenObject> selectedObject = this->GetObject();
    XenConnection* conn = selectedObject ? selectedObject->GetConnection() : nullptr;
    if (!conn || !conn->IsConnected())
    {
        qWarning() << "RevertToSnapshotCommand: Not connected";
        QMessageBox::critical(this->mainWindow(), tr("Revert Error"),
                              tr("Not connected to XenServer."));
        return;
    }

    // Create VMSnapshotRevertAction (matches C# VMSnapshotRevertAction pattern)
    // Action handles VM power cycle tracking and is cancellable
    VMSnapshotRevertAction* action = new VMSnapshotRevertAction(conn, this->m_snapshotUuid, this->mainWindow());

    // Register with OperationManager for history tracking (matches C# ConnectionsManager.History.Add)
    OperationManager::instance()->registerOperation(action);

    // Connect completion signal for cleanup and status update
    connect(action, &AsyncOperation::completed, this, [this, action]() {
        bool success = (action->state() == AsyncOperation::Completed && !action->isFailed());
        if (success)
        {
            qDebug() << "RevertToSnapshotCommand: VM reverted to snapshot successfully:" << this->m_snapshotUuid;
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
    action->runAsync();
}
