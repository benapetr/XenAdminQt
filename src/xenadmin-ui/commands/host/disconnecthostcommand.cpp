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

#include "disconnecthostcommand.h"
#include "../../mainwindow.h"
#include "xenlib.h"
#include "xen/network/connection.h"
#include "xencache.h"
#include <QMessageBox>

DisconnectHostCommand::DisconnectHostCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool DisconnectHostCommand::CanRun() const
{
    // Can disconnect if:
    // 1. Connection is connected AND selected object is pool coordinator (host)
    // 2. OR connection is in progress (to cancel connection attempt)

    if (!this->mainWindow()->xenLib())
        return false;

    XenConnection* conn = this->mainWindow()->xenLib()->getConnection();
    if (!conn)
        return false;

    // Can disconnect if connected and selected host is coordinator
    if (conn->isConnected() && this->isSelectedHostCoordinator())
        return true;

    // Can also disconnect if connection is in progress (to cancel)
    // TODO: Need to add inProgress() method to XenConnection

    return false;
}

void DisconnectHostCommand::Run()
{
    if (!this->mainWindow()->xenLib())
        return;

    XenConnection* conn = this->mainWindow()->xenLib()->getConnection();
    if (!conn)
        return;

    // Show confirmation dialog
    int ret = QMessageBox::question(this->mainWindow(), "Disconnect",
                                    "Are you sure you want to disconnect from the server?",
                                    QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes)
    {
        this->mainWindow()->showStatusMessage("Disconnecting...");
        conn->disconnect();
    }
}

QString DisconnectHostCommand::MenuText() const
{
    return "Disconnect";
}

bool DisconnectHostCommand::isConnectionConnected() const
{
    if (!this->mainWindow()->xenLib())
        return false;

    XenConnection* conn = this->mainWindow()->xenLib()->getConnection();
    return conn && conn->isConnected();
}

bool DisconnectHostCommand::isSelectedHostCoordinator() const
{
    QString objectType = this->getSelectedObjectType();
    if (objectType != "host")
        return false;

    QString hostRef = this->getSelectedObjectRef();
    if (hostRef.isEmpty())
        return false;

    // Get host data from cache
    if (!this->mainWindow()->xenLib())
        return false;

    XenCache* cache = this->mainWindow()->xenLib()->getCache();
    if (!cache)
        return false;

    QVariantMap hostData = cache->ResolveObjectData("host", hostRef);
    if (hostData.isEmpty())
        return false;

    // Check if this is the pool coordinator/master
    // In XenServer, the coordinator has master=true or is the pool master
    return hostData.value("master", false).toBool();
}
