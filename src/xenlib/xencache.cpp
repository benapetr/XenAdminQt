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
#include <QDebug>
#include <QMutexLocker>

XenCache::XenCache(QObject* parent)
    : QObject(parent)
{
    qDebug() << "XenCache: Created";
}

XenCache::~XenCache()
{
    clear();
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

QVariantMap XenCache::resolve(const QString& type, const QString& ref) const
{
    if (ref.isEmpty())
    {
        return QVariantMap();
    }

    QMutexLocker locker(&m_mutex);

    QString normalizedType = normalizeType(type);

    if (!m_cache.contains(normalizedType))
    {
        return QVariantMap();
    }

    const QMap<QString, QVariantMap>& typeCache = m_cache[normalizedType];
    if (!typeCache.contains(ref))
    {
        return QVariantMap();
    }

    return typeCache[ref];
}

QString XenCache::canonicalType(const QString& type) const
{
    return normalizeType(type);
}

bool XenCache::contains(const QString& type, const QString& ref) const
{
    if (ref.isEmpty())
    {
        return false;
    }

    QMutexLocker locker(&m_mutex);

    QString normalizedType = normalizeType(type);

    if (!m_cache.contains(normalizedType))
    {
        return false;
    }

    return m_cache[normalizedType].contains(ref);
}

QList<QVariantMap> XenCache::getAll(const QString& type) const
{
    QMutexLocker locker(&m_mutex);

    QString normalizedType = normalizeType(type);

    if (!m_cache.contains(normalizedType))
    {
        return QList<QVariantMap>();
    }

    return m_cache[normalizedType].values();
}

QStringList XenCache::getAllRefs(const QString& type) const
{
    QMutexLocker locker(&m_mutex);

    QString normalizedType = normalizeType(type);

    if (!m_cache.contains(normalizedType))
    {
        return QStringList();
    }

    return m_cache[normalizedType].keys();
}

// C# Equivalent: connection.Cache.XenSearchableObjects
// C# Reference: Cache.cs lines 565-600 - only returns searchable objects
// Used by: SearchTabPage::populateTree() -> GroupAlg.cs GetGrouped()
//
// CRITICAL: Only returns object types that should appear in searches/trees.
// Does NOT return internal objects like: console, host_cpu, host_metrics,
// message, pbd, pif, vbd, vif, bond, vgpu, etc.
QList<QPair<QString, QString>> XenCache::getAllObjects() const
{
    QMutexLocker locker(&m_mutex);

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
    searchableTypes << "vm" << "host" << "sr" << "network" << "vdi" << "pool";
    // Note: folder, vm_appliance, docker_container not yet implemented

    // Iterate only searchable types
    foreach (const QString& type, searchableTypes)
    {
        QString normalizedType = normalizeType(type);

        if (!m_cache.contains(normalizedType))
            continue;

        const QMap<QString, QVariantMap>& objectsOfType = m_cache[normalizedType];

        // Iterate all objects of this type
        for (auto refIt = objectsOfType.constBegin(); refIt != objectsOfType.constEnd(); ++refIt)
        {
            const QString& ref = refIt.key();
            allObjects.append(qMakePair(type, ref));
        }
    }

    return allObjects;
}

void XenCache::update(const QString& type, const QString& ref, const QVariantMap& data)
{
    if (ref.isEmpty())
    {
        qWarning() << "XenCache::update - Empty ref provided for type:" << type;
        return;
    }

    QString normalizedType = normalizeType(type);

    {
        QMutexLocker locker(&m_mutex);

        if (!m_cache.contains(normalizedType))
        {
            m_cache[normalizedType] = QMap<QString, QVariantMap>();
        }

        // Ensure ref is in the data
        QVariantMap dataWithRef = data;
        if (!dataWithRef.contains("ref"))
        {
            dataWithRef["ref"] = ref;
        }

        m_cache[normalizedType][ref] = dataWithRef;
    }

    emit objectChanged(normalizedType, ref);
}

void XenCache::updateBulk(const QString& type, const QVariantMap& allRecords)
{
    QString normalizedType = normalizeType(type);
    int updateCount = 0;

    {
        QMutexLocker locker(&m_mutex);

        if (!m_cache.contains(normalizedType))
        {
            m_cache[normalizedType] = QMap<QString, QVariantMap>();
        }

        // Iterate through all records and add to cache
        for (auto it = allRecords.constBegin(); it != allRecords.constEnd(); ++it)
        {
            QString ref = it.key();
            QVariantMap data = it.value().toMap();

            // Ensure ref is in the data
            if (!data.contains("ref"))
            {
                data["ref"] = ref;
            }

            m_cache[normalizedType][ref] = data;
            updateCount++;
        }
    }

    qDebug() << "XenCache: Bulk update completed for" << normalizedType
             << "- added/updated" << updateCount << "objects";

    emit bulkUpdateComplete(normalizedType, updateCount);
}

void XenCache::remove(const QString& type, const QString& ref)
{
    if (ref.isEmpty())
    {
        return;
    }

    QString normalizedType = normalizeType(type);

    {
        QMutexLocker locker(&m_mutex);

        if (!m_cache.contains(normalizedType))
        {
            return;
        }

        m_cache[normalizedType].remove(ref);
    }

    emit objectRemoved(normalizedType, ref);
}

void XenCache::clearType(const QString& type)
{
    QString normalizedType = normalizeType(type);

    int count = 0;
    {
        QMutexLocker locker(&m_mutex);

        if (m_cache.contains(normalizedType))
        {
            count = m_cache[normalizedType].count();
            m_cache.remove(normalizedType);
        }
    }

    if (count > 0)
    {
        qDebug() << "XenCache: Cleared" << count << normalizedType << "objects from cache";
        emit cacheCleared(); // Could add a typeCleared signal if needed
    }
}

void XenCache::clear()
{
    {
        QMutexLocker locker(&m_mutex);
        m_cache.clear();
    }

    qDebug() << "XenCache: Cache cleared";
    emit cacheCleared();
}

int XenCache::count(const QString& type) const
{
    QMutexLocker locker(&m_mutex);

    QString normalizedType = normalizeType(type);

    if (!m_cache.contains(normalizedType))
    {
        return 0;
    }

    return m_cache[normalizedType].count();
}

bool XenCache::isEmpty() const
{
    QMutexLocker locker(&m_mutex);
    return m_cache.isEmpty();
}
