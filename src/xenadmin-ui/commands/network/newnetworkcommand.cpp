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

#include "newnetworkcommand.h"
#include <QDebug>
#include "../../mainwindow.h"
#include "../../dialogs/newnetworkwizard.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/host.h"
#include <QtWidgets>

NewNetworkCommand::NewNetworkCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
    qDebug() << "NewNetworkCommand: Created with MainWindow";
}

void NewNetworkCommand::Run()
{
    qDebug() << "NewNetworkCommand: Executing New Network command";

    if (!this->CanRun())
    {
        qWarning() << "NewNetworkCommand: Cannot execute - requirements not met";
        QMessageBox::warning(nullptr, tr("Cannot Create Network"),
                             tr("Network creation is not available at this time.\n"
                                "Please select a host or pool, and ensure you have an active connection."));
        return;
    }

    this->showNewNetworkWizard();
}

bool NewNetworkCommand::CanRun() const
{
    QSharedPointer<XenObject> selected = this->GetObject();
    if (!selected)
        return false;

    const XenObjectType type = selected->GetObjectType();
    if (type != XenObjectType::Pool && type != XenObjectType::Host)
        return false;

    return selected->GetConnection() != nullptr;
}

QString NewNetworkCommand::MenuText() const
{
    return tr("New Network...");
}

void NewNetworkCommand::showNewNetworkWizard()
{
    qDebug() << "NewNetworkCommand: Opening New Network Wizard";

    QSharedPointer<XenObject> selected = this->GetObject();
    XenConnection* connection = selected ? selected->GetConnection() : nullptr;
    QSharedPointer<Pool> pool;
    QSharedPointer<Host> host;

    if (selected && selected->GetObjectType() == XenObjectType::Pool)
    {
        pool = qSharedPointerDynamicCast<Pool>(selected);
        host = pool ? pool->GetMasterHost() : QSharedPointer<Host>();
    } else if (selected && selected->GetObjectType() == XenObjectType::Host)
    {
        host = qSharedPointerDynamicCast<Host>(selected);
    }

    if (!connection && MainWindow::instance() && MainWindow::instance()->GetSelectionManager())
    {
        const QList<XenConnection*> connections = MainWindow::instance()->GetSelectionManager()->SelectedConnections();
        if (!connections.isEmpty())
            connection = connections.first();
    }

    if (!connection)
        return;

    NewNetworkWizard wizard(connection, pool, host, MainWindow::instance());

    if (wizard.exec() == QDialog::Accepted)
    {
        qDebug() << "NewNetworkCommand: New Network Wizard completed successfully";

        if (MainWindow::instance())
        {
            qDebug() << "NewNetworkCommand: Network creation completed";
        }
    } else
    {
        qDebug() << "NewNetworkCommand: New Network Wizard cancelled";
    }
}
