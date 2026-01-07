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

namespace
{
    bool IsControlDomainZero(XenCache* cache, const QVariantMap& vmData, const QString& vmRef)
    {
        if (!cache || vmData.isEmpty())
            return false;

        if (!vmData.value("is_control_domain").toBool())
            return false;

        QString hostRef = vmData.value("resident_on").toString();
        if (hostRef.isEmpty() || hostRef == "OpaqueRef:NULL")
            return false;

        QVariantMap hostData = cache->ResolveObjectData("host", hostRef);
        if (hostData.isEmpty())
            return false;

        QString hostControlDomain = hostData.value("control_domain").toString();
        if (!hostControlDomain.isEmpty() && hostControlDomain != "OpaqueRef:NULL")
            return hostControlDomain == vmRef;

        qint64 domid = vmData.value("domid").toLongLong();
        return domid == 0;
    }
}

SR::SR(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

QString SR::GetObjectType() const
{
    return "sr";
}

QString SR::GetType() const
{
    return stringProperty("type");
}

bool SR::IsShared() const
{
    return boolProperty("shared", false);
}

qint64 SR::PhysicalSize() const
{
    return longProperty("physical_size", 0);
}

qint64 SR::PhysicalUtilisation() const
{
    return longProperty("physical_utilisation", 0);
}

qint64 SR::VirtualAllocation() const
{
    return longProperty("virtual_allocation", 0);
}

qint64 SR::FreeSpace() const
{
    return PhysicalSize() - PhysicalUtilisation();
}
QSharedPointer<Host> SR::GetHost(XenCache* cache)
{
    if (!cache)
    {
        XenConnection* connection = this->GetConnection();
        if (!connection)
            return QSharedPointer<Host>();
        cache = connection->GetCache();
        if (!cache)
            return QSharedPointer<Host>();
    }

    // For shared SRs, return pool coordinator
    if (this->IsShared())
    {
        QString poolRef = cache->GetPoolRef();
        if (!poolRef.isEmpty())
        {
            QVariantMap poolData = cache->ResolveObjectData("pool", poolRef);
            QString masterRef = poolData.value("master").toString();
            if (!masterRef.isEmpty() && masterRef != "OpaqueRef:NULL")
                return cache->ResolveObject<Host>("host", masterRef);
        }
        return QSharedPointer<Host>();
    }

    // For local SRs, find the host it's connected to via PBD
    QStringList pbdRefs = this->PBDRefs();
    for (const QString& pbdRef : pbdRefs)
    {
        QVariantMap pbdData = cache->ResolveObjectData("pbd", pbdRef);
        if (!pbdData.isEmpty())
        {
            QString hostRef = pbdData.value("host").toString();
            if (!hostRef.isEmpty() && hostRef != "OpaqueRef:NULL")
                return cache->ResolveObject<Host>("host", hostRef);
        }
    }

    return QSharedPointer<Host>();
}
QStringList SR::VDIRefs() const
{
    return stringListProperty("VDIs");
}

QStringList SR::PBDRefs() const
{
    return stringListProperty("PBDs");
}

QString SR::ContentType() const
{
    return stringProperty("content_type", "user");
}

QVariantMap SR::OtherConfig() const
{
    return property("other_config").toMap();
}

QVariantMap SR::SMConfig() const
{
    return property("sm_config").toMap();
}

QStringList SR::Tags() const
{
    return stringListProperty("tags");
}

QStringList SR::AllowedOperations() const
{
    return stringListProperty("allowed_operations");
}

QVariantMap SR::CurrentOperations() const
{
    return property("current_operations").toMap();
}

bool SR::SupportsTrim() const
{
    // Check sm_config for trim support
    QVariantMap sm = this->SMConfig();
    return sm.value("supports_trim", false).toBool();
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

QString SR::HomeRef() const
{
    if (this->IsShared())
        return QString();

    QStringList pbds = this->PBDRefs();
    if (pbds.size() != 1)
        return QString();

    XenCache* cache = GetConnection()->GetCache();
    if (!cache)
        return QString();

    QVariantMap pbdData = cache->ResolveObjectData("pbd", pbds.first());
    return pbdData.value("host").toString();
}

Host* SR::GetFirstAttachedStorageHost() const
{
    QStringList pbds = this->PBDRefs();
    if (pbds.isEmpty())
        return nullptr;

    // Iterate through PBDs to find first currently_attached one
    XenCache* cache = GetConnection()->GetCache();
    if (!cache)
        return nullptr;

    for (const QString& pbdRef : pbds)
    {
        QVariantMap pbdData = cache->ResolveObjectData("pbd", pbdRef);
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
    XenCache* cache = this->GetConnection() ? this->GetConnection()->GetCache() : nullptr;
    if (!cache)
        return false;

    QString srRef = this->OpaqueRef();
    if (srRef.isEmpty() || srRef == "OpaqueRef:NULL")
        return false;

    QStringList pbdRefs = this->PBDRefs();
    for (const QString& pbdRef : pbdRefs)
    {
        if (pbdRef.isEmpty() || pbdRef == "OpaqueRef:NULL")
            continue;

        QVariantMap pbdData = cache->ResolveObjectData("pbd", pbdRef);
        if (pbdData.isEmpty())
            continue;

        QVariantMap otherConfig = pbdData.value("other_config").toMap();
        QString vmRef = otherConfig.value("storage_driver_domain").toString();
        if (vmRef.isEmpty() || vmRef == "OpaqueRef:NULL")
            continue;

        QVariantMap vmData = cache->ResolveObjectData("vm", vmRef);
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
    return !PBDRefs().isEmpty();
}

bool SR::IsBroken(bool checkAttached) const
{
    XenConnection* connection = GetConnection();
    if (!connection)
        return true;

    XenCache* cache = connection->GetCache();
    if (!cache)
        return true;

    QStringList pbdRefs = PBDRefs();
    if (pbdRefs.isEmpty())
        return true;

    const bool shared = IsShared();
    const int poolCount = cache->GetAllData("pool").size();
    const int hostCount = cache->GetAllData("host").size();
    int expectedPbdCount = 1;

    if (poolCount > 0 && shared)
        expectedPbdCount = hostCount;

    if (pbdRefs.size() != expectedPbdCount)
        return true;

    if (checkAttached)
    {
        for (const QString& pbdRef : pbdRefs)
        {
            QVariantMap pbdData = cache->ResolveObjectData("pbd", pbdRef);
            if (pbdData.isEmpty() || !pbdData.value("currently_attached").toBool())
                return true;
        }
    }

    return false;
}

bool SR::MultipathAOK() const
{
    XenConnection* connection = GetConnection();
    if (!connection)
        return true;

    const QVariantMap smConfig = SMConfig();
    if (smConfig.value("multipathable", "false").toString() != "true")
        return true;

    XenCache* cache = connection->GetCache();
    if (!cache)
        return true;

    QStringList pbdRefs = PBDRefs();
    for (const QString& pbdRef : pbdRefs)
    {
        QVariantMap pbdData = cache->ResolveObjectData("pbd", pbdRef);
        if (pbdData.isEmpty())
            continue;

        const QVariantMap deviceConfig = pbdData.value("device_config").toMap();
        if (deviceConfig.value("multipathed", "false").toString() != "true")
            continue;

        const QVariantMap otherConfig = pbdData.value("other_config").toMap();
        const int currentPaths = otherConfig.value("multipath-current-paths", "0").toString().toInt();
        const int maxPaths = otherConfig.value("multipath-maximum-paths", "0").toString().toInt();
        if (maxPaths > 0 && currentPaths < maxPaths)
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

    QStringList pbdRefs = PBDRefs();
    for (const QString& pbdRef : pbdRefs)
    {
        QVariantMap pbdData = cache->ResolveObjectData("pbd", pbdRef);
        const QVariantMap deviceConfig = pbdData.value("device_config").toMap();
        if (deviceConfig.contains("adapterid"))
            return true;
    }

    return false;
}

bool SR::IsDetached() const
{
    XenConnection* connection = GetConnection();
    if (!connection)
        return true;

    XenCache* cache = connection->GetCache();
    if (!cache)
        return true;

    QStringList pbdRefs = PBDRefs();
    if (pbdRefs.isEmpty())
        return true;

    for (const QString& pbdRef : pbdRefs)
    {
        QVariantMap pbdData = cache->ResolveObjectData("pbd", pbdRef);
        if (!pbdData.isEmpty() && pbdData.value("currently_attached").toBool())
            return false;
    }

    return true;
}

bool SR::HasRunningVMs() const
{
    XenConnection* connection = GetConnection();
    if (!connection)
        return false;

    XenCache* cache = connection->GetCache();
    if (!cache)
        return false;

    QStringList vdiRefs = VDIRefs();
    for (const QString& vdiRef : vdiRefs)
    {
        QVariantMap vdiData = cache->ResolveObjectData("vdi", vdiRef);
        if (vdiData.isEmpty())
            continue;

        const QString vdiType = vdiData.value("type").toString();
        const bool metadataVdi = (vdiType == "metadata");

        QVariantList vbdRefs = vdiData.value("VBDs").toList();
        for (const QVariant& vbdRefVar : vbdRefs)
        {
            const QString vbdRef = vbdRefVar.toString();
            QVariantMap vbdData = cache->ResolveObjectData("vbd", vbdRef);
            const QString vmRef = vbdData.value("VM").toString();
            if (vmRef.isEmpty())
                continue;

            QVariantMap vmData = cache->ResolveObjectData("vm", vmRef);
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
