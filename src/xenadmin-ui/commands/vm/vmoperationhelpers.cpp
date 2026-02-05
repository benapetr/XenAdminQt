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

#include "vmoperationhelpers.h"
#include "../../dialogs/commanderrordialog.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/friendlyerrornames.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/session.h"
#include "xenlib/xen/failure.h"
#include "xenlib/xen/xenapi/xenapi_VM.h"
#include <QMessageBox>
#include <QDebug>
#include <QRegularExpression>

namespace
{
    QList<int> parseVersionParts(const QString& version)
    {
        QList<int> parts;
        QStringList tokens = version.split(QRegularExpression("[^0-9]"), Qt::SkipEmptyParts);
        for (const QString& token : tokens)
        {
            bool ok = false;
            int value = token.toInt(&ok);
            if (ok)
                parts.append(value);
        }
        return parts;
    }

    int compareVersions(const QString& a, const QString& b)
    {
        QList<int> partsA = parseVersionParts(a);
        QList<int> partsB = parseVersionParts(b);
        int maxSize = qMax(partsA.size(), partsB.size());
        for (int i = 0; i < maxSize; ++i)
        {
            int va = i < partsA.size() ? partsA[i] : 0;
            int vb = i < partsB.size() ? partsB[i] : 0;
            if (va < vb)
                return -1;
            if (va > vb)
                return 1;
        }
        return 0;
    }

    bool vmCpuIncompatibleWithHost(const QSharedPointer<VM>& vm, const QSharedPointer<Host>& host)
    {
        if (!vm || !host)
            return false;

        QString powerState = vm->GetPowerState();
        if (powerState != "Running" && powerState != "Suspended")
            return false;

        QVariantMap vmFlags = vm->LastBootCPUFlags();
        QVariantMap hostCpuInfo = host->GetCPUInfo();
        if (!vmFlags.contains("vendor") || !hostCpuInfo.contains("vendor"))
            return false;

        return vmFlags.value("vendor").toString() != hostCpuInfo.value("vendor").toString();
    }
}

void VMOperationHelpers::StartDiagnosisForm(XenConnection* connection, const QString& vmRef, const QString& vmName, bool isStart, QWidget* parent)
{
    if (!connection)
    {
        qWarning() << "VMOperationHelpers::startDiagnosisForm: Invalid connection";
        return;
    }

    XenAPI::Session* session = connection->GetSession();
    if (!session || !session->IsLoggedIn())
    {
        qWarning() << "VMOperationHelpers::startDiagnosisForm: Session is not valid";
        return;
    }

    XenCache* cache = connection->GetCache();

    QString title = isStart ? QObject::tr("Error Starting VM") : QObject::tr("Error Resuming VM");
    QString text = QObject::tr("The VM '%1' could not be %2. The following servers cannot run this VM:")
                       .arg(vmName, isStart ? "started" : "resumed");

    QMap<QString, QPair<QString, QString>> reasons;
    QList<QSharedPointer<Host>> hosts = cache->GetAll<Host>(XenObjectType::Host);
    
    if (hosts.isEmpty())
    {
        qWarning() << "VMOperationHelpers::startDiagnosisForm: No hosts found in cache";
        QMessageBox::warning(parent, title, 
                           QObject::tr("Could not retrieve host information from the server."));
        return;
    }

    qDebug() << "VMOperationHelpers: Checking" << hosts.size() << "hosts for VM" << vmName;

    foreach (const QSharedPointer<Host> &host, hosts)
    {
        QString hostRef = host->OpaqueRef();
        QString hostName = host->GetName();
        QString reason;
        bool canBoot = false;

        try
        {
            XenAPI::VM::assert_can_boot_here(session, vmRef, hostRef);
            canBoot = true;
        }
        catch (const Failure& failure)
        {
            reason = failure.message();
            qDebug() << "VMOperationHelpers: Host" << hostName << "cannot run VM:" << reason;
        }
        catch (const std::exception& e)
        {
            qWarning() << "VMOperationHelpers: Error calling assert_can_boot_here on host" 
                      << hostName << ":" << e.what();
            reason = QObject::tr("Unknown error checking this server");
        }

        if (!canBoot && !reason.isEmpty())
        {
            QString iconPath = ":/images/tree_host.png";
            reasons.insert(hostName, qMakePair(iconPath, reason));
        }
    }

    if (reasons.isEmpty())
    {
        QMessageBox::information(parent, title,
                                QObject::tr("The VM '%1' could not be %2, but all servers "
                                          "appear capable of running it. This may be a temporary condition.")
                                .arg(vmName, isStart ? "started" : "resumed"));
    }
    else
    {
        CommandErrorDialog dialog(title, text, reasons, CommandErrorDialog::DialogMode::Close, parent);
        dialog.exec();
    }
}

