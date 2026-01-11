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

#include "restarttoolstackcommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xen/actions/host/restarttoolstackaction.h"
#include "xen/network/connection.h"
#include <QMessageBox>
#include "xen/host.h"

RestartToolstackCommand::RestartToolstackCommand(MainWindow* mainWindow, QObject* parent) : HostCommand(mainWindow, parent)
{
}

bool RestartToolstackCommand::CanRun() const
{
    QSharedPointer<Host> host = this->getSelectedHost();
    if (!host)
        return false;

    // Can restart toolstack if host is live
    return host->IsLive();
}

void RestartToolstackCommand::Run()
{
    QSharedPointer<Host> host = this->getSelectedHost();
    if (!host)
        return;

    QString hostName = this->getSelectedHostName();

    // Show confirmation dialog
    int ret = QMessageBox::warning(this->mainWindow(), "Restart Toolstack",
                                   QString("Are you sure you want to restart the toolstack on '%1'?\n\n"
                                           "The management interface will restart. This may take a minute.")
                                       .arg(hostName),
                                   QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes)
    {
        this->mainWindow()->ShowStatusMessage(QString("Restarting toolstack on '%1'...").arg(hostName));

        XenConnection* conn = host->GetConnection();
        if (!conn || !conn->IsConnected())
        {
            QMessageBox::warning(this->mainWindow(), "Not Connected",
                                 "Not connected to XenServer");
            return;
        }

        RestartToolstackAction* action = new RestartToolstackAction(host, nullptr);
        OperationManager::instance()->RegisterOperation(action);

        connect(action, &AsyncOperation::completed, this, [this, hostName, action]()
        {
            if (action->GetState() == AsyncOperation::Completed && !action->IsFailed())
            {
                this->mainWindow()->ShowStatusMessage(QString("Toolstack restarted on '%1'").arg(hostName), 5000);
            }
            else
            {
                QMessageBox::warning(this->mainWindow(), "Restart Toolstack Failed",
                                     QString("Failed to restart toolstack on '%1'. Check the error log for details.").arg(hostName));
                this->mainWindow()->ShowStatusMessage("Toolstack restart failed", 5000);
            }
            action->deleteLater();
        });

        action->RunAsync();
    }
}

QString RestartToolstackCommand::MenuText() const
{
    return "Restart Toolstack";
}
