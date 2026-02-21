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

#include "vappexportcommand.h"
#include "../../dialogs/exportwizard.h"
#include "../../mainwindow.h"
#include "xenlib/xen/vmappliance.h"
#include <QDir>
#include <QMessageBox>

VappExportCommand::VappExportCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent), m_exportWizard(nullptr)
{
}

QSharedPointer<VMAppliance> VappExportCommand::getSelectedAppliance() const
{
    const QList<QSharedPointer<XenObject>> selected = this->getSelectedObjects();
    if (selected.size() != 1)
        return QSharedPointer<VMAppliance>();

    QSharedPointer<XenObject> obj = selected.first();
    if (!obj || obj->GetObjectType() != XenObjectType::VMAppliance)
        return QSharedPointer<VMAppliance>();

    return qSharedPointerDynamicCast<VMAppliance>(obj);
}

bool VappExportCommand::CanRun() const
{
    QSharedPointer<VMAppliance> appliance = this->getSelectedAppliance();
    return appliance && appliance->IsValid() && appliance->IsConnected();
}

void VappExportCommand::Run()
{
    if (!this->CanRun())
        return;

    if (!this->m_exportWizard)
    {
        this->m_exportWizard = new ExportWizard(MainWindow::instance());
        connect(this->m_exportWizard, QOverload<int>::of(&QWizard::finished), this, &VappExportCommand::onWizardFinished);
    }

    this->m_exportWizard->show();
    this->m_exportWizard->raise();
    this->m_exportWizard->activateWindow();
}

void VappExportCommand::onWizardFinished(int result)
{
    if (result == QDialog::Accepted && this->m_exportWizard)
    {
        const QString directory = this->m_exportWizard->exportDirectory();
        const QString fileName = this->m_exportWizard->exportFileName();
        const bool isXVA = this->m_exportWizard->exportAsXVA();

        if (directory.isEmpty() || fileName.isEmpty())
        {
            QMessageBox::warning(MainWindow::instance(), tr("Export vApp"), tr("Invalid export settings. Please check directory and filename."));
            return;
        }

        QString fullPath = QDir(directory).filePath(fileName);
        if (isXVA && !fullPath.toLower().endsWith(".xva"))
            fullPath += ".xva";
        else if (!isXVA && !fullPath.toLower().endsWith(".ovf"))
            fullPath += ".ovf";

        QMessageBox::information(MainWindow::instance(), tr("Export Started"), tr("vApp export operation has been started.\nDestination: %1").arg(fullPath));
        MainWindow::instance()->ShowStatusMessage(tr("vApp export started"), 3000);
    }

    if (this->m_exportWizard)
    {
        this->m_exportWizard->deleteLater();
        this->m_exportWizard = nullptr;
    }
}

QString VappExportCommand::MenuText() const
{
    return tr("Export...");
}
