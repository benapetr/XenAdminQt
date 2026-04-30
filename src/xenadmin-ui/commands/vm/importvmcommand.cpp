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

#include "importvmcommand.h"
#include <QDebug>
#include "../../mainwindow.h"
#include "../../dialogs/importwizard.h"
#include "../../dialogs/actionprogressdialog.h"
#include "xenlib/xen/network/connectionsmanager.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/network.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/actions/vm/importapplianceaction.h"
#include "xenlib/xen/actions/vm/importimageaction.h"
#include <QtWidgets>

ImportVMCommand::ImportVMCommand(QObject* parent) : Command(nullptr, parent)
{
    // qDebug() << "ImportVMCommand: Created default constructor";
}

ImportVMCommand::ImportVMCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
    // qDebug() << "ImportVMCommand: Created with MainWindow";
}

void ImportVMCommand::Run()
{
    // qDebug() << "ImportVMCommand: Executing Import VM command";

    if (!this->CanRun())
    {
        qWarning() << "ImportVMCommand: Cannot execute - conditions not met";
        QMessageBox::warning(nullptr, tr("Cannot Import VM"), tr("Import functionality is not available at this time."));
        return;
    }

    this->showImportWizard();
}

bool ImportVMCommand::CanRun() const
{
    // Import requires at least one connected server
    Xen::ConnectionsManager* mgr = Xen::ConnectionsManager::instance();
    if (!mgr)
        return false;
    return !mgr->GetConnectedConnections().isEmpty();
}

QString ImportVMCommand::MenuText() const
{
    return tr("Import VM...");
}

