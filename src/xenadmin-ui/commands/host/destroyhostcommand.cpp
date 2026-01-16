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

#include "destroyhostcommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib/xen/actions/host/destroyhostaction.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/hostmetrics.h"
#include "xenlib/xencache.h"
#include <QMessageBox>

namespace
{
    bool canDestroyHost(const QSharedPointer<Host>& host)
    {
        if (!host)
            return false;

        if (host->GetPoolRef().isEmpty())
            return false;

        if (host->IsMaster())
            return false;

        if (host->IsConnected())
        {
            QSharedPointer<HostMetrics> metrics = host->GetMetrics();
            if (metrics && metrics->IsLive())
                return false;
        }

        return true;
    }
}

DestroyHostCommand::DestroyHostCommand(MainWindow* mainWindow, QObject* parent) : HostCommand(mainWindow, parent)
{
}

DestroyHostCommand::DestroyHostCommand(const QList<QSharedPointer<Host>>& hosts, MainWindow* mainWindow, QObject* parent) : HostCommand(hosts, mainWindow, parent)
{
}

bool DestroyHostCommand::CanRun() const
{
    const QList<QSharedPointer<Host>> hosts = this->getHosts();
    if (hosts.isEmpty())
        return false;

    for (const QSharedPointer<Host>& host : hosts)
    {
        if (canDestroyHost(host))
            return true;
    }

    return false;
}

void DestroyHostCommand::Run()
{
    QList<QSharedPointer<Host>> runnable;
    const QList<QSharedPointer<Host>> hosts = this->getHosts();
    for (const QSharedPointer<Host>& host : hosts)
    {
        if (canDestroyHost(host))
            runnable.append(host);
    }

    if (runnable.isEmpty())
        return;

    const QString title = this->buildConfirmationTitle();
    QMessageBox msgBox(this->mainWindow());
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setWindowTitle(title);
    msgBox.setText(runnable.count() == 1
                       ? this->buildConfirmationMessage(runnable.first())
                       : tr("Are you sure you want to destroy the selected hosts?\n\n"
                            "This will permanently remove the hosts from the pool. "
                            "This operation cannot be undone."));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    if (msgBox.exec() != QMessageBox::Yes)
        return;

    for (const QSharedPointer<Host>& host : runnable)
    {
        const QString hostName = host->GetName();
        XenConnection* conn = host->GetConnection();
        if (!conn || !conn->IsConnected())
        {
            QMessageBox::warning(this->mainWindow(), tr("Not Connected"), tr("Not connected to XenServer for host '%1'.").arg(hostName));
            continue;
        }

        DestroyHostAction* action = new DestroyHostAction(host, nullptr);
        action->SetTitle(tr("Destroying host '%1'...").arg(hostName));
        OperationManager::instance()->RegisterOperation(action);
        action->RunAsync(true);
        this->mainWindow()->ShowStatusMessage(tr("Destroying host: %1").arg(hostName), 5000);
    }
}

QString DestroyHostCommand::MenuText() const
{
    return tr("&Destroy Host");
}

QString DestroyHostCommand::buildConfirmationMessage(QSharedPointer<Host> host) const
{
    QString hostName = host ? host->GetName() : QString();

    return tr("Are you sure you want to destroy host '%1'?\n\n"
              "This will permanently remove the host from the pool. "
              "This operation cannot be undone.")
        .arg(hostName);
}

QString DestroyHostCommand::buildConfirmationTitle() const
{
    return tr("Confirm Destroy Host");
}

bool DestroyHostCommand::isHostLive(QSharedPointer<Host> host) const
{
    if (!host->IsConnected())
        return false;

    // Check if host is live (has a metrics object with live flag)
    QSharedPointer<HostMetrics> metrics = host->GetMetrics();
    if (metrics)
        return metrics->IsLive();

    // If no metrics, assume host is live to be safe
    return true;
}
