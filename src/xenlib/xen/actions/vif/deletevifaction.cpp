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

#include "deletevifaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_VIF.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../../xencache.h"
#include <QDebug>

DeleteVIFAction::DeleteVIFAction(XenConnection* connection,
                                 const QString& vifRef,
                                 QObject* parent)
    : AsyncOperation(connection,
                     "Deleting VIF",
                     "Deleting virtual network interface",
                     parent),
      m_vifRef(vifRef)
{
    if (m_vifRef.isEmpty())
        throw std::invalid_argument("VIF reference cannot be empty");

    // Get VIF details for display
    QVariantMap vifData = connection->GetCache()->ResolveObjectData("vif", m_vifRef);
    m_vmRef = vifData.value("VM").toString();
    QString networkRef = vifData.value("network").toString();

    QVariantMap vmData = connection->GetCache()->ResolveObjectData("vm", m_vmRef);
    m_vmName = vmData.value("name_label").toString();

    QVariantMap networkData = connection->GetCache()->ResolveObjectData("network", networkRef);
    m_networkName = networkData.value("name_label").toString();

    SetTitle(QString("Deleting VIF for %1").arg(m_vmName));
    SetDescription(QString("Deleting %1 from %2").arg(m_networkName, m_vmName));
}

void DeleteVIFAction::run()
{
    try
    {
        SetDescription("Deleting VIF...");

        // Check if VM is running and if we need to unplug first
        QVariantMap vmData = GetConnection()->GetCache()->ResolveObjectData("vm", m_vmRef);
        QString powerState = vmData.value("power_state").toString();

        if (powerState == "Running")
        {
            QStringList allowedOps = XenAPI::VIF::get_allowed_operations(GetSession(), m_vifRef);

            if (allowedOps.contains("unplug"))
            {
                SetDescription("Unplugging VIF...");

                try
                {
                    XenAPI::VIF::unplug(GetSession(), m_vifRef);
                    qDebug() << "VIF unplugged successfully";
                    SetPercentComplete(50);
                } catch (const std::exception& e)
                {
                    QString error = e.what();
                    // Ignore if already detached - this is expected in some cases
                    if (!error.contains("DEVICE_ALREADY_DETACHED"))
                    {
                        qWarning() << "Unplug failed:" << error;
                        throw;
                    } else
                    {
                        qDebug() << "VIF already detached, continuing...";
                        SetPercentComplete(50);
                    }
                }
            } else
            {
                qDebug() << "Unplug not allowed, destroying anyway";
                SetPercentComplete(50);
            }
        } else
        {
            SetPercentComplete(50);
        }

        SetDescription("Destroying VIF...");
        XenAPI::VIF::destroy(GetSession(), m_vifRef);

        SetPercentComplete(100);
        SetDescription("VIF deleted");
        qDebug() << "VIF destroyed successfully";

    } catch (const std::exception& e)
    {
        setError(QString("Failed to delete VIF: %1").arg(e.what()));
    }
}
