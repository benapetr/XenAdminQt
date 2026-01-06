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

#include "updatevifaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_VIF.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../../xencache.h"
#include <QDebug>

UpdateVIFAction::UpdateVIFAction(XenConnection* connection,
                                 const QString& vmRef,
                                 const QString& oldVifRef,
                                 const QVariantMap& newVifRecord,
                                 QObject* parent)
    : AsyncOperation(connection,
                     "Updating VIF",
                     "Updating virtual network interface",
                     parent),
      m_vmRef(vmRef),
      m_oldVifRef(oldVifRef),
      m_newVifRecord(newVifRecord),
      m_rebootRequired(false)
{
    if (m_vmRef.isEmpty())
        throw std::invalid_argument("VM reference cannot be empty");
    if (m_oldVifRef.isEmpty())
        throw std::invalid_argument("Old VIF reference cannot be empty");

    // Get VM and network names for display
    QVariantMap vmData = connection->GetCache()->ResolveObjectData("vm", m_vmRef);
    m_vmName = vmData.value("name_label").toString();

    QVariantMap oldVifData = connection->GetCache()->ResolveObjectData("vif", m_oldVifRef);
    QString networkRef = oldVifData.value("network").toString();
    QVariantMap networkData = connection->GetCache()->ResolveObjectData("network", networkRef);
    m_networkName = networkData.value("name_label").toString();

    SetTitle(QString("Updating VIF for %1").arg(m_vmName));
    SetDescription(QString("Updating %1 on %2").arg(m_networkName, m_vmName));
}

void UpdateVIFAction::run()
{
    try
    {
        SetDescription("Updating VIF...");

        // Step 1: Delete the old VIF (includes unplugging if needed)
        SetDescription("Removing old VIF configuration...");

        QVariantMap vmData = GetConnection()->GetCache()->ResolveObjectData("vm", m_vmRef);
        QString powerState = vmData.value("power_state").toString();

        if (powerState == "Running")
        {
            QStringList allowedOps = XenAPI::VIF::get_allowed_operations(GetSession(), m_oldVifRef);

            if (allowedOps.contains("unplug"))
            {
                SetDescription("Unplugging old VIF...");

                try
                {
                    XenAPI::VIF::unplug(GetSession(), m_oldVifRef);
                    qDebug() << "Old VIF unplugged successfully";
                } catch (const std::exception& e)
                {
                    QString error = e.what();
                    // Ignore if already detached
                    if (!error.contains("DEVICE_ALREADY_DETACHED"))
                    {
                        qWarning() << "Unplug failed:" << error;
                        throw;
                    } else
                    {
                        qDebug() << "VIF already detached, continuing...";
                    }
                }
            }
        }

        SetPercentComplete(30);

        // Destroy the old VIF
        SetDescription("Destroying old VIF...");
        XenAPI::VIF::destroy(GetSession(), m_oldVifRef);
        qDebug() << "Old VIF destroyed successfully";

        SetPercentComplete(50);

        // Step 2: Create the new VIF
        SetDescription("Creating new VIF...");
        QString taskRef = XenAPI::VIF::async_create(GetSession(), m_newVifRecord);
        pollToCompletion(taskRef, 50, 80);
        QString newVifRef = GetResult();

        qDebug() << "New VIF created:" << newVifRef;

        // Step 3: Attempt to hot-plug if VM is running
        // Refresh VM power state in case it changed
        vmData = GetConnection()->GetCache()->ResolveObjectData("vm", m_vmRef);
        powerState = vmData.value("power_state").toString();

        if (powerState == "Running")
        {
            SetDescription("Checking if hot-plug is possible...");

            QStringList allowedOps = XenAPI::VIF::get_allowed_operations(GetSession(), newVifRef);

            if (allowedOps.contains("plug"))
            {
                SetDescription("Hot-plugging new VIF...");

                try
                {
                    XenAPI::VIF::plug(GetSession(), newVifRef);
                    SetPercentComplete(100);
                    SetDescription("VIF updated and hot-plugged");
                    qDebug() << "New VIF hot-plugged successfully";
                } catch (const std::exception& e)
                {
                    qWarning() << "Hot-plug failed:" << e.what();
                    m_rebootRequired = true;
                    SetPercentComplete(100);
                    SetDescription("VIF updated (reboot required for activation)");
                }
            } else
            {
                m_rebootRequired = true;
                SetPercentComplete(100);
                SetDescription("VIF updated (reboot required for activation)");
                qDebug() << "Hot-plug not allowed, reboot required";
            }
        } else
        {
            SetPercentComplete(100);
            SetDescription("VIF updated");
        }

    } catch (const std::exception& e)
    {
        setError(QString("Failed to update VIF: %1").arg(e.what()));
    }
}
