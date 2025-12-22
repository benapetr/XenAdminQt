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

#ifndef XENCACHE_H
#define XENCACHE_H

#include <QObject>
#include <QMap>
#include <QVariantMap>
#include <QMutex>
#include <QList>
#include <QSharedPointer>
#include "xen/xenobject.h"

/**
 * @brief XenCache - Caches all XenServer objects locally for fast lookups
 *
 * Similar to C# XenAdmin's Cache class, this stores all objects in memory
 * and provides instant lookups without API calls. The cache is populated
 * once on connection and kept up-to-date via events.
 *
 * This dramatically improves performance:
 * - No network latency on selection
 * - No duplicate API calls
 * - Instant UI updates
 * - Better scalability
 */
class XenCache : public QObject
{
    Q_OBJECT

    public:
        explicit XenCache(QObject* parent = nullptr);
        virtual ~XenCache();

        /**
         * @brief Resolve object by reference (instant memory lookup)
         * @param type Object type (e.g., "VM", "host", "SR", "network")
         * @param ref XenAPI object reference
         * @return QVariantMap containing object data, or empty map if not found
         */
        QVariantMap ResolveObjectData(const QString& type, const QString& ref) const;

        /**
         * @brief Resolve object as a typed XenObject instance
         * @param type Object type (e.g., "VM", "host", "SR")
         * @param ref XenAPI object reference
         * @return Shared pointer to cached object, or null if not found/unsupported
         */
        QSharedPointer<XenObject> ResolveObject(const QString& type, const QString& ref);

        /**
         * @brief Resolve object and cast to the requested type
         */
        template <typename T>
        QSharedPointer<T> ResolveObject(const QString& type, const QString& ref)
        {
            QSharedPointer<XenObject> base = ResolveObject(type, ref);
            return qSharedPointerDynamicCast<T>(base);
        }

        /**
         * @brief Canonicalize a type string so all callers share the same mapping logic
         * @param type Object type
         * @return Lowercase canonical form used internally (e.g. "VMs" -> "vm")
         */
        QString CanonicalType(const QString& type) const;

        /**
         * @brief Check if cache has an object
         * @param type Object type
         * @param ref Object reference
         * @return true if object exists in cache
         */
        bool Contains(const QString& type, const QString& ref) const;

        /**
         * @brief Get all objects of a specific type
         * @param type Object type (e.g., "VM", "host")
         * @return List of all objects of that type
         */
        QList<QVariantMap> GetAll(const QString& type) const;

        /**
         * @brief Get all object refs of a specific type
         * @param type Object type
         * @return List of all refs for that type
         */
        QStringList GetAllRefs(const QString& type) const;

        /**
         * @brief Get all objects across all types (for iteration/filtering)
         * @return List of (type, ref) pairs for all cached objects
         *
         * This matches C# connection.Cache.XenSearchableObjects which returns
         * all cached objects so they can be filtered by Query.Match().
         * Used by SearchTabPage to iterate all objects and let Query do filtering.
         */
        QList<QPair<QString, QString>> GetAllObjectsData() const;

        /**
         * @brief Get all cached objects as shared pointers
         * @return List of cached XenObject instances
         */
        QList<QSharedPointer<XenObject>> GetAllObjects();

        /**
         * @brief Update or add object to cache
         * @param type Object type
         * @param ref Object reference
         * @param data Object data
         */
        void Update(const QString& type, const QString& ref, const QVariantMap& data);

        /**
         * @brief Update cache from bulk records (all_records response)
         * @param type Object type
         * @param allRecords Map of ref -> object data
         */
        void UpdateBulk(const QString& type, const QVariantMap& allRecords);

        /**
         * @brief Remove object from cache
         * @param type Object type
         * @param ref Object reference
         */
        void Remove(const QString& type, const QString& ref);

        /**
         * @brief Clear all objects of a specific type from cache
         * @param type Object type to clear
         */
        void ClearType(const QString& type);

        /**
         * @brief Clear entire cache
         */
        void Clear();

        /**
         * @brief Get number of objects of a type
         * @param type Object type
         * @return Count of objects
         */
        int Count(const QString& type) const;

        /**
         * @brief Check if cache is empty
         * @return true if cache has no objects
         */
        bool IsEmpty() const;

    signals:
        /**
         * @brief Emitted when an object is added or updated
         * @param type Object type
         * @param ref Object reference
         */
        void objectChanged(const QString& type, const QString& ref);

        /**
         * @brief Emitted when an object is removed
         * @param type Object type
         * @param ref Object reference
         */
        void objectRemoved(const QString& type, const QString& ref);

        /**
         * @brief Emitted when cache is cleared
         */
        void cacheCleared();

        /**
         * @brief Emitted when bulk update completes
         * @param type Object type that was updated
         * @param count Number of objects updated
         */
        void bulkUpdateComplete(const QString& type, int count);

    private:
        // Type -> (Ref -> ObjectData)
        mutable QMutex m_mutex;
        QMap<QString, QMap<QString, QVariantMap>> m_cache;
        QMap<QString, QMap<QString, QSharedPointer<XenObject>>> m_objects;
        QPointer<XenConnection> m_connection;

        QString normalizeType(const QString& type) const;
        QSharedPointer<XenObject> createObjectForType(const QString& type, const QString& ref);
        void refreshObject(const QString& type, const QString& ref);
        void evictObject(const QString& type, const QString& ref);
};

#endif // XENCACHE_H
