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

#include "cancelhostconnectioncommand.h"
#include "disconnectcommand.h"
#include "../../mainwindow.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/xenobject.h"

CancelHostConnectionCommand::CancelHostConnectionCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

CancelHostConnectionCommand::CancelHostConnectionCommand(const QList<XenConnection*>& connections, MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent), m_connections(connections)
{
}

QList<XenConnection*> CancelHostConnectionCommand::getConnections() const
{
    if (!this->m_connections.isEmpty())
        return this->m_connections;

    // Get from selection
    QList<XenConnection*> connections;
    QSharedPointer<XenObject> obj = this->getSelectedObject();
    
    if (obj && obj->GetConnection())
        connections.append(obj->GetConnection());

    return connections;
}

bool CancelHostConnectionCommand::CanRun() const
{
    QList<XenConnection*> connections = this->getConnections();
    
    if (connections.isEmpty())
        return false;

    // All connections must be in progress (connecting but not connected)
    for (XenConnection* connection : connections)
    {
        if (!connection || connection->IsConnected() || !connection->InProgress())
            return false;
    }

    return true;
}

void CancelHostConnectionCommand::Run()
{
    QList<XenConnection*> connections = this->getConnections();

    for (XenConnection* connection : connections)
    {
        if (connection && !connection->IsConnected() && connection->InProgress())
        {
            // Cancel by disconnecting
            DisconnectCommand* disconnectCmd = new DisconnectCommand(MainWindow::instance(), connection, false, this);
            disconnectCmd->Run();
            disconnectCmd->deleteLater();
        }
    }
}

QString CancelHostConnectionCommand::MenuText() const
{
    return tr("Cancel Connection");
}
