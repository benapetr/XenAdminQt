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

#include "vdi.h"
#include "network/connection.h"
#include "../xencache.h"
#include "sr.h"
#include "vbd.h"
#include <QLocale>

VDI::VDI(XenConnection* connection, const QString& opaqueRef, QObject* parent) : XenObject(connection, opaqueRef, parent)
{
}

qint64 VDI::VirtualSize() const
{
    return longProperty("virtual_size");
}

qint64 VDI::PhysicalUtilisation() const
{
    return longProperty("physical_utilisation");
}

QString VDI::GetType() const
{
    return stringProperty("type");
}

bool VDI::Sharable() const
{
    return boolProperty("sharable");
}

bool VDI::ReadOnly() const
{
    return boolProperty("read_only");
}

QString VDI::SRRef() const
{
    return stringProperty("SR");
}

QStringList VDI::GetVBDRefs() const
{
    return stringListProperty("VBDs");
}

bool VDI::IsInUse() const
{
    return !GetVBDRefs().isEmpty();
}

QString VDI::SizeString() const
{
    qint64 size = VirtualSize();

    if (size < 0)
    {
        return "Unknown";
    }

    // Format as human-readable size
    const qint64 KB = 1024;
    const qint64 MB = 1024 * KB;
    const qint64 GB = 1024 * MB;
    const qint64 TB = 1024 * GB;

    if (size >= TB)
    {
        return QString("%1 TB").arg(QLocale().toString(size / (double) TB, 'f', 2));
    } else if (size >= GB)
    {
        return QString("%1 GB").arg(QLocale().toString(size / (double) GB, 'f', 2));
    } else if (size >= MB)
    {
        return QString("%1 MB").arg(QLocale().toString(size / (double) MB, 'f', 2));
    } else if (size >= KB)
    {
        return QString("%1 KB").arg(QLocale().toString(size / (double) KB, 'f', 2));
    } else
    {
        return QString("%1 bytes").arg(size);
    }
}

QString VDI::SnapshotOfRef() const
{
    return stringProperty("snapshot_of");
}

bool VDI::IsSnapshot() const
{
    QString snapshotOf = this->SnapshotOfRef();
    return !snapshotOf.isEmpty() && snapshotOf != XENOBJECT_NULL;
}

QStringList VDI::AllowedOperations() const
{
    return this->stringListProperty("allowed_operations");
}

QVariantMap VDI::CurrentOperations() const
{
    return this->property("current_operations").toMap();
}

bool VDI::StorageLock() const
{
    return this->boolProperty("storage_lock", false);
}

QString VDI::Location() const
{
    return this->stringProperty("location");
}

bool VDI::Managed() const
{
    return this->boolProperty("managed", true);
}

bool VDI::Missing() const
{
    return this->boolProperty("missing", false);
}

QString VDI::ParentRef() const
{
    return this->stringProperty("parent");
}

QStringList VDI::CrashDumpRefs() const
{
    return this->stringListProperty("crash_dumps");
}

QVariantMap VDI::XenstoreData() const
{
    return this->property("xenstore_data").toMap();
}

QVariantMap VDI::SMConfig() const
{
    return this->property("sm_config").toMap();
}

QStringList VDI::SnapshotRefs() const
{
    return this->stringListProperty("snapshots");
}

QDateTime VDI::SnapshotTime() const
{
    QString dateStr = this->stringProperty("snapshot_time");
    if (dateStr.isEmpty())
        return QDateTime();
    return QDateTime::fromString(dateStr, Qt::ISODate);
}

bool VDI::AllowCaching() const
{
    return this->boolProperty("allow_caching", false);
}

QString VDI::OnBoot() const
{
    return this->stringProperty("on_boot");
}

QString VDI::MetadataOfPoolRef() const
{
    return this->stringProperty("metadata_of_pool");
}

bool VDI::MetadataLatest() const
{
    return this->boolProperty("metadata_latest", false);
}

bool VDI::IsToolsIso() const
{
    // C# equivalent: VDI.IsToolsIso()
    // Checks both is_tools_iso flag and known tools ISO filenames
    if (this->boolProperty("is_tools_iso", false))
        return true;
    
    // Check against known tools ISO names (legacy detection)
    QString nameLabel = this->GetName();
    QStringList toolsIsoNames = {"xswindrivers.iso", "xs-tools.iso", "guest-tools.iso"};
    
    if (toolsIsoNames.contains(nameLabel))
        return true;
    
    return false;
}

bool VDI::IsCBTEnabled() const
{
    return this->boolProperty("cbt_enabled", false);
}


QSharedPointer<SR> VDI::GetSR() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QSharedPointer<SR>();
    
    XenCache* cache = connection->GetCache();
    if (!cache)
        return QSharedPointer<SR>();
    
    QString ref = this->SRRef();
    if (ref.isEmpty() || ref == XENOBJECT_NULL)
        return QSharedPointer<SR>();
    
    return cache->ResolveObject<SR>(ref);
}

QString VDI::NameWithLocation() const
{
    if (this->GetConnection())
    {
        QSharedPointer<SR> sr = GetSR();
        if (sr)
        {
            return QString("%1 on '%2' %3").arg(GetName(), sr->GetName(), sr->LocationString());
        }
    }

    return XenObject::NameWithLocation();
}

QList<QSharedPointer<VBD>> VDI::GetVBDs() const
{
    QList<QSharedPointer<VBD>> result;
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return result;
    
    XenCache* cache = connection->GetCache();
    if (!cache)
        return result;
    
    QStringList refs = this->GetVBDRefs();
    for (const QString& ref : refs)
    {
        if (!ref.isEmpty() && ref != XENOBJECT_NULL)
        {
            QSharedPointer<VBD> obj = cache->ResolveObject<VBD>(ref);
            if (obj)
                result.append(obj);
        }
    }
    return result;
}
