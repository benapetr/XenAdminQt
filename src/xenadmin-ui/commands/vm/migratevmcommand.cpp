
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
#include "xenlib.h"
#include "xen/connection.h"
#include "xen/actions/vm/vmmigrateaction.h"
#include "xencache.h"
#include <QMessageBox>
#include <QInputDialog>

MigrateVMCommand::MigrateVMCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool MigrateVMCommand::canRun() const
{
    QString vmRef = this->getSelectedVMRef();
    if (vmRef.isEmpty())
        return false;

    // Only enable if VM is running (can only live migrate running VMs)
    if (!this->isVMRunning(vmRef))
        return false;

    // Check if there are other hosts available in the pool
    QStringList hosts = this->getAvailableHosts();
    QString currentHost = this->getCurrentHostRef(vmRef);

    // Need at least one other host to migrate to
    return hosts.size() > 1;
}

void MigrateVMCommand::run()
{
    QString vmRef = this->getSelectedVMRef();
    QString vmName = this->getSelectedVMName();

    if (vmRef.isEmpty() || vmName.isEmpty())
        return;

    // Get current host
    QString currentHostRef = this->getCurrentHostRef(vmRef);

    // Get all available hosts
    QStringList hosts = this->getAvailableHosts();

    if (hosts.isEmpty())
    {
        QMessageBox::warning(this->mainWindow(), "Migrate VM",
                             "No hosts available for migration.");
        return;
    }

    // Build a map of host names to refs
    QMap<QString, QString> hostMap;
    QStringList hostNames;

    for (const QString& hostRef : hosts)
    {
        if (hostRef == currentHostRef)
            continue; // Skip current host

        // Use cache instead of async API call
        QVariantMap hostData = this->mainWindow()->xenLib()->getCache()->resolve("host", hostRef);
        QString hostName = hostData.value("name_label", "Unknown Host").toString();
        hostMap[hostName] = hostRef;
        hostNames.append(hostName);
    }

    if (hostNames.isEmpty())
    {
        QMessageBox::warning(this->mainWindow(), "Migrate VM",
                             "No other hosts available for migration.");
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

    // Check if migration is possible
    if (!this->mainWindow()->xenLib()->canMigrateVM(vmRef, destHostRef))
    {
        QString error = this->mainWindow()->xenLib()->getLastError();
        QMessageBox::warning(this->mainWindow(), "Migrate VM",
                             QString("Cannot migrate VM '%1' to host '%2'.\n\nReason: %3")
                                 .arg(vmName, selectedHostName, error));
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
        // Get XenConnection from XenLib
        XenConnection* conn = this->mainWindow()->xenLib()->getConnection();
        if (!conn || !conn->isConnected())
        {
            QMessageBox::warning(this->mainWindow(), "Not Connected",
                                 "Not connected to XenServer");
            return;
        }

        // Create VMMigrateAction (matches C# VMMigrateAction pattern for within-pool migration)
        // Action handles HA pre-check failures and task polling
        VMMigrateAction* action = new VMMigrateAction(conn, vmRef, destHostRef, this->mainWindow());

        // Register with OperationManager for history tracking (matches C# ConnectionsManager.History.Add)
        OperationManager::instance()->registerOperation(action);

        // Connect completion signal for cleanup and status update
        connect(action, &AsyncOperation::completed, this, [this, vmName, selectedHostName, action]() {
            if (action->state() == AsyncOperation::Completed && !action->isFailed())
            {
                this->mainWindow()->showStatusMessage(QString("VM '%1' migrated successfully to '%2'").arg(vmName, selectedHostName), 5000);
                // Cache will be automatically refreshed via event polling
            } else
            {
                this->mainWindow()->showStatusMessage(QString("Failed to migrate VM '%1'").arg(vmName), 5000);
            }
            // Auto-delete when complete (matches C# GC behavior)
            action->deleteLater();
        });

        // Run action asynchronously (matches C# pattern - no modal dialog)
        // Progress shown in status bar via OperationManager signals
        action->runAsync();
    }
}

QString MigrateVMCommand::menuText() const
{
    return "Migrate VM...";
}

QString MigrateVMCommand::getSelectedVMRef() const
{
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    QString objectType = this->getSelectedObjectType();
    if (objectType != "vm")
        return QString();

    return this->getSelectedObjectRef();
}

QString MigrateVMCommand::getSelectedVMName() const
{
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    QString objectType = this->getSelectedObjectType();
    if (objectType != "vm")
        return QString();

    return item->text(0);
}

bool MigrateVMCommand::isVMRunning(const QString& vmRef) const
{
    // Use cache instead of async API call
    QVariantMap vmData = this->mainWindow()->xenLib()->getCache()->resolve("vm", vmRef);
    QString powerState = vmData.value("power_state", "Halted").toString();

    return (powerState == "Running");
}

QStringList MigrateVMCommand::getAvailableHosts() const
{
    QStringList hostRefs;
    XenCache* cache = this->mainWindow()->xenLib()->getCache();
    if (!cache)
        return hostRefs;

    hostRefs = cache->getAllRefs("host");

    return hostRefs;
}

QString MigrateVMCommand::getCurrentHostRef(const QString& vmRef) const
{
    // Use cache instead of async API call
    QVariantMap vmData = this->mainWindow()->xenLib()->getCache()->resolve("vm", vmRef);
    return vmData.value("resident_on", QString()).toString();
}
