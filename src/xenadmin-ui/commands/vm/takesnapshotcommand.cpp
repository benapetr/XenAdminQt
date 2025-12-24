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

#include "takesnapshotcommand.h"
#include <QDebug>
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "../../dialogs/vmsnapshotdialog.h"
#include "../../ConsoleView/ConsolePanel.h"
#include "xenlib.h"
#include "xencache.h"
#include "xen/api.h"
#include "xen/network/connection.h"
#include "xen/actions/vm/vmsnapshotcreateaction.h"
#include <QtWidgets>

TakeSnapshotCommand::TakeSnapshotCommand(QObject* parent)
    : Command(nullptr, parent)
{
    // qDebug() << "TakeSnapshotCommand: Created default constructor";
}

TakeSnapshotCommand::TakeSnapshotCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
    // qDebug() << "TakeSnapshotCommand: Created with MainWindow";
}

TakeSnapshotCommand::TakeSnapshotCommand(const QString& vmUuid, MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent), m_vmUuid(vmUuid)
{
    // qDebug() << "TakeSnapshotCommand: Created with VM UUID:" << vmUuid;
}

void TakeSnapshotCommand::Run()
{
    // qDebug() << "TakeSnapshotCommand: Executing Take Snapshot command";

    if (!this->CanRun())
    {
        qWarning() << "TakeSnapshotCommand: Cannot execute - VM doesn't support snapshots";
        QMessageBox::warning(nullptr, tr("Cannot Take Snapshot"),
                             tr("The selected VM does not support snapshot operations.\n"
                                "Please ensure the VM is not a template and supports snapshots."));
        return;
    }

    this->showSnapshotDialog();
}

bool TakeSnapshotCommand::CanRun() const
{
    if (!this->mainWindow() || this->m_vmUuid.isEmpty())
    {
        return false;
    }

    return this->canTakeSnapshot();
}

QString TakeSnapshotCommand::MenuText() const
{
    return tr("Take Snapshot...");
}

bool TakeSnapshotCommand::canTakeSnapshot() const
{
    if (this->m_vmUuid.isEmpty())
    {
        return false;
    }

    // TODO: Replace with actual XenAPI VM snapshot capability check
    // Should verify:
    // - VM is not a template
    // - VM is not locked
    // - VM allowed operations include snapshot or checkpoint
    return true;
}

void TakeSnapshotCommand::showSnapshotDialog()
{
    qDebug() << "TakeSnapshotCommand: Opening Take Snapshot dialog";

    if (!this->mainWindow())
    {
        qWarning() << "TakeSnapshotCommand: No main window available";
        return;
    }

    XenLib* xenLib = this->mainWindow()->xenLib();
    if (!xenLib)
    {
        qWarning() << "TakeSnapshotCommand: XenLib not available";
        return;
    }

    // Get VM data from cache
    QVariantMap vmData = xenLib->getCache()->ResolveObjectData("vm", this->m_vmUuid);
    if (vmData.isEmpty())
    {
        qWarning() << "TakeSnapshotCommand: Could not find VM data for" << this->m_vmUuid;
        QMessageBox::warning(this->mainWindow(), tr("Cannot Take Snapshot"),
                             tr("Could not retrieve VM information."));
        return;
    }

    // Show the snapshot dialog
    VmSnapshotDialog dialog(vmData, xenLib, this->mainWindow());
    if (dialog.exec() == QDialog::Accepted)
    {
        QString name = dialog.snapshotName();
        QString description = dialog.snapshotDescription();
        VmSnapshotDialog::SnapshotType type = dialog.snapshotType();

        // C#: Program.Invoke(Program.MainWindow, () => Program.MainWindow.ConsolePanel.SetCurrentSource(vm));
        // Set console to this VM before snapshot (matches C# line 89)
        if (this->mainWindow() && this->mainWindow()->consolePanel())
        {
            this->mainWindow()->consolePanel()->setCurrentSource(this->m_vmUuid);
        }

        this->executeSnapshotOperation(name, description, type);
    }
}

