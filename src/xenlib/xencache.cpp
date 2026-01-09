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

#include "xencache.h"
#include "xen/network/connection.h"
#include "xen/vm.h"
#include "xen/host.h"
#include "xen/hostmetrics.h"
#include "xen/pool.h"
#include "xen/sr.h"
#include "xen/vdi.h"
#include "xen/vbd.h"
#include "xen/vif.h"
#include "xen/pif.h"
#include <QDebug>
#include <QMutexLocker>

XenCache::XenCache(QObject* parent) : QObject(parent)
{
    this->m_connection = qobject_cast<XenConnection*>(parent);
}

XenCache::~XenCache()
{
    this->Clear();
}

QString XenCache::normalizeType(const QString& type) const
{
    // Normalize type names to lowercase for consistency
    // C# uses capitalized names, we'll use lowercase internally
    QString normalized = type.toLower();

    // Handle variations
    if (normalized == "vm" || normalized == "vms")
        return "vm";
    if (normalized == "host" || normalized == "hosts")
        return "host";
    if (normalized == "sr" || normalized == "srs")
        return "sr";
    if (normalized == "network" || normalized == "networks")
        return "network";
    if (normalized == "pool" || normalized == "pools")
        return "pool";
    if (normalized == "vdi" || normalized == "vdis")
        return "vdi";
    if (normalized == "vbd" || normalized == "vbds")
        return "vbd";
    if (normalized == "vif" || normalized == "vifs")
        return "vif";
    if (normalized == "pbd" || normalized == "pbds")
        return "pbd";
    if (normalized == "pif" || normalized == "pifs")
        return "pif";

    return normalized;
}

QVariantMap XenCache::ResolveObjectData(const QString& type, const QString& ref) const
{
    if (ref.isEmpty())
    {
        return QVariantMap();
    }

    QMutexLocker locker(&this->m_mutex);

    QString normalizedType = normalizeType(type);

    if (!this->m_cache.contains(normalizedType))
    {
        return QVariantMap();
    }

    const QMap<QString, QVariantMap>& typeCache = this->m_cache[normalizedType];
    if (!typeCache.contains(ref))
    {
        return QVariantMap();
    }

    return typeCache[ref];
}

QSharedPointer<XenObject> XenCache::ResolveObject(const QString& type, const QString& ref)
{
    if (ref.isEmpty())
        return QSharedPointer<XenObject>();

    QString normalizedType = this->normalizeType(type);

    QMutexLocker locker(&this->m_mutex);

    if (this->m_objects.contains(normalizedType))
    {
        auto it = this->m_objects[normalizedType].find(ref);
        if (it != this->m_objects[normalizedType].end())
        {
            return it.value();
        }
    }

    if (!this->m_cache.contains(normalizedType) || !this->m_cache[normalizedType].contains(ref))
        return QSharedPointer<XenObject>();

    QSharedPointer<XenObject> created = this->createObjectForType(normalizedType, ref);
    if (!created)
        return QSharedPointer<XenObject>();

    if (!this->m_objects.contains(normalizedType))
    {
        this->m_objects[normalizedType] = QMap<QString, QSharedPointer<XenObject>>();
    }
    this->m_objects[normalizedType].insert(ref, created);
    return created;
}

QString XenCache::CanonicalType(const QString& type) const
{
    return this->normalizeType(type);
}

// Note: don't work with m_objects in this because m_objects is dynamic cache of stuff
// that was looked up explicitly, while m_cache contains everything
bool XenCache::Contains(const QString& type, const QString& ref) const
{
    if (ref.isEmpty())
        return false;

    QMutexLocker locker(&this->m_mutex);

    QString normalizedType = normalizeType(type);

    if (!this->m_cache.contains(normalizedType))
        return false;

    return this->m_cache[normalizedType].contains(ref);
}

QList<QVariantMap> XenCache::GetAllData(const QString& type) const
{
    QMutexLocker locker(&this->m_mutex);

    QString normalizedType = normalizeType(type);

    if (!this->m_cache.contains(normalizedType))
    {
        return QList<QVariantMap>();
    }

    return this->m_cache[normalizedType].values();
}

QList<QSharedPointer<XenObject>> XenCache::GetAll(const QString& type)
{
    QStringList refs = GetAllRefs(type);
    QList<QSharedPointer<XenObject>> objects;
    objects.reserve(refs.size());

    for (const QString& ref : refs)
    {
        QSharedPointer<XenObject> obj = ResolveObject(type, ref);
        if (obj)
            objects.append(obj);
    }

    return objects;
}

QStringList XenCache::GetAllRefs(const QString& type) const
{
    QMutexLocker locker(&this->m_mutex);

    QString normalizedType = normalizeType(type);

    if (!this->m_cache.contains(normalizedType))
    {
        return QStringList();
    }

    return this->m_cache[normalizedType].keys();
}

