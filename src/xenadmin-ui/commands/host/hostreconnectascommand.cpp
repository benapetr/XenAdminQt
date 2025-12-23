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

#include "hostreconnectascommand.h"
#include "../../mainwindow.h"
#include "../../dialogs/connectdialog.h"
#include "xen/network/connection.h"
#include "xenlib.h"
#include "xen/host.h"

HostReconnectAsCommand::HostReconnectAsCommand(MainWindow* mainWindow, QObject* parent) : HostCommand(mainWindow, parent)
{
}

bool HostReconnectAsCommand::CanRun() const
{
    // Can reconnect as if:
    // 1. Connection is connected AND selected object is pool coordinator
    // 2. OR connection is in progress (to change credentials during connection)

    QSharedPointer<Host> host = this->getSelectedHost();
    if (!host)
        return false;

    XenConnection* conn = host->connection();
    if (!conn)
        return false;

    // Can reconnect-as if connected and selected host is coordinator
    if (conn->isConnected() && host->isMaster())
        return true;

    // Can also reconnect-as if connection is in progress (to change creds)
    // TODO: Need to add inProgress() method to XenConnection

    return false;
}

void HostReconnectAsCommand::Run()
{
    QSharedPointer<Host> host = this->getSelectedHost();
    if (!host)
        return;

    XenConnection* conn = host->connection();
    if (!conn)
        return;

    // Show reconnect dialog with current connection details
    ConnectDialog dialog(mainWindow());
    dialog.setWindowTitle("Reconnect As...");

    // Pre-fill with current connection details
    // Note: ConnectDialog doesn't have setHostname/setPort/setUsername methods
    // The dialog will use default/empty values

    if (dialog.exec() == QDialog::Accepted)
    {
        QString hostname = conn->getHostname(); // Keep same hostname
        int port = conn->getPort();             // Keep same port
        QString username = dialog.getUsername();
        QString password = dialog.getPassword();

        // Disconnect current connection
        conn->disconnect();

        // Reconnect with new credentials
        mainWindow()->showStatusMessage("Reconnecting as different user...");
        mainWindow()->xenLib()->connectToServer(hostname, port, username, password);
    }
}

QString HostReconnectAsCommand::MenuText() const
{
    return "Reconnect As...";
}
