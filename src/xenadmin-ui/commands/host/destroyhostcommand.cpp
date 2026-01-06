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

#include "destroyhostcommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xen/actions/host/destroyhostaction.h"
#include "xen/pool.h"
#include "xen/host.h"
#include "xencache.h"
#include <QMessageBox>

DestroyHostCommand::DestroyHostCommand(MainWindow* mainWindow, QObject* parent) : HostCommand(mainWindow, parent)
{
}

bool DestroyHostCommand::CanRun() const
{
    QSharedPointer<Host> host = this->getSelectedHost();
    if (!host)
        return false;

    // Host must be in a pool
    if (host->PoolRef().isEmpty())
        return false;

    // Host must not be the pool coordinator
    if (host->IsMaster())
        return false;

    // Host must not be live (running)
    if (this->isHostLive(host))
        return false;

    return true;
}

void DestroyHostCommand::Run()
{
    QSharedPointer<Host> host = this->getSelectedHost();
    if (!host)
        return;

    QString hostName = host->GetName();

    // Show confirmation dialog
    QString message = this->buildConfirmationMessage(host);
    QString title = this->buildConfirmationTitle();

    QMessageBox msgBox(this->mainWindow());
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setWindowTitle(title);
    msgBox.setText(message);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    if (msgBox.exec() != QMessageBox::Yes)
    {
        return;
    }

    // Get GetConnection
    XenConnection* conn = host->GetConnection();
    if (!conn || !conn->IsConnected())
    {
        QMessageBox::warning(this->mainWindow(), tr("Not Connected"), tr("Not connected to XenServer"));
        return;
    }

    // Create and run the destroy host action
    DestroyHostAction* action = new DestroyHostAction(conn, host, nullptr);

    action->SetTitle(tr("Destroying host '%1'...").arg(hostName));

    // Register with OperationManager for history tracking
    OperationManager::instance()->RegisterOperation(action);

    // Run the action asynchronously
    action->RunAsync(true);

    this->mainWindow()->ShowStatusMessage(tr("Destroying host: %1").arg(hostName), 5000);
}

QString DestroyHostCommand::MenuText() const
{
    return tr("&Destroy Host");
}

QString DestroyHostCommand::buildConfirmationMessage(QSharedPointer<Host> host) const
{
    QVariantMap hostData = host->GetData();
    QString hostName = hostData.value("name_label").toString();

    return tr("Are you sure you want to destroy host '%1'?\n\n"
              "This will permanently remove the host from the pool. "
              "This operation cannot be undone.")
        .arg(hostName);
}

QString DestroyHostCommand::buildConfirmationTitle() const
{
    return tr("Confirm Destroy Host");
}

bool DestroyHostCommand::isHostLive(QSharedPointer<Host> host) const
{
    if (!host->IsConnected())
        return false;

    // Check if host is live (has a metrics object with live flag)
    QString metricsRef = host->GetData().value("metrics").toString();
    if (!metricsRef.isEmpty())
    {
        QVariantMap metricsData = host->GetCache()->ResolveObjectData("host_metrics", metricsRef);
        if (!metricsData.isEmpty())
        {
            return metricsData.value("live", false).toBool();
        }
    }

    // If no metrics, assume host is live to be safe
    return true;
}
