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

#include "editmultipathaction.h"
#include "../../host.h"
#include "../../session.h"
#include "../../network/connection.h"
#include "../../xenapi/xenapi_Host.h"
#include "../../xenapi/xenapi_PBD.h"
#include <QDebug>

const char* EditMultipathAction::DEFAULT_MULTIPATH_HANDLE = "dmp";

EditMultipathAction::EditMultipathAction(Host* host,
                                         bool enableMultipath,
                                         QObject* parent)
    : AsyncOperation(host->GetConnection(),
                     QString("Changing multipath setting on %1").arg(host->GetName()),
                     QString("Changing multipath..."),
                     parent),
      m_host(host), m_enableMultipath(enableMultipath)
{
    // Set applies-to context
    if (host)
        setAppliesToFromObject(host);
}

void EditMultipathAction::run()
{
    if (!m_host)
    {
        setError("No host specified");
        return;
    }

    XenSession* sess = this->session();
    if (!sess || !sess->IsLoggedIn())
    {
        setError("Not connected to XenServer");
        return;
    }

    qDebug() << "EditMultipathAction: Changing multipath setting on host" << m_host->GetName()
             << "to" << m_enableMultipath;

    QString hostRef = m_host->OpaqueRef();
    QStringList pluggedPBDs;

    try
    {
        // Get all PBDs for this host
        QVariantMap hostRecord = m_host->GetData();
        QVariantList pbdRefs = hostRecord.value("PBDs").toList();

        // Step 1: Unplug all currently attached PBDs
        setDescription("Unplugging storage connections...");
        foreach (const QVariant& pbdRefVar, pbdRefs)
        {
            QString pbdRef = pbdRefVar.toString();
            
            // Check if currently attached
            bool attached = XenAPI::PBD::get_currently_attached(sess, pbdRef);
            if (attached)
            {
                qDebug() << "EditMultipathAction: Unplugging PBD" << pbdRef;
                XenAPI::PBD::unplug(sess, pbdRef);
                pluggedPBDs.append(pbdRef);
            }
        }

        // Step 2: Set multipath configuration
        setDescription("Configuring multipath setting...");
        
        // Use other_config method (works on all XenServer versions)
        // TODO: For XenServer 6.0+ (Kolkata), use Host.set_multipathing() instead
        XenAPI::Host::remove_from_other_config(sess, hostRef, "multipathing");
        XenAPI::Host::add_to_other_config(sess, hostRef, "multipathing", 
                                          m_enableMultipath ? "true" : "false");

        // Set multipath handle if enabling
        XenAPI::Host::remove_from_other_config(sess, hostRef, "multipath-handle");
        if (m_enableMultipath)
        {
            XenAPI::Host::add_to_other_config(sess, hostRef, "multipath-handle", 
                                             DEFAULT_MULTIPATH_HANDLE);
        }

        qDebug() << "EditMultipathAction: Multipath setting changed successfully";

    }
    catch (const std::exception& e)
    {
        qWarning() << "EditMultipathAction: Error changing multipath setting:" << e.what();
        
        // Try to re-plug PBDs even if setting multipath failed
        setDescription("Re-plugging storage connections...");
        foreach (const QString& pbdRef, pluggedPBDs)
        {
            try
            {
                qDebug() << "EditMultipathAction: Re-plugging PBD" << pbdRef;
                XenAPI::PBD::plug(sess, pbdRef);
            }
            catch (const std::exception& plugError)
            {
                qWarning() << "EditMultipathAction: Failed to re-plug PBD" << pbdRef 
                          << ":" << plugError.what();
            }
        }

        setError(QString("Failed to change multipath setting: %1").arg(e.what()));
        return;
    }

    // Step 3: Re-plug all PBDs that were previously plugged
    setDescription("Re-plugging storage connections...");
    QString plugErrors;
    foreach (const QString& pbdRef, pluggedPBDs)
    {
        try
        {
            qDebug() << "EditMultipathAction: Re-plugging PBD" << pbdRef;
            XenAPI::PBD::plug(sess, pbdRef);
        }
        catch (const std::exception& e)
        {
            QString error = QString("Failed to re-plug PBD %1: %2").arg(pbdRef).arg(e.what());
            qWarning() << "EditMultipathAction:" << error;
            if (!plugErrors.isEmpty())
                plugErrors += "\n";
            plugErrors += error;
        }
    }

    if (!plugErrors.isEmpty())
    {
        setError(QString("Multipath setting changed but some storage connections could not be re-plugged:\n%1")
                     .arg(plugErrors));
        return;
    }

    qDebug() << "EditMultipathAction: All PBDs re-plugged successfully";
    setDescription("Multipath setting changed successfully");
}
