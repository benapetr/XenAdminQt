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
    QVariantMap vmData = connection->getCache()->ResolveObjectData("vm", m_vmRef);
    m_vmName = vmData.value("name_label").toString();

    QVariantMap oldVifData = connection->getCache()->ResolveObjectData("vif", m_oldVifRef);
    QString networkRef = oldVifData.value("network").toString();
    QVariantMap networkData = connection->getCache()->ResolveObjectData("network", networkRef);
    m_networkName = networkData.value("name_label").toString();

    setTitle(QString("Updating VIF for %1").arg(m_vmName));
    setDescription(QString("Updating %1 on %2").arg(m_networkName, m_vmName));
}

void UpdateVIFAction::run()
{
    try
    {
        setDescription("Updating VIF...");

        // Step 1: Delete the old VIF (includes unplugging if needed)
        setDescription("Removing old VIF configuration...");

        QVariantMap vmData = connection()->getCache()->ResolveObjectData("vm", m_vmRef);
        QString powerState = vmData.value("power_state").toString();

        if (powerState == "Running")
        {
            QStringList allowedOps = XenAPI::VIF::get_allowed_operations(session(), m_oldVifRef);

            if (allowedOps.contains("unplug"))
            {
                setDescription("Unplugging old VIF...");

                try
                {
                    XenAPI::VIF::unplug(session(), m_oldVifRef);
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

        setPercentComplete(30);

        // Destroy the old VIF
        setDescription("Destroying old VIF...");
        XenAPI::VIF::destroy(session(), m_oldVifRef);
        qDebug() << "Old VIF destroyed successfully";

        setPercentComplete(50);

        // Step 2: Create the new VIF
        setDescription("Creating new VIF...");
        QString taskRef = XenAPI::VIF::async_create(session(), m_newVifRecord);
        pollToCompletion(taskRef, 50, 80);
        QString newVifRef = result();

        qDebug() << "New VIF created:" << newVifRef;

        // Step 3: Attempt to hot-plug if VM is running
        // Refresh VM power state in case it changed
        vmData = connection()->getCache()->ResolveObjectData("vm", m_vmRef);
        powerState = vmData.value("power_state").toString();

        if (powerState == "Running")
        {
            setDescription("Checking if hot-plug is possible...");

            QStringList allowedOps = XenAPI::VIF::get_allowed_operations(session(), newVifRef);

            if (allowedOps.contains("plug"))
            {
                setDescription("Hot-plugging new VIF...");

                try
                {
                    XenAPI::VIF::plug(session(), newVifRef);
                    setPercentComplete(100);
                    setDescription("VIF updated and hot-plugged");
                    qDebug() << "New VIF hot-plugged successfully";
                } catch (const std::exception& e)
                {
                    qWarning() << "Hot-plug failed:" << e.what();
                    m_rebootRequired = true;
                    setPercentComplete(100);
                    setDescription("VIF updated (reboot required for activation)");
                }
            } else
            {
                m_rebootRequired = true;
                setPercentComplete(100);
                setDescription("VIF updated (reboot required for activation)");
                qDebug() << "Hot-plug not allowed, reboot required";
            }
        } else
        {
            setPercentComplete(100);
            setDescription("VIF updated");
        }

    } catch (const std::exception& e)
    {
        setError(QString("Failed to update VIF: %1").arg(e.what()));
    }
}
