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

#include <QMessageBox>
#include "restarttoolstackcommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib/xen/actions/host/restarttoolstackaction.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/host.h"

RestartToolstackCommand::RestartToolstackCommand(MainWindow* mainWindow, QObject* parent) : HostCommand(mainWindow, parent)
{
}

bool RestartToolstackCommand::CanRun() const
{
    const QList<QSharedPointer<Host>> hosts = this->getHosts();
    if (hosts.isEmpty())
        return false;

    for (const QSharedPointer<Host>& host : hosts)
    {
        if (host && host->IsLive())
            return true;
    }

    return false;
}

void RestartToolstackCommand::Run()
{
    const QList<QSharedPointer<Host>> hosts = this->getHosts();
    QList<QSharedPointer<Host>> runnable;
    for (const QSharedPointer<Host>& host : hosts)
    {
        if (host && host->IsLive())
            runnable.append(host);
    }

    if (runnable.isEmpty())
        return;

    const int count = runnable.count();
    const QString confirmTitle = "Restart Toolstack";
    const QString confirmText = count == 1
        ? QString("Are you sure you want to restart the toolstack on '%1'?\n\n"
                  "The management interface will restart. This may take a minute.")
              .arg(runnable.first()->GetName())
        : "Are you sure you want to restart the toolstack on the selected hosts?\n\n"
          "The management interface will restart. This may take a minute.";

    int ret = QMessageBox::warning(this->mainWindow(), confirmTitle, confirmText,
                                   QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes)
    {
        if (count == 1)
        {
            this->mainWindow()->ShowStatusMessage(QString("Restarting toolstack on '%1'...").arg(runnable.first()->GetName()));
        } else
        {
            this->mainWindow()->ShowStatusMessage(QString("Restarting toolstack on %1 hosts...").arg(count));
        }

        for (const QSharedPointer<Host>& host : runnable)
        {
            if (!host->IsConnected())
            {
                QMessageBox::warning(this->mainWindow(), "Not Connected", QString("Not connected to XenServer for host '%1'.").arg(host->GetName()));
                continue;
            }

            const QString hostName = host->GetName();
            RestartToolstackAction* action = new RestartToolstackAction(host, nullptr);
            OperationManager::instance()->RegisterOperation(action);

            connect(action, &AsyncOperation::completed, action, [this, hostName, action]()
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
}

QString RestartToolstackCommand::MenuText() const
{
    return "Restart Toolstack";
}
