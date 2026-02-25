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
    if (!MainWindow::instance())
    {
        return false;
    }

    return this->canDeleteSnapshot();
}

QString DeleteSnapshotCommand::MenuText() const
{
    const QList<QSharedPointer<VM>> snapshots = this->collectSelectedSnapshots();
    return snapshots.size() > 1 ? tr("&Delete") : tr("Delete Snapshot");
}

bool DeleteSnapshotCommand::canDeleteSnapshot() const
{
    const QList<QSharedPointer<VM>> snapshots = this->collectSelectedSnapshots();
    if (snapshots.isEmpty() || !MainWindow::instance())
        return false;

    for (const QSharedPointer<VM>& snapshot : snapshots)
    {
        if (!this->canDeleteSnapshot(snapshot))
            return false;
    }
    return true;
}

bool DeleteSnapshotCommand::showConfirmationDialog()
{
    const QList<QSharedPointer<VM>> snapshots = this->collectSelectedSnapshots();
    if (snapshots.isEmpty())
        return false;

    QString title = tr("Delete Snapshot");
    QString message;
    if (snapshots.size() == 1)
    {
        const QSharedPointer<VM> snapshot = snapshots.first();
        QString snapshotName = snapshot ? snapshot->GetName() : QString();
        if (snapshotName.isEmpty())
            snapshotName = snapshot ? snapshot->OpaqueRef() : QString();
        message = tr("Are you sure you want to delete this snapshot?\n\n"
                     "This action cannot be undone.\n\n"
                     "Snapshot: %1")
                      .arg(snapshotName);
    } else
    {
        title = tr("Delete Snapshots");
        message = tr("Are you sure you want to delete %1 snapshots?\n\n"
                     "This action cannot be undone.")
                      .arg(snapshots.size());
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        MainWindow::instance(),
        title,
        message,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    return reply == QMessageBox::Yes;
}

void DeleteSnapshotCommand::deleteSnapshot()
{
    const QList<QSharedPointer<VM>> snapshots = this->collectSelectedSnapshots();
    if (snapshots.isEmpty())
    {
        qWarning() << "DeleteSnapshotCommand: No snapshots selected";
        return;
    }

    if (!MainWindow::instance())
    {
        qWarning() << "DeleteSnapshotCommand: No main window available";
        return;
    }

    for (const QSharedPointer<VM>& snapshot : snapshots)
    {
        if (!snapshot || !snapshot->IsValid())
        {
            qWarning() << "DeleteSnapshotCommand: Failed to resolve snapshot VM";
            continue;
        }

        XenConnection* conn = snapshot->GetConnection();
        if (!conn || !conn->IsConnected())
        {
            qWarning() << "DeleteSnapshotCommand: Not connected";
            continue;
        }

        const QString snapshotRef = snapshot->OpaqueRef();
        qDebug() << "DeleteSnapshotCommand: Deleting snapshot:" << snapshotRef;

        // Create VMSnapshotDeleteAction (matches C# VMSnapshotDeleteAction pattern)
        // Action handles task polling, history tracking, and automatic cache refresh
        VMSnapshotDeleteAction* action = new VMSnapshotDeleteAction(snapshot, MainWindow::instance());

        // Connect completion signal for cleanup and status update
        connect(action, &AsyncOperation::completed, action, [snapshotRef, action]()
        {
            bool success = (action->GetState() == AsyncOperation::Completed && !action->IsFailed());
            if (success)
            {
                qDebug() << "DeleteSnapshotCommand: Snapshot deleted successfully:" << snapshotRef;
                // Cache will be automatically refreshed via event polling
            } else
            {
                qWarning() << "DeleteSnapshotCommand: Failed to delete snapshot:" << snapshotRef;
            }
            // Auto-delete when complete (matches C# GC behavior)
            action->deleteLater();
        });

        // Run action asynchronously (matches C# pattern - no modal dialog)
        // Progress shown in status bar via OperationManager signals
        action->RunAsync();
    }
}

QList<QSharedPointer<VM>> DeleteSnapshotCommand::collectSelectedSnapshots() const
{
    QList<QSharedPointer<VM>> snapshots;

    if (!this->m_snapshotUuid.isEmpty())
    {
        QSharedPointer<XenObject> selectedObject = this->GetObject();
        if (!selectedObject)
            return snapshots;

        XenCache* cache = selectedObject->GetCache();
        if (!cache)
            return snapshots;

        QSharedPointer<VM> snapshot = cache->ResolveObject<VM>(XenObjectType::VM, this->m_snapshotUuid);
        if (snapshot)
            snapshots.append(snapshot);
        return snapshots;
    }

    const QList<QSharedPointer<XenObject>> selectedObjects = this->getSelectedObjects();
    for (const QSharedPointer<XenObject>& obj : selectedObjects)
    {
        if (!obj || obj->GetObjectType() != XenObjectType::VM)
            continue;

        QSharedPointer<VM> vm = qSharedPointerDynamicCast<VM>(obj);
        if (vm)
            snapshots.append(vm);
    }

    if (!snapshots.isEmpty())
        return snapshots;

    const QString snapshotUuid = this->effectiveSnapshotUuid();
    if (snapshotUuid.isEmpty())
        return snapshots;

    QSharedPointer<XenObject> selectedObject = this->GetObject();
    if (!selectedObject)
        return snapshots;

    XenCache* cache = selectedObject->GetCache();
    if (!cache)
        return snapshots;

    QSharedPointer<VM> snapshot = cache->ResolveObject<VM>(XenObjectType::VM, snapshotUuid);
    if (snapshot)
        snapshots.append(snapshot);

    return snapshots;
}

bool DeleteSnapshotCommand::canDeleteSnapshot(const QSharedPointer<VM>& snapshot) const
{
    if (!snapshot)
        return false;

    if (!snapshot->IsSnapshot())
    {
        qDebug() << "DeleteSnapshotCommand: Object is not a snapshot:" << snapshot->OpaqueRef();
        return false;
    }

    if (!snapshot->CurrentOperations().isEmpty())
    {
        qDebug() << "DeleteSnapshotCommand: Snapshot has active operations:" << snapshot->OpaqueRef();
        return false;
    }

    if (!snapshot->GetAllowedOperations().contains("destroy"))
    {
        qDebug() << "DeleteSnapshotCommand: Destroy operation not allowed for snapshot:" << snapshot->OpaqueRef();
        return false;
    }

    XenConnection* conn = snapshot->GetConnection();
    if (!conn || !conn->IsConnected())
    {
        qDebug() << "DeleteSnapshotCommand: Snapshot connection is not active:" << snapshot->OpaqueRef();
        return false;
    }

    return true;
}

QString DeleteSnapshotCommand::effectiveSnapshotUuid() const
{
    if (!this->m_snapshotUuid.isEmpty())
        return this->m_snapshotUuid;

    if (this->getSelectedObjectType() != XenObjectType::VM)
        return QString();

    return this->getSelectedObjectRef();
}
