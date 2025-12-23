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

#include "plugvifaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_VIF.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../../xencache.h"
#include <QDebug>

PlugVIFAction::PlugVIFAction(XenConnection* connection,
                             const QString& vifRef,
                             QObject* parent)
    : AsyncOperation(connection,
                     "Plugging VIF",
                     "Plugging virtual network interface",
                     parent),
      m_vifRef(vifRef)
{
    if (m_vifRef.isEmpty())
        throw std::invalid_argument("VIF reference cannot be empty");

    // Get VIF details for display
    QVariantMap vifData = connection->getCache()->ResolveObjectData("vif", m_vifRef);
    m_vmRef = vifData.value("VM").toString();

    QVariantMap vmData = connection->getCache()->ResolveObjectData("vm", m_vmRef);
    m_vmName = vmData.value("name_label").toString();

    setTitle(QString("Plugging VIF on %1").arg(m_vmName));
    setDescription(QString("Plugging virtual network interface on %1").arg(m_vmName));
}

void PlugVIFAction::run()
{
    try
    {
        // Check if VM is running
        QVariantMap vmData = connection()->getCache()->ResolveObjectData("vm", m_vmRef);
        QString powerState = vmData.value("power_state").toString();

        if (powerState != "Running")
        {
            setDescription("VIF will be plugged when VM starts");
            setPercentComplete(100);
            return;
        }

        setDescription("Plugging VIF...");

        // Check if plug operation is allowed
        QStringList allowedOps = XenAPI::VIF::get_allowed_operations(session(), m_vifRef);

        if (allowedOps.contains("plug"))
        {
            XenAPI::VIF::plug(session(), m_vifRef);
            setPercentComplete(100);
            setDescription("VIF plugged");
            qDebug() << "VIF plugged successfully";
        } else
        {
            setDescription("Plug operation not allowed");
            setPercentComplete(100);
            qWarning() << "VIF plug operation not in allowed operations";
        }

    } catch (const std::exception& e)
    {
        setError(QString("Failed to plug VIF: %1").arg(e.what()));
    }
}
