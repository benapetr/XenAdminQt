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

#include "certificatecommand.h"
#include "../../mainwindow.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xencache.h"
#include <QMessageBox>

// TODO: Uncomment when dialogs are ported
// #include "../../dialogs/installcertificatedialog.h"
// #include "../../dialogs/warningdialogs/warningdialog.h"

// TODO: Uncomment when Helpers class is ported
// #include "../../../xenlib/utils/helpers.h"

// =============================================================================
// CertificateCommand (Base Class)
// =============================================================================

CertificateCommand::CertificateCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

CertificateCommand::CertificateCommand(MainWindow* mainWindow, const QList<QSharedPointer<Host>>& hosts, QObject* parent) : Command(mainWindow, parent), m_hosts(hosts)
{
}

bool CertificateCommand::CanRun() const
{
    QList<QSharedPointer<Host>> hosts = this->getHosts();
    
    // Must have exactly one host selected
    if (hosts.size() != 1)
        return false;
    
    QSharedPointer<Host> host = hosts.first();
    if (!host || !host->IsValid())
        return false;
    
    XenConnection* connection = host->GetConnection();
    if (!connection)
        return false;
    
    // Must be Stockholm or greater
    // TODO: Uncomment when Helpers::StockholmOrGreater is ported
    // if (!Helpers::StockholmOrGreater(connection))
    //     return false;
    
    // Host must be standalone or pool coordinator
    QSharedPointer<Pool> pool = host->GetPool();
    if (pool && pool->IsValid())
    {
        // If in a pool, must be the coordinator (master)
        QString masterRef = pool->GetMasterHostRef();
        if (masterRef != host->OpaqueRef())
            return false;
    }
    
    return true;
}

QList<QSharedPointer<Host>> CertificateCommand::getHosts() const
{
    if (!this->m_hosts.isEmpty())
        return this->m_hosts;
    
    // TODO: Get from main window selection
    // For now, return empty list
    QList<QSharedPointer<Host>> hosts;
    
    QString type = this->getSelectedObjectType();
    if (type == "host")
    {
        QSharedPointer<Host> host = this->getSelectedObject().dynamicCast<Host>();
        if (host && host->IsValid())
        {
            hosts.append(host);
        }
    }
    
    return hosts;
}

bool CertificateCommand::isVersionSupported(XenConnection* connection) const
{
    // TODO: Implement when Helpers::StockholmOrGreater is ported
    // return Helpers::StockholmOrGreater(connection);
    Q_UNUSED(connection);
    return true;  // Placeholder
}

// =============================================================================
// InstallCertificateCommand
// =============================================================================

InstallCertificateCommand::InstallCertificateCommand(MainWindow* mainWindow, QObject* parent)
    : CertificateCommand(mainWindow, parent)
{
}

InstallCertificateCommand::InstallCertificateCommand(MainWindow* mainWindow, const QList<QSharedPointer<Host>>& hosts, QObject* parent)
    : CertificateCommand(mainWindow, hosts, parent)
{
}

void InstallCertificateCommand::Run()
{
    QList<QSharedPointer<Host>> hosts = this->getHosts();
    if (hosts.isEmpty())
        return;
    
    QSharedPointer<Host> host = hosts.first();
    if (!host || !host->IsValid())
        return;
    
    // Check if HA is enabled
    QSharedPointer<Pool> pool = host->GetPool();
    if (pool && pool->IsValid() && pool->HAEnabled())
    {
        QMessageBox::warning(
            this->mainWindow(),
            "Cannot Install Certificate",
            "HA is enabled on this pool. Disable HA before installing certificates."
        );
        return;
    }
    
    // TODO: Show InstallCertificateDialog when ported
    // InstallCertificateDialog* dialog = new InstallCertificateDialog(host, this->mainWindow());
    // dialog->exec();
    // dialog->deleteLater();
    
    QMessageBox::information(
        this->mainWindow(),
        "Install Certificate",
        "TODO: Show InstallCertificateDialog (not yet ported)"
    );
}

// =============================================================================
// ResetCertificateCommand
// =============================================================================

ResetCertificateCommand::ResetCertificateCommand(MainWindow* mainWindow, QObject* parent)
    : CertificateCommand(mainWindow, parent)
{
}

ResetCertificateCommand::ResetCertificateCommand(MainWindow* mainWindow, const QList<QSharedPointer<Host>>& hosts, QObject* parent)
    : CertificateCommand(mainWindow, hosts, parent)
{
}

bool ResetCertificateCommand::CanRun() const
{
    // First check base requirements
    if (!CertificateCommand::CanRun())
        return false;
    
    QList<QSharedPointer<Host>> hosts = this->getHosts();
    if (hosts.isEmpty())
        return false;
    
    XenConnection* connection = hosts.first()->GetConnection();
    if (!connection)
        return false;
    
    // Reset requires Cloud or greater (API 1.290.0+)
    return this->isResetVersionSupported(connection);
}

void ResetCertificateCommand::Run()
{
    QList<QSharedPointer<Host>> hosts = this->getHosts();
    if (hosts.isEmpty())
        return;
    
    QSharedPointer<Host> host = hosts.first();
    if (!host || !host->IsValid())
        return;
    
    // Check if HA is enabled
    QSharedPointer<Pool> pool = host->GetPool();
    if (pool && pool->IsValid() && pool->HAEnabled())
    {
        QMessageBox::warning(
            this->mainWindow(),
            "Cannot Reset Certificate",
            "HA is enabled on this pool. Disable HA before resetting certificates."
        );
        return;
    }
    
    // Show warning dialog
    // TODO: Use WarningDialog when ported
    QMessageBox::StandardButton result = QMessageBox::warning(
        this->mainWindow(),
        "Reset Server Certificate",
        QString("Are you sure you want to reset the server certificate on %1 to a self-signed certificate?\n\n"
                "This will restart the toolstack and may disrupt connections.").arg(host->GetName()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    
    if (result != QMessageBox::Yes)
        return;
    
    // TODO: Create and run DelegatedAsyncAction for Host::reset_server_certificate
    // For now, show placeholder
    QMessageBox::information(
        this->mainWindow(),
        "Reset Certificate",
        "TODO: Run Host.reset_server_certificate async action (not yet implemented)"
    );
}

bool ResetCertificateCommand::isResetVersionSupported(XenConnection* connection) const
{
    // TODO: Implement when Helpers::CloudOrGreater and Helpers::XapiEqualOrGreater_1_290_0 are ported
    // return Helpers::CloudOrGreater(connection) && Helpers::XapiEqualOrGreater_1_290_0(connection);
    Q_UNUSED(connection);
    return true;  // Placeholder
}
