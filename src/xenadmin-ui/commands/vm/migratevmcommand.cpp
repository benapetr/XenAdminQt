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

#include "migratevmcommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/host.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/actions/vm/vmmigrateaction.h"
#include "vmoperationhelpers.h"
#include <QMessageBox>
#include <QInputDialog>

MigrateVMCommand::MigrateVMCommand(MainWindow* mainWindow, QObject* parent) : VMCommand(mainWindow, parent)
{
}

bool MigrateVMCommand::CanRun() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    XenConnection* connection = vm->GetConnection();
    if (!connection || !connection->IsConnected())
        return false;

    // C# checks: not template, not locked, and migration allowed
    if (vm->IsTemplate())
        return false;

    if (vm->IsLocked())
        return false;

    // TODO: FeatureForbidden (Host.RestrictIntraPoolMigrate) is not implemented yet.
    if (!vm->GetAllowedOperations().contains("pool_migrate"))
        return false;

    // Need at least one other host to migrate to
    QStringList hosts = this->getAvailableHosts();
    return hosts.size() > 1;
}

void MigrateVMCommand::Run()
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return;

    QString vmName = this->getSelectedVMName();
    if (vmName.isEmpty())
        return;

    // Get current host
    QString currentHostRef = vm->ResidentOnRef();

    // Get all available hosts
    QStringList hosts = this->getAvailableHosts();

    if (hosts.isEmpty())
    {
        QMessageBox::warning(this->mainWindow(), "Migrate VM", "No hosts available for migration.");
        return;
    }

    // Build a map of host names to refs
    QMap<QString, QString> hostMap;
    QStringList hostNames;

    XenConnection* conn = vm->GetConnection();

    if (!conn || !conn->IsConnected())
    {
        QMessageBox::warning(this->mainWindow(), "Not Connected", "Not connected to XenServer");
        return;
    }

    XenCache* cache = vm->GetCache();

    for (const QString& hostRef : hosts)
    {
        if (hostRef == currentHostRef)
            continue; // Skip current host

        // Use cache instead of async API call
        if (cache)
        {
            QString cannotBootReason;
            if (!VMOperationHelpers::VmCanBootOnHost(conn, vm, hostRef, "pool_migrate", &cannotBootReason))
                continue;

            QVariantMap hostData = cache->ResolveObjectData("host", hostRef);
            QString hostName = hostData.value("name_label", "Unknown Host").toString();
            hostMap[hostName] = hostRef;
            hostNames.append(hostName);
        }
    }

    if (hostNames.isEmpty())
    {
        QMessageBox::warning(this->mainWindow(), "Migrate VM", "No eligible hosts available for migration.");
        return;
    }

    // Show dialog to select destination host
    bool ok;
    QString selectedHostName = QInputDialog::getItem(this->mainWindow(),
                                                     "Migrate VM",
                                                     QString("Select destination host for VM '%1':").arg(vmName),
                                                     hostNames,
                                                     0,
                                                     false,
                                                     &ok);

    if (!ok || selectedHostName.isEmpty())
        return;

    QString destHostRef = hostMap[selectedHostName];
    QString vmRef = vm->OpaqueRef();

    // Check if migration is possible
    QString error;
    if (!vm->CanMigrateToHost(destHostRef, &error))
    {
        QMessageBox::warning(this->mainWindow(), "Migrate VM",
                             QString("Cannot migrate VM '%1' to host '%2'.\n\nReason: %3")
                                 .arg(vmName, selectedHostName, error));
        return;
    }

    QString cannotBootReason;
    if (!VMOperationHelpers::VmCanBootOnHost(vm->GetConnection(), vm, destHostRef, "pool_migrate", &cannotBootReason))
    {
        if (cannotBootReason.isEmpty())
            cannotBootReason = tr("The VM cannot be migrated to the selected host.");
        QMessageBox::warning(this->mainWindow(), "Migrate VM",
                             QString("Cannot migrate VM '%1' to host '%2'.\n\nReason: %3")
                                 .arg(vmName, selectedHostName, cannotBootReason));
        return;
    }

    // Confirm migration
    int ret = QMessageBox::question(this->mainWindow(), "Migrate VM",
                                    QString("Migrate VM '%1' to host '%2'?\n\n"
                                            "This will perform a live migration without downtime.")
                                        .arg(vmName, selectedHostName),
                                    QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes)
    {
        QSharedPointer<Host> host = vm->GetCache()->ResolveObject<Host>("host", destHostRef);

        if (!host)
        {
            QMessageBox::warning(this->mainWindow(), "Host not found", "Selected host was not found in Xen Cache");
            return;
        }

        // Create VMMigrateAction (matches C# VMMigrateAction pattern for within-pool migration)
        // Action handles HA pre-check failures and task polling
        VMMigrateAction* action = new VMMigrateAction(vm, host, this->mainWindow());

        // Register with OperationManager for history tracking (matches C# ConnectionsManager.History.Add)
        OperationManager::instance()->RegisterOperation(action);

        // Connect completion signal for cleanup and status update
        connect(action, &AsyncOperation::completed, this, [this, vmName, selectedHostName, action]() {
            if (action->GetState() == AsyncOperation::Completed && !action->IsFailed())
            {
                this->mainWindow()->ShowStatusMessage(QString("VM '%1' migrated successfully to '%2'").arg(vmName, selectedHostName), 5000);
                // Cache will be automatically refreshed via event polling
            } else
            {
                this->mainWindow()->ShowStatusMessage(QString("Failed to migrate VM '%1'").arg(vmName), 5000);
            }
            // Auto-delete when complete (matches C# GC behavior)
            action->deleteLater();
        });

        // Run action asynchronously (matches C# pattern - no modal dialog)
        // Progress shown in status bar via OperationManager signals
        action->RunAsync();
    }
}

QString MigrateVMCommand::MenuText() const
{
    return "Migrate VM...";
}

QStringList MigrateVMCommand::getAvailableHosts() const
{
    QStringList hostRefs;
    QSharedPointer<VM> vm = this->getVM();
    if (!vm || !vm->GetConnection())
        return hostRefs;

    hostRefs = vm->GetConnection()->GetCache()->GetAllRefs("host");

    return hostRefs;
}
