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

#include "srcreateaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../../xencache.h"
#include "../../host.h"
#include "../../sr.h"
#include "../../pool.h"
#include "../../xenapi/xenapi_SR.h"
#include "../../xenapi/xenapi_PBD.h"
#include "../../xenapi/xenapi_Pool.h"
#include "../../xenapi/xenapi_Secret.h"
#include <QDebug>

SrCreateAction::SrCreateAction(XenConnection* connection,
                               Host* host,
                               const QString& srName,
                               const QString& srDescription,
                               const QString& srType,
                               const QString& srContentType,
                               const QVariantMap& deviceConfig,
                               const QVariantMap& smConfig,
                               QObject* parent)
    : AsyncOperation(connection,
                     QString("Creating %1 SR '%2'").arg(srType).arg(srName),
                     QString("Creating %1 storage repository...").arg(srType),
                     parent),
      m_host(host), m_srName(srName), m_srDescription(srDescription), m_srType(srType), m_srContentType(srContentType), m_srIsShared(true) // Always true now (was conditional on pool license)
      ,
      m_deviceConfig(deviceConfig), m_smConfig(smConfig)
{
    // Set applies-to context
    if (host)
        setAppliesToFromObject(host);
}

void SrCreateAction::run()
{
    if (!m_host)
    {
        setError("No host specified for SR creation");
        return;
    }

    qDebug() << "SrCreateAction: Creating SR" << m_srName
             << "type:" << m_srType
             << "contentType:" << m_srContentType
             << "shared:" << m_srIsShared;

    XenSession* session = this->session();
    if (!session || !session->isLoggedIn())
    {
        setError("Not connected to XenServer");
        return;
    }

    // Handle password secrets (CIFS, iSCSI, etc.)
    QString secretUuid;
    if (m_deviceConfig.contains("cifspassword"))
    {
        secretUuid = createSecret("cifspassword", m_deviceConfig.value("cifspassword").toString());
    } else if (m_deviceConfig.contains("password"))
    {
        secretUuid = createSecret("password", m_deviceConfig.value("password").toString());
    } else if (m_deviceConfig.contains("chappassword"))
    {
        secretUuid = createSecret("chappassword", m_deviceConfig.value("chappassword").toString());
    }

    setDescription("Creating storage repository...");

    QString srRef;
    try
    {
        // Create the SR
        srRef = XenAPI::SR::create(session,
                                   m_host->opaqueRef(),
                                   m_deviceConfig,
                                   0, // physical_size (let server determine)
                                   m_srName,
                                   m_srDescription,
                                   m_srType,
                                   m_srContentType,
                                   m_srIsShared,
                                   m_smConfig.isEmpty() ? QVariantMap() : m_smConfig);

        if (srRef.isEmpty())
        {
            throw std::runtime_error("SR.create returned empty reference");
        }

        setResult(srRef);
    } catch (const std::exception& e)
    {
        // Cleanup secret on failure
        if (!secretUuid.isEmpty())
        {
            try
            {
                QString secretRef = XenAPI::Secret::get_by_uuid(session, secretUuid);
                if (!secretRef.isEmpty())
                {
                    XenAPI::Secret::destroy(session, secretRef);
                }
            } catch (...)
            {
                // Ignore secret cleanup errors
            }
        }
        throw;
    }

    // Destroy secret after SR creation (PBDs have duplicated it)
    // This is safe and prevents secret accumulation (CA-113396)
    if (!secretUuid.isEmpty())
    {
        try
        {
            QString secretRef = XenAPI::Secret::get_by_uuid(session, secretUuid);
            if (!secretRef.isEmpty())
            {
                XenAPI::Secret::destroy(session, secretRef);
            }
        } catch (const std::exception& e)
        {
            qDebug() << "SrCreateAction: Failed to destroy secret (non-fatal):" << e.what();
            // Ignore - secret cleanup is best-effort
        }
    }

    // Verify all PBDs are attached
    qDebug() << "SrCreateAction: Verifying PBD attachment";
    setDescription("Verifying storage connections...");

    try
    {
        QVariantList pbdRefs = XenAPI::SR::get_PBDs(session, srRef);

        for (const QVariant& pbdVar : pbdRefs)
        {
            QString pbdRef = pbdVar.toString();
            bool currentlyAttached = XenAPI::PBD::get_currently_attached(session, pbdRef);

            if (!currentlyAttached)
            {
                // Auto-plug failed, try manual plug
                qDebug() << "SrCreateAction: PBD" << pbdRef << "not attached, attempting manual plug";
                setDescription("Plugging storage on host...");

                try
                {
                    XenAPI::PBD::plug(session, pbdRef);
                } catch (const std::exception& plugErr)
                {
                    QString errorMsg(plugErr.what());

                    // Ignore host offline/booting errors
                    if (errorMsg.contains("HOST_OFFLINE") || errorMsg.contains("HOST_STILL_BOOTING"))
                    {
                        qWarning() << "SrCreateAction: Unable to verify PBD plug (host down)";
                        continue;
                    }

                    // Real plug failure - rollback
                    qDebug() << "SrCreateAction: PBD plug failed, performing SR.forget rollback";
                    forgetFailedSR(srRef);
                    throw;
                }
            }
        }
    } catch (const std::exception& e)
    {
        setError(QString("Failed to verify SR attachment: %1").arg(e.what()));
        return;
    }

    // Set auto-scan other_config
    try
    {
        QVariantMap otherConfig;
        otherConfig["auto-scan"] = (m_srContentType == "iso") ? "true" : "false";
        XenAPI::SR::set_other_config(session, srRef, otherConfig);
    } catch (const std::exception& e)
    {
        qWarning() << "SrCreateAction: Failed to set auto-scan config:" << e.what();
        // Non-fatal
    }

    // Set as default SR if this is the first shared non-ISO SR
    if (isFirstSharedNonISOSR())
    {
        qDebug() << "SrCreateAction: This is first shared non-ISO SR, setting as default";

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
                qDebug() << "SrCreateAction: Set SR as default for pool";
            }
        } catch (const std::exception& e)
        {
            qWarning() << "SrCreateAction: Failed to set default SR (non-fatal):" << e.what();
            // Non-fatal - SR is created successfully even if we can't set it as default
        }
    }

    setDescription("Storage repository created successfully");
    setPercentComplete(100);
}

