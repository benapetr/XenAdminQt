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
#include "xenlib/xen/actions/vm/importvmaction.h"
#include <QtWidgets>

ImportVMCommand::ImportVMCommand(QObject* parent) : Command(nullptr, parent)
{
    this->m_ovfOnlyMode = false;
    // qDebug() << "ImportVMCommand: Created default constructor";
}

ImportVMCommand::ImportVMCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
    this->m_ovfOnlyMode = false;
    // qDebug() << "ImportVMCommand: Created with MainWindow";
}

void ImportVMCommand::SetOvfOnlyMode(bool ovfOnly)
{
    this->m_ovfOnlyMode = ovfOnly;
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
    if (this->m_ovfOnlyMode)
        return tr("Import vApp...");

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
    wizard.SetOvfOnlyMode(this->m_ovfOnlyMode);

    if (wizard.exec() != QDialog::Accepted)
    {
        qDebug() << "ImportVMCommand: Import Wizard cancelled";
        return;
    }

    qDebug() << "ImportVMCommand: Import Wizard accepted, dispatching action";
    XenConnection* targetConnection = wizard.GetSelectedConnection() ? wizard.GetSelectedConnection() : connection;

    if (wizard.GetImportType() == ImportWizard::ImportType_OVF)
    {
        if (!targetConnection)
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
        const QMap<QString, QString> ovfDiskSrMappings = wizard.GetOvfDiskSrMappings();
        const QStringList ovfDiskHrefs = wizard.GetOvfDiskHrefs();
        const QMap<QString, QStringList> ovfDiskHrefsBySystem = wizard.GetOvfDiskHrefsBySystem();

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

            // Build per-disk SR overrides: wizard supplies one SR combo per disk href.
            // When no per-disk override was set by the user, diskMappings is left empty
            // and the action falls back to defaultSrRef for every disk.
            QStringList diskHrefsForVm = ovfDiskHrefsBySystem.value(vsId);
            if (diskHrefsForVm.isEmpty())
                diskHrefsForVm = ovfDiskHrefs;

            for (const QString& href : diskHrefsForVm)
            {
                OvfDiskMapping dm;
                dm.diskHref    = href;
                dm.targetSrRef = ovfDiskSrMappings.value(href, srRef);
                mapping.diskMappings << dm;
            }

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
            targetConnection,
            wizard.GetSourceFilePath(),
            vmMappings,
            wizard.GetVerifyManifest(),
            wizard.GetVerifySignature(),
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
        if (!targetConnection)
        {
            QMessageBox::warning(MainWindow::instance(), tr("No Connection"),
                                 tr("No connected server found. Please connect to a server first."));
            return;
        }

        if (wizard.GetSourceFilePath().endsWith(".vmdk", Qt::CaseInsensitive))
        {
            QMessageBox::warning(MainWindow::instance(), tr("Unsupported Disk Image"),
                                 tr("VMDK disk-image import is not supported yet. "
                                    "Convert the disk to VHD and import the VHD file."));
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
            targetConnection,
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

    if (!targetConnection)
    {
        QMessageBox::warning(MainWindow::instance(), tr("No Connection"),
                             tr("No connected server found. Please connect to a server first."));
        return;
    }

    const auto selectedSR = wizard.GetSelectedSR();
    const QString srRef = selectedSR ? selectedSR->OpaqueRef() : QString();
    if (srRef.isEmpty())
    {
        QMessageBox::warning(MainWindow::instance(), tr("No Storage Selected"),
                             tr("No storage repository was selected. Cannot start import."));
        return;
    }

    const auto selectedHost = wizard.GetSelectedHost();
    const QString hostRef = selectedHost ? selectedHost->OpaqueRef() : QString();
    ImportVmAction* action = new ImportVmAction(
        targetConnection,
        hostRef,
        wizard.GetSourceFilePath(),
        srRef,
        nullptr);

    MainWindow* mainWindow = MainWindow::instance();
    if (mainWindow)
    {
        connect(action, &AsyncOperation::completed, mainWindow, [mainWindow]()
        {
            mainWindow->ShowStatusMessage(QObject::tr("Import completed"), 3000);
        });
        mainWindow->ShowStatusMessage(tr("Import started in the background"), 3000);
    }

    action->RunAsync(true);
}
