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
#include "../../dialogs/actionprogressdialog.h"
#include "xenlib/xen/actions/vm/exportapplianceaction.h"
#include "xenlib/xen/vmappliance.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xencache.h"
#include <QFileInfo>
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

    QSharedPointer<VMAppliance> appliance = this->getSelectedAppliance();
    if (!appliance)
        return;

    XenConnection* conn = appliance->GetConnection();

    // Resolve member VMs from the cache
    QList<QSharedPointer<VM>> memberVms;
    if (conn)
    {
        XenCache* cache = conn->GetCache();
        for (const QString& vmRef : appliance->VMRefs())
        {
            QSharedPointer<VM> vm = cache->ResolveObject<VM>(XenObjectType::VM, vmRef);
            if (vm && vm->IsValid())
                memberVms << vm;
        }
    }

    this->m_exportWizard = new ExportWizard(conn, MainWindow::instance());
    this->m_exportWizard->SetPreselectedVMs(memberVms);
    this->m_exportWizard->SetOvfModeOnly();
    connect(this->m_exportWizard, QOverload<int>::of(&QWizard::finished),
            this, &VappExportCommand::onWizardFinished);

    this->m_exportWizard->show();
    this->m_exportWizard->raise();
    this->m_exportWizard->activateWindow();
}

void VappExportCommand::onWizardFinished(int result)
{
    if (result == QDialog::Accepted && this->m_exportWizard)
    {
        // vApp export is always OVF/OVA (XVA option is hidden by SetOvfModeOnly()).
        QString resolvedPath;
        if (this->m_exportWizard->ValidateOvfDestination(MainWindow::instance(), &resolvedPath))
        {
            const QString appDir  = QFileInfo(resolvedPath).absolutePath();
            const QString appName = QFileInfo(resolvedPath).baseName();
            const bool createOva  = this->m_exportWizard->createOva();
            const bool createMf   = this->m_exportWizard->createManifest();
            const bool compress   = this->m_exportWizard->compressOVF();
            const bool verify     = this->m_exportWizard->verifyExport();

            const QList<QSharedPointer<VM>> vms = this->m_exportWizard->GetSelectedVMs();
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
