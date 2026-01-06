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

#include "srreattachaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../../xencache.h"
#include "../../sr.h"
#include "../../host.h"
#include "../../xenapi/xenapi_SR.h"
#include "../../xenapi/xenapi_PBD.h"
#include "../../xenapi/xenapi_Host.h"
#include <QDebug>

SrReattachAction::SrReattachAction(QSharedPointer<SR> sr,
                                   const QString& name,
                                   const QString& description,
                                   const QVariantMap& deviceConfig,
                                   QObject* parent)
    : AsyncOperation(sr ? sr->GetConnection() : nullptr,
                     QString("Reattaching SR '%1'").arg(name),
                     QString("Reattaching storage repository..."),
                     parent),
      m_sr(sr), m_name(name), m_description(description), m_deviceConfig(deviceConfig)
{
    if (sr)
    {
        setAppliesToFromObject(sr);
    }
}

void SrReattachAction::run()
{
    if (!m_sr)
    {
        setError("No SR specified for reattachment");
        return;
    }

    qDebug() << "SrReattachAction: Reattaching SR" << m_sr->GetUUID()
             << "name:" << m_name
             << "description:" << m_description;

    XenAPI::Session* session = this->GetSession();
    if (!session || !session->IsLoggedIn())
    {
        setError("Not connected to XenServer");
        return;
    }

    SetDescription("Reattaching storage repository...");

    QString srRef = m_sr->OpaqueRef();

    // Get all hosts in the pool using XenAPI
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

    // Create and plug PBDs for each host with new device config
    int progressPerHost = 100 / (hostRefs.count() * 2); // 2 steps per host (create + plug)
    int currentProgress = 0;

    for (const QString& hostRef : hostRefs)
    {
        // Create PBD
        qDebug() << "SrReattachAction: Creating PBD for host" << hostRef;
        SetDescription("Creating storage connection for host...");

        QVariantMap pbdRecord;
        pbdRecord["SR"] = srRef;
        pbdRecord["host"] = hostRef;
        pbdRecord["device_config"] = m_deviceConfig;
        pbdRecord["currently_attached"] = false;

        QString pbdRef;
        try
        {
            QString taskRef = XenAPI::PBD::async_create(session, pbdRecord);
            pollToCompletion(taskRef, currentProgress, currentProgress + progressPerHost);
            pbdRef = GetResult();
            currentProgress += progressPerHost;
        } catch (const std::exception& e)
        {
            setError(QString("Failed to create PBD for host: %1")
                         .arg(e.what()));
            return;
        }

        // Plug PBD
        qDebug() << "SrReattachAction: Plugging PBD";
        SetDescription("Plugging storage on host...");

        try
        {
            QString taskRef = XenAPI::PBD::async_plug(session, pbdRef);
            pollToCompletion(taskRef, currentProgress, currentProgress + progressPerHost);
            currentProgress += progressPerHost;
        } catch (const std::exception& e)
        {
            setError(QString("Failed to plug PBD: %1")
                         .arg(e.what()));
            return;
        }
    }

    // Update SR name and description
    qDebug() << "SrReattachAction: Updating SR metadata";
    SetDescription("Updating storage repository properties...");

    try
    {
        XenAPI::SR::set_name_label(session, srRef, m_name);
        XenAPI::SR::set_name_description(session, srRef, m_description);
    } catch (const std::exception& e)
    {
        qWarning() << "SrReattachAction: Failed to update SR metadata:" << e.what();
        // Non-fatal - SR is already reattached
    }

    SetDescription("Storage repository attached successfully");
    SetPercentComplete(100);
}
