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

#include "sr.h"
#include "network/connection.h"
#include "host.h"
#include "../xencache.h"
#include "pbd.h"
#include "sm.h"
#include "vdi.h"
#include "blob.h"
#include "../utils/misc.h"

namespace
{
    bool IsControlDomainZero(XenCache* cache, const QVariantMap& vmData, const QString& vmRef)
    {
        if (!cache || vmData.isEmpty())
            return false;

        if (!vmData.value("is_control_domain").toBool())
            return false;

        QString hostRef = vmData.value("resident_on").toString();
        if (hostRef.isEmpty() || hostRef == XENOBJECT_NULL)
            return false;

        QVariantMap hostData = cache->ResolveObjectData(XenObjectType::Host, hostRef);
        if (hostData.isEmpty())
            return false;

        QString hostControlDomain = hostData.value("control_domain").toString();
        if (!hostControlDomain.isEmpty() && hostControlDomain != XENOBJECT_NULL)
            return hostControlDomain == vmRef;

        qint64 domid = vmData.value("domid").toLongLong();
        return domid == 0;
    }
}

SR::SR(XenConnection* connection, const QString& opaqueRef, QObject* parent) : XenObject(connection, opaqueRef, parent)
{
}


QString SR::GetType() const
{
    return this->stringProperty("type");
}

bool SR::IsShared() const
{
    return this->boolProperty("shared", false);
}

bool SR::IsLocked() const
{
    return this->boolProperty("locked", false);
}

qint64 SR::PhysicalSize() const
{
    return this->longProperty("physical_size", 0);
}

qint64 SR::PhysicalUtilisation() const
{
    return this->longProperty("physical_utilisation", 0);
}

qint64 SR::VirtualAllocation() const
{
    return this->longProperty("virtual_allocation", 0);
}

qint64 SR::FreeSpace() const
{
    return this->PhysicalSize() - this->PhysicalUtilisation();
}

QString SR::SizeString() const
{
    return QObject::tr("%1 used of %2 (%3 allocated)")
        .arg(Misc::FormatSize(PhysicalUtilisation()))
        .arg(Misc::FormatSize(PhysicalSize()))
        .arg(Misc::FormatSize(VirtualAllocation()));
}

QSharedPointer<Host> SR::GetHost() const
{
    XenCache* cache = this->GetCache();

    // For shared SRs, return pool coordinator
    if (this->IsShared())
    {
        QString poolRef = cache->GetPoolRef();
        if (!poolRef.isEmpty())
        {
            QVariantMap poolData = cache->ResolveObjectData(XenObjectType::Pool, poolRef);
            QString masterRef = poolData.value("master").toString();
            if (!masterRef.isEmpty() && masterRef != XENOBJECT_NULL)
                return cache->ResolveObject<Host>(XenObjectType::Host, masterRef);
        }
        return QSharedPointer<Host>();
    }

    // For local SRs, find the host it's connected to via PBD
    QStringList pbdRefs = this->GetPBDRefs();
    for (const QString& pbdRef : pbdRefs)
    {
        QVariantMap pbdData = cache->ResolveObjectData(XenObjectType::PBD, pbdRef);
        if (!pbdData.isEmpty())
        {
            QString hostRef = pbdData.value("host").toString();
            if (!hostRef.isEmpty() && hostRef != XENOBJECT_NULL)
                return cache->ResolveObject<Host>(XenObjectType::Host, hostRef);
        }
    }

    return QSharedPointer<Host>();
}

QString SR::NameWithLocation() const
{
    // Return only the name for local SRs (matches C#)
    if (this->GetConnection() && !this->IsShared())
        return GetName();

    return XenObject::NameWithLocation();
}

QString SR::LocationString() const
{
    QSharedPointer<Host> home = GetHost();
    if (home)
        return home->LocationString();

    return XenObject::LocationString();
}

QStringList SR::GetVDIRefs() const
{
    return stringListProperty("VDIs");
}

QStringList SR::GetPBDRefs() const
{
    return stringListProperty("PBDs");
}

QString SR::ContentType() const
{
    return stringProperty("content_type", "user");
}

