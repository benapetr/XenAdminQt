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

#include "xenobject.h"
#include "network/connection.h"
#include "../xencache.h"

XenObject::XenObject(XenConnection* connection, const QString& opaqueRef, QObject* parent) : QObject(parent), m_connection(connection), m_opaqueRef(opaqueRef)
{
    if (connection)
        this->m_cache = connection->GetCache();
    else
        this->m_cache = nullptr;
}

XenObject::~XenObject()
{
}

QString XenObject::GetUUID() const
{
    return stringProperty("uuid");
}

QString XenObject::GetName() const
{
    return stringProperty("name_label");
}

QString XenObject::GetDescription() const
{
    return stringProperty("name_description");
}

QStringList XenObject::GetTags() const
{
    return stringListProperty("tags");
}

bool XenObject::IsLocked() const
{
    return boolProperty("locked", false);
}

bool XenObject::IsConnected() const
{
    if (!this->m_connection)
        return false;
    return this->m_connection->IsConnected();
}

QVariantMap XenObject::GetData() const
{
    if (!this->m_connection || this->m_opaqueRef.isEmpty())
        return QVariantMap();

    // Get cache from connection's XenLib
    if (!this->m_connection->GetCache())
        return QVariantMap();

    return this->m_connection->GetCache()->ResolveObjectData(GetObjectType(), this->m_opaqueRef);
}

void XenObject::Refresh()
{
    emit dataChanged();
}

bool XenObject::IsValid() const
{
    return !GetData().isEmpty();
}

QVariant XenObject::property(const QString& key, const QVariant& defaultValue) const
{
    QVariantMap d = GetData();
    return d.value(key, defaultValue);
}

QString XenObject::stringProperty(const QString& key, const QString& defaultValue) const
{
    return property(key, defaultValue).toString();
}

bool XenObject::boolProperty(const QString& key, bool defaultValue) const
{
    return property(key, defaultValue).toBool();
}

int XenObject::intProperty(const QString& key, int defaultValue) const
{
    return property(key, defaultValue).toInt();
}

qint64 XenObject::longProperty(const QString& key, qint64 defaultValue) const
{
    return property(key, defaultValue).toLongLong();
}

QStringList XenObject::stringListProperty(const QString& key) const
{
    QVariant value = property(key);
    if (value.canConvert<QStringList>())
        return value.toStringList();

    // Handle QVariantList (from JSON arrays)
    if (value.canConvert<QVariantList>())
    {
        QStringList result;
        QVariantList list = value.toList();
        for (const QVariant& item : list)
            result << item.toString();
        return result;
    }

    return QStringList();
}
