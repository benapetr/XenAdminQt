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
#include "../../dialogs/commanderrordialog.h"
#include "../../operations/operationmanager.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/actions/host/reboothostaction.h"
#include <QMessageBox>
#include <QTreeWidget>

RebootHostCommand::RebootHostCommand(MainWindow* mainWindow, QObject* parent) : HostCommand(mainWindow, parent)
{
}

bool RebootHostCommand::CanRun() const
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

void RebootHostCommand::Run()
{
    QList<QSharedPointer<Host>> runnable;
    QHash<QSharedPointer<XenObject>, QString> cantRunReasons;

    const QList<QSharedPointer<Host>> hosts = this->getHosts();
    for (const QSharedPointer<Host>& host : hosts)
    {
        if (!host)
            continue;

        if (host->IsLive())
            runnable.append(host);
        else
            cantRunReasons.insert(host, "Host is not live.");
    }

    if (runnable.isEmpty() && cantRunReasons.isEmpty())
        return;

    if (!cantRunReasons.isEmpty())
    {
        CommandErrorDialog::DialogMode mode =
            runnable.isEmpty() ? CommandErrorDialog::DialogMode::Close
                               : CommandErrorDialog::DialogMode::OKCancel;
        CommandErrorDialog dialog("Reboot Host", "Some hosts cannot be rebooted.", cantRunReasons, mode, MainWindow::instance());
        if (dialog.exec() != QDialog::Accepted || runnable.isEmpty())
            return;
    }

    bool hasRunningVMs = false;
    for (const QSharedPointer<Host>& host : runnable)
    {
        if (host && host->HasRunningVMs())
        {
            hasRunningVMs = true;
            break;
        }
    }

    // Show warning dialog
    // TODO: Add HCI warning handling once we have equivalent APIs.
    const int count = runnable.count();
    const QString confirmTitle = count == 1 ? "Reboot Host" : "Reboot Hosts";
    const QString confirmText = count == 1
        ? QString(hasRunningVMs
                      ? "Rebooting host '%1' will shut down all VMs running on it.\n\n"
                        "Are you sure you want to continue?"
                      : "Rebooting host '%1' will restart this host.\n\n"
                        "Are you sure you want to continue?")
              .arg(runnable.first()->GetName())
        : QString(hasRunningVMs
                      ? "Rebooting these hosts will shut down all VMs running on them.\n\n"
                        "Are you sure you want to continue?"
                      : "Rebooting these hosts will restart them.\n\n"
                        "Are you sure you want to continue?");

    QMessageBox msgBox(MainWindow::instance());
    msgBox.setWindowTitle(confirmTitle);
    msgBox.setText(confirmText);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    int ret = msgBox.exec();

    if (ret == QMessageBox::Yes)
    {
        if (count == 1)
        {
            MainWindow::instance()->ShowStatusMessage(QString("Rebooting host '%1'...").arg(runnable.first()->GetName()));
        }
        else
        {
            MainWindow::instance()->ShowStatusMessage(QString("Rebooting %1 hosts...").arg(count));
        }

        for (const QSharedPointer<Host>& host : runnable)
        {
            if (!host)
                continue;

            if (!host->IsConnected())
            {
                QMessageBox::warning(MainWindow::instance(), "Not Connected", QString("Not connected to XenServer for host '%1'.").arg(host->GetName()));
                continue;
            }

            const QString hostName = host->GetName();
            auto ntolPrompt = [](QSharedPointer<Pool> pool, qint64 current, qint64 target) {
                const QString poolName = pool ? pool->GetName() : QString();
                const QString poolLabel = poolName.isEmpty() ? "pool" : QString("pool '%1'").arg(poolName);
                const QString text = QString("HA is enabled for %1.\n\n"
                                             "To reboot this host, the pool's host failures to tolerate must be "
                                             "reduced from %2 to %3.\n\n"
                                             "Do you want to continue?")
                                         .arg(poolLabel)
                                         .arg(current)
                                         .arg(target);

                return QMessageBox::question(MainWindow::instance(),
                                             "Adjust HA Failures to Tolerate",
                                             text,
                                             QMessageBox::Yes | QMessageBox::No,
                                             QMessageBox::No) != QMessageBox::Yes;
            };

            RebootHostAction* action = new RebootHostAction(host, ntolPrompt, nullptr);

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
                    MainWindow::instance()->ShowStatusMessage(QString("Host '%1' reboot initiated successfully").arg(hostName), 5000);
                } else
                {
                    // TODO: Add detailed error dialog and cant-run reason handling like C#.
                    QMessageBox::warning(MainWindow::instance(), "Reboot Host Failed", QString("Failed to reboot host '%1'. Check the error log for details.").arg(hostName));
                    MainWindow::instance()->ShowStatusMessage("Host reboot failed", 5000);
                }
                action->deleteLater();
            });

            action->RunAsync();
        }
    }
}

QString RebootHostCommand::MenuText() const
{
    return "Reboot Host";
}

QIcon RebootHostCommand::GetIcon() const
{
    return QIcon(":/icons/reboot.png");
}
