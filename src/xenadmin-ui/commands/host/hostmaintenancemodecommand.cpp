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

#include "hostmaintenancemodecommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/actions/host/evacuatehostaction.h"
#include "xenlib/xen/actions/host/enablehostaction.h"
#include <QMessageBox>
#include <QTimer>

HostMaintenanceModeCommand::HostMaintenanceModeCommand(MainWindow* mainWindow, bool enterMode, QObject* parent) : HostCommand(mainWindow, parent), m_enterMode(enterMode)
{
}

HostMaintenanceModeCommand::HostMaintenanceModeCommand(MainWindow* mainWindow, const QStringList& selection, bool enterMode, QObject* parent) : HostCommand(mainWindow, parent), m_enterMode(enterMode)
{
    Q_UNUSED(selection);
}

bool HostMaintenanceModeCommand::CanRun() const
{
    QSharedPointer<Host> host = this->getSelectedHost();
    if (!host)
        return false;

    // Check if host exists and we have a valid connection
    if (!host->GetConnection())
        return false;

    // For enter mode: host must be IsEnabled
    // For exit mode: host must be disabled (in maintenance mode)
    bool hostEnabled = host->IsEnabled();

    if (this->m_enterMode)
        return hostEnabled; // Can only enter maintenance if host is currently enabled
    else
        return !hostEnabled; // Can only exit maintenance if host is currently disabled
}

void HostMaintenanceModeCommand::Run()
{
    if (!this->CanRun())
        return;

    QSharedPointer<Host> host = this->getSelectedHost();
    if (!host)
        return;

    QString hostName = this->getSelectedObjectName();

    MainWindow* mw = MainWindow::instance();
    if (!mw)
        return;

    if (this->m_enterMode)
    {
        // Entering maintenance mode
        int ret = QMessageBox::question(mw, "Enter Maintenance Mode",
                                        QString("Entering maintenance mode will disable host '%1' and migrate "
                                                "all VMs to other hosts in the pool.\n\n"
                                                "Are you sure you want to continue?")
                                            .arg(hostName),
                                        QMessageBox::Yes | QMessageBox::No);

        if (ret == QMessageBox::Yes)
        {
            mw->ShowStatusMessage(QString("Entering maintenance mode for host '%1'...").arg(hostName));

            if (!host->IsConnected())
            {
                QMessageBox::warning(mw, "Not Connected", "Not connected to XenServer");
                return;
            }

            auto ntolPrompt = [this](QSharedPointer<Pool> pool, qint64 current, qint64 target) {
                const QString poolName = pool ? pool->GetName() : QString();
                const QString poolLabel = poolName.isEmpty() ? "pool" : QString("pool '%1'").arg(poolName);
                const QString text = QString("HA is enabled for %1.\n\n"
                                             "To enter maintenance mode, the pool's host failures to tolerate must be "
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

            EvacuateHostAction* action = new EvacuateHostAction(host, QSharedPointer<Host>(), ntolPrompt, nullptr, nullptr);

            OperationManager::instance()->RegisterOperation(action);

            connect(action, &AsyncOperation::completed, mw, [mw, hostName, action]()
            {
                if (action->GetState() == AsyncOperation::Completed && !action->IsFailed())
                {
                    mw->ShowStatusMessage(QString("Host '%1' entered maintenance mode successfully").arg(hostName), 5000);
                    QTimer::singleShot(2000, mw, &MainWindow::RefreshServerTree);
                } else
                {
                    QMessageBox::warning(mw, "Enter Maintenance Mode Failed", QString("Failed to enter maintenance mode for host '%1'. Check the error log for details.").arg(hostName));
                    mw->ShowStatusMessage("Enter maintenance mode failed", 5000);
                }
                action->deleteLater();
            });

            action->RunAsync();
        }
    } else
    {
        // Exiting maintenance mode
        int ret = QMessageBox::question(mw, "Exit Maintenance Mode", QString("Are you sure you want to exit maintenance mode for host '%1'?").arg(hostName), QMessageBox::Yes | QMessageBox::No);

        if (ret == QMessageBox::Yes)
        {
            mw->ShowStatusMessage(QString("Exiting maintenance mode for host '%1'...").arg(hostName));

            if (!host->IsConnected())
            {
                QMessageBox::warning(mw, "Not Connected", "Not connected to XenServer");
                return;
            }

            auto ntolIncreasePrompt = [](QSharedPointer<Pool> pool, QSharedPointer<Host> targetHost,
                                             qint64 current, qint64 target) {
                const QString poolName = pool ? pool->GetName() : QString();
                const QString hostNameLocal = targetHost ? targetHost->GetName() : QString();
                const QString poolLabel = poolName.isEmpty() ? "pool" : QString("pool '%1'").arg(poolName);
                const QString hostLabel = hostNameLocal.isEmpty() ? "the host" : QString("host '%1'").arg(hostNameLocal);
                const QString text = QString("HA is enabled for %1.\n\n"
                                             "Now that %2 is enabled, the pool's host failures to tolerate can be "
                                             "increased from %3 to %4.\n\n"
                                             "Do you want to increase it?")
                                         .arg(poolLabel)
                                         .arg(hostLabel)
                                         .arg(current)
                                         .arg(target);

                return QMessageBox::question(MainWindow::instance(),
                                             "Increase HA Failures to Tolerate",
                                             text,
                                             QMessageBox::Yes | QMessageBox::No,
                                             QMessageBox::No) == QMessageBox::Yes;
            };

            EnableHostAction* action = new EnableHostAction(host, false, ntolIncreasePrompt, nullptr);

            OperationManager::instance()->RegisterOperation(action);

            connect(action, &AsyncOperation::completed, MainWindow::instance(), [hostName, action]()
            {
                if (!MainWindow::instance())
                    return;

                if (action->GetState() == AsyncOperation::Completed && !action->IsFailed())
                {
                    MainWindow::instance()->ShowStatusMessage(QString("Host '%1' exited maintenance mode successfully").arg(hostName), 5000);
                    QTimer::singleShot(2000, MainWindow::instance(), &MainWindow::RefreshServerTree);
                } else
                {
                    QMessageBox::warning(MainWindow::instance(), "Exit Maintenance Mode Failed", QString("Failed to exit maintenance mode for host '%1'. Check the error log for details.").arg(hostName));
                    MainWindow::instance()->ShowStatusMessage("Exit maintenance mode failed", 5000);
                }
                action->deleteLater();
            });

            action->RunAsync();
        }
    }
}

QString HostMaintenanceModeCommand::MenuText() const
{
    if (this->m_enterMode)
        return "Enter Maintenance Mode";
    else
        return "Exit Maintenance Mode";
}
