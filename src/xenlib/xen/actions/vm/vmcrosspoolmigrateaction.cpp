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
#include "xen/pif.h"
#include "xen/pool.h"
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
    this->AddApiMethodToRoleCheck("Host.migrate_receive");
    this->AddApiMethodToRoleCheck("VM.migrate_send");
    this->AddApiMethodToRoleCheck("VM.async_migrate_send");
    this->AddApiMethodToRoleCheck("VM.assert_can_migrate");
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
    if (!this->GetConnection() || !this->GetConnection()->IsConnected())
    {
        this->setError("Not connected to server");
        return;
    }

    if (!this->m_destinationConnection || !this->m_destinationConnection->IsConnected())
    {
        this->setError("Destination connection is not connected");
        return;
    }

    try
    {
        this->SetPercentComplete(0);
        this->SetDescription("Preparing migration...");

        XenCache* sourceCache = this->GetConnection()->GetCache();
        XenCache* destCache = this->m_destinationConnection->GetCache();
        if (!sourceCache || !destCache)
            throw std::runtime_error("Cache not available");

        QVariantMap vmData = sourceCache->ResolveObjectData(XenObjectType::VM, this->m_vmRef);
        if (vmData.isEmpty())
            throw std::runtime_error("VM not found in cache");

        QVariantMap hostData = destCache->ResolveObjectData(XenObjectType::Host, this->m_destinationHostRef);
        if (hostData.isEmpty())
            throw std::runtime_error("Destination host not found in cache");

        this->SetTitle(GetTitle(vmData, hostData, this->m_copy));
        this->SetDescription(this->m_copy ? "Copying VM..." : "Migrating VM...");

        XenAPI::Session* destSession = XenAPI::Session::DuplicateSession(this->m_destinationConnection->GetSession(), nullptr);
        if (!destSession || !destSession->IsLoggedIn())
            throw std::runtime_error("Failed to create destination session");

        const QString transferNetworkRef = this->resolveTransferNetworkRef(destCache);
        if (transferNetworkRef.isEmpty())
            throw std::runtime_error("No transfer network available on destination host");

        QVariantMap sendData = XenAPI::Host::migrate_receive(destSession,
                                                             this->m_destinationHostRef,
                                                             transferNetworkRef,
                                                             QVariantMap());
        this->SetPercentComplete(5);

        QVariantMap options;
        if (this->m_copy)
            options.insert("copy", "true");

        QVariantMap vdiMap;
        for (auto it = this->m_mapping.storage.cbegin(); it != this->m_mapping.storage.cend(); ++it)
            vdiMap.insert(it.key(), it.value());

        QVariantMap vifMap;
        for (auto it = this->m_mapping.vifs.cbegin(); it != this->m_mapping.vifs.cend(); ++it)
            vifMap.insert(it.key(), it.value());

        QString taskRef = XenAPI::VM::async_migrate_send(this->GetSession(),
                                                         this->m_vmRef,
                                                         sendData,
                                                         true,
                                                         vdiMap,
                                                         vifMap,
                                                         options);

        this->pollToCompletion(taskRef, 5, 100);

        this->SetDescription(this->m_copy ? "VM copied successfully" : "VM migrated successfully");
    }
    catch (const Failure& failure)
    {
        this->setError(failure.message());
    }
    catch (const std::exception& e)
    {
        this->setError(QString("Failed to migrate VM: %1").arg(QString::fromUtf8(e.what())));
    }
}

QString VMCrossPoolMigrateAction::resolveTransferNetworkRef(XenCache* destCache) const
{
    if (!destCache)
        return QString();

    auto hasUsablePifOnHost = [this, destCache](const QString& networkRef) -> bool
    {
        if (networkRef.isEmpty())
            return false;

        QList<QSharedPointer<PIF>> pifs = destCache->GetAll<PIF>(XenObjectType::PIF);
        for (const QSharedPointer<PIF>& pif : pifs)
        {
            if (!pif || !pif->IsValid())
                continue;
            if (pif->GetHostRef() != this->m_destinationHostRef)
                continue;
            if (pif->GetNetworkRef() != networkRef)
                continue;
            if (pif->IP().isEmpty())
                continue;
            return true;
        }

        return false;
    };

    if (hasUsablePifOnHost(this->m_transferNetworkRef))
        return this->m_transferNetworkRef;

    QSharedPointer<Pool> pool = destCache->GetPool();
    if (pool && pool->IsValid())
    {
        const QString xoNetworkRef = pool->GetOtherConfig().value("xo:migrationNetwork").toString();
        if (hasUsablePifOnHost(xoNetworkRef))
            return xoNetworkRef;
    }

    QList<QSharedPointer<PIF>> pifs = destCache->GetAll<PIF>(XenObjectType::PIF);
    QString firstHostNetworkRef;
    for (const QSharedPointer<PIF>& pif : pifs)
    {
        if (!pif || !pif->IsValid())
            continue;
        if (pif->GetHostRef() != this->m_destinationHostRef)
            continue;
        if (pif->IP().isEmpty())
            continue;

        if (firstHostNetworkRef.isEmpty())
            firstHostNetworkRef = pif->GetNetworkRef();

        if (pif->Management())
            return pif->GetNetworkRef();
    }

    return firstHostNetworkRef;
}