QSharedPointer<SM> SR::GetSM() const
{
    XenCache* cache = this->GetCache();
    if (!cache)
        return QSharedPointer<SM>();

    const QList<QSharedPointer<SM>> sms = cache->GetAll<SM>(XenObjectType::SM);
    const QString srType = this->GetType().toLower();
    for (const QSharedPointer<SM>& sm : sms)
    {
        if (!sm || !sm->IsValid())
            continue;
        if (sm->Type().toLower() == srType)
            return sm;
    }
    return QSharedPointer<SM>();
}

QString SR::NameWithoutHost() const
{
    return this->GetName();
}

QVariantMap SR::SMConfig() const
{
    return property("sm_config").toMap();
}

QString SR::GetSCSIID() const
{
    const QList<QSharedPointer<PBD>> pbds = GetPBDs();
    for (const QSharedPointer<PBD>& pbd : pbds)
    {
        if (!pbd || !pbd->IsValid())
            continue;

        const QString scsiId = pbd->GetDeviceConfigValue("SCSIid");
        if (!scsiId.isEmpty())
            return scsiId;
    }

    QString scsiId = SMConfig().value("devserial").toString();
    if (scsiId.isEmpty())
        return QString();

    if (scsiId.startsWith("scsi-"))
        scsiId.remove(0, 5);

    while (scsiId.endsWith(','))
        scsiId.chop(1);

    return scsiId;
}

QStringList SR::AllowedOperations() const
{
    return stringListProperty("allowed_operations");
}

QStringList SR::GetCapabilities() const
{
    return stringListProperty("capabilities");
}

QVariantMap SR::CurrentOperations() const
{
    return property("current_operations").toMap();
}

bool SR::SupportsTrim() const
{
    XenCache* cache = this->GetCache();
    if (!cache)
        return false;

    const QString srType = this->GetType();
    if (srType.isEmpty())
        return false;

    const QList<QSharedPointer<SM>> sms = cache->GetAll<SM>(XenObjectType::SM);
    for (const QSharedPointer<SM>& sm : sms)
    {
        if (!sm || !sm->IsValid())
            continue;
        if (sm->Type() != srType)
            continue;

        return sm->Features().contains("SR_TRIM");
    }

    return false;
}

QVariantMap SR::Blobs() const
{
    return this->property("blobs").toMap();
}

bool SR::LocalCacheEnabled() const
{
    return this->boolProperty("local_cache_enabled", false);
}

QString SR::IntroducedBy() const
{
    return this->stringProperty("introduced_by");
}

bool SR::Clustered() const
{
    return this->boolProperty("clustered", false);
}

bool SR::IsToolsSR() const
{
    // C# equivalent: SR.IsToolsSR() - checks both is_tools_sr flag and name_label
    if (this->boolProperty("is_tools_sr", false))
        return true;
    
    QString nameLabel = this->GetName();
    if (nameLabel == "XenServer Tools")
        return true;
    
    return false;
}

bool SR::SupportsStorageMigration() const
{
    QString type = this->GetType();
    if (this->ContentType() == "iso")
        return false;

    if (type == "tmpfs")
        return false;

    return true;
}

bool SR::HBALunPerVDI() const
{
    return this->GetType() == "rawhba";
}

bool SR::LunPerVDI() const
{
    const QVariantMap smConfig = this->SMConfig();
    for (auto it = smConfig.constBegin(); it != smConfig.constEnd(); ++it)
    {
        const QString key = it.key();
        if (key.contains("LUNperVDI") || key.startsWith("scsi-"))
            return true;
    }
    return false;
}

QHash<QString, QString> SR::GetMultiPathStatusLunPerSR() const
{
    QHash<QString, QString> result;

    const QList<QSharedPointer<PBD>> pbds = this->GetPBDs();
    for (const QSharedPointer<PBD>& pbd : pbds)
    {
        if (!pbd || !pbd->IsValid() || !pbd->MultipathActive())
            continue;

        QString status;
        const QVariantMap otherConfig = pbd->GetOtherConfig();
        for (auto it = otherConfig.constBegin(); it != otherConfig.constEnd(); ++it)
        {
            if (it.key().startsWith("mpath"))
            {
                status = it.value().toString();
                break;
            }
        }

        int currentPaths = 0;
        int maxPaths = 0;
        if (!PBD::ParsePathCounts(status, currentPaths, maxPaths))
            continue;

        result.insert(pbd->OpaqueRef(), status);
    }

    return result;
}

