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

#include "removehostcommand.h"
#include "../../mainwindow.h"
#include "xen/network/connection.h"
#include "xen/host.h"
#include <QMessageBox>
#include <QDebug>

RemoveHostCommand::RemoveHostCommand(MainWindow* mainWindow, QObject* parent)
    : HostCommand(mainWindow, parent)
{
}

bool RemoveHostCommand::CanRun() const
{
    QSharedPointer<Host> host = this->getSelectedHost();
    if (!host)
        return false;

    return this->canHostBeRemoved(host);
}

void RemoveHostCommand::Run()
{
    QSharedPointer<Host> host = this->getSelectedHost();
    if (!host)
        return;

    XenConnection* connection = host->connection();
    if (!connection)
        return;

    QString hostname = connection->getHostname();
    QString hostName = host->nameLabel();

    // Show confirmation dialog
    QMessageBox msgBox(this->mainWindow());
    msgBox.setWindowTitle("Remove Host Connection");
    msgBox.setText(QString("Are you sure you want to remove the connection to '%1'?").arg(hostName));
    msgBox.setInformativeText("This will remove the host from your server list.\n"
                              "You can add it back later by connecting to it again.");
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    int ret = msgBox.exec();

    if (ret != QMessageBox::Yes)
    {
        return;
    }

    qDebug() << "RemoveHostCommand: Removing host connection" << hostName << "(" << hostname << ")";

    // Disconnect if still connected
    if (connection->isConnected())
    {
        qDebug() << "RemoveHostCommand: Disconnecting from" << hostname;
        connection->disconnect();
    }

    // Remove from connection profiles
    // This will be handled by MainWindow when connection is closed
    this->mainWindow()->showStatusMessage(QString("Removed connection to '%1'").arg(hostName), 5000);

    // TODO: Implement proper connection removal via ConnectionManager
    // For now, just disconnect - MainWindow should handle the rest
    QMessageBox::information(
        this->mainWindow(),
        "Remove Host",
        QString("Connection to '%1' has been disconnected.\n\n"
                "Full connection removal from server list will be implemented in ConnectionManager.")
            .arg(hostName));
}

QString RemoveHostCommand::MenuText() const
{
    return "Remove Host from XenAdmin";
}

bool RemoveHostCommand::canHostBeRemoved(QSharedPointer<Host> host) const
{
    if (!host)
        return false;

    XenConnection* connection = host->connection();
    if (!connection)
        return false;

    // Can remove if:
    // 1. Connection is disconnected
    // 2. OR host is the pool coordinator (master)

    if (!connection->isConnected())
    {
        // Disconnected host - can always remove
        return true;
    }

    // Connected - can only remove if this is the coordinator
    return this->isHostCoordinator(host);
}

bool RemoveHostCommand::isHostCoordinator(QSharedPointer<Host> host) const
{
    if (!host)
        return false;

    // Use the isMaster() method from Host class
    return host->isMaster();
}
