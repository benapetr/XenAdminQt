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

#include "ejecthostfrompoolcommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib.h"
#include "xen/network/connection.h"
#include "xen/host.h"
#include "xen/pool.h"
#include "xen/actions/pool/ejecthostfrompoolaction.h"
#include "xencache.h"
#include <QMessageBox>

EjectHostFromPoolCommand::EjectHostFromPoolCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool EjectHostFromPoolCommand::CanRun() const
{
    QString hostRef = this->getSelectedHostRef();
    if (hostRef.isEmpty() || !this->mainWindow()->xenLib()->isConnected())
        return false;

    // Can't eject if this is the pool master
    if (this->isPoolMaster(hostRef))
        return false;

    // Check if host is in a pool (more than one host exists)
    // Use cache instead of making API calls
    XenLib* xenLib = this->mainWindow()->xenLib();
    int hostCount = xenLib->getCache()->Count("host");

    // Need at least 2 hosts to eject one
    return hostCount >= 2;
}

void EjectHostFromPoolCommand::Run()
{
    QString hostRef = this->getSelectedHostRef();

    if (hostRef.isEmpty())
        return;

    if (this->isPoolMaster(hostRef))
    {
        QMessageBox::warning(this->mainWindow(), "Eject Host",
                             "Cannot eject the pool master.\n"
                             "Please designate a new master first.");
        return;
    }

    // Get host name for confirmation
    XenLib* xenLib = this->mainWindow()->xenLib();
    // Use cache instead of async API call
    QVariantMap hostData = xenLib->getCache()->ResolveObjectData("host", hostRef);
    QString hostName = hostData.value("name_label", "this host").toString();

    // Confirm the operation
    int result = QMessageBox::question(this->mainWindow(), "Eject Host from Pool",
                                       QString("Are you sure you want to eject '%1' from the pool?\n\n"
                                               "The host will become a standalone server and will need to be rebooted.\n"
                                               "All running VMs on this host will be shut down.")
                                           .arg(hostName),
                                       QMessageBox::Yes | QMessageBox::No);

    if (result != QMessageBox::Yes)
        return;

    // Get pool and host objects for the action
    XenConnection* connection = xenLib->getConnection();
    if (!connection)
    {
        QMessageBox::critical(this->mainWindow(), "Eject Host",
                              "No active connection.");
        return;
    }

    // Get the pool reference
    QStringList poolRefs = xenLib->getCache()->GetAllRefs("pool");
    if (poolRefs.isEmpty())
    {
        QMessageBox::critical(this->mainWindow(), "Eject Host",
                              "Could not find pool.");
        return;
    }

    // Create Pool and Host wrapper objects for the action
    Pool* pool = new Pool(connection, poolRefs.first(), this);
    Host* host = new Host(connection, hostRef, this);

    // Create and run the EjectHostFromPoolAction
    EjectHostFromPoolAction* action = new EjectHostFromPoolAction(
        connection,
        pool,
        host,
        this);

    // Register with OperationManager for history tracking
    OperationManager::instance()->registerOperation(action);

    // Connect completion signals
    connect(action, &AsyncOperation::completed, [this, hostName, action]() {
        QMessageBox::information(this->mainWindow(), "Eject Host",
                                 QString("Successfully ejected '%1' from the pool.\n"
                                         "The host will now be rebooted.")
                                     .arg(hostName));
        action->deleteLater();
    });

    connect(action, &AsyncOperation::failed, [this, hostName, action](const QString& error) {
        QMessageBox::critical(this->mainWindow(), "Eject Host",
                              QString("Failed to eject '%1' from the pool:\n%2")
                                  .arg(hostName, error));
        action->deleteLater();
    });

    // Run action asynchronously
    action->runAsync();
    this->mainWindow()->showStatusMessage(QString("Ejecting '%1' from pool...").arg(hostName), 0);
}

QString EjectHostFromPoolCommand::MenuText() const
{
    return "Eject from Pool...";
}

QString EjectHostFromPoolCommand::getSelectedHostRef() const
{
    QTreeWidgetItem* item = getSelectedItem();
    if (!item)
        return QString();

    QString objectType = getSelectedObjectType();
    if (objectType != "host")
        return QString();

    return getSelectedObjectRef();
}

bool EjectHostFromPoolCommand::isPoolMaster(const QString& hostRef) const
{
    if (hostRef.isEmpty())
        return false;

    XenLib* xenLib = this->mainWindow()->xenLib();
    QStringList poolRefs = xenLib->getCache()->GetAllRefs("pool");
    for (const QString& poolRef : poolRefs)
    {
        QVariantMap pool = xenLib->getCache()->ResolveObjectData("pool", poolRef);
        QString masterRef = pool.value("master").toString();

        if (masterRef == hostRef)
            return true;
    }

    return false;
}