void TakeSnapshotCommand::executeSnapshotOperation(const QString& name, const QString& description, VmSnapshotDialog::SnapshotType type)
{
    qDebug() << "TakeSnapshotCommand: Creating snapshot" << name << "for VM" << this->m_vmUuid
             << "with type" << type;

    emit snapshotStarted();

    if (!this->mainWindow())
    {
        qWarning() << "TakeSnapshotCommand: No main window available";
        emit snapshotCompleted(false);
        return;
    }

    // Get XenConnection from XenLib
    XenConnection* conn = this->mainWindow()->xenLib()->getConnection();
    if (!conn || !conn->IsConnected())
    {
        qWarning() << "TakeSnapshotCommand: Not connected";
        QMessageBox::critical(this->mainWindow(), tr("Snapshot Error"),
                              tr("Not connected to XenServer."));
        emit snapshotCompleted(false);
        return;
    }

    // Convert dialog type to action type
    VMSnapshotCreateAction::SnapshotType actionType;
    switch (type)
    {
    case VmSnapshotDialog::DISK:
        actionType = VMSnapshotCreateAction::DISK;
        break;
    case VmSnapshotDialog::QUIESCED_DISK:
        actionType = VMSnapshotCreateAction::QUIESCED_DISK;
        break;
    case VmSnapshotDialog::DISK_AND_MEMORY:
        actionType = VMSnapshotCreateAction::DISK_AND_MEMORY;
        break;
    default:
        actionType = VMSnapshotCreateAction::DISK;
    }

    // C#: TakeSnapshotCommand.cs lines 89-92
    // Capture console screenshot before snapshot (for checkpoints only)
    // Screenshot captured BEFORE snapshot to avoid console switching (CA-211369)
    QImage screenshot;
    if (actionType == VMSnapshotCreateAction::DISK_AND_MEMORY)
    {
        // Get VM data to check power state
        QVariantMap vmData = this->mainWindow()->xenLib()->getCache()->ResolveObjectData("vm", this->m_vmUuid);
        QString powerState = vmData.value("power_state").toString();

        if (powerState == "Running" && this->mainWindow()->consolePanel())
        {
            qDebug() << "TakeSnapshotCommand: Capturing console screenshot for checkpoint";
            try
            {
                // C#: screenshot = _screenShotProvider(VM, sudoUsername, sudoPassword);
                // Note: We don't have sudo credentials yet - pass empty strings
                screenshot = this->mainWindow()->consolePanel()->snapshot(this->m_vmUuid, QString(), QString());
                if (!screenshot.isNull())
                {
                    qDebug() << "TakeSnapshotCommand: Screenshot captured, size:"
                             << screenshot.width() << "x" << screenshot.height();
                } else
                {
                    qDebug() << "TakeSnapshotCommand: Screenshot capture returned null image";
                }
            } catch (...)
            {
                // C#: Ignore; the screenshot is optional, we will do without it (CA-37095/CA-37103)
                qWarning() << "TakeSnapshotCommand: Failed to capture screenshot (optional, continuing anyway)";
            }
        }
    }

    // Create VMSnapshotCreateAction (matches C# VMSnapshotCreateAction pattern)
    // Action handles disk/quiesce/memory options and runs asynchronously
    // Pass screenshot for checkpoint snapshots
    VMSnapshotCreateAction* action = new VMSnapshotCreateAction(
        conn, this->m_vmUuid, name, description, actionType, screenshot, this->mainWindow());

    // Register with OperationManager for history tracking (matches C# ConnectionsManager.History.Add)
    OperationManager::instance()->registerOperation(action);

    // Connect completion signal for cleanup and status update
    connect(action, &AsyncOperation::completed, this, [this, action]() {
        bool success = (action->state() == AsyncOperation::Completed && !action->isFailed());
        if (success)
        {
            qDebug() << "TakeSnapshotCommand: Snapshot created successfully";
            // Cache will be automatically refreshed via event polling
            // No message box - matches C# behavior (success shown in history/status bar)
        } else
        {
            qWarning() << "TakeSnapshotCommand: Failed to create snapshot";
        }
        emit snapshotCompleted(success);
        // Auto-delete when complete (matches C# GC behavior)
        action->deleteLater();
    });

    // Run action asynchronously (matches C# pattern - no modal dialog)
    // Progress shown in status bar via OperationManager signals
    action->runAsync();
}
