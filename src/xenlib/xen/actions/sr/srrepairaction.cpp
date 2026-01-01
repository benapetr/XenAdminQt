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

#include "srrepairaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../sr.h"
#include "../../xenapi/xenapi_SR.h"
#include "../../xenapi/xenapi_PBD.h"
#include "../../xenapi/xenapi_Host.h"
#include <QDebug>

SrRepairAction::SrRepairAction(SR* sr,
                               bool isSharedAction,
                               QObject* parent)
    : AsyncOperation(sr ? sr->GetConnection() : nullptr,
                     isSharedAction ? QString("Sharing SR '%1'").arg(sr ? sr->GetName() : "")
                                    : QString("Repairing SR '%1'").arg(sr ? sr->GetName() : ""),
                     isSharedAction ? QString("Sharing storage repository...")
                                    : QString("Repairing storage repository..."),
                     parent),
      m_sr(sr), m_isSharedAction(isSharedAction)
{
    if (sr)
    {
        setAppliesToFromObject(sr);
    }

    // RBAC dependencies
    addApiMethodToRoleCheck("PBD.plug");
    addApiMethodToRoleCheck("PBD.create");
}

void SrRepairAction::run()
{
    if (!m_sr)
    {
        setError("No SR specified for repair");
        return;
    }

    XenAPI::Session* session = this->session();
    if (!session || !session->IsLoggedIn())
    {
        setError("Not connected to XenServer");
        return;
    }

    qDebug() << "SrRepairAction: Repairing SR" << m_sr->GetName();

    QString srRef = m_sr->OpaqueRef();
    bool srShared = m_sr->IsShared();

    // Get all PBDs for this SR
    QVariant pbdsVar = XenAPI::SR::get_PBDs(session, srRef);
    QStringList pbdRefs;

    if (pbdsVar.canConvert<QStringList>())
    {
        pbdRefs = pbdsVar.toStringList();
    } else if (pbdsVar.canConvert<QVariantList>())
    {
        for (const QVariant& pbd : pbdsVar.toList())
        {
            pbdRefs.append(pbd.toString());
        }
    }

    // Get template PBD for creating new ones (use first PBD)
    QString templatePbdRef;
    QVariantMap templateDeviceConfig;

    if (!pbdRefs.isEmpty())
    {
        templatePbdRef = pbdRefs.first();
        try
        {
            QVariantMap pbdRecord = XenAPI::PBD::get_record(session, templatePbdRef);
            templateDeviceConfig = pbdRecord.value("device_config").toMap();
        } catch (const std::exception& e)
        {
            qWarning() << "SrRepairAction: Failed to get template PBD:" << e.what();
        }
    }

    // Get all hosts
    QVariant hostsVar;
    try
    {
        hostsVar = XenAPI::Host::get_all(session);
    } catch (const std::exception& e)
    {
        setError(QString("Failed to get host list: %1").arg(e.what()));
        return;
    }

    QStringList hostRefs;
    if (hostsVar.canConvert<QStringList>())
    {
        hostRefs = hostsVar.toStringList();
    } else if (hostsVar.canConvert<QVariantList>())
    {
        for (const QVariant& host : hostsVar.toList())
        {
            hostRefs.append(host.toString());
        }
    }

    if (hostRefs.isEmpty())
    {
        setError("No hosts found in pool");
        return;
    }

    // TODO: Sort hosts to put coordinator first (requires Pool.get_master API)
    // For now, process in order returned

    int max = hostRefs.count() * 2; // 2 operations per host: create PBD + plug
    int current = 0;

    QString lastError;
    QString lastErrorDescription;

    for (const QString& hostRef : hostRefs)
    {
        // Check if this host already has a PBD to this SR
        bool hasPbd = false;
        QString existingPbdRef;
        bool pbdAttached = false;

        for (const QString& pbdRef : pbdRefs)
        {
            try
            {
                QVariantMap pbdRecord = XenAPI::PBD::get_record(session, pbdRef);
                if (pbdRecord.value("host").toString() == hostRef)
                {
                    hasPbd = true;
                    existingPbdRef = pbdRef;
                    pbdAttached = pbdRecord.value("currently_attached").toBool();
                    break;
                }
            } catch (const std::exception& e)
            {
                qWarning() << "SrRepairAction: Failed to get PBD record:" << e.what();
            }
        }

        // Create PBD if it doesn't exist and SR is shared
        if (!hasPbd && srShared && !templateDeviceConfig.isEmpty())
        {
            qDebug() << "SrRepairAction: Creating PBD for host" << hostRef;
            setDescription("Creating storage connection for host...");

            QVariantMap newPbdRecord;
            newPbdRecord["SR"] = srRef;
            newPbdRecord["host"] = hostRef;
            newPbdRecord["device_config"] = templateDeviceConfig;
            newPbdRecord["currently_attached"] = false;

            try
            {
                QString taskRef = XenAPI::PBD::async_create(session, newPbdRecord);
                int progressStart = (current * 100) / max;
                int progressEnd = ((current + 1) * 100) / max;
                pollToCompletion(taskRef, progressStart, progressEnd);

                existingPbdRef = result();
                hasPbd = true;
                pbdAttached = false;

                current++;
            } catch (const std::exception& e)
            {
                lastError = QString("Failed to create PBD: %1").arg(e.what());
                lastErrorDescription = description();
                qWarning() << "SrRepairAction:" << lastError;
                current++;
                continue; // Continue with other hosts
            }
        } else
        {
            current++;
        }

        // Plug PBD if it exists but not attached
        if (hasPbd && !pbdAttached && !existingPbdRef.isEmpty())
        {
            qDebug() << "SrRepairAction: Plugging PBD" << existingPbdRef;
            setDescription("Plugging storage on host...");

            try
            {
                QString taskRef = XenAPI::PBD::async_plug(session, existingPbdRef);
                int progressStart = (current * 100) / max;
                int progressEnd = ((current + 1) * 100) / max;
                pollToCompletion(taskRef, progressStart, progressEnd);

                current++;
            } catch (const std::exception& e)
            {
                lastError = QString("Failed to plug PBD: %1").arg(e.what());
                lastErrorDescription = description();
                qWarning() << "SrRepairAction:" << lastError;
                current++;
                continue; // Continue with other hosts
            }
        } else
        {
            current++;
        }
    }

    // Report last error if any occurred
    if (!lastError.isEmpty())
    {
        setDescription(lastErrorDescription);
        throw std::runtime_error(lastError.toStdString());
    }

    // Success
    setPercentComplete(100);
    if (m_isSharedAction)
        setDescription(QString("SR '%1' shared successfully").arg(m_sr->GetName()));
    else
        setDescription(QString("SR '%1' repaired successfully").arg(m_sr->GetName()));

    qDebug() << "SrRepairAction: Repair complete";
}
