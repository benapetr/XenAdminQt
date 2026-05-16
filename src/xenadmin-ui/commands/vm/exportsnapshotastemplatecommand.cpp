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

#include "exportsnapshotastemplatecommand.h"
#include "../../dialogs/exportwizard.h"
#include "../../dialogs/actionprogressdialog.h"
#include "../../mainwindow.h"
#include "../../settingsmanager.h"
#include "xenlib/xen/actions/vm/exportvmaction.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/host.h"
#include <QFileInfo>
#include <QMessageBox>

ExportSnapshotAsTemplateCommand::ExportSnapshotAsTemplateCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

ExportSnapshotAsTemplateCommand::ExportSnapshotAsTemplateCommand(const QString& snapshotRef, XenConnection* connection, MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent), m_snapshotRef(snapshotRef), m_connection(connection)
{
}

bool ExportSnapshotAsTemplateCommand::CanRun() const
{
    QString vmRef = !this->m_snapshotRef.isEmpty() ? this->m_snapshotRef : this->getSelectedObjectRef();
    XenObjectType type = !this->m_snapshotRef.isEmpty() ? XenObjectType::VM : this->getSelectedObjectType();

    if (vmRef.isEmpty() || type != XenObjectType::VM)
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

    QSharedPointer<VM> snapshot = cache->ResolveObject<VM>(XenObjectType::VM, vmRef);
    if (!snapshot)
        return false;

    return snapshot->IsSnapshot();
}

void ExportSnapshotAsTemplateCommand::Run()
{
    const QString vmRef = !this->m_snapshotRef.isEmpty() ? this->m_snapshotRef : this->getSelectedObjectRef();

    XenConnection* connection = this->m_connection;
    if (!connection)
    {
        QSharedPointer<XenObject> selectedObject = this->GetObject();
        connection = selectedObject ? selectedObject->GetConnection() : nullptr;
    }
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    if (!cache || vmRef.isEmpty())
        return;

    QSharedPointer<VM> snapshot = cache->ResolveObject<VM>(XenObjectType::VM, vmRef);
    if (!snapshot || !snapshot->IsSnapshot())
        return;

    ExportWizard wizard(connection, MainWindow::instance());
    wizard.SetPreselectedVMs({snapshot});

    if (wizard.exec() != QDialog::Accepted)
        return;

    if (!wizard.exportAsXVA())
    {
        QMessageBox::warning(MainWindow::instance(), tr("Export Snapshot"),
                             tr("OVF/OVA export is not implemented yet. Select XVA Package to export this snapshot."));
        return;
    }

    QString fullPath;
    if (!wizard.ValidateXvaDestination(MainWindow::instance(), &fullPath))
        return;

    QSharedPointer<Host> host = snapshot->GetHome();
    ExportVmAction* action = new ExportVmAction(snapshot, host, fullPath, wizard.verifyExport(), MainWindow::instance());
    ActionProgressDialog* progressDialog = new ActionProgressDialog(action, MainWindow::instance());
    progressDialog->setShowCancel(true);
    progressDialog->exec();
    progressDialog->deleteLater();

    if (action->IsCompleted())
    {
        SettingsManager::instance().SetDefaultExportPath(QFileInfo(fullPath).absolutePath());
        MainWindow::instance()->ShowStatusMessage(tr("Export completed"), 3000);
    }
}

QString ExportSnapshotAsTemplateCommand::MenuText() const
{
    return tr("E&xport Snapshot as Template...");
}
