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

#include "removehostfrompoolcommand.h"
#include "../../mainwindow.h"
#include "xenadmin-ui/network/xenconnectionui.h"
#include "xenlib/xen/actions/pool/ejecthostfrompoolaction.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/network/connectionsmanager.h"
#include <QMessageBox>
#include <QSharedPointer>
#include <QTcpSocket>
#include <QThread>

namespace
{
    constexpr int kInitialDelaySeconds = 30;
    constexpr int kRetryIntervalSeconds = 15;
    constexpr int kMaxRetries = 27;
}

RemoveHostFromPoolCommand::RemoveHostFromPoolCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

RemoveHostFromPoolCommand::RemoveHostFromPoolCommand(MainWindow* mainWindow, const QSharedPointer<Host>& host, QObject* parent) : Command(mainWindow, parent)
{
    if (host)
        this->SetSelectionOverride(QList<QSharedPointer<XenObject>>{host});
}

RemoveHostFromPoolCommand::RemoveHostFromPoolCommand(MainWindow* mainWindow, const QList<QSharedPointer<Host>>& hosts, QObject* parent) : Command(mainWindow, parent)
{
    QList<QSharedPointer<XenObject>> objects;
    objects.reserve(hosts.size());
    for (const QSharedPointer<Host>& host : hosts)
    {
        if (host)
            objects.append(host);
    }
    if (!objects.isEmpty())
        this->SetSelectionOverride(objects);
}

bool RemoveHostFromPoolCommand::CanRun() const
{
    QList<QSharedPointer<XenObject>> selected = this->getSelectedObjects();
    QList<QSharedPointer<Host>> hosts = this->selectedHosts();
    if (hosts.isEmpty() || hosts.size() != selected.size())
        return false;

    XenConnection* connection = hosts.first()->GetConnection();
    if (!connection)
        return false;

    QString poolRef = hosts.first()->GetPoolRef();
    if (poolRef.isEmpty())
        return false;

    for (const QSharedPointer<Host>& host : hosts)
    {
        if (!host || host->GetConnection() != connection)
            return false;
        if (host->GetPoolRef() != poolRef)
            return false;
        if (!RemoveHostFromPoolCommand::CanRunForHost(host))
            return false;
    }

    return true;
}

void RemoveHostFromPoolCommand::Run()
{
    QList<QSharedPointer<Host>> hosts = this->selectedHosts();
    if (hosts.isEmpty())
        return;

    if (!this->CanRun())
        return;

    QSharedPointer<Pool> pool = hosts.first()->GetPool();
    QString poolName = pool ? pool->GetName() : QString();

    QString confirmation;
    if (hosts.size() == 1)
    {
        QString hostName = hosts.first()->GetName();
        confirmation = QString("Are you sure you want to eject '%1' from the pool '%2'?\n\n"
                               "The host will become a standalone server and will need to be rebooted.\n"
                               "All running VMs on this host will be shut down.")
                           .arg(hostName, poolName);
    } else
    {
        confirmation = QString("Are you sure you want to eject the selected hosts from the pool '%1'?")
                           .arg(poolName);
    }

    int result = QMessageBox::question(MainWindow::instance(), "Remove Server from Pool", confirmation, QMessageBox::Yes | QMessageBox::No);
    if (result != QMessageBox::Yes)
        return;

    QList<AsyncOperation*> actions;
    actions.reserve(hosts.size());

    for (const QSharedPointer<Host>& host : hosts)
    {
        XenConnection* hostConnection = host->GetConnection();
        if (!hostConnection)
            continue;

        QString hostname = host->GetAddress();
        int port = hostConnection->GetPort();
        QString username = hostConnection->GetUsername();
        QString password = hostConnection->GetPassword();

        XenConnection* newConnection = nullptr;
        if (!hostname.isEmpty())
        {
            newConnection = new XenConnection(nullptr);
            newConnection->SetHostname(hostname);
            newConnection->SetPort(port);
            newConnection->SetUsername(username);
            newConnection->SetPassword(password);
            newConnection->SetExpectPasswordIsCorrect(false);
            newConnection->SetFromDialog(false);
            Xen::ConnectionsManager::instance()->AddConnection(newConnection);
        }

        EjectHostFromPoolAction* action = new EjectHostFromPoolAction(pool, host, nullptr);

        connect(action, &AsyncOperation::completed, this, [action, newConnection, hostname, port]()
        {
            if (newConnection)
                RemoveHostFromPoolCommand::scheduleReconnect(MainWindow::instance(), newConnection, hostname, port);
            action->deleteLater();
        });

        connect(action, &AsyncOperation::failed, this, [newConnection]()
        {
            if (newConnection)
            {
                Xen::ConnectionsManager::instance()->RemoveConnection(newConnection);
                newConnection->deleteLater();
            }
        });

        actions.append(action);
    }

    if (actions.isEmpty())
        return;

    this->RunMultipleActions(actions,
                             tr("Removing Servers from Pool"),
                             tr("Removing Servers from Pool"),
                             tr("Removed"),
                             true);
}

QString RemoveHostFromPoolCommand::MenuText() const
{
    return tr("Remove Server from Pool...");
}

bool RemoveHostFromPoolCommand::CanRunForHost(const QSharedPointer<Host>& host)
{
    if (!host || !host->IsValid())
        return false;

    if (host->GetPoolRef().isEmpty())
        return false;

    if (host->IsMaster())
        return false;

    if (!host->IsLive())
        return false;

    if (host->GetResidentVMRefs().size() > 1)
        return false;

    return true;
}

QList<QSharedPointer<Host>> RemoveHostFromPoolCommand::selectedHosts() const
{
    QList<QSharedPointer<Host>> hosts;
    QList<QSharedPointer<XenObject>> selected = this->getSelectedObjects();

    for (const QSharedPointer<XenObject>& obj : selected)
    {
        QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(obj);
        if (host)
            hosts.append(host);
    }

    return hosts;
}

void RemoveHostFromPoolCommand::scheduleReconnect(MainWindow* mainWindow, XenConnection* connection, const QString& hostname, int port)
{
    if (!mainWindow || !connection)
        return;

    QThread* thread = QThread::create([mainWindow, connection, hostname, port]() {
        QThread::sleep(kInitialDelaySeconds);

        int attempts = 0;
        while (attempts <= kMaxRetries)
        {
            QTcpSocket socket;
            socket.connectToHost(hostname, port);
            if (socket.waitForConnected(2000))
            {
                socket.disconnectFromHost();
                QMetaObject::invokeMethod(mainWindow, [mainWindow, connection]() {
                    XenConnectionUI::BeginConnect(connection, false, mainWindow, false);
                }, Qt::QueuedConnection);
                return;
            }

            attempts++;
            QThread::sleep(kRetryIntervalSeconds);
        }

        QMetaObject::invokeMethod(mainWindow, [mainWindow, hostname]() {
            QMessageBox::critical(mainWindow, "Reconnect Failed", QString("Failed to reconnect to '%1' after removing it from the pool.").arg(hostname));
        }, Qt::QueuedConnection);
    });

    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}