void VMOperationHelpers::StartDiagnosisForm(XenConnection* connection, const QString& vmRef, const QString& vmName, bool isStart, const Failure& failure, QWidget* parent)
{
    QString errorCode = failure.errorCode();
    
    qDebug() << "VMOperationHelpers::startDiagnosisForm: Error code:" << errorCode 
             << "for VM:" << vmName;
    
    if (errorCode == Failure::NO_HOSTS_AVAILABLE)
    {
        qDebug() << "VMOperationHelpers: NO_HOSTS_AVAILABLE - starting host diagnosis";
        StartDiagnosisForm(connection, vmRef, vmName, isStart, parent);
    }
    else if (errorCode == Failure::HA_OPERATION_WOULD_BREAK_FAILOVER_PLAN)
    {
        QStringList errorParams = failure.errorDescription();
        QString title = isStart ? QObject::tr("Error Starting VM") : QObject::tr("Error Resuming VM");
        
        int currentNtol = 0;
        int newNtol = 0;
        
        if (errorParams.size() >= 3)
        {
            bool ok1, ok2;
            currentNtol = errorParams[1].toInt(&ok1);
            int maxNtol = errorParams[2].toInt(&ok2);
            
            if (ok1 && ok2 && currentNtol > 0)
            {
                newNtol = currentNtol - 1;
                
                QString message = QObject::tr(
                    "Starting this VM would break the High Availability failover plan.\n\n"
                    "Current servers to tolerate: %1\n"
                    "Maximum possible: %2\n\n"
                    "Would you like to reduce the number of servers to tolerate to %3 and try again?"
                ).arg(currentNtol).arg(maxNtol).arg(newNtol);
                
                QMessageBox::StandardButton reply = QMessageBox::question(
                    parent, title, message,
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No
                );
                
                if (reply == QMessageBox::Yes)
                {
                    QMessageBox::information(parent, title,
                        QObject::tr("Reducing the number of servers to tolerate and retrying "
                                  "is not yet implemented.\n\n"
                                  "You can manually adjust HA settings in the pool properties."));
                }
                return;
            }
        }
        
        QString message = QObject::tr(
            "Starting this VM would break the High Availability failover plan.\n\n"
            "You may need to reduce the number of server failures to tolerate in your HA configuration."
        );
        QMessageBox::warning(parent, title, message);
    }
    else
    {
        QString title = isStart ? QObject::tr("Error Starting VM") : QObject::tr("Error Resuming VM");
        QString text = QObject::tr("The VM '%1' could not be %2:\n\n%3")
                           .arg(vmName, isStart ? "started" : "resumed", failure.message());
        QMessageBox::critical(parent, title, text);
    }
}

bool VMOperationHelpers::VMCanBootOnHost(XenConnection* connection, const QSharedPointer<VM>& vm, const QString& hostRef, const QString& operation, QString* cannotBootReason)
{
    if (!vm)
    {
        if (cannotBootReason)
            *cannotBootReason = QObject::tr("Unknown VM");
        return false;
    }

    if (!connection || !connection->IsConnected())
    {
        if (cannotBootReason)
            *cannotBootReason = QObject::tr("Not connected to server");
        return false;
    }

    XenCache* cache = connection->GetCache();
    if (!cache)
    {
        if (cannotBootReason)
            *cannotBootReason = QObject::tr("Cache is not available");
        return false;
    }

    if (hostRef.isEmpty() || hostRef == XENOBJECT_NULL)
    {
        if (cannotBootReason)
            *cannotBootReason = QObject::tr("No home server");
        return false;
    }

    QSharedPointer<Host> host = cache->ResolveObject<Host>(XenObjectType::Host, hostRef);
    if (!host)
    {
        if (cannotBootReason)
            *cannotBootReason = QObject::tr("No home server");
        return false;
    }

    if (vm->GetPowerState() == "Running")
    {
        QString residentRef = vm->GetResidentOnRef();
        if (!residentRef.isEmpty() && residentRef != XENOBJECT_NULL)
        {
            if (residentRef == hostRef)
            {
                if (cannotBootReason)
                    *cannotBootReason = QObject::tr("The VM is already on the selected host.");
                return false;
            }

            QSharedPointer<Host> residentHost = cache->ResolveObject<Host>(XenObjectType::Host, residentRef);
            if (residentHost)
            {
                QString targetVersion = host->SoftwareVersion().value("product_version").toString();
                QString residentVersion = residentHost->SoftwareVersion().value("product_version").toString();
                if (!targetVersion.isEmpty() && !residentVersion.isEmpty() && compareVersions(targetVersion, residentVersion) < 0)
                {
                    if (cannotBootReason)
                        *cannotBootReason = QObject::tr("The destination host is older than the current host.");
                    return false;
                }
            }
        }
    }

    if (operation == "pool_migrate" || operation == "resume_on")
    {
        if (vmCpuIncompatibleWithHost(vm, host))
        {
            if (cannotBootReason)
            {
                QString msg = FriendlyErrorNames::getString("VM_INCOMPATIBLE_WITH_THIS_HOST");
                *cannotBootReason = msg.isEmpty() ? QObject::tr("VM is incompatible with this host.") : msg;
            }
            return false;
        }
    }

    XenAPI::Session* session = connection->GetSession();
    if (!session || !session->IsLoggedIn())
    {
        if (cannotBootReason)
            *cannotBootReason = QObject::tr("Session is not valid");
        return false;
    }

    try
    {
        XenAPI::VM::assert_can_boot_here(session, vm->OpaqueRef(), hostRef);
    }
    catch (const Failure& failure)
    {
        QStringList params = failure.errorDescription();
        if (params.size() > 2 && params[0] == Failure::VM_REQUIRES_SR)
        {
            QString srRef = params[2];
            QSharedPointer<SR> sr = cache->ResolveObject<SR>(XenObjectType::SR, srRef);
            if (sr && sr->ContentType() == "iso")
            {
                if (cannotBootReason)
                    *cannotBootReason = QObject::tr("Please eject the CD/DVD from the VM and try again.");
                return false;
            }
        }

        if (cannotBootReason)
            *cannotBootReason = failure.message();
        return false;
    }
    catch (const std::exception&)
    {
        if (cannotBootReason)
            *cannotBootReason = QObject::tr("Unknown error checking this server");
        return false;
    }

    if (cannotBootReason)
        *cannotBootReason = QString();

    return true;
}
