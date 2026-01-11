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

#include "migratevmmenu.h"
#include "../mainwindow.h"
#include "../operations/operationmanager.h"
#include "../commands/vm/vmoperationhelpers.h"
#include "../commands/vm/crosspoolmigratecommand.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/actions/vm/vmmigrateaction.h"
#include <QMessageBox>

MigrateVMMenu::MigrateVMMenu(MainWindow* mainWindow,
                             const QSharedPointer<VM>& vm,
                             QWidget* parent)
    : QMenu(tr("Migrate VM"), parent),
      m_mainWindow(mainWindow),
      m_vm(vm)
{
    this->populate();
}

void MigrateVMMenu::addDisabledReason(const QString& reason)
{
    QAction* action = this->addAction(reason);
    action->setEnabled(false);
}

void MigrateVMMenu::populate()
{
    if (!m_vm)
    {
        addDisabledReason(tr("No VM selected."));
        return;
    }

    XenConnection* connection = m_vm->GetConnection();
    if (!connection || !connection->IsConnected())
    {
        addDisabledReason(tr("Not connected to server."));
        return;
    }

    if (m_vm->IsTemplate())
    {
        addDisabledReason(tr("VM is a template."));
        return;
    }

    if (m_vm->IsLocked())
    {
        addDisabledReason(tr("VM is locked."));
        return;
    }

    if (!m_vm->GetAllowedOperations().contains("pool_migrate"))
    {
        addDisabledReason(tr("VM does not allow migration."));
        return;
    }

    XenCache* cache = connection->GetCache();
    if (!cache)
    {
        addDisabledReason(tr("Cache is not available."));
        return;
    }

    QString currentHostRef = m_vm->ResidentOnRef();
    QStringList hosts = cache->GetAllRefs("host");

    bool anyEnabled = false;
    for (const QString& hostRef : hosts)
    {
        if (hostRef == currentHostRef)
            continue;

        QVariantMap hostData = cache->ResolveObjectData("host", hostRef);
        QString hostName = hostData.value("name_label", "Unknown Host").toString();

        QString cannotBootReason;
        bool canBoot = VMOperationHelpers::VmCanBootOnHost(connection, m_vm, hostRef, "pool_migrate", &cannotBootReason);
        QString label = canBoot ? hostName : QString("%1 (%2)").arg(hostName, cannotBootReason);

        QAction* action = this->addAction(label);
        action->setEnabled(canBoot);
        if (canBoot)
        {
            anyEnabled = true;
            connect(action, &QAction::triggered, this, [this, hostRef, hostName]() {
                this->runMigrationToHost(hostRef, hostName);
            });
        }
    }

    if (!anyEnabled)
    {
        if (this->actions().isEmpty())
            addDisabledReason(tr("No other hosts available for migration."));
        else
            addDisabledReason(tr("No eligible hosts available for migration."));
    }

    this->addSeparator();
    CrossPoolMigrateCommand crossPoolCmd(this->m_mainWindow,
                                         CrossPoolMigrateWizard::WizardMode::Migrate,
                                         this);
    QAction* crossPoolAction = this->addAction(tr("Cross Pool Migrate..."));
    crossPoolAction->setEnabled(crossPoolCmd.CanRun());
    connect(crossPoolAction, &QAction::triggered, this, [this]()
    {
        if (!this->m_mainWindow)
            return;
        CrossPoolMigrateCommand cmd(this->m_mainWindow,
                                    CrossPoolMigrateWizard::WizardMode::Migrate,
                                    this);
        if (cmd.CanRun())
            cmd.Run();
    });
}

void MigrateVMMenu::runMigrationToHost(const QString& hostRef, const QString& hostName)
{
    if (!m_vm)
        return;

    QString vmName = m_vm->GetName();
    if (vmName.isEmpty())
        vmName = tr("VM");

    QString error;
    if (!m_vm->CanMigrateToHost(hostRef, &error))
    {
        QMessageBox::warning(this->m_mainWindow, tr("Migrate VM"),
                             tr("Cannot migrate VM '%1' to host '%2'.\n\nReason: %3")
                                 .arg(vmName, hostName, error));
        return;
    }

    QString cannotBootReason;
    if (!VMOperationHelpers::VmCanBootOnHost(m_vm->GetConnection(), m_vm, hostRef, "pool_migrate", &cannotBootReason))
    {
        if (cannotBootReason.isEmpty())
            cannotBootReason = tr("The VM cannot be migrated to the selected host.");
        QMessageBox::warning(this->m_mainWindow, tr("Migrate VM"),
                             tr("Cannot migrate VM '%1' to host '%2'.\n\nReason: %3")
                                 .arg(vmName, hostName, cannotBootReason));
        return;
    }

    int ret = QMessageBox::question(this->m_mainWindow, tr("Migrate VM"),
                                    tr("Migrate VM '%1' to host '%2'?\n\n"
                                       "This will perform a live migration without downtime.")
                                        .arg(vmName, hostName),
                                    QMessageBox::Yes | QMessageBox::No);

    if (ret != QMessageBox::Yes)
        return;

    XenConnection* conn = m_vm->GetConnection();
    if (!conn || !conn->IsConnected())
    {
        QMessageBox::warning(this->m_mainWindow, tr("Not Connected"),
                             tr("Not connected to XenServer"));
        return;
    }

    VMMigrateAction* action = new VMMigrateAction(conn, m_vm->OpaqueRef(), hostRef, this->m_mainWindow);
    OperationManager::instance()->RegisterOperation(action);

    connect(action, &AsyncOperation::completed, this, [this, vmName, hostName, action]()
    {
        if (action->GetState() == AsyncOperation::Completed && !action->IsFailed())
        {
            this->m_mainWindow->ShowStatusMessage(
                tr("VM '%1' migrated successfully to '%2'").arg(vmName, hostName), 5000);
        } else
        {
            this->m_mainWindow->ShowStatusMessage(
                tr("Failed to migrate VM '%1'").arg(vmName), 5000);
        }
        action->deleteLater();
    });

    action->RunAsync();
}