QHash<QString, QHash<QString, QString>> SR::GetMultiPathStatusLunPerVDI() const
{
    QHash<QString, QHash<QString, QString>> result;

    XenCache* cache = this->GetCache();
    if (!cache)
        return result;

    const QVariantMap smConfig = this->SMConfig();
    const QList<QSharedPointer<PBD>> pbds = this->GetPBDs();
    const QStringList vdiRefs = this->GetVDIRefs();

    for (const QSharedPointer<PBD>& pbd : pbds)
    {
        if (!pbd || !pbd->IsValid() || !pbd->MultipathActive())
            continue;

        const QVariantMap otherConfig = pbd->GetOtherConfig();
        for (auto it = otherConfig.constBegin(); it != otherConfig.constEnd(); ++it)
        {
            if (!it.key().startsWith("mpath"))
                continue;

            int currentPaths = 0;
            int maxPaths = 0;
            if (!PBD::ParsePathCounts(it.value().toString(), currentPaths, maxPaths))
                continue;

            const QString scsiIdKey = QString("scsi-%1").arg(it.key().mid(QString("mpath").length() + 1));
            if (!smConfig.contains(scsiIdKey))
                continue;

            const QString vdiUuid = smConfig.value(scsiIdKey).toString();
            if (vdiUuid.isEmpty())
                continue;

            QString vdiRef;
            for (const QString& candidateVdiRef : vdiRefs)
            {
                const QVariantMap vdiData = cache->ResolveObjectData(XenObjectType::VDI, candidateVdiRef);
                if (vdiData.isEmpty())
                    continue;
                if (vdiData.value("uuid").toString() == vdiUuid)
                {
                    vdiRef = candidateVdiRef;
                    break;
                }
            }

            if (vdiRef.isEmpty())
                continue;

            const QVariantMap vdiData = cache->ResolveObjectData(XenObjectType::VDI, vdiRef);
            const QVariantList vbdRefs = vdiData.value("VBDs").toList();
            for (const QVariant& vbdRefVar : vbdRefs)
            {
                const QString vbdRef = vbdRefVar.toString();
                const QVariantMap vbdData = cache->ResolveObjectData(XenObjectType::VBD, vbdRef);
                const QString vmRef = vbdData.value("VM").toString();
                if (vmRef.isEmpty() || vmRef == XENOBJECT_NULL)
                    continue;

                const QVariantMap vmData = cache->ResolveObjectData(XenObjectType::VM, vmRef);
                if (vmData.isEmpty())
                    continue;
                if (vmData.value("power_state").toString() != "Running")
                    continue;

                result[vmRef].insert(vdiRef, it.value().toString());
            }
        }
    }

    return result;
}

QString SR::HomeRef() const
{
    if (this->IsShared())
        return QString();

    QStringList pbds = this->GetPBDRefs();
    if (pbds.size() != 1)
        return QString();

    XenCache* cache = GetConnection()->GetCache();
    if (!cache)
        return QString();

    QVariantMap pbdData = cache->ResolveObjectData(XenObjectType::PBD, pbds.first());
    return pbdData.value("host").toString();
}

Host* SR::GetFirstAttachedStorageHost() const
{
    QStringList pbds = this->GetPBDRefs();
    if (pbds.isEmpty())
        return nullptr;

    // Iterate through PBDs to find first currently_attached one
    XenCache* cache = this->GetCache();

    for (const QString& pbdRef : pbds)
    {
        QVariantMap pbdData = cache->ResolveObjectData(XenObjectType::PBD, pbdRef);
        if (pbdData.isEmpty())
            continue;

        bool currentlyAttached = pbdData.value("currently_attached", false).toBool();
        if (currentlyAttached)
        {
            QString hostRef = pbdData.value("host").toString();
            if (!hostRef.isEmpty())
            {
                return new Host(GetConnection(), hostRef, nullptr);
            }
        }
    }

    return nullptr;
}

