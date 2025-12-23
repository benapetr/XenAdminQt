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

#include "vmcrosspoolmigrateaction.h"
#include "../../../xen/network/connection.h"
#include "../../../xen/session.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../xenapi/xenapi_Host.h"
#include "../../../xencache.h"
#include <stdexcept>

VMCrossPoolMigrateAction::VMCrossPoolMigrateAction(XenConnection* connection,
                                                   XenConnection* destConnection,
                                                   const QString& vmRef,
                                                   const QString& destinationHostRef,
                                                   const QString& transferNetworkRef,
                                                   const QVariantMap& vdiMap,
                                                   const QVariantMap& vifMap,
                                                   bool copy,
                                                   QObject* parent)
    : AsyncOperation(connection,
                     copy ? QString("Copying VM across pools") : QString("Migrating VM across pools"),
                     copy ? QString("Cross-pool copy") : QString("Cross-pool migration"),
                     parent),
      m_destConnection(destConnection),
      m_vmRef(vmRef),
      m_destinationHostRef(destinationHostRef),
      m_transferNetworkRef(transferNetworkRef),
      m_vdiMap(vdiMap),
      m_vifMap(vifMap),
      m_copy(copy)
{
}

void VMCrossPoolMigrateAction::run()
{
    try
    {
        setPercentComplete(0);
        setDescription("Preparing cross-pool operation...");

        // Get VM data from source
        QVariantMap vmData = connection()->getCache()->ResolveObjectData("vm", m_vmRef);
        if (vmData.isEmpty())
        {
            throw std::runtime_error("VM not found in cache");
        }

        QString vmName = vmData.value("name_label").toString();

        // Get destination host data
        QVariantMap destHostData = m_destConnection->getCache()->ResolveObjectData("host", m_destinationHostRef);
        if (destHostData.isEmpty())
        {
            throw std::runtime_error("Destination host not found");
        }

        QString destHostName = destHostData.value("name_label").toString();

        // Get source pool/connection name
        QString sourcePoolName;
        QStringList poolRefs = connection()->getCache()->GetAllRefs("pool");
        if (!poolRefs.isEmpty())
        {
            QVariantMap poolData = connection()->getCache()->ResolveObjectData("pool", poolRefs.first());
            sourcePoolName = poolData.value("name_label").toString();
        }
        if (sourcePoolName.isEmpty())
        {
            sourcePoolName = connection()->getHostname();
        }

        if (m_copy)
        {
            setTitle(QString("Copying %1 from %2 to %3")
                         .arg(vmName)
                         .arg(sourcePoolName)
                         .arg(destHostName));
        } else
        {
            QString residentOnRef = vmData.value("resident_on").toString();
            if (!residentOnRef.isEmpty() && residentOnRef != "OpaqueRef:NULL")
            {
                QVariantMap residentHostData = connection()->getCache()->ResolveObjectData("host", residentOnRef);
                QString sourceHostName = residentHostData.value("name_label").toString();
                setTitle(QString("Migrating %1 from %2 to %3")
                             .arg(vmName)
                             .arg(sourceHostName)
                             .arg(destHostName));
            } else
            {
                setTitle(QString("Migrating %1 to %2").arg(vmName).arg(destHostName));
            }
        }

        setPercentComplete(5);
        setDescription("Setting up destination host...");

        // Step 1: Call Host.migrate_receive on destination to prepare
        QVariantMap receiveOptions;
        QVariantMap receiveData;

        try
        {
            receiveData = XenAPI::Host::migrate_receive(m_destConnection->getSession(),
                                                        m_destinationHostRef,
                                                        m_transferNetworkRef,
                                                        receiveOptions);
        } catch (const std::exception& e)
        {
            throw std::runtime_error(QString("Failed to prepare destination: %1").arg(e.what()).toStdString());
        }

        setPercentComplete(10);
        setDescription(m_copy ? "Copying VM..." : "Migrating VM...");

        // Step 2: Convert VIF MAC map to VIF ref map
        QVariantMap vifRefMap = convertVifMapToRefs(m_vifMap);

        // Step 3: Call VM.async_migrate_send on source
        QVariantMap sendOptions;
        if (m_copy)
        {
            sendOptions["copy"] = "true";
        }

        QString taskRef = XenAPI::VM::async_migrate_send(session(), m_vmRef,
                                                         receiveData, true,
                                                         m_vdiMap, vifRefMap,
                                                         sendOptions);

        // Poll the task to completion
        pollToCompletion(taskRef, 10, 100);

        setDescription(m_copy ? "VM copied successfully" : "VM migrated successfully");

    } catch (const std::exception& e)
    {
        setError(QString("Failed to %1 VM: %2")
                     .arg(m_copy ? "copy" : "migrate")
                     .arg(e.what()));
    }
}

QVariantMap VMCrossPoolMigrateAction::convertVifMapToRefs(const QVariantMap& macToNetworkMap)
{
    // Convert VIF MAC addresses to VIF opaque references
    QVariantMap vifRefMap;

    // Get VM's VIFs
    QVariantMap vmData = connection()->getCache()->ResolveObjectData("vm", m_vmRef);
    QVariantList vifRefs = vmData.value("VIFs").toList();

    // Build MAC -> VIF ref mapping
    QMap<QString, QString> macToVifRef;
    for (const QVariant& vifRefVar : vifRefs)
    {
        QString vifRef = vifRefVar.toString();
        QVariantMap vifData = connection()->getCache()->ResolveObjectData("vif", vifRef);
        QString mac = vifData.value("MAC").toString();
        if (!mac.isEmpty())
        {
            macToVifRef[mac] = vifRef;
        }
    }

    // Also check VM snapshots for additional VIFs
    QVariantList snapshotRefs = vmData.value("snapshots").toList();
    for (const QVariant& snapRefVar : snapshotRefs)
    {
        QString snapRef = snapRefVar.toString();
        QVariantMap snapData = connection()->getCache()->ResolveObjectData("vm", snapRef);
        QVariantList snapVifRefs = snapData.value("VIFs").toList();

        for (const QVariant& vifRefVar : snapVifRefs)
        {
            QString vifRef = vifRefVar.toString();
            QVariantMap vifData = connection()->getCache()->ResolveObjectData("vif", vifRef);
            QString mac = vifData.value("MAC").toString();
            if (!mac.isEmpty() && !macToVifRef.contains(mac))
            {
                macToVifRef[mac] = vifRef;
            }
        }
    }

    // Build VIF ref -> destination network ref map
    for (auto it = macToNetworkMap.constBegin(); it != macToNetworkMap.constEnd(); ++it)
    {
        QString mac = it.key();
        QString destNetworkRef = it.value().toString();

        if (macToVifRef.contains(mac))
        {
            QString vifRef = macToVifRef[mac];
            vifRefMap[vifRef] = destNetworkRef;
        }
    }

    return vifRefMap;
}
