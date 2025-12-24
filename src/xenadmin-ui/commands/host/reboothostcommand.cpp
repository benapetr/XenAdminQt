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

#include "reboothostcommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xen/network/connection.h"
#include "xen/host.h"
#include "xen/actions/host/reboothostaction.h"
#include <QMessageBox>

RebootHostCommand::RebootHostCommand(MainWindow* mainWindow, QObject* parent)
    : HostCommand(mainWindow, parent)
{
}

bool RebootHostCommand::CanRun() const
{
    QString hostRef = this->getSelectedHostRef();
    if (hostRef.isEmpty())
        return false;

    // Can reboot if host is enabled (not in maintenance mode)
    return this->isHostEnabled();
}

void RebootHostCommand::Run()
{
    QSharedPointer<Host> host = this->getSelectedHost();
    if (!host)
        return;

    QString hostRef = host->OpaqueRef();
    QString hostName = this->getSelectedHostName();

    // Show warning dialog
    int ret = QMessageBox::warning(this->mainWindow(), "Reboot Host",
                                   QString("Rebooting host '%1' will shut down all VMs running on it.\n\n"
                                           "Are you sure you want to continue?")
                                       .arg(hostName),
                                   QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes)
    {
        this->mainWindow()->showStatusMessage(QString("Rebooting host '%1'...").arg(hostName));

        XenConnection* conn = host->GetConnection();
        if (!conn || !conn->isConnected())
        {
            QMessageBox::warning(this->mainWindow(), "Not Connected",
                                 "Not connected to XenServer");
            return;
        }

        Host* host = new Host(conn, hostRef, this);
        RebootHostAction* action = new RebootHostAction(conn, host, this);

        OperationManager::instance()->registerOperation(action);

        connect(action, &AsyncOperation::completed, this, [this, hostName, action]()
        {
            if (action->state() == AsyncOperation::Completed && !action->isFailed())
            {
                this->mainWindow()->showStatusMessage(QString("Host '%1' reboot initiated successfully").arg(hostName), 5000);
            }
            else
            {
                QMessageBox::warning(this->mainWindow(), "Reboot Host Failed",
                                     QString("Failed to reboot host '%1'. Check the error log for details.").arg(hostName));
                this->mainWindow()->showStatusMessage("Host reboot failed", 5000);
            }
            action->deleteLater();
        });

        action->runAsync();
    }
}

QString RebootHostCommand::MenuText() const
{
    return "Reboot Host";
}