bool SR::HasDriverDomain(QString* outVMRef) const
{
    XenCache* cache = this->GetCache();

    QString srRef = this->OpaqueRef();
    if (srRef.isEmpty() || srRef == XENOBJECT_NULL)
        return false;

    QStringList pbdRefs = this->GetPBDRefs();
    for (const QString& pbdRef : pbdRefs)
    {
        if (pbdRef.isEmpty() || pbdRef == XENOBJECT_NULL)
            continue;

        QVariantMap pbdData = cache->ResolveObjectData(XenObjectType::PBD, pbdRef);
        if (pbdData.isEmpty())
            continue;

        QVariantMap otherConfig = pbdData.value("other_config").toMap();
        QString vmRef = otherConfig.value("storage_driver_domain").toString();
        if (vmRef.isEmpty() || vmRef == XENOBJECT_NULL)
            continue;

        QVariantMap vmData = cache->ResolveObjectData(XenObjectType::VM, vmRef);
        if (!vmData.isEmpty() && !IsControlDomainZero(cache, vmData, vmRef))
        {
            if (outVMRef)
                *outVMRef = vmRef;
            return true;
        }
    }

    return false;
}

bool SR::HasPBDs() const
{
    return !GetPBDRefs().isEmpty();
}

bool SR::IsBroken(bool checkAttached) const
{
    XenCache* cache = this->GetCache();

    QStringList pbdRefs = GetPBDRefs();
    if (pbdRefs.isEmpty())
        return true;

    const bool shared = IsShared();
    const int poolCount = cache->GetAllData(XenObjectType::Pool).size();
    const int hostCount = cache->GetAllData(XenObjectType::Host).size();
    int expectedPbdCount = 1;

    if (poolCount > 0 && shared)
        expectedPbdCount = hostCount;

    if (pbdRefs.size() != expectedPbdCount)
        return true;

    if (checkAttached)
    {
        for (const QString& pbdRef : pbdRefs)
        {
            QVariantMap pbdData = cache->ResolveObjectData(XenObjectType::PBD, pbdRef);
            if (pbdData.isEmpty() || !pbdData.value("currently_attached").toBool())
                return true;
        }
    }

    return false;
}

bool SR::MultipathAOK() const
{
    const QVariantMap smConfig = SMConfig();
    if (smConfig.value("multipathable", "false").toString() != "true")
        return true;

    if (this->LunPerVDI())
    {
        const QHash<QString, QHash<QString, QString>> statusByVm = this->GetMultiPathStatusLunPerVDI();
        for (auto vmIt = statusByVm.constBegin(); vmIt != statusByVm.constEnd(); ++vmIt)
        {
            const QHash<QString, QString>& statusByVdi = vmIt.value();
            for (auto vdiIt = statusByVdi.constBegin(); vdiIt != statusByVdi.constEnd(); ++vdiIt)
            {
                int currentPaths = 0;
                int maxPaths = 0;
                if (PBD::ParsePathCounts(vdiIt.value(), currentPaths, maxPaths) && currentPaths < maxPaths)
                    return false;
            }
        }
        return true;
    }

    const QHash<QString, QString> statusByPbd = this->GetMultiPathStatusLunPerSR();
    for (auto it = statusByPbd.constBegin(); it != statusByPbd.constEnd(); ++it)
    {
        int currentPaths = 0;
        int maxPaths = 0;
        if (PBD::ParsePathCounts(it.value(), currentPaths, maxPaths) && currentPaths < maxPaths)
            return false;
    }

    return true;
}

bool SR::CanRepairAfterUpgradeFromLegacySL() const
{
    if (GetType() != "cslg")
        return true;

    XenConnection* connection = GetConnection();
    if (!connection)
        return false;

    XenCache* cache = connection->GetCache();
    if (!cache)
        return false;

    QStringList pbdRefs = GetPBDRefs();
    for (const QString& pbdRef : pbdRefs)
    {
        QVariantMap pbdData = cache->ResolveObjectData(XenObjectType::PBD, pbdRef);
        const QVariantMap deviceConfig = pbdData.value("device_config").toMap();
        if (deviceConfig.contains("adapterid"))
            return true;
    }

    return false;
}

