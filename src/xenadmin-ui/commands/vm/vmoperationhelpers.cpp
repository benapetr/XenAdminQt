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
#include "xenlib.h"
#include "xencache.h"
#include "xen/connection.h"
#include "xen/session.h"
#include "xen/failure.h"
#include "xen/xenapi/xenapi_VM.h"
#include "xen/xenapi/xenapi_Pool.h"
#include <QMessageBox>
#include <QDebug>
#include <new>

using namespace XenAPI;

void VMOperationHelpers::startDiagnosisForm(XenLib* xenLib,
                                           XenConnection* connection,
                                           const QString& vmRef,
                                           const QString& vmName,
                                           bool isStart,
                                           QWidget* parent)
{
    if (!xenLib || !connection)
    {
        qWarning() << "VMOperationHelpers::startDiagnosisForm: Invalid xenLib or connection";
        return;
    }

    XenSession* session = connection->getSession();
    if (!session || !session->isLoggedIn())
    {
        qWarning() << "VMOperationHelpers::startDiagnosisForm: Session is not valid";
        return;
    }

    XenCache* cache = xenLib->getCache();
    if (!cache)
    {
        qWarning() << "VMOperationHelpers::startDiagnosisForm: XenCache is not available";
        return;
    }

    QString title = isStart ? QObject::tr("Error Starting VM") : QObject::tr("Error Resuming VM");
    QString text = QObject::tr("The VM '%1' could not be %2. The following servers cannot run this VM:")
                       .arg(vmName, isStart ? "started" : "resumed");

    QMap<QString, QPair<QString, QString>> reasons;
    QList<QVariantMap> hosts = cache->GetAll("host");
    
    if (hosts.isEmpty())
    {
        qWarning() << "VMOperationHelpers::startDiagnosisForm: No hosts found in cache";
        QMessageBox::warning(parent, title, 
                           QObject::tr("Could not retrieve host information from the server."));
        return;
    }

    qDebug() << "VMOperationHelpers: Checking" << hosts.size() << "hosts for VM" << vmName;

    for (const QVariantMap& hostData : hosts)
    {
        QString hostRef = hostData.value("ref").toString();
        QString hostName = hostData.value("name_label", "Unknown Host").toString();

        if (hostRef.isEmpty())
        {
            qWarning() << "VMOperationHelpers: Host with no ref, skipping";
            continue;
        }

        QString reason;
        bool canBoot = false;

        try
        {
            VM::assert_can_boot_here(session, vmRef, hostRef);
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
        CommandErrorDialog dialog(title, text, reasons, 
                                 CommandErrorDialog::DialogMode::Close, 
                                 parent);
        dialog.exec();
    }
}

void VMOperationHelpers::startDiagnosisForm(XenLib* xenLib,
                                           XenConnection* connection,
                                           const QString& vmRef,
                                           const QString& vmName,
                                           bool isStart,
                                           const Failure& failure,
                                           QWidget* parent)
{
    QString errorCode = failure.errorCode();
    
    qDebug() << "VMOperationHelpers::startDiagnosisForm: Error code:" << errorCode 
             << "for VM:" << vmName;
    
    if (errorCode == Failure::NO_HOSTS_AVAILABLE)
    {
        qDebug() << "VMOperationHelpers: NO_HOSTS_AVAILABLE - starting host diagnosis";
        startDiagnosisForm(xenLib, connection, vmRef, vmName, isStart, parent);
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