// C# Equivalent: connection.Cache.XenSearchableObjects
// C# Reference: Cache.cs lines 565-600 - only returns searchable objects
// Used by: SearchTabPage::populateTree() -> GroupAlg.cs GetGrouped()
//
// CRITICAL: Only returns object types that should appear in searches/trees.
// Does NOT return internal objects like: console, host_cpu, host_metrics,
// message, pbd, pif, vbd, vif, bond, vgpu, etc.
QList<QPair<QString, QString>> XenCache::GetXenSearchableObjects() const
{
    QMutexLocker locker(&this->m_mutex);

    QList<QPair<QString, QString>> allObjects;

    // C# XenSearchableObjects returns (in order):
    // 1. VMs (includes templates and snapshots)
    // 2. VM_appliances
    // 3. Hosts
    // 4. SRs
    // 5. Networks
    // 6. VDIs
    // 7. Folders
    // 8. DockerContainers
    // 9. Pools (only if visible)

    // Define searchable types matching C# Cache.XenSearchableObjects
    QStringList searchableTypes;
    searchableTypes << "vm" << "vm_appliance" << "host" << "sr" << "network"
                    << "vdi" << "folder" << "dockercontainer" << "pool";

    // Iterate only searchable types
    foreach (const QString& type, searchableTypes)
    {
        QString normalizedType = normalizeType(type);

        if (!this->m_cache.contains(normalizedType))
            continue;

        const QMap<QString, QVariantMap>& objectsOfType = this->m_cache[normalizedType];

        // Iterate all objects of this type
        for (auto refIt = objectsOfType.constBegin(); refIt != objectsOfType.constEnd(); ++refIt)
        {
            const QString& ref = refIt.key();
            allObjects.append(qMakePair(type, ref));
        }
    }

    return allObjects;
}

QList<QSharedPointer<XenObject>> XenCache::GetAllObjects()
{
    QList<QPair<QString, QString>> refs = this->GetXenSearchableObjects();
    QList<QSharedPointer<XenObject>> objects;
    objects.reserve(refs.size());

    for (const auto& entry : refs)
    {
        QSharedPointer<XenObject> obj = this->ResolveObject(entry.first, entry.second);
        if (obj)
            objects.append(obj);
    }

    return objects;
}

void XenCache::Update(const QString& type, const QString& ref, const QVariantMap& data)
{
    if (ref.isEmpty())
    {
        qWarning() << "XenCache::update - Empty ref provided for type:" << type;
        return;
    }

    QString normalizedType = this->normalizeType(type);

    bool refresh = false;
    {
        QMutexLocker locker(&this->m_mutex);

        if (!this->m_cache.contains(normalizedType))
            this->m_cache[normalizedType] = QMap<QString, QVariantMap>();

        // Ensure ref is in the data
        QVariantMap dataWithRef = data;
        if (!dataWithRef.contains("ref"))
            dataWithRef["ref"] = ref;

        this->m_cache[normalizedType][ref] = dataWithRef;
        refresh = this->m_objects.contains(normalizedType) && this->m_objects[normalizedType].contains(ref);
    }

    if (refresh)
        this->refreshObject(normalizedType, ref);

    emit objectChanged(this->m_connection, normalizedType, ref);
}

void XenCache::UpdateBulk(const QString& type, const QVariantMap& allRecords)
{
    QString normalizedType = normalizeType(type);
    int updateCount = 0;
    QStringList refreshedRefs;

    {
        QMutexLocker locker(&this->m_mutex);

        if (!this->m_cache.contains(normalizedType))
            this->m_cache[normalizedType] = QMap<QString, QVariantMap>();

        // Iterate through all records and add to cache
        for (auto it = allRecords.constBegin(); it != allRecords.constEnd(); ++it)
        {
            QString ref = it.key();
            QVariantMap data = it.value().toMap();

            // Ensure ref is in the data
            if (!data.contains("ref"))
                data["ref"] = ref;

            this->m_cache[normalizedType][ref] = data;
            if (this->m_objects.contains(normalizedType) && this->m_objects[normalizedType].contains(ref))
                refreshedRefs.append(ref);
            updateCount++;
        }
    }

    for (const QString& ref : refreshedRefs)
        refreshObject(normalizedType, ref);

    qDebug() << "XenCache: Bulk update completed for" << normalizedType << "- added/updated" << updateCount << "objects";

    emit bulkUpdateComplete(normalizedType, updateCount);
}

void XenCache::Remove(const QString& type, const QString& ref)
{
    if (ref.isEmpty())
        return;

    QString normalizedType = this->normalizeType(type);

    {
        QMutexLocker locker(&this->m_mutex);

        if (!this->m_cache.contains(normalizedType))
            return;

        this->m_cache[normalizedType].remove(ref);
    }

    this->evictObject(normalizedType, ref);
    emit objectRemoved(this->m_connection, normalizedType, ref);
}