bool SR::IsDetached() const
{
    XenCache* cache = this->GetCache();

    QStringList pbdRefs = GetPBDRefs();
    if (pbdRefs.isEmpty())
        return true;

    for (const QString& pbdRef : pbdRefs)
    {
        QVariantMap pbdData = cache->ResolveObjectData(XenObjectType::PBD, pbdRef);
        if (!pbdData.isEmpty() && pbdData.value("currently_attached").toBool())
            return false;
    }

    return true;
}

QList<QSharedPointer<PBD>> SR::GetPBDs() const
{
    QList<QSharedPointer<PBD>> result;
    
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return result;
    
    XenCache* cache = connection->GetCache();
    if (!cache)
        return result;
    
    QStringList pbdRefs = this->GetPBDRefs();
    for (const QString& ref : pbdRefs)
    {
        if (!ref.isEmpty() && ref != XENOBJECT_NULL)
        {
            QSharedPointer<PBD> pbd = cache->ResolveObject<PBD>(ref);
            if (pbd)
                result.append(pbd);
        }
    }
    
    return result;
}

QList<QSharedPointer<VDI>> SR::GetVDIs() const
{
    QList<QSharedPointer<VDI>> result;
    
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return result;
    
    XenCache* cache = connection->GetCache();
    if (!cache)
        return result;
    
    QStringList vdiRefs = this->GetVDIRefs();
    for (const QString& ref : vdiRefs)
    {
        if (!ref.isEmpty() && ref != XENOBJECT_NULL)
        {
            QSharedPointer<VDI> vdi = cache->ResolveObject<VDI>(ref);
            if (vdi)
                result.append(vdi);
        }
    }
    
    return result;
}

QList<QSharedPointer<Blob>> SR::GetBlobs() const
{
    QList<QSharedPointer<Blob>> result;
    
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return result;
    
    XenCache* cache = connection->GetCache();
    if (!cache)
        return result;
    
    QVariantMap blobMap = this->Blobs();
    QMapIterator<QString, QVariant> iter(blobMap);
    while (iter.hasNext())
    {
        iter.next();
        QString ref = iter.value().toString();
        if (!ref.isEmpty() && ref != XENOBJECT_NULL)
        {
            QSharedPointer<Blob> blob = cache->ResolveObject<Blob>(ref);
            if (blob)
                result.append(blob);
        }
    }
    
    return result;
}

bool SR::HasRunningVMs() const
{
    XenConnection* connection = GetConnection();
    if (!connection)
        return false;

    XenCache* cache = connection->GetCache();
    if (!cache)
        return false;

    QStringList vdiRefs = GetVDIRefs();
    for (const QString& vdiRef : vdiRefs)
    {
        QVariantMap vdiData = cache->ResolveObjectData(XenObjectType::VDI, vdiRef);
        if (vdiData.isEmpty())
            continue;

        const QString vdiType = vdiData.value("type").toString();
        const bool metadataVdi = (vdiType == "metadata");

        QVariantList vbdRefs = vdiData.value("VBDs").toList();
        for (const QVariant& vbdRefVar : vbdRefs)
        {
            const QString vbdRef = vbdRefVar.toString();
            QVariantMap vbdData = cache->ResolveObjectData(XenObjectType::VBD, vbdRef);
            const QString vmRef = vbdData.value("VM").toString();
            if (vmRef.isEmpty())
                continue;

            QVariantMap vmData = cache->ResolveObjectData(XenObjectType::VM, vmRef);
            if (vmData.isEmpty())
                continue;

            const bool isControlDomain = vmData.value("is_control_domain").toBool();
            if (metadataVdi && isControlDomain)
                continue;

            const QString powerState = vmData.value("power_state").toString();
            if (powerState == "Running")
                return true;
        }
    }

    return false;
}
