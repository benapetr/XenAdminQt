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

#include "srintroduceaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../../xencache.h"
#include "../../sr.h"
#include "../../host.h"
#include "../../pool.h"
#include "../../xenapi/xenapi_SR.h"
#include "../../xenapi/xenapi_PBD.h"
#include "../../xenapi/xenapi_Pool.h"
#include "../../xenapi/xenapi_Host.h"
#include <QDebug>

using namespace XenAPI;

SrIntroduceAction::SrIntroduceAction(XenConnection* connection,
                                     const QString& srUuid,
                                     const QString& srName,
                                     const QString& srDescription,
                                     const QString& srType,
                                     const QString& srContentType,
                                     const QVariantMap& deviceConfig,
                                     QObject* parent)
    : AsyncOperation(connection,
                     QString("Attaching SR '%1'").arg(srName),
                     QString("Introducing storage repository..."),
                     parent),
      m_srUuid(srUuid), m_srName(srName), m_srDescription(srDescription), m_srType(srType), m_srContentType(srContentType), m_srIsShared(true) // Always true now
      ,
      m_deviceConfig(deviceConfig)
{
}

void SrIntroduceAction::run()
{
    qDebug() << "SrIntroduceAction: Introducing SR" << this->m_srUuid
             << "name:" << this->m_srName
             << "type:" << this->m_srType;

    Session* session = this->GetSession();
    if (!session || !session->IsLoggedIn())
    {
        this->setError("Not connected to XenServer");
        return;
    }

    // Step 1: Preemptive SR.forget() in case SR is in broken state
    qDebug() << "SrIntroduceAction: Performing preemptive SR.forget";
    this->SetDescription("Checking existing SR state...");

    try
    {
        QString existingRef = XenAPI::SR::get_by_uuid(session, this->m_srUuid);
        if (!existingRef.isEmpty())
        {
            qDebug() << "SrIntroduceAction: Found existing SR, forgetting it";
            QString taskRef = XenAPI::SR::async_forget(session, existingRef);
            this->pollToCompletion(taskRef, 0, 5);
        }
    } catch (const std::exception& e)
    {
        // Allow failure - SR might not exist
        qDebug() << "SrIntroduceAction: Preemptive forget failed (expected):" << e.what();
    }

    // Step 2: Introduce the SR
    qDebug() << "SrIntroduceAction: Introducing SR";
    this->SetDescription("Introducing storage repository...");

    QString srRef;
    try
    {
        QString taskRef = XenAPI::SR::async_introduce(session,
                                                      this->m_srUuid,
                                                      this->m_srName,
                                                      this->m_srDescription,
                                                      this->m_srType,
                                                      this->m_srContentType,
                                                      this->m_srIsShared,
                                                      QVariantMap()); // Empty smconfig

        this->pollToCompletion(taskRef, 5, 10);
        srRef = this->GetResult();

        if (srRef.isEmpty())
        {
            throw std::runtime_error("SR.async_introduce returned empty reference");
        }

        // Cache the result for later
        this->SetResult(srRef);
    } catch (const std::exception& e)
    {
        this->setError(QString("Failed to introduce SR: %1").arg(e.what()));
        return;
    }

    // Step 3: Create and plug PBDs for each host
    qDebug() << "SrIntroduceAction: Creating PBDs for all hosts";
    this->SetDescription("Creating storage connections...");

    // Get host list from XenAPI (simple approach - just get all hosts)
    QVariant hostsVar;
    try
    {
        hostsVar = XenAPI::Host::get_all(session);
    } catch (const std::exception& e)
    {
        this->setError(QString("Failed to get host list: %1").arg(e.what()));
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
        this->setError("No hosts found in pool");
        return;
    }

    int progressPerHost = 90 / (hostRefs.count() * 2); // 2 steps per host (create + plug)
    int currentProgress = 10;

    for (const QString& hostRef : hostRefs)
    {
        // Create PBD
        qDebug() << "SrIntroduceAction: Creating PBD for host" << hostRef;
        this->SetDescription(QString("Creating storage connection for host..."));

        QVariantMap pbdRecord;
        pbdRecord["SR"] = srRef;
        pbdRecord["host"] = hostRef;
        pbdRecord["device_config"] = this->m_deviceConfig;
        pbdRecord["currently_attached"] = false;

        QString pbdRef;
        try
        {
            QString taskRef = XenAPI::PBD::async_create(session, pbdRecord);
            this->pollToCompletion(taskRef, currentProgress, currentProgress + progressPerHost);
            pbdRef = this->GetResult();
            currentProgress += progressPerHost;
        } catch (const std::exception& e)
        {
            this->setError(QString("Failed to create PBD for host: %1")
                         .arg(e.what()));
            return;
        }

        // Plug PBD
        qDebug() << "SrIntroduceAction: Plugging PBD";
        this->SetDescription("Plugging storage on host...");

        try
        {
            QString taskRef = XenAPI::PBD::async_plug(session, pbdRef);
            this->pollToCompletion(taskRef, currentProgress, currentProgress + progressPerHost);
            currentProgress += progressPerHost;
        } catch (const std::exception& e)
        {
            this->setError(QString("Failed to plug PBD: %1")
                         .arg(e.what()));
            return;
        }
    }

    // Set as default SR if this is the first shared non-ISO SR
    if (this->isFirstSharedNonISOSR())
    {
        qDebug() << "SrIntroduceAction: This is first shared non-ISO SR, setting as default";

        try
        {
            // Get the pool reference (there's typically only one pool)
            QVariant poolsVar = XenAPI::Pool::get_all(session);
            QStringList poolRefs;

            if (poolsVar.canConvert<QStringList>())
            {
                poolRefs = poolsVar.toStringList();
            } else if (poolsVar.canConvert<QVariantList>())
            {
                for (const QVariant& pool : poolsVar.toList())
                {
                    poolRefs.append(pool.toString());
                }
            }

            if (!poolRefs.isEmpty())
            {
                QString poolRef = poolRefs.first();
                XenAPI::Pool::set_default_SR(session, poolRef, srRef);
                qDebug() << "SrIntroduceAction: Set SR as default for pool";
            }
        } catch (const std::exception& e)
        {
            qWarning() << "SrIntroduceAction: Failed to set default SR (non-fatal):" << e.what();
            // Non-fatal - SR is introduced successfully even if we can't set it as default
        }
    }

    this->SetPercentComplete(100);
    this->SetDescription("Storage repository introduced successfully");
}

bool SrIntroduceAction::isFirstSharedNonISOSR() const
{
    // Only applies to shared non-ISO SRs
    if (this->m_srContentType == "iso" || !this->m_srIsShared)
    {
        return false;
    }

    // Check cache for existing shared non-ISO SRs
    XenCache* cache = this->GetConnection()->GetCache();
    if (!cache)
    {
        return false;
    }

    QList<QVariantMap> allSRs = cache->GetAllData("sr");
    for (const QVariantMap& srData : allSRs)
    {
        QString srRef = srData.value("ref").toString();

        // Skip the SR we just introduced (result() contains the new SR ref)
        if (srRef == this->GetResult())
        {
            continue;
        }

        // Check if it's a shared non-ISO SR
        QString contentType = srData.value("content_type").toString();
        bool shared = srData.value("shared").toBool();

        if (contentType != "iso" && shared)
        {
            // Found another shared non-ISO SR, so this is not the first
            return false;
        }
    }

    // No other shared non-ISO SRs found - this is the first one
    return true;
}