void XenCache::ClearType(const QString& type)
{
    QString normalizedType = normalizeType(type);

    int count = 0;
    QStringList refs;
    {
        QMutexLocker locker(&this->m_mutex);

        if (this->m_cache.contains(normalizedType))
        {
            count = this->m_cache[normalizedType].count();
            refs = this->m_cache[normalizedType].keys();
            this->m_cache.remove(normalizedType);
        }
    }

    for (const QString& ref : refs)
        this->evictObject(normalizedType, ref);

    if (count > 0)
    {
        qDebug() << "XenCache: Cleared" << count << normalizedType << "objects from cache";
        emit cacheCleared(); // Could add a typeCleared signal if needed
    }
}

void XenCache::Clear()
{
    QList<QPair<QString, QString>> refs;
    {
        QMutexLocker locker(&this->m_mutex);
        for (auto typeIt = this->m_cache.constBegin(); typeIt != this->m_cache.constEnd(); ++typeIt)
        {
            const QString& type = typeIt.key();
            const QMap<QString, QVariantMap>& records = typeIt.value();
            for (auto refIt = records.constBegin(); refIt != records.constEnd(); ++refIt)
                refs.append(qMakePair(type, refIt.key()));
        }
        this->m_cache.clear();
    }

    for (const auto& entry : refs)
        this->evictObject(entry.first, entry.second);

    qDebug() << "XenCache: Cache cleared";
    emit cacheCleared();
}

int XenCache::Count(const QString& type) const
{
    QMutexLocker locker(&this->m_mutex);

    QString normalizedType = normalizeType(type);

    if (!this->m_cache.contains(normalizedType))
        return 0;

    return this->m_cache[normalizedType].count();
}

bool XenCache::IsEmpty() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_cache.isEmpty();
}

QSharedPointer<XenObject> XenCache::createObjectForType(const QString& type, const QString& ref)
{
    if (!this->m_connection)
        return QSharedPointer<XenObject>();

    if (type == "vm")
        return QSharedPointer<XenObject>(new VM(this->m_connection, ref));
    if (type == "host")
        return QSharedPointer<XenObject>(new Host(this->m_connection, ref));
    if (type == "host_metrics")
        return QSharedPointer<XenObject>(new HostMetrics(this->m_connection, ref));
    if (type == "pool")
        return QSharedPointer<XenObject>(new Pool(this->m_connection, ref));
    if (type == "sr")
        return QSharedPointer<XenObject>(new SR(this->m_connection, ref));
    if (type == "vdi")
        return QSharedPointer<XenObject>(new VDI(this->m_connection, ref));
    if (type == "vbd")
        return QSharedPointer<XenObject>(new VBD(this->m_connection, ref));
    if (type == "vif")
        return QSharedPointer<XenObject>(new VIF(this->m_connection, ref));
    if (type == "pif")
        return QSharedPointer<XenObject>(new PIF(this->m_connection, ref));

    return QSharedPointer<XenObject>();
}

void XenCache::refreshObject(const QString& type, const QString& ref)
{
    // TODO - consider migrating to QHash for better performance of lookups
    QSharedPointer<XenObject> obj;
    {
        QMutexLocker locker(&this->m_mutex);
        if (!this->m_objects.contains(type))
            return;
        auto it = this->m_objects[type].find(ref);
        if (it == this->m_objects[type].end())
            return;
        obj = it.value();
        if (obj)
            obj->SetEvicted(false);
    }

    if (obj)
        obj->Refresh();
}

void XenCache::evictObject(const QString& type, const QString& ref)
{
    QSharedPointer<XenObject> obj;
    {
        QMutexLocker locker(&this->m_mutex);
        if (!this->m_objects.contains(type))
            return;
        auto it = this->m_objects[type].find(ref);
        if (it == this->m_objects[type].end())
            return;
        obj = it.value();
        this->m_objects[type].remove(ref);
    }

    if (obj)
        obj->SetEvicted(true);
}

// NOTE: if you ever get the idea to cache this, keep in mind that pool ref changes when host leaves / joins a pool
// so we might need to hook to such events and invalidate any cached reference.
// Easy, but stupid solution is what we do now (and what C# version does) - always look it up from all cached data
QString XenCache::GetPoolRef() const
{
    QMutexLocker locker(&this->m_mutex);
    
    // Get pool type (normalized)
    QString poolType = this->normalizeType("pool");
    
    // Get all pool refs (there should be exactly one)
    if (this->m_cache.contains(poolType) && !this->m_cache[poolType].isEmpty())
        return this->m_cache[poolType].firstKey();
    
    return QString();
}

QSharedPointer<Pool> XenCache::GetPool()
{
    QString pool_ref = this->GetPoolRef();
    if (pool_ref.isEmpty())
        return QSharedPointer<Pool>();

    return this->ResolveObject<Pool>("pool", pool_ref);
}