void ImportVMCommand::showImportWizard()
{
    qDebug() << "ImportVMCommand: Opening Import Wizard";

    // Resolve a connection — prefer the selection's connection, fall back to first connected
    XenConnection* connection = nullptr;
    QSharedPointer<XenObject> selectedObject = this->GetObject();
    if (selectedObject)
        connection = selectedObject->GetConnection();

    if (!connection)
    {
        Xen::ConnectionsManager* mgr = Xen::ConnectionsManager::instance();
        QList<XenConnection*> connections = mgr->GetConnectedConnections();
        if (!connections.isEmpty())
            connection = connections.first();
    }

    ImportWizard wizard(connection, MainWindow::instance());

    if (wizard.exec() != QDialog::Accepted)
    {
        qDebug() << "ImportVMCommand: Import Wizard cancelled";
        return;
    }

    qDebug() << "ImportVMCommand: Import Wizard accepted, dispatching action";

    // XVA: upload was already started by the wizard; OVF/VHD start new actions below.
    if (wizard.GetImportType() == ImportWizard::ImportType_OVF)
    {
        if (!connection)
        {
            QMessageBox::warning(MainWindow::instance(), tr("No Connection"),
                                 tr("No connected server found. Please connect to a server first."));
            return;
        }

        // Build a single OvfVmMapping per virtual system in the package.
        const auto selectedSR      = wizard.GetSelectedSR();
        const auto selectedNetwork = wizard.GetSelectedNetwork();
        const QString srRef        = selectedSR      ? selectedSR->OpaqueRef()      : QString();
        const QString networkRef   = selectedNetwork ? selectedNetwork->OpaqueRef() : QString();
        const auto selectedHost    = wizard.GetSelectedHost();
        const QString hostRef      = selectedHost    ? selectedHost->OpaqueRef()    : QString();

        // Per-network mappings from the network page (one combo per OVF network name)
        const QMap<QString, QString> ovfNetMappings = wizard.GetOvfNetworkMappings();
        const QMap<QString, QString> ovfMacMappings = wizard.GetOvfMacMappings();

        QList<OvfVmMapping> vmMappings;
        const QStringList vsNames = wizard.GetOvfVirtualSystemNames();
        const QStringList netNames = wizard.GetOvfNetworkNames();

        // Build one mapping per virtual system (or one generic mapping when names aren't exposed)
        QStringList effectiveVsNames = vsNames;
        if (effectiveVsNames.isEmpty())
            effectiveVsNames << QString(); // one anonymous VM

        for (const QString& vsId : effectiveVsNames)
        {
            OvfVmMapping mapping;
            mapping.virtualSystemId = vsId;
            mapping.targetHostRef   = hostRef;
            mapping.defaultSrRef    = srRef;

            // Use per-network wizard mappings when available; fall back to single ref
            for (const QString& netName : netNames)
            {
                const QString targetRef = ovfNetMappings.contains(netName)
                                          ? ovfNetMappings.value(netName)
                                          : networkRef;
                if (!targetRef.isEmpty())
                {
                    OvfNetworkMapping nm;
                    nm.ovfNetworkName   = netName;
                    nm.targetNetworkRef = targetRef;
                    nm.mac              = ovfMacMappings.value(netName);  // empty = auto-generate
                    mapping.networkMappings << nm;
                }
            }
            // If no named networks in OVF but a network was selected, add one default VIF
            if (mapping.networkMappings.isEmpty() && !networkRef.isEmpty())
            {
                OvfNetworkMapping nm;
                nm.ovfNetworkName   = "Network";
                nm.targetNetworkRef = networkRef;
                nm.mac              = QString();  // auto-generate for default VIF
                mapping.networkMappings << nm;
            }

            vmMappings << mapping;
        }

        ImportApplianceAction* action = new ImportApplianceAction(
            connection,
            wizard.GetSourceFilePath(),
            vmMappings,
            wizard.GetVerifyManifest(),
            /*verifySignature=*/false,
            wizard.GetRunFixups(),
            wizard.GetFixupIsoSrRef(),
            wizard.GetStartAutomatically(),
            MainWindow::instance());

        ActionProgressDialog* progressDialog = new ActionProgressDialog(action, MainWindow::instance());
        progressDialog->setShowCancel(true);
        progressDialog->exec();
        progressDialog->deleteLater();
        return;
    }

    if (wizard.GetImportType() == ImportWizard::ImportType_VHD)
    {
        if (!connection)
        {
            QMessageBox::warning(MainWindow::instance(), tr("No Connection"),
                                 tr("No connected server found. Please connect to a server first."));
            return;
        }
        const auto selectedSR2 = wizard.GetSelectedSR();
        const QString srRef = selectedSR2 ? selectedSR2->OpaqueRef() : QString();
        if (srRef.isEmpty())
        {
            QMessageBox::warning(MainWindow::instance(), tr("No Storage Selected"),
                                 tr("No storage repository was selected. Cannot start import."));
            return;
        }

        ImportImageAction* action = new ImportImageAction(
            connection,
            wizard.GetVmName(),
            wizard.GetVcpuCount(),
            wizard.GetMemoryMb(),
            srRef,
            wizard.GetSelectedNetwork() ? wizard.GetSelectedNetwork()->OpaqueRef() : QString(),
            wizard.GetSelectedHost() ? wizard.GetSelectedHost()->OpaqueRef() : QString(),
            wizard.GetSourceFilePath(),
            wizard.GetDiskImageCapacityBytes(),
            static_cast<ImportImageAction::BootMode>(wizard.GetBootMode()),
            wizard.GetAssignVtpm(),
            wizard.GetStartAutomatically(),
            wizard.GetRunFixups(),
            wizard.GetFixupIsoSrRef(),
            MainWindow::instance());

        ActionProgressDialog* progressDialog = new ActionProgressDialog(action, MainWindow::instance());
        progressDialog->setShowCancel(true);
        progressDialog->exec();
        progressDialog->deleteLater();
        return;
    }

    if (wizard.GetImportType() != ImportWizard::ImportType_XVA)
    {
        QMessageBox::information(MainWindow::instance(),
                                 tr("Unknown Import Type"),
                                 tr("Unrecognised import type — cannot start import."));
        return;
    }

    // XVA: the upload was started by the wizard on the Storage page and endWizard()
    // was called in ImportWizard::accept().  The action's VIF-remapping and optional
    // VM-start phase now runs in the background — no further dialog is needed here.
    // This mirrors the C# ImportWizard.FinishWizard() pattern where no second
    // blocking dialog is shown after the wizard closes.
}
