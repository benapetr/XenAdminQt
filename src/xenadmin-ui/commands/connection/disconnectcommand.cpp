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

#include "disconnectcommand.h"
#include "../../mainwindow.h"
#include "xen/network/connection.h"
#include <QMessageBox>
#include <QMetaObject>

DisconnectCommand::DisconnectCommand(MainWindow* mainWindow,
                                     XenConnection* connection,
                                     bool prompt,
                                     QObject* parent)
    : Command(mainWindow, parent),
      m_connection(connection),
      m_prompt(prompt)
{
}

bool DisconnectCommand::CanRun() const
{
    return m_connection && (m_connection->IsConnected() || m_connection->InProgress());
}

void DisconnectCommand::Run()
{
    if (!CanRun())
        return;

    if (m_prompt && !confirmDisconnect())
        return;

    doDisconnect();
}

QString DisconnectCommand::MenuText() const
{
    return "Disconnect";
}

bool DisconnectCommand::confirmDisconnect() const
{
    return QMessageBox::question(this->mainWindow(),
                                 "Disconnect",
                                 "Are you sure you want to disconnect from the server?",
                                 QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
}

void DisconnectCommand::doDisconnect()
{
    if (!m_connection)
        return;

    this->mainWindow()->showStatusMessage("Disconnecting...");

    m_connection->EndConnect(true, false);
    QMetaObject::invokeMethod(this->mainWindow(), "SaveServerList", Qt::QueuedConnection);

    // TODO: mirror C# DisconnectCommand.ConfirmCancelRunningActions by canceling running actions
    // and showing a progress dialog while tasks are canceled.
}
