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
    return this->boolProperty("is_tools_sr", false);
}

QString SR::HomeRef() const
{
    // For local SRs, find the host via PBD connections
    // For shared SRs, could return any connected host or empty
    QStringList pbds = this->PBDRefs();
    if (pbds.isEmpty())
        return QString();

    // Get first PBD and extract host reference from it
    // TODO: Query cache for PBD.host
    // For now, return empty - will be implemented when PBD class is added
    return QString();
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
