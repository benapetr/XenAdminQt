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

#include "customfieldsmanager.h"
#include "../xen/network/connection.h"
#include "../xen/network/connectionsmanager.h"
#include "../xen/pool.h"
#include "../xencache.h"
#include "../otherconfig/otherconfigandtagswatcher.h"
#include <QXmlStreamReader>
#include <QMutexLocker>
#include <QDebug>

CustomFieldsManager* CustomFieldsManager::m_instance = nullptr;

const QString CustomFieldsManager::CUSTOM_FIELD_BASE_KEY = "XenCenter.CustomFields";
const QString CustomFieldsManager::CUSTOM_FIELD_DELIM = ".";

CustomFieldsManager::CustomFieldsManager(QObject* parent) : QObject(parent)
{
    QObject::connect(OtherConfigAndTagsWatcher::instance(), &OtherConfigAndTagsWatcher::GuiConfigChanged, this, &CustomFieldsManager::onGuiConfigChanged);
    this->recalculateCustomFields();
}

CustomFieldsManager::~CustomFieldsManager()
{
}

CustomFieldsManager* CustomFieldsManager::instance()
{
    if (!m_instance)
        m_instance = new CustomFieldsManager();
    return m_instance;
}

QList<CustomFieldDefinition> CustomFieldsManager::getCustomFields() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_allCustomFields;
}

QList<CustomFieldDefinition> CustomFieldsManager::getCustomFields(XenConnection* connection) const
{
    if (!connection)
    {
        return QList<CustomFieldDefinition>();
    }

    QMutexLocker locker(&this->m_mutex);
    if (this->m_customFieldsPerConnection.contains(connection))
    {
        return this->m_customFieldsPerConnection[connection];
    }

    return QList<CustomFieldDefinition>();
}

CustomFieldDefinition* CustomFieldsManager::getCustomFieldDefinition(const QString& name) const
{
    QMutexLocker locker(&this->m_mutex);
    for (const CustomFieldDefinition& def : this->m_allCustomFields)
    {
        if (def.GetName() == name)
        {
            return new CustomFieldDefinition(def);
        }
    }
    return nullptr;
}

QString CustomFieldsManager::getCustomFieldKey(const CustomFieldDefinition& definition)
{
    return CUSTOM_FIELD_BASE_KEY + CUSTOM_FIELD_DELIM + definition.GetName();
}

bool CustomFieldsManager::hasCustomFields(const QVariantMap& otherConfig, XenConnection* connection)
{
    if (!connection)
    {
        return false;
    }

    QList<CustomFieldDefinition> fields = instance()->getCustomFields(connection);
    for (const CustomFieldDefinition& def : fields)
    {
        QString key = getCustomFieldKey(def);
        if (otherConfig.contains(key) && !otherConfig[key].toString().isEmpty())
        {
            return true;
        }
    }

    return false;
}

void CustomFieldsManager::onGuiConfigChanged()
{
    this->recalculateCustomFields();
    emit customFieldsChanged();
}

void CustomFieldsManager::recalculateCustomFields()
{
    QMutexLocker locker(&this->m_mutex);

    this->m_customFieldsPerConnection.clear();
    this->m_allCustomFields.clear();

    Xen::ConnectionsManager* manager = Xen::ConnectionsManager::instance();
    if (!manager)
        return;

    const QList<XenConnection*> connections = manager->GetAllConnections();
    for (XenConnection* connection : connections)
    {
        if (!connection || !connection->IsConnected())
            continue;

        const QList<CustomFieldDefinition> fields = this->getCustomFieldsFromGuiConfig(connection);
        this->m_customFieldsPerConnection.insert(connection, fields);
        for (const CustomFieldDefinition& field : fields)
        {
            if (!this->m_allCustomFields.contains(field))
                this->m_allCustomFields.append(field);
        }
    }
}

QList<CustomFieldDefinition> CustomFieldsManager::getCustomFieldsFromGuiConfig(XenConnection* connection) const
{
    if (!connection || !connection->IsConnected())
    {
        return QList<CustomFieldDefinition>();
    }

    XenCache* cache = connection->GetCache();
    if (!cache)
    {
        return QList<CustomFieldDefinition>();
    }

    // Get pool (always first pool in cache)
    // C# uses Helpers.GetPoolOfOne(connection) which gets the first pool
    QSharedPointer<Pool> pool = cache->GetPoolOfOne();
    if (!pool || !pool->IsValid())
    {
        return QList<CustomFieldDefinition>();
    }

    // Get gui_config from pool
    QVariantMap guiConfig = pool->GUIConfig();
    if (!guiConfig.contains(CUSTOM_FIELD_BASE_KEY))
    {
        return QList<CustomFieldDefinition>();
    }

    QString customFieldsXml = guiConfig[CUSTOM_FIELD_BASE_KEY].toString().trimmed();
    if (customFieldsXml.isEmpty())
    {
        return QList<CustomFieldDefinition>();
    }

    return this->parseCustomFieldDefinitions(customFieldsXml);
}

QList<CustomFieldDefinition> CustomFieldsManager::parseCustomFieldDefinitions(const QString& xml) const
{
    QList<CustomFieldDefinition> definitions;

    QXmlStreamReader reader(xml);

    // Read root element (CustomFieldDefinitions)
    if (!reader.readNextStartElement())
    {
        qWarning() << "CustomFieldsManager: Invalid XML (no root element)";
        return definitions;
    }

    // Read each CustomFieldDefinition
    while (reader.readNextStartElement())
    {
        if (reader.name() == CustomFieldDefinition::TAG_NAME)
        {
            try
            {
                definitions.append(CustomFieldDefinition::FromXml(reader));
            }
            catch (const std::exception& e)
            {
                qWarning() << "CustomFieldsManager: Failed to parse custom field definition:" << e.what();
                reader.skipCurrentElement();
            }
        }
        else
        {
            reader.skipCurrentElement();
        }
    }

    if (reader.hasError())
    {
        qWarning() << "CustomFieldsManager: XML parse error:" << reader.errorString();
    }

    return definitions;
}
