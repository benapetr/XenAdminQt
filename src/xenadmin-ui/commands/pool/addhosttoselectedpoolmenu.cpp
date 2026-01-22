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

#include "addhosttoselectedpoolmenu.h"
#include "addhosttopoolcommand.h"
#include "addnewhosttopoolcommand.h"
#include "../../mainwindow.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/network/connectionsmanager.h"
#include "xenlib/xencache.h"
#include <QAction>
#include <algorithm>

AddHostToSelectedPoolMenu::AddHostToSelectedPoolMenu(MainWindow* mainWindow, QWidget* parent)
    : QMenu(parent)
    , mainWindow_(mainWindow)
{
    this->setTitle(tr("Add Server"));
    
    // Populate menu when about to show
    connect(this, &QMenu::aboutToShow, this, &AddHostToSelectedPoolMenu::onAboutToShow);
}

bool AddHostToSelectedPoolMenu::CanRun() const
{
    AddHostToSelectedPoolCommand cmd(this->mainWindow_);
    return cmd.CanRun();
}

void AddHostToSelectedPoolMenu::onAboutToShow()
{
    this->clear();
    
    QSharedPointer<Pool> selectedPool = this->getSelectedPool();
    if (!selectedPool)
        return;
    
    // Get all standalone hosts sorted by name
    QList<QSharedPointer<Host>> standaloneHosts = this->getSortedStandaloneHosts();
    
    // Add menu item for each standalone host
    for (const auto& host : standaloneHosts)
    {
        if (!host)
            continue;
        
        QString hostName = host->GetName();
        
        // TODO: Check Host::RestrictPooling when ported
        // If restricted, show different text format
        // if (Host::RestrictPooling(host))
        // {
        //     hostName = tr("Add Server %1").arg(hostName);
        // }
        
        // Truncate long names
        if (hostName.length() > 50)
            hostName = hostName.left(47) + "...";
        
        // Replace & with && to escape ampersands in menu text
        hostName.replace("&", "&&");
        
        QAction* action = this->addAction(hostName);
        
        // Capture host and pool for the lambda
        QSharedPointer<Host> hostCopy = host;
        QSharedPointer<Pool> poolCopy = selectedPool;
        
        connect(action, &QAction::triggered, this, [this, hostCopy, poolCopy]() {
            QList<QSharedPointer<Host>> hosts;
            hosts.append(hostCopy);
            AddHostToPoolCommand* cmd = new AddHostToPoolCommand(this->mainWindow_, hosts, poolCopy, true);
            cmd->Run();
            cmd->deleteLater();
        });
    }
    
    // Only add separator and "Connect and Add" if we have a valid pool
    if (selectedPool)
    {
        if (!this->actions().isEmpty())
            this->addSeparator();
        
        QAction* connectAndAddAction = this->addAction(tr("Connect and Add to Pool..."));
        QSharedPointer<Pool> poolCopy = selectedPool;
        
        connect(connectAndAddAction, &QAction::triggered, this, [this, poolCopy]() {
            AddNewHostToPoolCommand* cmd = new AddNewHostToPoolCommand(this->mainWindow_, poolCopy);
            cmd->Run();
            cmd->deleteLater();
        });
    }
}

QList<QSharedPointer<Host>> AddHostToSelectedPoolMenu::getSortedStandaloneHosts() const
{
    QList<QSharedPointer<Host>> hosts;
    
    // Get all connections
    QList<XenConnection*> connections = Xen::ConnectionsManager::instance()->GetAllConnections();
    
    for (XenConnection* conn : connections)
    {
        if (!conn || !conn->IsConnected())
            continue;
        
        // Skip if connection is part of a pool
        QSharedPointer<Pool> pool = conn->GetCache()->GetPool();
        if (pool)
            continue;
        
        // Get the standalone host - for standalone servers, there's exactly one host
        QList<QSharedPointer<Host>> allHosts = conn->GetCache()->GetAll<Host>("host");
        if (!allHosts.isEmpty())
        {
            // Use the first (and only) host
            hosts.append(allHosts.first());
        }
    }
    
    // Sort hosts by name
    std::sort(hosts.begin(), hosts.end(), [](const QSharedPointer<Host>& a, const QSharedPointer<Host>& b) {
        return a->GetName() < b->GetName();
    });
    
    return hosts;
}

QSharedPointer<Pool> AddHostToSelectedPoolMenu::getSelectedPool() const
{
    // TODO: Get selected pool from main window selection
    // For now, return nullptr
    
    return nullptr;
}

// AddHostToSelectedPoolCommand implementation

AddHostToSelectedPoolCommand::AddHostToSelectedPoolCommand(MainWindow* mainWindow)
    : Command(mainWindow)
{
}

bool AddHostToSelectedPoolCommand::CanRun() const
{
    // TODO: Get selection from main window
    // Need exactly one item selected and it must be in a pool
    
    // For now, check if we have at least one pool
    QList<XenConnection*> connections = Xen::ConnectionsManager::instance()->GetAllConnections();
    
    for (XenConnection* conn : connections)
    {
        if (!conn || !conn->IsConnected())
            continue;
        
        QSharedPointer<Pool> pool = conn->GetCache()->GetPool();
        if (pool)
            return true; // Found at least one pool
    }
    
    return false;
}

QString AddHostToSelectedPoolCommand::MenuText() const
{
    return tr("Add Server");
}