QString SrCreateAction::createSecret(const QString& key, const QString& value)
{
    // Remove password from device config
    m_deviceConfig.remove(key);

    // Create secret and add <key>_secret entry
    XenSession* session = this->session();
    QString uuid = XenAPI::Secret::create(session, value);

    m_deviceConfig[key + "_secret"] = uuid;

    qDebug() << "SrCreateAction: Created secret for" << key;
    return uuid;
}

void SrCreateAction::forgetFailedSR(const QString& srRef)
{
    qDebug() << "SrCreateAction: Forgetting failed SR" << srRef;

    try
    {
        XenSession* session = this->session();

        // Unplug all PBDs
        QVariantList pbdRefs = XenAPI::SR::get_PBDs(session, srRef);
        for (const QVariant& pbdVar : pbdRefs)
        {
            QString pbdRef = pbdVar.toString();
            try
            {
                bool attached = XenAPI::PBD::get_currently_attached(session, pbdRef);
                if (attached)
                {
                    XenAPI::PBD::unplug(session, pbdRef);
                }
            } catch (...)
            {
                // Ignore unplug errors
            }
        }

        // Forget the SR
        XenAPI::SR::forget(session, srRef);
    } catch (const std::exception& e)
    {
        qDebug() << "SrCreateAction: SR.forget failed (continuing):" << e.what();
        // Never throw from rollback
    }
}

bool SrCreateAction::isFirstSharedNonISOSR() const
{
    // Only applies to shared non-ISO SRs
    if (m_srContentType == "iso" || !m_srIsShared)
    {
        return false;
    }

    // Check cache for existing shared non-ISO SRs
    XenCache* cache = connection()->getCache();
    if (!cache)
    {
        return false;
    }

    QList<QVariantMap> allSRs = cache->GetAllData("sr");
    for (const QVariantMap& srData : allSRs)
    {
        QString srRef = srData.value("ref").toString();

        // Skip the SR we just created (result() contains the new SR ref)
        if (srRef == result())
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
