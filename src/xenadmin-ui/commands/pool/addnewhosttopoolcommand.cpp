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

#include "addnewhosttopoolcommand.h"
#include "addhosttopoolcommand.h"
#include "../../mainwindow.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/host.h"
#include "xenlib/xencache.h"
#include <QMessageBox>

AddNewHostToPoolCommand::AddNewHostToPoolCommand(MainWindow* mainWindow,  QSharedPointer<Pool> pool) : Command(mainWindow), pool_(pool)
{
}

void AddNewHostToPoolCommand::Run()
{
    // TODO: Show AddServerDialog when ported
    // For now, show placeholder message
    QMessageBox::information(this->mainWindow(), 
                           tr("Add New Server"), 
                           tr("Add Server dialog will be shown here.\n\n"
                              "After connecting to the new server, it will be added to pool '%1'.\n\n"
                              "TODO: Implement AddServerDialog")
                           .arg(this->pool_ ? this->pool_->GetName() : tr("Unknown Pool")));
    
    // TODO: When AddServerDialog is implemented:
    // 1. Show the dialog
    // 2. Connect to the dialog's cachePopulated signal
    // 3. In the slot, get the coordinator of the new connection
    // 4. Check if the new host is already in a pool
    // 5. If not, create and run AddHostToPoolCommand with confirm=false
}

void AddNewHostToPoolCommand::onCachePopulated(XenConnection* connection)
{
    if (!connection)
        return;
    
    // Get the standalone host - for standalone servers, there's exactly one host
    QList<QSharedPointer<Host>> allHosts = connection->GetCache()->GetAll<Host>("host");
    if (allHosts.isEmpty())
    {
        qDebug() << "No hosts found in newly connected server";
        return;
    }
    
    QSharedPointer<Host> hostToAdd = allHosts.first();
    if (!hostToAdd)
    {
        qDebug() << "hostToAdd is null while joining host to pool in AddNewHostToPoolCommand";
        return;
    }
    
    // Check if newly-connected host is already in a pool
    QSharedPointer<Pool> hostPool = connection->GetCache()->GetPool();
    
    if (hostPool)
    {
        QString message = tr("Host '%1' is already in pool '%2'. Cannot add to pool '%3'.")
                         .arg(hostToAdd->GetName())
                         .arg(hostPool->GetName())
                         .arg(this->pool_ ? this->pool_->GetName() : tr("Unknown Pool"));
        
        QMessageBox::warning(this->mainWindow(), tr("Host Already in Pool"), message);
    }
    else
    {
        // Add the host to the pool without confirmation (user already confirmed by connecting)
        QList<QSharedPointer<Host>> hosts;
        hosts.append(hostToAdd);
        
        AddHostToPoolCommand* cmd = new AddHostToPoolCommand(this->mainWindow(), hosts, this->pool_, false);
        cmd->Run();
        delete cmd;
    }
}

QString AddNewHostToPoolCommand::MenuText() const
{
    return tr("Connect and Add to Pool...");
}
