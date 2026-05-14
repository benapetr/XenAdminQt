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

    // Create and show the Export Wizard
    if (!this->m_exportWizard)
    {
        this->m_exportWizard = new ExportWizard(MainWindow::instance());
        connect(this->m_exportWizard, QOverload<int>::of(&QWizard::finished),
                this, &ExportVMCommand::onWizardFinished);
    }

    QSharedPointer<VM> vm = this->getVM();
    if (vm)
    {
        this->m_exportWizard->setSelectedObjectName(vm->GetName());
        if (this->m_exportWizard->exportFileName().isEmpty())
            this->m_exportWizard->setExportFileName(vm->GetName());
    }

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
            QMessageBox::warning(MainWindow::instance(), tr("Export VM"),
                                 tr("OVF/OVA export is not implemented yet. Select XVA Package to export this VM."));
        }
        else
        {
            QString fullPath;
            QSharedPointer<VM> vm = this->getVM();
            if (vm && this->validateXvaDestination(&fullPath))
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

bool ExportVMCommand::validateXvaDestination(QString* fullPath) const
{
    if (!this->m_exportWizard || !fullPath)
        return false;

    const QString directory = this->m_exportWizard->exportDirectory().trimmed();
    const QString fileName = this->m_exportWizard->exportFileName().trimmed();

    if (directory.isEmpty())
    {
        QMessageBox::warning(MainWindow::instance(), tr("Export VM"),
                             tr("Please select an export directory."));
        return false;
    }

    if (fileName.isEmpty())
    {
        QMessageBox::warning(MainWindow::instance(), tr("Export VM"),
                             tr("Please enter an export file name."));
        return false;
    }

    if (QDir::isAbsolutePath(fileName) || fileName.contains('/') || fileName.contains('\\') ||
        fileName == "." || fileName == "..")
    {
        QMessageBox::warning(MainWindow::instance(), tr("Export VM"),
                             tr("Please enter a file name only, without path separators."));
        return false;
    }

    QDir dir(directory);
    if (!dir.exists())
    {
        const QMessageBox::StandardButton reply = QMessageBox::question(
            MainWindow::instance(),
            tr("Create Export Directory"),
            tr("The export directory does not exist:\n%1\n\nCreate it?").arg(directory),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return false;

        if (!QDir().mkpath(directory))
        {
            QMessageBox::warning(MainWindow::instance(), tr("Export VM"),
                                 tr("Could not create the export directory:\n%1").arg(directory));
            return false;
        }
        dir = QDir(directory);
    }

    if (!dir.exists())
    {
        QMessageBox::warning(MainWindow::instance(), tr("Export VM"),
                             tr("The export directory does not exist."));
        return false;
    }

    QString path = dir.filePath(fileName);
    if (!path.toLower().endsWith(".xva"))
        path += ".xva";

    if (QFile::exists(path))
    {
        const QMessageBox::StandardButton reply = QMessageBox::question(
            MainWindow::instance(),
            tr("Overwrite Export"),
            tr("The export file already exists:\n%1\n\nOverwrite it?").arg(path),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return false;
    }

    QFile probe(path + ".write-test");
    if (!probe.open(QIODevice::WriteOnly))
    {
        QMessageBox::warning(MainWindow::instance(), tr("Export VM"),
                             tr("Cannot write to the export directory:\n%1").arg(probe.errorString()));
        return false;
    }
    probe.close();
    QFile::remove(probe.fileName());

    *fullPath = QFileInfo(path).absoluteFilePath();
    return true;
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
