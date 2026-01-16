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

#include "disconnecthostsandpoolscommand.h"
#include "disconnectcommand.h"
#include "../host/disconnecthostcommand.h"
#include "../pool/disconnectpoolcommand.h"
#include "../../mainwindow.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xencache.h"

DisconnectHostsAndPoolsCommand::DisconnectHostsAndPoolsCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

DisconnectHostsAndPoolsCommand::DisconnectHostsAndPoolsCommand(const QList<XenConnection*>& connections,
                                                               MainWindow* mainWindow,
                                                               QObject* parent)
    : Command(mainWindow, parent)
    , m_connections(connections)
{
}

QList<XenConnection*> DisconnectHostsAndPoolsCommand::getConnections() const
{
    if (!this->m_connections.isEmpty())
        return this->m_connections;

    // TODO: Get all selected connections from main window selection
    QList<XenConnection*> connections;
    QSharedPointer<XenObject> obj = this->getSelectedObject();
    
    if (obj && obj->GetConnection())
        connections.append(obj->GetConnection());

    return connections;
}

bool DisconnectHostsAndPoolsCommand::CanRun() const
{
    QList<XenConnection*> connections = this->getConnections();
    
    if (connections.isEmpty())
        return false;

    bool oneIsConnected = false;
    bool foundHost = false;
    bool foundPool = false;

    for (XenConnection* connection : connections)
    {
        if (!connection)
            continue;

        XenCache* cache = connection->GetCache();
        if (!cache)
            continue;

        // Check if we can disconnect this (either as host or pool)
        QSharedPointer<Pool> pool = cache->GetPool();
        
        if (pool)
        {
            // This is a pool connection
            QSharedPointer<Host> host = pool->GetMasterHost();
            DisconnectPoolCommand poolCmd(this->mainWindow());
            // TODO: Need to check if this specific pool connection can run
            if (connection->IsConnected())
                oneIsConnected = true;
            foundPool = true;
        }
        else
        {
            // This is a standalone host connection
            DisconnectHostCommand hostCmd(this->mainWindow());
            // TODO: Need to check if this specific host connection can run
            if (connection->IsConnected())
                oneIsConnected = true;
            foundHost = true;
        }
    }

    return oneIsConnected && foundHost && foundPool;
}

void DisconnectHostsAndPoolsCommand::Run()
{
    QList<XenConnection*> connections = this->getConnections();

    for (XenConnection* connection : connections)
    {
        if (connection)
        {
            DisconnectCommand* disconnectCmd = new DisconnectCommand(
                this->mainWindow(), connection, false, this);
            disconnectCmd->Run();
            disconnectCmd->deleteLater();
        }
    }
}

QString DisconnectHostsAndPoolsCommand::MenuText() const
{

    return tr("Disconnect");
}
