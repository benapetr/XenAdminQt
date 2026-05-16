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

#include "exporttemplatecommand.h"
#include "../../mainwindow.h"
#include "../../settingsmanager.h"
#include "../../dialogs/exportwizard.h"
#include "../../dialogs/actionprogressdialog.h"
#include "xenlib/xen/actions/vm/exportvmaction.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/vm.h"
#include <QFileInfo>
#include <QMessageBox>

ExportTemplateCommand::ExportTemplateCommand(MainWindow* mainWindow, QObject* parent) : TemplateCommand(mainWindow, parent)
{
}

bool ExportTemplateCommand::CanRun() const
{
    QString templateRef = this->getSelectedTemplateRef();
    if (templateRef.isEmpty())
        return false;

    return this->canExportTemplate(templateRef);
}

void ExportTemplateCommand::Run()
{
    QSharedPointer<VM> tmplVm = this->getTemplate();
    if (!tmplVm)
        return;

    XenConnection* conn = tmplVm->GetConnection();
    ExportWizard wizard(conn, MainWindow::instance());
    wizard.SetPreselectedVMs({tmplVm});

    if (wizard.exec() != QDialog::Accepted)
        return;

    if (!wizard.exportAsXVA())
    {
        QMessageBox::warning(MainWindow::instance(), tr("Export Template"),
                             tr("OVF/OVA export is not implemented yet. Select XVA Package to export this template."));
        return;
    }

    QString fullPath;
    if (!wizard.ValidateXvaDestination(MainWindow::instance(), &fullPath))
        return;

    QSharedPointer<Host> host = tmplVm->GetHome();
    ExportVmAction* action = new ExportVmAction(tmplVm, host, fullPath, wizard.verifyExport(), MainWindow::instance());
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

QString ExportTemplateCommand::MenuText() const
{
    return "Export Template";
}

bool ExportTemplateCommand::canExportTemplate(const QString& templateRef) const
{
    QSharedPointer<VM> vm = this->getTemplate();
    if (!vm || vm->OpaqueRef() != templateRef)
        return false;

    if (!TemplateCommand::canRunTemplate(vm))
        return false;

    // Check if the operation is allowed
    return vm->GetAllowedOperations().contains("export");
}
