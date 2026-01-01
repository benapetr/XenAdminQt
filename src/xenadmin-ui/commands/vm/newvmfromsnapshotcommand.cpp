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

#include "newvmfromsnapshotcommand.h"
#include "../../dialogs/newvmwizard.h"
#include "../../mainwindow.h"
#include "xencache.h"
#include "xen/xenobject.h"
#include "xen/network/connection.h"
#include <QMessageBox>

NewVMFromSnapshotCommand::NewVMFromSnapshotCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

NewVMFromSnapshotCommand::NewVMFromSnapshotCommand(const QString& snapshotRef,
                                                   XenConnection* connection,
                                                   MainWindow* mainWindow,
                                                   QObject* parent)
    : Command(mainWindow, parent),
      m_snapshotRef(snapshotRef),
      m_connection(connection)
{
}

bool NewVMFromSnapshotCommand::CanRun() const
{
    QString snapshotRef = !this->m_snapshotRef.isEmpty() ? this->m_snapshotRef : this->getSelectedObjectRef();
    QString type = !this->m_snapshotRef.isEmpty() ? "vm" : this->getSelectedObjectType();
    if (snapshotRef.isEmpty() || type != "vm")
        return false;

    XenConnection* connection = this->m_connection;
    if (!connection)
    {
        QSharedPointer<XenObject> selectedObject = this->GetObject();
        connection = selectedObject ? selectedObject->GetConnection() : nullptr;
    }

    XenCache* cache = connection ? connection->GetCache() : nullptr;
    if (!cache)
        return false;

    QVariantMap snapshotData = cache->ResolveObjectData("vm", snapshotRef);
    return snapshotData.value("is_a_snapshot").toBool();
}

void NewVMFromSnapshotCommand::Run()
{
    if (!this->mainWindow())
        return;

    QString snapshotRef = !this->m_snapshotRef.isEmpty() ? this->m_snapshotRef : this->getSelectedObjectRef();
    QString type = !this->m_snapshotRef.isEmpty() ? "vm" : this->getSelectedObjectType();
    if (snapshotRef.isEmpty() || type != "vm")
        return;

    XenConnection* connection = this->m_connection;
    if (!connection)
    {
        QSharedPointer<XenObject> selectedObject = this->GetObject();
        connection = selectedObject ? selectedObject->GetConnection() : nullptr;
    }

    XenCache* cache = connection ? connection->GetCache() : nullptr;
    if (!cache)
        return;

    QVariantMap snapshotData = cache->ResolveObjectData("vm", snapshotRef);
    if (!snapshotData.value("is_a_snapshot").toBool())
    {
        QMessageBox::warning(this->mainWindow(), tr("Not a Snapshot"),
                             tr("Selected item is not a VM snapshot"));
        return;
    }

    NewVMWizard wizard(connection, this->mainWindow());
    wizard.exec();
}

QString NewVMFromSnapshotCommand::MenuText() const
{
    return tr("New VM from Snapshot...");
}
