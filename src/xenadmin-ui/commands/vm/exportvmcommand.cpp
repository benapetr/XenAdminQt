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
#include "../../dialogs/exportwizard.h"
#include "xenlib.h"
#include <QProgressDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QThread>

ExportVMCommand::ExportVMCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent), m_exportWizard(nullptr)
{
}

bool ExportVMCommand::canRun() const
{
    QString vmRef = this->getSelectedVMRef();
    if (vmRef.isEmpty())
    {
        return false;
    }

    return this->isVMExportable(vmRef);
}

void ExportVMCommand::run()
{
    if (!this->canRun())
    {
        return;
    }

    // Create and show the Export Wizard
    if (!this->m_exportWizard)
    {
        this->m_exportWizard = new ExportWizard(this->mainWindow());
        connect(this->m_exportWizard, QOverload<int>::of(&QWizard::finished),
                this, &ExportVMCommand::onWizardFinished);
    }

    this->m_exportWizard->show();
    this->m_exportWizard->raise();
    this->m_exportWizard->activateWindow();
}

void ExportVMCommand::onWizardFinished(int result)
{
    if (result == QDialog::Accepted && this->m_exportWizard)
    {
        // Get export settings from wizard
        QString directory = this->m_exportWizard->exportDirectory();
        QString fileName = this->m_exportWizard->exportFileName();
        bool isXVA = this->m_exportWizard->exportAsXVA();

        if (directory.isEmpty() || fileName.isEmpty())
        {
            QMessageBox::warning(this->mainWindow(), tr("Export VM"),
                                 tr("Invalid export settings. Please check directory and filename."));
            return;
        }

        // Construct full file path with appropriate extension
        QString fullPath = QDir(directory).filePath(fileName);
        if (isXVA && !fullPath.toLower().endsWith(".xva"))
        {
            fullPath += ".xva";
        } else if (!isXVA && !fullPath.toLower().endsWith(".ovf"))
        {
            fullPath += ".ovf";
        }

        // TODO: Start actual export operation using selected VMs and settings
        // This would integrate with the XenLib to perform the actual export
        QMessageBox::information(this->mainWindow(), tr("Export Started"),
                                 tr("Export operation has been started.\nDestination: %1").arg(fullPath));

        this->mainWindow()->showStatusMessage(tr("Export started"), 3000);
    }

    // Clean up wizard
    if (this->m_exportWizard)
    {
        this->m_exportWizard->deleteLater();
        this->m_exportWizard = nullptr;
    }
}

QString ExportVMCommand::menuText() const
{
    return "Export...";
}

QString ExportVMCommand::getSelectedVMRef() const
{
    // Get currently selected VM from the server tree
    QTreeWidget* serverTree = this->mainWindow()->getServerTreeWidget();
    QTreeWidgetItem* currentItem = serverTree->currentItem();

    if (currentItem && currentItem->data(0, Qt::UserRole + 1).toString() == "vm")
    {
        return currentItem->data(0, Qt::UserRole).toString();
    }

    return QString();
}

QString ExportVMCommand::getSelectedVMName() const
{
    QTreeWidget* serverTree = this->mainWindow()->getServerTreeWidget();
    QTreeWidgetItem* currentItem = serverTree->currentItem();

    if (currentItem && currentItem->data(0, Qt::UserRole + 1).toString() == "vm")
    {
        return currentItem->text(0);
    }

    return QString();
}

bool ExportVMCommand::isVMExportable(const QString& vmRef) const
{
    // Check if VM is in a state that allows export
    // Typically VMs should be shut down for export, but some implementations
    // allow export of running VMs (with caveats)

    try
    {
        QString powerState = this->mainWindow()->xenLib()->getVMPowerState(vmRef);

        // Allow export of halted VMs, and optionally running/suspended VMs
        // (following XenCenter behavior)
        return powerState == "Halted" || powerState == "Running" || powerState == "Suspended";
    } catch (const std::exception&)
    {
        return false;
    }
}