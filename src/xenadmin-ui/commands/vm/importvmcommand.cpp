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
#include "xenlib/xen/actions/vm/importvmaction.h"
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

    qDebug() << "ImportVMCommand: Import Wizard accepted, launching ImportVmAction";

    // XVA imports use ImportVmAction; OVF/OVA use ImportApplianceAction
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

        QList<OvfVmMapping> vmMappings;
        const QStringList vsNames = wizard.property("ovfVirtualSystemNames").toStringList();
        const QStringList netNames = wizard.property("ovfNetworkNames").toStringList();

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
                    mapping.networkMappings << nm;
                }
            }
            // If no named networks in OVF but a network was selected, add one default VIF
            if (mapping.networkMappings.isEmpty() && !networkRef.isEmpty())
            {
                OvfNetworkMapping nm;
                nm.ovfNetworkName   = "Network";
                nm.targetNetworkRef = networkRef;
                mapping.networkMappings << nm;
            }

            vmMappings << mapping;
        }

        ImportApplianceAction* action = new ImportApplianceAction(
            connection,
            wizard.GetSourceFilePath(),
            vmMappings,
            /*verifyManifest=*/false,
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
            wizard.GetStartAutomatically(),
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

    if (!connection)
    {
        QMessageBox::warning(MainWindow::instance(), tr("No Connection"),
                             tr("No connected server found. Please connect to a server first."));
        return;
    }

    const auto selectedSR3 = wizard.GetSelectedSR();
    QString srRef = selectedSR3 ? selectedSR3->OpaqueRef() : QString();
    if (srRef.isEmpty())
    {
        QMessageBox::warning(MainWindow::instance(), tr("No Storage Selected"),
                             tr("No storage repository was selected. Cannot start import."));
        return;
    }

    // Build VIF mapping list from selected network
    QStringList vifNetworks;
    const auto selectedNetwork3 = wizard.GetSelectedNetwork();
    QString networkRef = selectedNetwork3 ? selectedNetwork3->OpaqueRef() : QString();
    if (!networkRef.isEmpty())
        vifNetworks << networkRef;

    ImportVmAction* action = new ImportVmAction(connection,
                                                wizard.GetSelectedHost() ? wizard.GetSelectedHost()->OpaqueRef() : QString(),
                                                wizard.GetSourceFilePath(),
                                                srRef,
                                                MainWindow::instance());

    ActionProgressDialog* progressDialog = new ActionProgressDialog(action, MainWindow::instance());
    progressDialog->setShowCancel(true);

    // When the progress dialog opens it starts the action; after upload the action waits for
    // endWizard() to be called before finishing network setup and optional auto-start.
    // We call endWizard() immediately here since the network mapping step is simplified for now.
    action->endWizard(wizard.GetStartAutomatically(), vifNetworks);

    progressDialog->exec();
    progressDialog->deleteLater();
}
