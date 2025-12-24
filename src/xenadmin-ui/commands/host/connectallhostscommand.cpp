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

#include "connectallhostscommand.h"
#include "../../mainwindow.h"
#include "xen/network/connectionsmanager.h"

ConnectAllHostsCommand::ConnectAllHostsCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

bool ConnectAllHostsCommand::CanRun() const
{
    // Can connect all if there are any disconnected connections
    return this->hasDisconnectedConnections();
}

void ConnectAllHostsCommand::Run()
{
    Xen::ConnectionsManager* manager = Xen::ConnectionsManager::instance();
    if (!manager)
        return;

    this->mainWindow()->showStatusMessage("Connecting to all servers...");
    manager->connectAll();
}

QString ConnectAllHostsCommand::MenuText() const
{
    return "Connect All";
}

bool ConnectAllHostsCommand::hasDisconnectedConnections() const
{
    Xen::ConnectionsManager* manager = Xen::ConnectionsManager::instance();
    if (!manager)
        return false;

    // Check if there are any disconnected connections
    QList<XenConnection*> allConnections = manager->getAllConnections();
    for (XenConnection* conn : allConnections)
    {
        if (conn && !conn->IsConnected())
        {
            // TODO: Also check if connection is not already in progress
            return true;
        }
    }

    return false;
}
