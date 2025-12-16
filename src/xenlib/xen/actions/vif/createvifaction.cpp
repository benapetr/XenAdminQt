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

#include "createvifaction.h"
#include "../../connection.h"
#include "../../session.h"
#include "../../xenapi/VIF.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../../xencache.h"
#include <QDebug>

CreateVIFAction::CreateVIFAction(XenConnection* connection,
                                 const QString& vmRef,
                                 const QVariantMap& vifRecord,
                                 QObject* parent)
    : AsyncOperation(connection,
                     "Creating VIF",
                     "Creating virtual network interface",
                     parent),
      m_vmRef(vmRef),
      m_vifRecord(vifRecord),
      m_rebootRequired(false)
{
    if (m_vmRef.isEmpty())
        throw std::invalid_argument("VM reference cannot be empty");

    // Get VM name for display
    QVariantMap vmData = connection->getCache()->resolve("vm", m_vmRef);
    m_vmName = vmData.value("name_label").toString();

    setTitle(QString("Creating VIF for %1").arg(m_vmName));
    setDescription(QString("Creating virtual network interface for %1").arg(m_vmName));
}

void CreateVIFAction::run()
{
    try
    {
        setDescription("Creating VIF...");

        // Create the VIF asynchronously
        QString taskRef = XenAPI::VIF::async_create(session(), m_vifRecord);

        QString newVifRef;
        if (taskRef.isEmpty())
        {
            // Some XenServer versions may create VIF synchronously (no task returned)
            // In this case, the VIF is already created but we don't have its reference
            qDebug() << "VIF created synchronously (no task reference)";
            // We'll need to find the new VIF by refreshing and looking for it
            // For now, complete successfully - the VIF list will refresh
            setPercentComplete(100);
            setDescription("VIF created");
            return;
        } else
        {
            pollToCompletion(taskRef, 0, 70);
            newVifRef = result();
            qDebug() << "Created VIF:" << newVifRef;
        }

        // If we still don't have a VIF reference, we can't proceed with hot-plug
        if (newVifRef.isEmpty())
        {
            qDebug() << "VIF created but reference not available";
            setPercentComplete(100);
            setDescription("VIF created");
            return;
        }

        // Check if VM is running and if we can hot-plug
        QVariantMap vmData = connection()->getCache()->resolve("vm", m_vmRef);
        QString powerState = vmData.value("power_state").toString();

        if (powerState == "Running")
        {
            setDescription("Checking if hot-plug is possible...");

            QStringList allowedOps = XenAPI::VIF::get_allowed_operations(session(), newVifRef);

            if (allowedOps.contains("plug"))
            {
                setDescription("Hot-plugging VIF...");

                // C# doesn't catch exceptions from VIF.plug - let it fail if it can't plug
                // This matches CreateVIFAction.cs Run() method behavior
                XenAPI::VIF::plug(session(), newVifRef);
                setPercentComplete(100);
                setDescription("VIF created and hot-plugged");
                qDebug() << "VIF hot-plugged successfully";
            } else
            {
                m_rebootRequired = true;
                setPercentComplete(100);
                setDescription("VIF created (reboot required for activation)");
                qDebug() << "Hot-plug not allowed, reboot required";
            }
        } else
        {
            setPercentComplete(100);
            setDescription("VIF created");
        }

    } catch (const std::exception& e)
    {
        setError(QString("Failed to create VIF: %1").arg(e.what()));
    }
}
