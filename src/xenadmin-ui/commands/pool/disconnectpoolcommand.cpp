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

#include "disconnectpoolcommand.h"
#include "../../mainwindow.h"
#include "xenlib.h"
#include "xen/network/connection.h"
#include "xencache.h"
#include <QMessageBox>

DisconnectPoolCommand::DisconnectPoolCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool DisconnectPoolCommand::CanRun() const
{
    // Can disconnect if:
    // 1. A pool is selected
    // 2. Connection is connected or in progress

    if (!this->mainWindow()->xenLib())
        return false;

    QString objectType = this->getSelectedObjectType();
    if (objectType != "pool")
        return false;

    XenConnection* conn = this->mainWindow()->xenLib()->getConnection();
    if (!conn)
        return false;

    // Can disconnect if connected
    return conn->isConnected();
}

void DisconnectPoolCommand::Run()
{
    if (!this->mainWindow()->xenLib())
        return;

    XenConnection* conn = this->mainWindow()->xenLib()->getConnection();
    if (!conn)
        return;

    QString poolRef = this->getSelectedObjectRef();

    // Get pool name for confirmation dialog
    QString poolName = "this pool";
    if (this->mainWindow()->xenLib()->getCache())
    {
        QVariantMap poolData = this->mainWindow()->xenLib()->getCache()->ResolveObjectData("pool", poolRef);
        if (!poolData.isEmpty())
        {
            poolName = poolData.value("name_label", "this pool").toString();
        }
    }

    // Show confirmation dialog
    QString message = QString("Are you sure you want to disconnect from pool '%1'?").arg(poolName);
    int ret = QMessageBox::question(this->mainWindow(), "Disconnect Pool",
                                    message,
                                    QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes)
    {
        this->mainWindow()->showStatusMessage(QString("Disconnecting from pool '%1'...").arg(poolName));
        conn->disconnect();
    }
}

QString DisconnectPoolCommand::MenuText() const
{
    return "Disconnect from Pool";
}

bool DisconnectPoolCommand::isPoolConnected() const
{
    if (!this->mainWindow()->xenLib())
        return false;

    XenConnection* conn = this->mainWindow()->xenLib()->getConnection();
    return conn && conn->isConnected();
}
