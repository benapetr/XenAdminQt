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

#include "shutdownhostcommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/actions/host/shutdownhostaction.h"
#include <QMessageBox>

ShutdownHostCommand::ShutdownHostCommand(MainWindow* mainWindow, QObject* parent) : HostCommand(mainWindow, parent)
{
}

ShutdownHostCommand::ShutdownHostCommand(const QList<QSharedPointer<Host>>& hosts, MainWindow* mainWindow, QObject* parent) : HostCommand(hosts, mainWindow, parent)
{
}

namespace
{
    bool canShutdownHost(const QSharedPointer<Host>& host)
    {
        if (!host)
            return false;

        if (!host->IsLive())
            return false;

        return host->CurrentOperations().isEmpty();
    }
}

bool ShutdownHostCommand::CanRun() const
{
    const QList<QSharedPointer<Host>> hosts = this->getHosts();
    if (hosts.isEmpty())
        return false;

    for (const QSharedPointer<Host>& host : hosts)
    {
        if (canShutdownHost(host))
            return true;
    }

    return false;
}

void ShutdownHostCommand::Run()
{
    QList<QSharedPointer<Host>> runnable;
    const QList<QSharedPointer<Host>> hosts = this->getHosts();
    for (const QSharedPointer<Host>& host : hosts)
    {
        if (canShutdownHost(host))
            runnable.append(host);
    }

    if (runnable.isEmpty())
        return;

    // Show warning dialog
    const int count = runnable.count();
    const QString confirmTitle = count == 1 ? "Shutdown Host" : "Shutdown Hosts";
    const QString confirmText = count == 1
        ? QString("Shutting down host '%1' will shut down all VMs running on it.\n\n"
                  "Are you sure you want to continue?")
              .arg(runnable.first()->GetName())
        : "Shutting down these hosts will shut down all VMs running on them.\n\n"
          "Are you sure you want to continue?";

    int ret = QMessageBox::warning(this->mainWindow(), confirmTitle, confirmText,
                                   QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes)
    {
        if (count == 1)
        {
            this->mainWindow()->ShowStatusMessage(
                QString("Shutting down host '%1'...").arg(runnable.first()->GetName()));
        }
        else
        {
            this->mainWindow()->ShowStatusMessage(QString("Shutting down %1 hosts...").arg(count));
        }

        for (const QSharedPointer<Host>& host : runnable)
        {
            if (!host)
                continue;

            XenConnection* conn = host->GetConnection();
            if (!conn || !conn->IsConnected())
            {
                QMessageBox::warning(this->mainWindow(), "Not Connected",
                                     QString("Not connected to XenServer for host '%1'.")
                                         .arg(host->GetName()));
                continue;
            }

            const QString hostName = host->GetName();
            ShutdownHostAction* action = new ShutdownHostAction(host, nullptr);

            OperationManager::instance()->RegisterOperation(action);

            connect(action, &AsyncOperation::completed, this, [this, hostName, action]()
            {
                if (action->GetState() == AsyncOperation::Completed && !action->IsFailed())
                {
                    this->mainWindow()->ShowStatusMessage(QString("Host '%1' shutdown initiated successfully").arg(hostName), 5000);
                }
                else
                {
                    QMessageBox::warning(this->mainWindow(), "Shutdown Host Failed",
                                         QString("Failed to shutdown host '%1'. Check the error log for details.").arg(hostName));
                    this->mainWindow()->ShowStatusMessage("Host shutdown failed", 5000);
                }
                action->deleteLater();
            });

            action->RunAsync();
        }
    }
}

QString ShutdownHostCommand::MenuText() const
{
    return "Shutdown Host";
}

QIcon ShutdownHostCommand::GetIcon() const
{
    return QIcon(":/icons/shutdown.png");
}
