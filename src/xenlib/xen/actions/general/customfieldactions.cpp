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


#include "customfieldactions.h"
#include "../../../customfields/customfieldsmanager.h"
#include "../../../xencache.h"
#include "../../api.h"
#include "../../host.h"
#include "../../jsonrpcclient.h"
#include "../../network.h"
#include "../../network/connection.h"
#include "../../pool.h"
#include "../../session.h"
#include "../../sr.h"
#include "../../vdi.h"
#include "../../vm.h"
#include "../../xenapi/xenapi_Pool.h"
#include "../../xenobject.h"

namespace
{
    void call(XenAPI::Session* session, const QString& method, const QVariantList& params)
    {
        XenRpcAPI api(session);
        const QByteArray response = session->SendApiRequest(QString::fromUtf8(api.BuildJsonRpcCall(method, params)));
        if (response.isEmpty())
            throw std::runtime_error("Empty response from XenAPI");
        api.ParseJsonRpcResponse(response);
        const QString error = Xen::JsonRpcClient::lastError();
        if (!error.isEmpty())
            throw std::runtime_error(error.toStdString());
    }

    //! <class>.remove_from_other_config; a missing key is fine
    void removeFromOtherConfig(XenAPI::Session* session, const QString& apiClass, const QString& ref, const QString& key)
    {
        try
        {
            call(session, apiClass + ".remove_from_other_config", QVariantList{session->GetSessionID(), ref, key});
        } catch (const std::exception& e)
        {
            if (!QString::fromUtf8(e.what()).contains("MAP_NO_SUCH_KEY", Qt::CaseInsensitive))
                throw;
        }
    }

    //! C# Helpers.SetOtherConfig: remove, then add
    void setOtherConfig(XenAPI::Session* session, const QString& apiClass, const QString& ref, const QString& key, const QString& value)
    {
        removeFromOtherConfig(session, apiClass, ref, key);
        call(session, apiClass + ".add_to_other_config", QVariantList{session->GetSessionID(), ref, key, value});
    }

    QList<CustomFieldDefinition> poolDefinitions(XenConnection* connection)
    {
        // Read the pool's list directly: the manager's copy may lag behind a change just made
        QSharedPointer<Pool> pool = connection && connection->GetCache() ? connection->GetCache()->GetPoolOfOne() : QSharedPointer<Pool>();
        if (!pool)
            return QList<CustomFieldDefinition>();
        return CustomFieldsManager::DefinitionsFromXml(pool->GUIConfig().value(CustomFieldsManager::CUSTOM_FIELD_BASE_KEY).toString());
    }

    //! C# SaveCustomFields
    void saveDefinitions(XenAPI::Session* session, XenConnection* connection, const QList<CustomFieldDefinition>& definitions)
    {
        QSharedPointer<Pool> pool = connection && connection->GetCache() ? connection->GetCache()->GetPoolOfOne() : QSharedPointer<Pool>();
        if (!pool)
            throw std::runtime_error(QObject::tr("The server has no pool to save the custom fields on.").toStdString());

        XenAPI::Pool::remove_from_gui_config(session, pool->OpaqueRef(), CustomFieldsManager::CUSTOM_FIELD_BASE_KEY);
        XenAPI::Pool::add_to_gui_config(session, pool->OpaqueRef(), CustomFieldsManager::CUSTOM_FIELD_BASE_KEY,
                                        CustomFieldsManager::DefinitionsToXml(definitions));
    }

    template <typename T>
    void removeValueFromAll(XenAPI::Session* session, XenCache* cache, const QString& key)
    {
        for (const QSharedPointer<T>& object : cache->GetAll<T>())
        {
            if (object && object->GetOtherConfig().contains(key))
                removeFromOtherConfig(session, object->GetObjectTypeName(), object->OpaqueRef(), key);
        }
    }
}

