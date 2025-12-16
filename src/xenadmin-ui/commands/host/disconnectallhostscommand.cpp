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

#include "disconnectallhostscommand.h"
#include "../../mainwindow.h"
#include "xenlib.h"
#include "collections/connectionsmanager.h"
#include <QMessageBox>

DisconnectAllHostsCommand::DisconnectAllHostsCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool DisconnectAllHostsCommand::canRun() const
{
    // Can disconnect all if there are any connected connections
    return this->hasConnectedConnections();
}

void DisconnectAllHostsCommand::run()
{
    if (!this->mainWindow()->xenLib())
        return;

    ConnectionsManager* manager = this->mainWindow()->xenLib()->getConnectionsManager();
    if (!manager)
        return;

    // Show confirmation dialog
    int ret = QMessageBox::question(this->mainWindow(), "Disconnect All",
                                    "Are you sure you want to disconnect from all servers?",
                                    QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes)
    {
        this->mainWindow()->showStatusMessage("Disconnecting from all servers...");
        manager->disconnectAll();
    }
}

QString DisconnectAllHostsCommand::menuText() const
{
    return "Disconnect All";
}

bool DisconnectAllHostsCommand::hasConnectedConnections() const
{
    if (!this->mainWindow()->xenLib())
        return false;

    ConnectionsManager* manager = this->mainWindow()->xenLib()->getConnectionsManager();
    if (!manager)
        return false;

    // Check if there are any connected connections
    QList<XenConnection*> connectedConnections = manager->getConnectedConnections();
    return !connectedConnections.isEmpty();
}
