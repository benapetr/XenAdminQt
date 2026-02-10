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
#include "../../settingsmanager.h"
#include "../../connectionprofile.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/network/connectionsmanager.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xencache.h"
#include <QMessageBox>
#include <QDebug>

RemoveHostCommand::RemoveHostCommand(MainWindow* mainWindow, QObject* parent) : HostCommand(mainWindow, parent)
{
}

RemoveHostCommand::RemoveHostCommand(const QList<XenConnection*>& connections, MainWindow* mainWindow, QObject* parent) : HostCommand(mainWindow, parent), m_connections(connections)
{
}

bool RemoveHostCommand::CanRun() const
{
    QList<XenConnection*> connections = this->getConnections();
    if (!connections.isEmpty())
    {
        for (XenConnection* connection : connections)
        {
            if (!connection)
                continue;

            if (!connection->IsConnected())
                return true;

            QSharedPointer<Pool> pool = connection->GetCache()->GetPool();
            if (pool)
            {
                QSharedPointer<Host> master = pool->GetMasterHost();
                if (master && master->GetConnection() == connection)
                    return true;
            }
        }
    }

    QSharedPointer<Host> host = this->getSelectedHost();
    return this->canHostBeRemoved(host);
}

void RemoveHostCommand::Run()
{
    QList<XenConnection*> connections = this->getConnections();
    if (connections.isEmpty())
    {
        QSharedPointer<Host> host = this->getSelectedHost();
        if (host && host->GetConnection())
            connections.append(host->GetConnection());
    }

    if (connections.isEmpty())
        return;

    XenConnection* connection = connections.first();
    if (!connection)
        return;

    QString connection_hostname = connection->GetHostname();
    QString hostName = connection_hostname;
    QSharedPointer<Pool> pool = connection->GetCache() ? connection->GetCache()->GetPool() : QSharedPointer<Pool>();
    if (pool && pool->GetMasterHost())
        hostName = pool->GetMasterHost()->GetName();

    // Show confirmation dialog
    QMessageBox msgBox(MainWindow::instance());
    msgBox.setWindowTitle("Remove Host Connection");
    msgBox.setText(QString("Are you sure you want to remove the connection to '%1'?").arg(hostName));
    msgBox.setInformativeText("This will remove the host from your server list.\n"
                              "You can add it back later by connecting to it again.");
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    int ret = msgBox.exec();

    if (ret != QMessageBox::Yes)
        return;

    qDebug() << "RemoveHostCommand: Removing host connection" << hostName << "(" << connection_hostname << ")";

    // Disconnect if still connected
    if (connection->IsConnected() || connection->InProgress())
    {
        qDebug() << "RemoveHostCommand: Disconnecting from" << connection_hostname;
        connection->EndConnect(true, false);
    }

    QList<ConnectionProfile> profiles = SettingsManager::instance().LoadConnectionProfiles();
    for (const ConnectionProfile& profile : profiles)
    {
        if (profile.GetHostname() == connection_hostname && profile.GetPort() == connection->GetPort())
        {
            if (!profile.GetName().isEmpty())
                SettingsManager::instance().RemoveConnectionProfile(profile.GetName());
        }
    }

    SettingsManager::instance().Sync();
    MainWindow::instance()->SaveServerList();
    MainWindow::instance()->ShowStatusMessage(QString("Removed connection to '%1'").arg(hostName), 5000);
    MainWindow::instance()->RefreshServerTree();
}

QString RemoveHostCommand::MenuText() const
{
    return "Remove Host from XenAdmin";
}

bool RemoveHostCommand::canHostBeRemoved(QSharedPointer<Host> host) const
{
    if (!host)
        return false;

    XenConnection* connection = host->GetConnection();
    if (!connection)
        return false;

    // Can remove if:
    // 1. Connection is disconnected
    // 2. OR host is the pool coordinator (master)

    if (!connection->IsConnected())
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
    return host->IsMaster();
}

QList<XenConnection*> RemoveHostCommand::getConnections() const
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
