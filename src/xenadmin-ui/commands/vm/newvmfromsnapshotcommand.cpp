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
#include "xenlib/xencache.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/network/connection.h"
#include <QMessageBox>

NewVMFromSnapshotCommand::NewVMFromSnapshotCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

NewVMFromSnapshotCommand::NewVMFromSnapshotCommand(const QString& snapshotRef, XenConnection* connection, MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent), m_snapshotRef(snapshotRef), m_connection(connection)
{
}

bool NewVMFromSnapshotCommand::CanRun() const
{
    QString snapshotRef = !this->m_snapshotRef.isEmpty() ? this->m_snapshotRef : this->getSelectedObjectRef();
    XenObjectType type = !this->m_snapshotRef.isEmpty() ? XenObjectType::VM : this->getSelectedObjectType();
    if (snapshotRef.isEmpty() || type != XenObjectType::VM)
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

    QSharedPointer<VM> snapshot = cache->ResolveObject<VM>(XenObjectType::VM, snapshotRef);
    return snapshot && snapshot->IsSnapshot();
}

void NewVMFromSnapshotCommand::Run()
{
    if (!MainWindow::instance())
        return;

    QString snapshotRef = !this->m_snapshotRef.isEmpty() ? this->m_snapshotRef : this->getSelectedObjectRef();
    XenObjectType type = !this->m_snapshotRef.isEmpty() ? XenObjectType::VM : this->getSelectedObjectType();
    if (snapshotRef.isEmpty() || type != XenObjectType::VM)
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

    QSharedPointer<VM> snapshot = cache->ResolveObject<VM>(XenObjectType::VM, snapshotRef);
    if (!snapshot || !snapshot->IsSnapshot())
    {
        QMessageBox::warning(MainWindow::instance(), tr("Not a Snapshot"), tr("Selected item is not a VM snapshot"));
        return;
    }

    NewVMWizard wizard(connection, QString(), MainWindow::instance());
    wizard.exec();
}

QString NewVMFromSnapshotCommand::MenuText() const
{
    return tr("New VM from Snapshot...");
}
