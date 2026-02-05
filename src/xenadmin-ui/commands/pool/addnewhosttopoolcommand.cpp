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
#include "xenadmin-ui/dialogs/addserverdialog.h"
#include "xenadmin-ui/network/xenconnectionui.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pooljoinrules.h"
#include "xenlib/xen/network/connectionsmanager.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xencache.h"
#include <QDebug>
#include <QMessageBox>

AddNewHostToPoolCommand::AddNewHostToPoolCommand(MainWindow* mainWindow,  QSharedPointer<Pool> pool) : Command(mainWindow), pool_(pool)
{
}

void AddNewHostToPoolCommand::Run()
{
    AddServerDialog dialog(nullptr, false, this->mainWindow());
    if (dialog.exec() != QDialog::Accepted)
        return;

    QString serverInput = dialog.serverInput();
    QString hostname = serverInput;
    int port = 443;

    const int lastColon = serverInput.lastIndexOf(':');
    if (lastColon > 0 && lastColon < serverInput.size() - 1)
    {
        bool ok = false;
        const int parsedPort = serverInput.mid(lastColon + 1).toInt(&ok);
        if (ok)
        {
            hostname = serverInput.left(lastColon).trimmed();
            port = parsedPort;
        }
    }

    XenConnection* connection = new XenConnection(nullptr);
    Xen::ConnectionsManager::instance()->AddConnection(connection);

    connection->SetHostname(hostname);
    connection->SetPort(port);
    connection->SetUsername(dialog.username());
    connection->SetPassword(dialog.password());
    connection->SetExpectPasswordIsCorrect(false);
    connection->SetFromDialog(true);

    connect(connection, &XenConnection::CachePopulated, this, [this, connection]() {
        this->onCachePopulated(connection);
    }, Qt::UniqueConnection);

    XenConnectionUI::BeginConnect(connection, true, this->mainWindow(), false);
}

void AddNewHostToPoolCommand::onCachePopulated(XenConnection* connection)
{
    if (!connection)
        return;

    disconnect(connection, &XenConnection::CachePopulated, this, nullptr);
    
    QSharedPointer<Host> hostToAdd = PoolJoinRules::GetCoordinator(connection);
    if (!hostToAdd)
    {
        qDebug() << "No hosts found in newly connected server";
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
