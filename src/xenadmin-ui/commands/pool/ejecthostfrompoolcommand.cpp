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
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/actions/pool/ejecthostfrompoolaction.h"
#include <QMessageBox>

EjectHostFromPoolCommand::EjectHostFromPoolCommand(MainWindow* mainWindow, QObject* parent) : HostCommand(mainWindow, parent)
{
}

bool EjectHostFromPoolCommand::CanRun() const
{
    QSharedPointer<Host> host = this->getSelectedHost();
    if (!host)
        return false;

    // Can't eject if this is the pool master
    if (host->IsMaster())
        return false;

    // Check if host is in a pool
    if (host->GetPoolRef().isEmpty())
        return false;

    return true;
}

void EjectHostFromPoolCommand::Run()
{
    QSharedPointer<Host> host = this->getSelectedHost();
    if (!host || !host->IsValid())
        return;

    QString hostName = host->GetName();

    if (host->IsMaster())
    {
        QMessageBox::warning(this->mainWindow(), "Eject Host",
                             "Cannot eject the pool master.\n"
                             "Please designate a new master first.");
        return;
    }

    // Confirm the operation
    int result = QMessageBox::question(this->mainWindow(), "Eject Host from Pool",
                                       QString("Are you sure you want to eject '%1' from the pool?\n\n"
                                               "The host will become a standalone server and will need to be rebooted.\n"
                                               "All running VMs on this host will be shut down.")
                                           .arg(hostName),
                                       QMessageBox::Yes | QMessageBox::No);

    if (result != QMessageBox::Yes)
        return;

    // Get pool and GetConnection for the action
    XenConnection* connection = host->GetConnection();
    if (!connection)
    {
        QMessageBox::critical(this->mainWindow(), "Eject Host", "No active connection.");
        return;
    }

    // Create and run the EjectHostFromPoolAction
    EjectHostFromPoolAction* action = new EjectHostFromPoolAction(host->GetPool(), host, nullptr);

    // Register with OperationManager for history tracking
    OperationManager::instance()->RegisterOperation(action);

    // Connect completion signals
    connect(action, &AsyncOperation::completed, [hostName, action]()
    {
        QMessageBox::information(MainWindow::instance(), "Eject Host", QString("Successfully ejected '%1' from the pool.\nThe host will now be rebooted.").arg(hostName));
        action->deleteLater();
    });

    connect(action, &AsyncOperation::failed, [hostName, action](const QString& error)
    {
        QMessageBox::critical(MainWindow::instance(), "Eject Host", QString("Failed to eject '%1' from the pool:\n%2").arg(hostName, error));
        action->deleteLater();
    });

    // Run action asynchronously
    action->RunAsync();
    this->mainWindow()->ShowStatusMessage(QString("Ejecting '%1' from pool...").arg(hostName), 0);
}

QString EjectHostFromPoolCommand::MenuText() const
{
    return "Eject from Pool...";
}
