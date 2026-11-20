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

DisconnectHostsAndPoolsCommand::DisconnectHostsAndPoolsCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

DisconnectHostsAndPoolsCommand::DisconnectHostsAndPoolsCommand(const QList<XenConnection*>& connections, MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent), m_connections(connections)
{
}

QList<QSharedPointer<XenObject>> DisconnectHostsAndPoolsCommand::getItems() const
{
    if (this->m_connections.isEmpty())
        return this->getSelectedObjects();

    QList<QSharedPointer<XenObject>> items;
    for (XenConnection* connection : this->m_connections)
    {
        XenCache* cache = connection ? connection->GetCache() : nullptr;
        if (!cache)
            continue;
        if (QSharedPointer<Pool> pool = cache->GetPool())
            items.append(pool);
        else if (QSharedPointer<Host> coordinator = Host::GetCoordinator(connection))
            items.append(coordinator);
    }
    return items;
}

bool DisconnectHostsAndPoolsCommand::CanRun() const
{
    // C# CanRunCore: some item can be disconnected, and the selection has hosts and pools
    bool oneIsConnected = false;
    bool foundHost = false;
    bool foundPool = false;

    for (const QSharedPointer<XenObject>& item : this->getItems())
    {
        if (!item)
            continue;

        const QList<QSharedPointer<XenObject>> single{item};
        DisconnectHostCommand hostCommand(this->mainWindow());
        hostCommand.SetSelectionOverride(single);
        DisconnectPoolCommand poolCommand(this->mainWindow());
        poolCommand.SetSelectionOverride(single);
        if (hostCommand.CanRun() || poolCommand.CanRun())
            oneIsConnected = true;

        if (item->GetObjectType() == XenObjectType::Host)
            foundHost = true;
        else if (item->GetObjectType() == XenObjectType::Pool)
            foundPool = true;
    }

    return oneIsConnected && foundHost && foundPool;
}

void DisconnectHostsAndPoolsCommand::Run()
{
    // C# RunCore: disconnect each item's connection (once, as several items may share one)
    QList<XenConnection*> done;
    for (const QSharedPointer<XenObject>& item : this->getItems())
    {
        XenConnection* connection = item ? item->GetConnection() : nullptr;
        if (!connection || done.contains(connection))
            continue;
        done.append(connection);

        DisconnectCommand* disconnectCmd = new DisconnectCommand(MainWindow::instance(), connection, false, this);
        disconnectCmd->Run();
        disconnectCmd->deleteLater();
    }
}

QString DisconnectHostsAndPoolsCommand::MenuText() const
{

    return tr("Disconnect");
}
