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
#include "../../network/connection.h"
#include "../../session.h"
#include "../../failure.h"
#include "../../../xencache.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../xenapi/xenapi_Host.h"
#include <stdexcept>

VMCrossPoolMigrateAction::VMCrossPoolMigrateAction(XenConnection* sourceConnection,
                                                   XenConnection* destinationConnection,
                                                   const QString& vmRef,
                                                   const QString& destinationHostRef,
                                                   const QString& transferNetworkRef,
                                                   const VmMapping& mapping,
                                                   bool copy,
                                                   QObject* parent)
    : AsyncOperation(sourceConnection,
                     QString("Cross-pool migrate VM"),
                     QString("Migrating VM across pools"),
                     parent),
      m_destinationConnection(destinationConnection),
      m_vmRef(vmRef),
      m_destinationHostRef(destinationHostRef),
      m_transferNetworkRef(transferNetworkRef),
      m_mapping(mapping),
      m_copy(copy)
{
    AddApiMethodToRoleCheck("Host.migrate_receive");
    AddApiMethodToRoleCheck("VM.migrate_send");
    AddApiMethodToRoleCheck("VM.async_migrate_send");
    AddApiMethodToRoleCheck("VM.assert_can_migrate");
}

QString VMCrossPoolMigrateAction::GetTitle(const QVariantMap& vmData, const QVariantMap& hostData, bool copy)
{
    QString vmName = vmData.value("name_label", "VM").toString();
    QString hostName = hostData.value("name_label", "Host").toString();

    if (copy)
        return QString("Copying %1 to %2").arg(vmName, hostName);

    return QString("Migrating %1 to %2").arg(vmName, hostName);
}

void VMCrossPoolMigrateAction::run()
{
    if (!GetConnection() || !GetConnection()->IsConnected())
    {
        setError("Not connected to server");
        return;
    }

    if (!m_destinationConnection || !m_destinationConnection->IsConnected())
    {
        setError("Destination connection is not connected");
        return;
    }

    try
    {
        SetPercentComplete(0);
        SetDescription("Preparing migration...");

        XenCache* sourceCache = GetConnection()->GetCache();
        XenCache* destCache = m_destinationConnection->GetCache();
        if (!sourceCache || !destCache)
            throw std::runtime_error("Cache not available");

        QVariantMap vmData = sourceCache->ResolveObjectData(XenObjectType::VM, m_vmRef);
        if (vmData.isEmpty())
            throw std::runtime_error("VM not found in cache");

        QVariantMap hostData = destCache->ResolveObjectData(XenObjectType::Host, m_destinationHostRef);
        if (hostData.isEmpty())
            throw std::runtime_error("Destination host not found in cache");

        SetTitle(GetTitle(vmData, hostData, m_copy));
        SetDescription(m_copy ? "Copying VM..." : "Migrating VM...");

        XenAPI::Session* destSession = XenAPI::Session::DuplicateSession(m_destinationConnection->GetSession(), nullptr);
        if (!destSession || !destSession->IsLoggedIn())
            throw std::runtime_error("Failed to create destination session");

        QVariantMap sendData = XenAPI::Host::migrate_receive(destSession,
                                                             m_destinationHostRef,
                                                             m_transferNetworkRef,
                                                             QVariantMap());
        SetPercentComplete(5);

        QVariantMap options;
        if (m_copy)
            options.insert("copy", "true");

        QVariantMap vdiMap;
        for (auto it = m_mapping.storage.cbegin(); it != m_mapping.storage.cend(); ++it)
            vdiMap.insert(it.key(), it.value());

        QVariantMap vifMap;
        for (auto it = m_mapping.vifs.cbegin(); it != m_mapping.vifs.cend(); ++it)
            vifMap.insert(it.key(), it.value());

        QString taskRef = XenAPI::VM::async_migrate_send(GetSession(),
                                                         m_vmRef,
                                                         sendData,
                                                         true,
                                                         vdiMap,
                                                         vifMap,
                                                         options);

        pollToCompletion(taskRef, 5, 100);

        SetDescription(m_copy ? "VM copied successfully" : "VM migrated successfully");
    }
    catch (const Failure& failure)
    {
        setError(failure.message());
    }
    catch (const std::exception& e)
    {
        setError(QString("Failed to migrate VM: %1").arg(QString::fromUtf8(e.what())));
    }
}
