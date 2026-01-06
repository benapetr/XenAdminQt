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
#include "../../dialogs/reconnectasdialog.h"
#include "../connection/disconnectcommand.h"
#include "../../network/xenconnectionui.h"
#include "xen/network/connection.h"
#include "xen/host.h"
#include <QtCore/QTimer>

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

    XenConnection* conn = host->GetConnection();
    if (!conn)
        return false;

    // Can reconnect-as if connected and selected host is coordinator
    if (conn->IsConnected() && host->IsMaster())
        return true;

    // Can also reconnect-as if connection is in progress (to change creds)
    if (conn->InProgress() && !conn->IsConnected())
        return true;

    return false;
}

void HostReconnectAsCommand::Run()
{
    QSharedPointer<Host> host = this->getSelectedHost();
    if (!host)
        return;

    XenConnection* conn = host->GetConnection();
    if (!conn)
        return;

    ReconnectAsDialog dialog(conn, mainWindow());
    if (dialog.exec() != QDialog::Accepted)
        return;

    conn->SetUsername(dialog.username().trimmed());
    conn->SetPassword(dialog.password());
    conn->SetExpectPasswordIsCorrect(true);

    if (this->m_disconnectHandler)
        QObject::disconnect(this->m_disconnectHandler);
    this->m_reconnectConnection = conn;
    this->m_disconnectHandler = connect(conn, &XenConnection::ConnectionStateChanged, this, &HostReconnectAsCommand::onReconnectConnectionStateChanged);

    DisconnectCommand disconnectCmd(this->mainWindow(), conn, true, this);
    disconnectCmd.Run();

    if (conn->IsConnected())
    {
        if (this->m_disconnectHandler)
            QObject::disconnect(this->m_disconnectHandler);
        this->m_disconnectHandler = QMetaObject::Connection();
        this->m_reconnectConnection.clear();
    }
}

QString HostReconnectAsCommand::MenuText() const
{
    return "Reconnect As...";
}

void HostReconnectAsCommand::onReconnectConnectionStateChanged()
{
    if (!this->m_reconnectConnection)
        return;

    if (this->m_reconnectConnection->IsConnected())
        return;

    if (this->m_disconnectHandler)
        QObject::disconnect(this->m_disconnectHandler);
    this->m_disconnectHandler = QMetaObject::Connection();

    QTimer::singleShot(500, this, &HostReconnectAsCommand::startReconnect);
}

void HostReconnectAsCommand::startReconnect()
{
    if (!this->m_reconnectConnection)
        return;

    this->mainWindow()->ShowStatusMessage("Reconnecting as different user...");
    XenConnectionUI::BeginConnect(this->m_reconnectConnection, true, this->mainWindow(), true);
}
