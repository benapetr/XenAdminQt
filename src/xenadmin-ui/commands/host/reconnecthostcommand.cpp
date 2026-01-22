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

#include "reconnecthostcommand.h"
#include "../../mainwindow.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/xenobject.h"
#include "../../network/xenconnectionui.h"

ReconnectHostCommand::ReconnectHostCommand(MainWindow* mainWindow, QObject* parent) : HostCommand(mainWindow, parent)
{
}

ReconnectHostCommand::ReconnectHostCommand(const QList<XenConnection*>& connections, MainWindow* mainWindow, QObject* parent) : HostCommand(mainWindow, parent), m_connections(connections)
{
}

bool ReconnectHostCommand::CanRun() const
{
    // Can reconnect if connection is disconnected
    return this->isConnectionDisconnected();
}

void ReconnectHostCommand::Run()
{
    QList<XenConnection*> connections = this->getConnections();
    for (XenConnection* conn : connections)
    {
        if (!conn || conn->IsConnected())
            continue;

        this->mainWindow()->ShowStatusMessage("Reconnecting...");
        XenConnectionUI::BeginConnect(conn, true, this->mainWindow(), false);
    }
}

QString ReconnectHostCommand::MenuText() const
{
    return "Reconnect";
}

bool ReconnectHostCommand::isConnectionDisconnected() const
{
    QList<XenConnection*> connections = this->getConnections();
    for (XenConnection* conn : connections)
    {
        if (conn && !conn->IsConnected())
            return true;
    }

    return false;
}

QList<XenConnection*> ReconnectHostCommand::getConnections() const
{
    if (!this->m_connections.isEmpty())
        return this->m_connections;

    QList<XenConnection*> connections;
    const QList<QSharedPointer<Host>> hosts = this->getHosts();
    for (const QSharedPointer<Host>& host : hosts)
    {
        if (host && host->GetConnection())
            connections.append(host->GetConnection());
    }

    if (!connections.isEmpty())
        return connections;

    QSharedPointer<XenObject> obj = this->getSelectedObject();
    if (obj && obj->GetConnection())
        connections.append(obj->GetConnection());

    return connections;
}
