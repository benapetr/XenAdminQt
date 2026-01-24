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

#include "poweronhostcommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/actions/host/hostpoweronaction.h"
#include <QMessageBox>
#include <QPointer>

PowerOnHostCommand::PowerOnHostCommand(MainWindow* mainWindow, QObject* parent) : HostCommand(mainWindow, parent)
{
}

bool PowerOnHostCommand::CanRun() const
{
    const QList<QSharedPointer<Host>> hosts = this->getHosts();
    if (hosts.isEmpty())
        return false;

    for (const QSharedPointer<Host>& host : hosts)
    {
        if (this->canPowerOn(host))
            return true;
    }

    return false;
}

void PowerOnHostCommand::Run()
{
    // Matches C# PowerOnHostCommand.RunCore() logic
    const QList<QSharedPointer<Host>> hosts = this->getHosts();
    QList<QSharedPointer<Host>> runnable;
    for (const QSharedPointer<Host>& host : hosts)
    {
        if (this->canPowerOn(host))
            runnable.append(host);
    }

    if (runnable.isEmpty())
        return;

    for (const QSharedPointer<Host>& host : runnable)
    {
        const QString hostName = host->GetName();
        const QString powerOnMode = host->PowerOnMode();

        // Check if power_on_mode is set (matches C# GetCantRunReasonCore logic)
        if (powerOnMode.isEmpty())
        {
            QMessageBox::warning(MainWindow::instance(), "Cannot Power On Host", QString("Cannot power on host '%1' because its power-on mode is not set.\n\n"
                                         "Configure the host's management interface in the host properties.")
                                     .arg(hostName));
            continue;
        }

        if (!host->IsConnected())
        {
            QMessageBox::warning(MainWindow::instance(), "Not Connected", QString("Not connected to XenServer for host '%1'.").arg(hostName));
            continue;
        }

        HostPowerOnAction* action = new HostPowerOnAction(host, nullptr);

        OperationManager::instance()->RegisterOperation(action);

        connect(action, &AsyncOperation::completed, MainWindow::instance(), [hostName, action]()
        {
            if (!MainWindow::instance())
            {
                action->deleteLater();
                return;
            }

            if (action->GetState() == AsyncOperation::Completed && !action->IsFailed())
            {
                MainWindow::instance()->ShowStatusMessage(QString("Host '%1' power on initiated successfully").arg(hostName), 5000);
            } else
            {
                QMessageBox::warning(MainWindow::instance(), "Power On Host Failed", QString("Failed to power on host '%1'. Check the error log for details.").arg(hostName));
                MainWindow::instance()->ShowStatusMessage("Host power on failed", 5000);
            }
            action->deleteLater();
        });

        action->RunAsync();
    }
}

QString PowerOnHostCommand::MenuText() const
{
    // Matches C# Messages.MAINWINDOW_POWER_ON
    return "Power On";
}

QIcon PowerOnHostCommand::GetIcon() const
{
    return QIcon(":/icons/power_on.png");
}

bool PowerOnHostCommand::canPowerOn() const
{
    // Matches C# PowerOnHostCommand.CanRun() logic:
    // return host != null
    //     && !host.IsLive()
    //     && host.allowed_operations != null && host.allowed_operations.Contains(host_allowed_operations.power_on)
    //     && !HelpersGUI.HasActiveHostAction(host)
    //     && host.power_on_mode != "";

    QSharedPointer<Host> host = this->getSelectedHost();
    return this->canPowerOn(host);
}

bool PowerOnHostCommand::canPowerOn(const QSharedPointer<Host>& host) const
{
    if (!host)
        return false;

    // Check if host is not live (not running)
    // Note: PowerOn uses enabled field, not live field (different from isHostLive base class)
    if (!host->IsEnabled())
        return false;

    // Check if power_on is in allowed_operations
    if (!host->AllowedOperations().contains("power_on"))
        return false;

    // Check if host has active actions (matches C# HelpersGUI.HasActiveHostAction)
    if (this->hasActiveHostAction(host))
        return false;

    // Check if power_on_mode is set
    QString powerOnMode = host->PowerOnMode();
    if (powerOnMode.isEmpty())
        return false;

    return true;
}

bool PowerOnHostCommand::hasActiveHostAction() const
{
    QSharedPointer<Host> host = this->getSelectedHost();
    return this->hasActiveHostAction(host);
}

bool PowerOnHostCommand::hasActiveHostAction(const QSharedPointer<Host>& host) const
{
    // Matches C# HelpersGUI.HasActiveHostAction(host) logic
    // Check if host has current_operations (active tasks)
    if (!host)
        return false;

    return !host->CurrentOperations().isEmpty();
}
