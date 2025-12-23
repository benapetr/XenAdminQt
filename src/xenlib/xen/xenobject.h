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

#ifndef XENOBJECT_H
#define XENOBJECT_H

#include "../xenlib_global.h"
#include <QObject>
#include <QString>
#include <QSharedPointer>
#include <QVariantMap>
#include <QPointer>
#include "network/connection.h"

/**
 * @brief Base class for all XenAPI objects (Pool, Host, VM, SR, etc.)
 *
 * Qt equivalent of C# XenObject<T> base class. Provides common properties
 * and methods for all XenServer objects. Unlike the C# version which has
 * thousands of auto-generated properties, this is a lightweight wrapper
 * around cached QVariantMap data.
 *
 * Design philosophy:
 * - Minimal memory overhead (stores ref + connection only)
 * - Lazy property access (reads from cache on demand)
 * - Derived classes add typed accessors for common properties
 * - Full data available via data() for uncommon properties
 */
class XENLIB_EXPORT XenObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString opaqueRef READ opaqueRef CONSTANT)
    Q_PROPERTY(QString uuid READ uuid NOTIFY dataChanged)
    Q_PROPERTY(QString nameLabel READ nameLabel NOTIFY dataChanged)
    Q_PROPERTY(QString nameDescription READ nameDescription NOTIFY dataChanged)

public:
    explicit XenObject(XenConnection* connection,
                       const QString& opaqueRef,
                       QObject* parent = nullptr);
    virtual ~XenObject();

    /**
     * @brief Get the XenAPI opaque reference for this object
     * @return Opaque reference (e.g., "OpaqueRef:12345678-...")
     */
    QString opaqueRef() const
    {
        return this->m_opaqueRef;
    }

    /**
     * @brief Get the object's UUID
     * @return UUID string
     */
    QString uuid() const;

    /**
     * @brief Get the object's human-readable name
     * @return Name label
     */
    QString nameLabel() const;

    /**
     * @brief Get the object's description
     * @return Name description
     */
    QString nameDescription() const;

    /**
     * @brief Get connection this object belongs to
     * @return XenConnection pointer (may be null if disconnected)
     */
    XenConnection* connection() const
    {
        return this->m_connection;
    }

    /**
     * @brief Get the object type string for cache lookups
     *
     * Must be overridden by derived classes to return the XenAPI type
     * (e.g., "vm", "host", "sr", "pool", "network")
     */
    virtual QString objectType() const = 0;

    /**
     * @brief Get all cached data for this object
     * @return QVariantMap with all XenAPI properties
     */
    QVariantMap data() const;

    /**
     * @brief Refresh object data from cache
     *
     * Call this after cache updates to emit dataChanged signal.
     * Derived classes should override to update specific properties.
     */
    virtual void refresh();

    /**
     * @brief Check if object exists in cache
     * @return true if object data is available
     */
    bool isValid() const;

    /**
     * @brief Mark object as evicted from cache
     *
     * Cache eviction should set this to true so consumers know the object is stale.
     */
    void setEvicted(bool evicted)
    {
        this->m_evicted = evicted;
    }

    /**
     * @brief Check if object was evicted from cache
     */
    bool isEvicted() const
    {
        return this->m_evicted;
    }

    signals:
        /**
         * @brief Emitted when object data changes
         */
        void dataChanged();

    protected:
        /**
         * @brief Get property value from cache
         * @param key Property key
         * @param defaultValue Default if property doesn't exist
         * @return Property value
         */
        QVariant property(const QString& key, const QVariant& defaultValue = QVariant()) const;

        /**
         * @brief Get typed property value
         */
        QString stringProperty(const QString& key, const QString& defaultValue = QString()) const;
        bool boolProperty(const QString& key, bool defaultValue = false) const;
        int intProperty(const QString& key, int defaultValue = 0) const;
        qint64 longProperty(const QString& key, qint64 defaultValue = 0) const;
        QStringList stringListProperty(const QString& key) const;

    private:
        QPointer<XenConnection> m_connection;
        QString m_opaqueRef;
        bool m_evicted = false;
};

Q_DECLARE_METATYPE(QSharedPointer<XenObject>)

#endif // XENOBJECT_H
