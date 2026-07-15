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
#include "../../dialogs/actionprogressdialog.h"
#include "../../dialogs/installcertificatedialog.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/actions/delegatedasyncoperation.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/session.h"
#include "xenlib/xen/xenapi/xenapi_Host.h"
#include "xenlib/xencache.h"
#include "xenlib/utils/misc.h"
#include <QMessageBox>
#include <stdexcept>

namespace
{
    QString hostXapiVersion(const QSharedPointer<Host>& host)
    {
        if (!host)
            return QString();

        const QVariantMap softwareVersion = host->SoftwareVersion();
        QString version = softwareVersion.value(QStringLiteral("xapi_build")).toString();
        if (version.isEmpty())
            version = softwareVersion.value(QStringLiteral("xapi")).toString();
        return version;
    }

}

// =============================================================================
// CertificateCommand (Base Class)
// =============================================================================

CertificateCommand::CertificateCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
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
    
    if (!this->isVersionSupported(host))
        return false;
    
    // Host must be standalone or pool coordinator
    QSharedPointer<Pool> pool = host->GetPoolOfOne();
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
    QList<QSharedPointer<Host>> hosts;
    const QList<QSharedPointer<XenObject>> objects = this->getSelectedObjects();
    for (const QSharedPointer<XenObject>& obj : objects)
    {
        if (!obj || obj->GetObjectType() != XenObjectType::Host)
            continue;

        QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(obj);
        if (host && host->IsValid())
            hosts.append(host);
    }

    if (!hosts.isEmpty())
        return hosts;

    QSharedPointer<Host> host = this->getSelectedObject().dynamicCast<Host>();
    if (host && host->IsValid())
        return { host };

    return {};
}

bool CertificateCommand::isVersionSupported(const QSharedPointer<Host>& host) const
{
    return host && !host->PlatformVersion().isEmpty() &&
           Misc::ProductVersionCompare(host->PlatformVersion(), QStringLiteral("3.1.50")) >= 0;
}

// =============================================================================
// InstallCertificateCommand
// =============================================================================

InstallCertificateCommand::InstallCertificateCommand(MainWindow* mainWindow, QObject* parent)
    : CertificateCommand(mainWindow, parent)
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
    QSharedPointer<Pool> pool = host->GetPoolOfOne();
    if (pool && pool->IsValid() && pool->HAEnabled())
    {
        QMessageBox::warning(MainWindow::instance(), "Cannot Install Certificate", "HA is enabled on this pool. Disable HA before installing certificates.");
        return;
    }
    
    InstallCertificateDialog dialog(host, MainWindow::instance());
    dialog.exec();
}

// =============================================================================
// ResetCertificateCommand
// =============================================================================

ResetCertificateCommand::ResetCertificateCommand(MainWindow* mainWindow, QObject* parent) : CertificateCommand(mainWindow, parent)
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
    
    return this->isResetVersionSupported(hosts.first());
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
    QSharedPointer<Pool> pool = host->GetPoolOfOne();
    if (pool && pool->IsValid() && pool->HAEnabled())
    {
        QMessageBox::warning(MainWindow::instance(), "Cannot Reset Certificate", "HA is enabled on this pool. Disable HA before resetting certificates.");
        return;
    }
    
    QMessageBox::StandardButton result = QMessageBox::warning(
        MainWindow::instance(),
        "Reset Server Certificate",
        QString("Are you sure you want to reset the server certificate on %1 to a self-signed certificate?\n\n"
                "This will restart the toolstack and may disrupt connections.").arg(host->GetName()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    
    if (result != QMessageBox::Yes)
        return;

    XenConnection* connection = host->GetConnection();
    const QString hostRef = host->OpaqueRef();
    const QString hostName = host->GetName();
    DelegatedAsyncOperation* operation = new DelegatedAsyncOperation(
        connection,
        tr("Resetting server certificate on %1").arg(hostName),
        tr("Resetting server certificate..."),
        [connection, hostRef](DelegatedAsyncOperation* self)
        {
            XenAPI::Session* session = self->GetSession();
            if (!connection || !session)
                throw std::runtime_error(QObject::tr("No active XenAPI session is available.").toLocal8Bit().constData());

            const bool previousExpectDisruption = connection->GetExpectDisruption();
            connection->SetExpectDisruption(true);
            try
            {
                XenAPI::Host::reset_server_certificate(session, hostRef);
            } catch (...)
            {
                connection->SetExpectDisruption(previousExpectDisruption);
                throw;
            }

            connection->SetExpectDisruption(previousExpectDisruption);
            self->SetPercentComplete(100);
            self->SetDescription(QObject::tr("Server certificate reset."));
        });
    operation->SetHost(host);

    ActionProgressDialog progress(operation, MainWindow::instance());
    progress.setShowCancel(false);
    progress.exec();
    operation->deleteLater();
}

bool ResetCertificateCommand::isResetVersionSupported(const QSharedPointer<Host>& host) const
{
    return host &&
           !host->PlatformVersion().isEmpty() &&
           !hostXapiVersion(host).isEmpty() &&
           Misc::ProductVersionCompare(host->PlatformVersion(), QStringLiteral("3.2.50")) >= 0 &&
           Misc::ProductVersionCompare(hostXapiVersion(host), QStringLiteral("1.290.0")) >= 0;
}
