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

#include "addselectedhosttopoolmenu.h"
#include "addhosttopoolcommand.h"
#include "newpoolcommand.h"
#include "../../mainwindow.h"
#include "xen/host.h"
#include "xen/pool.h"
#include "xen/network/connection.h"
#include "xen/network/connectionsmanager.h"
#include "xen/xenobject.h"
#include "xencache.h"
#include <QAction>
#include <QTreeWidget>

AddSelectedHostToPoolMenu::AddSelectedHostToPoolMenu(MainWindow* mainWindow, QWidget* parent)
    : QMenu(parent)
    , mainWindow_(mainWindow)
{
    this->setTitle(tr("Add to Pool"));
    
    // Populate menu when about to show
    connect(this, &QMenu::aboutToShow, this, &AddSelectedHostToPoolMenu::onAboutToShow);
}

bool AddSelectedHostToPoolMenu::CanRun() const
{
    AddSelectedHostToPoolCommand cmd(this->mainWindow_);
    return cmd.CanRun();
}

void AddSelectedHostToPoolMenu::onAboutToShow()
{
    this->clear();
    
    QList<QSharedPointer<Host>> selectedHosts = this->getSelectedHosts();
    if (selectedHosts.isEmpty())
        return;
    
    // Get all connections and sort them
    QList<XenConnection*> connections = Xen::ConnectionsManager::instance()->GetAllConnections();
    
    // Add menu item for each pool
    for (XenConnection* connection : connections)
    {
        if (!connection)
            continue;
            
        QSharedPointer<Pool> pool = connection->GetCache()->GetPool();
        if (!pool)
            continue;
        
        QString poolName = pool->GetName();
        if (poolName.length() > 50)
            poolName = poolName.left(47) + "...";
        
        // Replace & with && to escape ampersands in menu text
        poolName.replace("&", "&&");
        
        QAction* action = this->addAction(poolName);
        
        // Capture pool and hosts for the lambda
        QSharedPointer<Pool> poolCopy = pool;
        QList<QSharedPointer<Host>> hostsCopy = selectedHosts;
        
        connect(action, &QAction::triggered, this, [this, hostsCopy, poolCopy]() {
            AddHostToPoolCommand* cmd = new AddHostToPoolCommand(this->mainWindow_, hostsCopy, poolCopy, true);
            cmd->Run();
            cmd->deleteLater();
        });
    }
    
    // Add separator and "New Pool" option
    if (!this->actions().isEmpty())
        this->addSeparator();
    
    QAction* newPoolAction = this->addAction(tr("New Pool..."));
    connect(newPoolAction, &QAction::triggered, this, [this, selectedHosts]() {
        NewPoolCommand* cmd = new NewPoolCommand(this->mainWindow_);
        cmd->Run();
        cmd->deleteLater();
    });
}

QList<QSharedPointer<Host>> AddSelectedHostToPoolMenu::getSelectedHosts() const
{
    QList<QSharedPointer<Host>> hosts;

    if (!this->mainWindow_)
        return hosts;

    QTreeWidget* tree = this->mainWindow_->GetServerTreeWidget();
    if (!tree)
        return hosts;

    const QList<QTreeWidgetItem*> selectedItems = tree->selectedItems();
    for (QTreeWidgetItem* item : selectedItems)
    {
        if (!item)
            continue;

        QVariant data = item->data(0, Qt::UserRole);
        if (!data.canConvert<QSharedPointer<XenObject>>())
            continue;

        QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
        if (!obj || obj->GetObjectType() != "host")
            continue;

        QSharedPointer<Host> host = qSharedPointerCast<Host>(obj);
        if (host)
            hosts.append(host);
    }

    return hosts;
}

// AddSelectedHostToPoolCommand implementation

AddSelectedHostToPoolCommand::AddSelectedHostToPoolCommand(MainWindow* mainWindow)
    : Command(mainWindow)
{
}

bool AddSelectedHostToPoolCommand::CanRun() const
{
    if (!this->mainWindow())
        return false;

    QTreeWidget* tree = this->mainWindow()->GetServerTreeWidget();
    if (!tree)
        return false;

    const QList<QTreeWidgetItem*> selectedItems = tree->selectedItems();
    if (selectedItems.isEmpty())
        return false;

    for (QTreeWidgetItem* item : selectedItems)
    {
        if (!item)
            continue;

        QVariant data = item->data(0, Qt::UserRole);
        if (!data.canConvert<QSharedPointer<XenObject>>())
            return false;

        QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
        if (!obj || obj->GetObjectType() != "host")
            return false;

        QSharedPointer<Host> host = qSharedPointerCast<Host>(obj);
        if (!this->canRunOnHost(host))
            return false;
    }

    return true;
}

QString AddSelectedHostToPoolCommand::MenuText() const
{
    // Check if any selected host has pooling restriction
    // TODO: When Host::RestrictPooling is ported, check for license restriction
    
    return tr("Add to Pool");
}

bool AddSelectedHostToPoolCommand::canRunOnHost(QSharedPointer<Host> host) const
{
    if (!host || !host->GetConnection() || !host->GetConnection()->IsConnected())
        return false;
    
    // Check if host is in a pool
    QSharedPointer<Pool> pool = host->GetConnection()->GetCache()->GetPool();
    if (pool)
        return false;
    
    // TODO: Check Host::RestrictPooling when ported
    // if (Host::RestrictPooling(host))
    //     return false;
    
    return true;
}
