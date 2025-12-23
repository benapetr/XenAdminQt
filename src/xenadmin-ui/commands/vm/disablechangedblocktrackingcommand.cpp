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
#include "../../../xenlib/xen/actions/vdi/vdidisablecbtaction.h"
#include "../../../xenlib/xen/xenapi/xenapi_VDI.h"
#include "../../../xenlib/xen/xenapi/xenapi_VBD.h"
#include "../../../xenlib/xen/network/connection.h"
#include "../../../xenlib/xencache.h"
#include "../../../xenlib/operations/paralleloperation.h"
#include "../../../xenlib/xen/vm.h"
#include "../../../xenlib/xenlib.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include <QMessageBox>
#include <QVariantList>

DisableChangedBlockTrackingCommand::DisableChangedBlockTrackingCommand(MainWindow* mainWindow, QObject* parent)
    : VMCommand(mainWindow, parent)
{
}

bool DisableChangedBlockTrackingCommand::CanRun() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    QVariantMap vmData = vm->data();
    if (vmData.isEmpty())
        return false;

    // Skip templates
    if (vmData.value("is_a_template", false).toBool())
        return false;

    QString connRef = vmData.value("$connection").toString();

    // Check if CBT is licensed
    if (!this->isCbtLicensed(connRef))
        return false;

    // VM must have a VDI with CBT enabled
    return this->hasVdiWithCbtEnabled();
}

void DisableChangedBlockTrackingCommand::Run()
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return;

    QString vmRef = vm->opaqueRef();
    QString vmName = vm->nameLabel();

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

    // Get connection
    XenConnection* conn = vm->connection();
    if (!conn || !conn->isConnected())
    {
        QMessageBox::warning(this->mainWindow(), tr("Not Connected"),
                             tr("Not connected to XenServer"));
        return;
    }

    // Get VM data for VBD access
    QVariantMap vmData = vm->data();

    // Collect all actions for VDIs with CBT enabled
    QList<AsyncOperation*> actions;

    // Get cache from connection
    XenCache* cache = conn->getCache();
    if (!cache)
    {
        QMessageBox::warning(this->mainWindow(), tr("Error"),
                             tr("Failed to access cache"));
        return;
    }

    // Get all VBDs for this VM
    QVariantList vbds = vmData.value("VBDs").toList();

    for (const QVariant& vbdRefVariant : vbds)
    {
        QString vbdRef = vbdRefVariant.toString();
        QVariantMap vbdData = cache->ResolveObjectData("vbd", vbdRef);
        if (vbdData.isEmpty())
        {
            continue;
        }

        // Get VDI from VBD
        QString vdiRef = vbdData.value("VDI").toString();
        if (vdiRef.isEmpty() || vdiRef == "OpaqueRef:NULL")
        {
            continue;
        }

        QVariantMap vdiData = cache->ResolveObjectData("vdi", vdiRef);
        if (vdiData.isEmpty())
        {
            continue;
        }

        // Check if CBT is enabled on this VDI
        bool cbtEnabled = vdiData.value("cbt_enabled", false).toBool();
        if (cbtEnabled)
        {
            // Create action to disable CBT
            VDIDisableCbtAction* action = new VDIDisableCbtAction(
                conn, vmName, vdiRef, this);
            actions.append(action);
        }
    }

    if (actions.isEmpty())
    {
        QMessageBox::information(this->mainWindow(), tr("No CBT Enabled"),
                                 tr("The selected VM does not have Changed Block Tracking enabled."));
        return;
    }

    // Execute actions
    if (actions.count() == 1)
    {
        // Single action - register and run
        OperationManager::instance()->registerOperation(actions.first());
        actions.first()->runAsync();
    } else
    {
        // Multiple actions - use ParallelOperation
        ParallelOperation* parallelOp = new ParallelOperation(
            "Disable changed block tracking",
            "Disabling changed block tracking",
            "Disabled changed block tracking",
            actions,
            conn,
            false, // suppressHistory
            false, // runInParallel
            0,     // timeout
            this);
        OperationManager::instance()->registerOperation(parallelOp);
        parallelOp->runAsync();
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

    QVariantMap vmData = vm->data();
    if (vmData.isEmpty())
        return false;

    // Get all VBDs for this VM
    QVariantList vbds = vmData.value("VBDs").toList();

    XenCache* cache = vm->connection()->getCache();
    if (!cache)
        return false;

    for (const QVariant& vbdRefVariant : vbds)
    {
        QString vbdRef = vbdRefVariant.toString();
        QVariantMap vbdData = cache->ResolveObjectData("vbd", vbdRef);
        if (vbdData.isEmpty())
            continue;

        // Get VDI from VBD
        QString vdiRef = vbdData.value("VDI").toString();
        if (vdiRef.isEmpty() || vdiRef == "OpaqueRef:NULL")
            continue;

        QVariantMap vdiData = cache->ResolveObjectData("vdi", vdiRef);
        if (vdiData.isEmpty())
            continue;

        // Check if CBT is enabled
        if (vdiData.value("cbt_enabled", false).toBool())
            return true;
    }

    return false;
}
