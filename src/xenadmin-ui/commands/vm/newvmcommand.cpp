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

#include "newvmcommand.h"
#include <QDebug>
#include "../../mainwindow.h"
#include "../../dialogs/newvmwizard.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/network/connectionsmanager.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/xenobject.h"
#include <QtWidgets>

NewVMCommand::NewVMCommand(QObject* parent) : Command(nullptr, parent)
{
    // qDebug() << "NewVMCommand: Created default constructor";
}

NewVMCommand::NewVMCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
    // qDebug() << "NewVMCommand: Created with MainWindow";
}

NewVMCommand::NewVMCommand(const QString& templateUuid, MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent), m_defaultTemplateUuid(templateUuid)
{
    // qDebug() << "NewVMCommand: Created with template UUID:" << templateUuid;
}

void NewVMCommand::Run()
{
    // qDebug() << "NewVMCommand: Executing New VM command";

    if (!this->CanRun())
    {
        qWarning() << "NewVMCommand: Cannot execute - no suitable host available";
        QMessageBox::warning(nullptr, tr("Cannot Create VM"),
                             tr("No enabled hosts are available to create a VM.\n"
                                "Please ensure at least one host is connected and enabled."));
        return;
    }

    this->showNewVMWizard();
}

bool NewVMCommand::CanRun() const
{
    // Check if we have an active connection with at least one enabled host
    if (!MainWindow::instance())
    {
        return false;
    }

    return this->hasEnabledHost();
}

QString NewVMCommand::MenuText() const
{
    return tr("New VM...");
}

QIcon NewVMCommand::GetIcon() const
{
    return QIcon(":/icons/vm_create_16.png");
}

void NewVMCommand::showNewVMWizard()
{
    // qDebug() << "NewVMCommand: Opening New VM Wizard";

    XenConnection* connection = nullptr;
    QSharedPointer<XenObject> selectedObject = this->GetObject();
    if (selectedObject)
        connection = selectedObject->GetConnection();

    if (!connection)
    {
        Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
        if (connMgr)
        {
            QList<XenConnection*> connections = connMgr->GetConnectedConnections();
            if (!connections.isEmpty())
                connection = connections.first();
        }
    }

    if (!connection)
    {
        QMessageBox::warning(MainWindow::instance(), tr("Not Connected"),
                             tr("Not connected to XenServer"));
        return;
    }

    NewVMWizard wizard(connection, MainWindow::instance());

    // If we have a default template, we could set it here
    if (!this->m_defaultTemplateUuid.isEmpty())
    {
        qDebug() << "NewVMCommand: Using default template:" << this->m_defaultTemplateUuid;
    }

    if (wizard.exec() == QDialog::Accepted)
    {
        qDebug() << "NewVMCommand: New VM Wizard completed successfully";

        if (MainWindow::instance())
        {
            qDebug() << "NewVMCommand: New VM wizard completed successfully";
        }
    } else
    {
        qDebug() << "NewVMCommand: New VM Wizard cancelled";
    }
}

void NewVMCommand::runWithConnection()
{
    if (!MainWindow::instance())
    {
        qWarning() << "NewVMCommand: No main window available";
        return;
    }

    // Get current connection info and proceed with VM creation
    this->showNewVMWizard();
}

bool NewVMCommand::hasEnabledHost() const
{
    if (!MainWindow::instance())
        return false;

    XenConnection* connection = nullptr;
    QSharedPointer<XenObject> selectedObject = this->GetObject();
    if (selectedObject)
        connection = selectedObject->GetConnection();

    if (!connection)
    {
        Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
        if (connMgr)
        {
            QList<XenConnection*> connections = connMgr->GetConnectedConnections();
            if (!connections.isEmpty())
                connection = connections.first();
        }
    }

    if (!connection || !connection->GetCache())
        return false;

    QList<QVariantMap> hosts = connection->GetCache()->GetAllData(XenObjectType::Host);
    for (const QVariantMap& host : hosts)
    {
        if (host.value("enabled").toBool())
            return true;
    }

    return false;
}
