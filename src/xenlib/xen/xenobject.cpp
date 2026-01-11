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

bool XenObject::ValueIsNULL(const QString &value)
{
    return value.isEmpty() || value == "OpaqueRef:NULL";
}

XenObject::XenObject(XenConnection* connection, const QString& opaqueRef, QObject* parent) : QObject(parent), m_connection(connection), m_opaqueRef(opaqueRef)
{
    if (connection)
        this->m_cache = connection->GetCache();
    else
        this->m_cache = XenCache::GetDummy();
}

XenObject::~XenObject()
{
}

QString XenObject::GetUUID() const
{
    return this->stringProperty("uuid");
}

QString XenObject::GetName() const
{
    return this->stringProperty("name_label");
}

QString XenObject::GetDescription() const
{
    return this->stringProperty("name_description");
}

QStringList XenObject::GetTags() const
{
    return this->stringListProperty("tags");
}

bool XenObject::IsLocked() const
{
    return this->m_locked;;
}

void XenObject::Lock()
{
    this->m_locked = true;
}

void XenObject::Unlock()
{
    this->m_locked = false;
}

bool XenObject::IsConnected() const
{
    if (!this->m_connection)
        return false;
    return this->m_connection->IsConnected();
}

QVariantMap XenObject::GetOtherConfig() const
{
    return this->property("other_config").toMap();
}

bool XenObject::IsHidden() const
{
    QVariantMap otherConfig = this->GetOtherConfig();
    QVariant hiddenValue = otherConfig.value("HideFromXenCenter");
    if (!hiddenValue.isValid())
        hiddenValue = otherConfig.value("hide_from_xencenter");

    const QString hidden = hiddenValue.toString().trimmed().toLower();
    return hidden == "true" || hidden == "1";
}

QString XenObject::GetObjectType() const
{
    return "null";
}

QVariantMap XenObject::GetData() const
{
    if (!this->m_cache || this->m_opaqueRef.isEmpty())
        return QVariantMap();

    return this->m_cache->ResolveObjectData(this->GetObjectType(), this->m_opaqueRef);
}

void XenObject::Refresh()
{
    emit dataChanged();
}

bool XenObject::IsValid() const
{
    return !this->GetData().isEmpty();
}

QVariant XenObject::property(const QString& key, const QVariant& defaultValue) const
{
    QVariantMap d = this->GetData();
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
