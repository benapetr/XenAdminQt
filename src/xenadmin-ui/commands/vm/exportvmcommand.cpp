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

#include "exportvmcommand.h"
#include "../../mainwindow.h"
#include "../../settingsmanager.h"
#include "../../dialogs/actionprogressdialog.h"
#include "../../dialogs/exportwizard.h"
#include "xenlib/xen/actions/vm/exportvmaction.h"
#include "xenlib/xen/actions/vm/exportapplianceaction.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/vm.h"
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QDir>

ExportVMCommand::ExportVMCommand(MainWindow* mainWindow, QObject* parent)
    : VMCommand(mainWindow, parent), m_exportWizard(nullptr)
{
}

bool ExportVMCommand::CanRun() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    return this->isVMExportable();
}

void ExportVMCommand::Run()
{
    if (!this->CanRun())
    {
        return;
    }

    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return;

    // Create wizard with connection context and preselect the current VM
    this->m_exportWizard = new ExportWizard(vm->GetConnection(), MainWindow::instance());
    this->m_exportWizard->SetPreselectedVMs({vm});
    connect(this->m_exportWizard, QOverload<int>::of(&QWizard::finished),
            this, &ExportVMCommand::onWizardFinished);

    this->m_exportWizard->show();
    this->m_exportWizard->raise();
    this->m_exportWizard->activateWindow();
}

void ExportVMCommand::onWizardFinished(int result)
{
    if (result == QDialog::Accepted && this->m_exportWizard)
    {
        if (!this->m_exportWizard->exportAsXVA())
        {
            // OVF/OVA export: validate destination then launch ExportApplianceAction.
            // Matches C# ExportApplianceWizard.WizardPagesCollection + ExportApplianceAction.
            QString resolvedPath;
            if (this->m_exportWizard->ValidateOvfDestination(MainWindow::instance(), &resolvedPath))
            {
                const QString appDir  = QFileInfo(resolvedPath).absolutePath();
                const QString appName = QFileInfo(resolvedPath).baseName();
                const bool createOva  = this->m_exportWizard->createOva();
                const bool createMf   = this->m_exportWizard->createManifest();
                const bool compress   = this->m_exportWizard->compressOVF();
                const bool verify     = this->m_exportWizard->verifyExport();

                QList<QSharedPointer<VM>> vms = this->m_exportWizard->GetSelectedVMs();
                if (vms.isEmpty())
                {
                    QSharedPointer<VM> vm = this->getVM();
                    if (vm)
                        vms << vm;
                }

                if (!vms.isEmpty())
                {
                    ExportApplianceAction* action = new ExportApplianceAction(
                        vms, appDir, appName,
                        this->m_exportWizard->GetEulas(),
                        createMf, createOva, compress, verify,
                        MainWindow::instance());
                    ActionProgressDialog* dlg = new ActionProgressDialog(action, MainWindow::instance());
                    dlg->setShowCancel(true);
                    dlg->exec();
                    dlg->deleteLater();

                    if (action->IsCompleted())
                        MainWindow::instance()->ShowStatusMessage(tr("Export completed"), 3000);
                }
            }
        }
        else
        {
            QString fullPath;
            QSharedPointer<VM> vm = this->getVM();
            if (vm && this->m_exportWizard->ValidateXvaDestination(MainWindow::instance(), &fullPath))
            {
                QSharedPointer<Host> host = vm->GetHome();
                ExportVmAction* action = new ExportVmAction(vm, host, fullPath, this->m_exportWizard->verifyExport(), MainWindow::instance());
                ActionProgressDialog* progressDialog = new ActionProgressDialog(action, MainWindow::instance());
                progressDialog->setShowCancel(true);
                progressDialog->exec();
                progressDialog->deleteLater();

                if (action->IsCompleted())
                {
                    const QString exportDir = QFileInfo(fullPath).absolutePath();
                    SettingsManager::instance().SetDefaultExportPath(exportDir);
                    SettingsManager::instance().AddRecentExportPath(exportDir);
                    MainWindow::instance()->ShowStatusMessage(tr("Export completed"), 3000);
                }
            }
        }
    }

    // Clean up wizard
    if (this->m_exportWizard)
    {
        this->m_exportWizard->deleteLater();
        this->m_exportWizard = nullptr;
    }
}

QString ExportVMCommand::MenuText() const
{
    return "Export...";
}

bool ExportVMCommand::isVMExportable() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    if (vm->IsTemplate() || vm->IsLocked())
        return false;

    for (const QString& op : vm->GetAllowedOperations())
    {
        if (op == "export")
            return true;
    }

    return false;
}
