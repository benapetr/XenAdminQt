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

#include "disablechangedblocktrackingcommand.h"
#include "xenlib/xen/actions/vdi/vdidisablecbtaction.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xencache.h"
#include "xenlib/operations/parallelaction.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/vbd.h"
#include "xenlib/xen/vdi.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include <QMessageBox>
#include <QVariantList>

DisableChangedBlockTrackingCommand::DisableChangedBlockTrackingCommand(MainWindow* mainWindow, QObject* parent) : VMCommand(mainWindow, parent)
{
}

bool DisableChangedBlockTrackingCommand::CanRun() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    if (vm->IsTemplate())
        return false;

    // Check if CBT is licensed
    if (!this->isCbtLicensed(QString()))
        return false;

    // VM must have a VDI with CBT enabled
    return this->hasVdiWithCbtEnabled();
}

void DisableChangedBlockTrackingCommand::Run()
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return;

    QString vmRef = vm->OpaqueRef();
    QString vmName = vm->GetName();

    // Show confirmation dialog
    QString message = tr("Are you sure you want to disable Changed Block Tracking for VM '%1'?\n\n"
                         "This will prevent incremental backups from working until CBT is re-enabled.")
                          .arg(vmName);

    QMessageBox::StandardButton reply = QMessageBox::question(
        this->mainWindow(),
        tr("Disable Changed Block Tracking"),
        message,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply != QMessageBox::Yes)
    {
        return;
    }

    // Get GetConnection
    XenConnection* conn = vm->GetConnection();
    if (!conn || !conn->IsConnected())
    {
        QMessageBox::warning(this->mainWindow(), tr("Not Connected"), tr("Not connected to XenServer"));
        return;
    }

    // Collect all actions for VDIs with CBT enabled
    QList<AsyncOperation*> actions;

    // Get all VBDs for this VM
    const QList<QSharedPointer<VBD>> vbds = vm->GetVBDs();
    for (const QSharedPointer<VBD>& vbd : vbds)
    {
        if (!vbd || !vbd->IsValid())
            continue;

        QSharedPointer<VDI> vdi = vbd->GetVDI();
        if (!vdi || !vdi->IsValid())
            continue;

        // Check if CBT is enabled on this VDI
        if (vdi->IsCBTEnabled())
        {
            // Create action to disable CBT
            VDIDisableCbtAction* action = new VDIDisableCbtAction(conn, vmName, vdi->OpaqueRef(), nullptr);
            actions.append(action);
        }
    }

    if (actions.isEmpty())
    {
        QMessageBox::information(this->mainWindow(), tr("No CBT Enabled"), tr("The selected VM does not have Changed Block Tracking enabled."));
        return;
    }

    // Execute actions
    if (actions.count() == 1)
    {
        // Single action - register and run
        OperationManager::instance()->RegisterOperation(actions.first());
        actions.first()->RunAsync();
    } else
    {
        // Multiple actions - use ParallelAction
        ParallelAction* parallelOp = new ParallelAction(
            "Disable changed block tracking",
            "Disabling changed block tracking",
            "Disabled changed block tracking",
            actions,
            conn,
            false, // suppressHistory
            false, // runInParallel
            0,     // timeout
            this);
        OperationManager::instance()->RegisterOperation(parallelOp);
        parallelOp->RunAsync();
    }
}

QString DisableChangedBlockTrackingCommand::MenuText() const
{
    return tr("Disable Changed Block &Tracking");
}

bool DisableChangedBlockTrackingCommand::isCbtLicensed(const QString& connRef) const
{
    // TODO: Check Host.RestrictChangedBlockTracking restriction
    // For now, assume CBT is licensed
    // In C#: Helpers.FeatureForbidden(vm.Connection, Host.RestrictChangedBlockTracking)
    Q_UNUSED(connRef);
    return true;
}

bool DisableChangedBlockTrackingCommand::hasVdiWithCbtEnabled() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    // Get all VBDs for this VM
    const QList<QSharedPointer<VBD>> vbds = vm->GetVBDs();
    for (const QSharedPointer<VBD>& vbd : vbds)
    {
        if (!vbd || !vbd->IsValid())
            continue;

        QSharedPointer<VDI> vdi = vbd->GetVDI();
        if (!vdi || !vdi->IsValid())
            continue;

        // Check if CBT is enabled
        if (vdi->IsCBTEnabled())
            return true;
    }

    return false;
}
