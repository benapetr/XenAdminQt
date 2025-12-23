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
#include "../../../xenlib/xen/actions/host/destroyhostaction.h"
#include "../../../xenlib/xen/pool.h"
#include "../../../xenlib/xen/host.h"
#include "../../../xenlib/xenlib.h"
#include "../../../xenlib/xencache.h"
#include <QMessageBox>

DestroyHostCommand::DestroyHostCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool DestroyHostCommand::CanRun() const
{
    if (!this->isHostSelected())
        return false;

    QString hostRef = this->getSelectedObjectRef();
    return this->canRunForHost(hostRef);
}

bool DestroyHostCommand::canRunForHost(const QString& hostRef) const
{
    if (hostRef.isEmpty())
        return false;

    QVariantMap hostData = this->xenLib()->getCachedObjectData("host", hostRef);
    if (hostData.isEmpty())
        return false;

    // Host must be in a pool
    QString poolRef = hostData.value("pool").toString();
    if (poolRef.isEmpty())
        return false;

    // Host must not be the pool coordinator
    if (this->isHostCoordinator(hostRef))
        return false;

    // Host must not be live (running)
    if (this->isHostLive(hostRef))
        return false;

    return true;
}

void DestroyHostCommand::Run()
{
    if (!this->isHostSelected())
        return;

    QString hostRef = this->getSelectedObjectRef();
    QVariantMap hostData = this->xenLib()->getCachedObjectData("host", hostRef);
    QString hostName = hostData.value("name_label").toString();

    // Show confirmation dialog
    QString message = this->buildConfirmationMessage();
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

    // Get connection
    XenConnection* conn = this->xenLib()->getConnection();
    if (!conn || !conn->isConnected())
    {
        QMessageBox::warning(this->mainWindow(), tr("Not Connected"),
                             tr("Not connected to XenServer"));
        return;
    }

    // Get pool reference
    QString poolRef = hostData.value("pool").toString();
    if (poolRef.isEmpty())
    {
        QMessageBox::warning(this->mainWindow(), tr("Error"),
                             tr("Host does not belong to a pool"));
        return;
    }

    // Create Pool and Host objects
    Pool* pool = new Pool(conn, poolRef, this);
    Host* host = new Host(conn, hostRef, this);

    // Create and run the destroy host action
    DestroyHostAction* action = new DestroyHostAction(
        conn,
        pool,
        host,
        this);

    action->setTitle(tr("Destroying host '%1'...").arg(hostName));

    // Register with OperationManager for history tracking
    OperationManager::instance()->registerOperation(action);

    // Run the action asynchronously
    action->runAsync();

    this->mainWindow()->showStatusMessage(
        tr("Destroying host: %1").arg(hostName),
        5000);
}

QString DestroyHostCommand::MenuText() const
{
    return tr("&Destroy Host");
}

QString DestroyHostCommand::buildConfirmationMessage() const
{
    QString hostRef = this->getSelectedObjectRef();
    QVariantMap hostData = this->xenLib()->getCachedObjectData("host", hostRef);
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

bool DestroyHostCommand::isHostSelected() const
{
    return this->getSelectedObjectType() == "host";
}

bool DestroyHostCommand::isHostCoordinator(const QString& hostRef) const
{
    QVariantMap hostData = this->xenLib()->getCachedObjectData("host", hostRef);
    QString poolRef = hostData.value("pool").toString();

    if (poolRef.isEmpty())
        return false;

    QVariantMap poolData = this->xenLib()->getCachedObjectData("pool", poolRef);
    QString coordinatorRef = poolData.value("master").toString();

    return hostRef == coordinatorRef;
}

bool DestroyHostCommand::isHostLive(const QString& hostRef) const
{
    QVariantMap hostData = this->xenLib()->getCachedObjectData("host", hostRef);

    // Check if host is live (has a metrics object with live flag)
    QString metricsRef = hostData.value("metrics").toString();
    if (!metricsRef.isEmpty())
    {
        QVariantMap metricsData = this->xenLib()->getCache()->ResolveObjectData("host_metrics", metricsRef);
        if (!metricsData.isEmpty())
        {
            return metricsData.value("live", false).toBool();
        }
    }

    // If no metrics, assume host is live to be safe
    return true;
}