AddCustomFieldAction::AddCustomFieldAction(XenConnection* connection, const CustomFieldDefinition& definition, QObject* parent)
    : AsyncOperation(connection, tr("Add custom field '%1'").arg(definition.GetName()),
                     tr("Adding custom field '%1'").arg(definition.GetName()), parent),
      m_definition(definition)
{
    this->AddApiMethodToRoleCheck("pool.add_to_gui_config");
    this->AddApiMethodToRoleCheck("pool.remove_from_gui_config");
}

void AddCustomFieldAction::run()
{
    try
    {
        QList<CustomFieldDefinition> definitions = poolDefinitions(this->GetConnection());
        if (!definitions.contains(this->m_definition))
        {
            definitions.append(this->m_definition);
            saveDefinitions(this->GetSession(), this->GetConnection(), definitions);
        }
        this->SetDescription(tr("Added custom field '%1'").arg(this->m_definition.GetName()));
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}

RemoveCustomFieldAction::RemoveCustomFieldAction(XenConnection* connection, const CustomFieldDefinition& definition, QObject* parent)
    : AsyncOperation(connection, tr("Delete custom field '%1'").arg(definition.GetName()),
                     tr("Deleting custom field '%1'").arg(definition.GetName()), parent),
      m_definition(definition)
{
    this->AddApiMethodToRoleCheck("pool.add_to_gui_config");
    this->AddApiMethodToRoleCheck("pool.remove_from_gui_config");
    for (const char* apiClass : {"pool", "host", "VM", "SR", "VDI", "network"})
        this->AddApiMethodToRoleCheck(QString("%1.remove_from_other_config").arg(apiClass));
}

void RemoveCustomFieldAction::run()
{
    try
    {
        QList<CustomFieldDefinition> definitions = poolDefinitions(this->GetConnection());
        if (definitions.removeAll(this->m_definition) > 0)
        {
            saveDefinitions(this->GetSession(), this->GetConnection(), definitions);

            // C#: the object types likely to have a value are those shown in the tree
            XenCache* cache = this->GetConnection()->GetCache();
            const QString key = CustomFieldsManager::getCustomFieldKey(this->m_definition);
            removeValueFromAll<Pool>(this->GetSession(), cache, key);
            removeValueFromAll<Host>(this->GetSession(), cache, key);
            removeValueFromAll<VM>(this->GetSession(), cache, key);
            removeValueFromAll<SR>(this->GetSession(), cache, key);
            removeValueFromAll<VDI>(this->GetSession(), cache, key);
            removeValueFromAll<Network>(this->GetSession(), cache, key);
        }
        this->SetDescription(tr("Deleted custom field '%1'").arg(this->m_definition.GetName()));
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}

SaveCustomFieldsAction::SaveCustomFieldsAction(QSharedPointer<XenObject> object, const QList<FieldValue>& values, QObject* parent)
    : AsyncOperation(object ? object->GetConnection() : nullptr, tr("Save Custom Fields"),
                     tr("Saving Custom Fields for '%1'...").arg(object ? object->GetName() : QString()), parent),
      m_object(object),
      m_values(values)
{
    if (object)
    {
        const QString apiClass = object->GetObjectTypeName();
        this->AddApiMethodToRoleCheck(apiClass + ".remove_from_other_config");
        this->AddApiMethodToRoleCheck(apiClass + ".add_to_other_config");
    }
}

void SaveCustomFieldsAction::run()
{
    try
    {
        if (!this->m_object)
            return;

        const QString apiClass = this->m_object->GetObjectTypeName();
        for (const FieldValue& field : std::as_const(this->m_values))
        {
            const QString key = CustomFieldsManager::getCustomFieldKey(field.first);
            const QString value = CustomFieldsManager::ValueToInvariantString(field.first.GetType(), field.second);
            if (value.isEmpty())
                removeFromOtherConfig(this->GetSession(), apiClass, this->m_object->OpaqueRef(), key);
            else
                setOtherConfig(this->GetSession(), apiClass, this->m_object->OpaqueRef(), key, value);
        }
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}
