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

#include "deletepoolcommand.h"
#include "../../mainwindow.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/actions/pool/destroypoolaction.h"
#include <QPointer>
#include <QMessageBox>
#include <QDebug>

DeletePoolCommand::DeletePoolCommand(MainWindow* mainWindow, QObject* parent) : PoolCommand(mainWindow, parent)
{
}

bool DeletePoolCommand::CanRun() const
{
    QSharedPointer<Pool> pool = this->getPool();
    if (!pool || !pool->IsValid())
        return false;

    // Can delete pool if connected and single host
    return pool->IsConnected() && !this->hasMultipleHosts(pool);
}

void DeletePoolCommand::Run()
{
    QSharedPointer<Pool> pool = this->getPool();
    if (!pool || !pool->IsValid())
        return;

    QString poolName = pool->GetName();

    // Check if HA is enabled or enabling
    bool haEnabled = pool->HAEnabled();
    QVariantMap currentOps = pool->CurrentOperations();

    bool haEnabling = false;
    for (const QVariant& opVar : currentOps.values())
    {
        QString op = opVar.toString();
        if (op == "ha_enable")
        {
            haEnabling = true;
            break;
        }
    }

    if (haEnabling)
    {
        QMessageBox::warning(
            MainWindow::instance(),
            "Cannot Delete Pool",
            QString("Cannot delete pool '%1' because High Availability is currently being enabled.\n\n"
                    "Please wait for the operation to complete, then disable HA before deleting the pool.")
                .arg(poolName));
        return;
    }

    if (haEnabled)
    {
        QMessageBox::warning(
            MainWindow::instance(),
            "Cannot Delete Pool",
            QString("Cannot delete pool '%1' because High Availability is enabled.\n\n"
                    "You must disable HA before deleting the pool.")
                .arg(poolName));
        return;
    }

    // Check if pool has multiple hosts
    if (this->hasMultipleHosts(pool))
    {
        QMessageBox::warning(
            MainWindow::instance(),
            "Cannot Delete Pool",
            QString("Pool '%1' contains multiple servers.\n\n"
                    "You must eject all servers except the coordinator before deleting the pool.\n\n"
                    "To eject servers, right-click on each non-coordinator server and select 'Eject from Pool'.")
                .arg(poolName));
        return;
    }

    // Show confirmation dialog
    QMessageBox msgBox(MainWindow::instance());
    msgBox.setWindowTitle("Delete Pool");
    msgBox.setText(QString("Are you sure you want to delete pool '%1'?").arg(poolName));
    msgBox.setInformativeText("This will convert the pool back to a standalone server. The server configuration will remain unchanged.");
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    int ret = msgBox.exec();

    if (ret != QMessageBox::Yes)
        return;

    qDebug() << "DeletePoolCommand: Deleting pool" << poolName << "(" << pool->OpaqueRef() << ")";

    // Create and run destroy pool action
    DestroyPoolAction* action = new DestroyPoolAction(pool, nullptr);

    QPointer<MainWindow> mainWindow = MainWindow::instance();
    if (!mainWindow)
    {
        action->deleteLater();
        return;
    }

    // Connect completion signal for cleanup and status update
    connect(action, &AsyncOperation::completed, mainWindow, [poolName, action, mainWindow]()
    {
        if (action->GetState() == AsyncOperation::Completed && !action->IsFailed())
        {
            mainWindow->ShowStatusMessage(QString("Successfully deleted pool '%1'").arg(poolName), 5000);
        } else
        {
            QMessageBox::warning(mainWindow, "Delete Pool Failed", QString("Failed to delete pool '%1'.\n\n%2").arg(poolName, action->GetErrorMessage()));
        }
        // Auto-delete when complete
        action->deleteLater();
    }, Qt::QueuedConnection);

    // Run action asynchronously
    action->RunAsync();
}

QString DeletePoolCommand::MenuText() const
{
    return "Delete Pool";
}

bool DeletePoolCommand::hasMultipleHosts(QSharedPointer<Pool> pool) const
{
    if (!pool)
        return false;

    QStringList hosts = pool->GetHostRefs();;
    return hosts.count() > 1;
}
