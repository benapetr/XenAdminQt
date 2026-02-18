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

#include "savechangesaction.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../xenapi/xenapi_Host.h"
#include "../../xenapi/xenapi_Pool.h"
#include "../../xenapi/xenapi_SR.h"
#include "../../xenapi/xenapi_Network.h"
#include "../../xenapi/xenapi_VDI.h"
#include "xenlib/xen/jsonrpcclient.h"
#include "xenlib/xen/session.h"
#include "xenlib/xen/xenobject.h"
#include <QtGlobal>
#include <QSet>
#include <stdexcept>

SaveChangesAction::SaveChangesAction(QSharedPointer<XenObject> object,
                                     const QVariantMap& objectDataBefore,
                                     const QVariantMap& objectDataCopy,
                                     bool suppressHistory,
                                     QObject* parent)
    : AsyncOperation(object ? object->GetConnection() : nullptr,
                     QObject::tr("Save Changes"),
                     QObject::tr("Saving properties..."),
                     suppressHistory,
                     parent),
      m_object(object),
      m_objectDataBefore(objectDataBefore),
      m_objectDataCopy(objectDataCopy)
{
    if (!this->m_object)
        throw std::invalid_argument("SaveChangesAction requires a valid object");

    this->m_objectRef = this->m_object->OpaqueRef();
    this->m_objectType = this->m_object->GetObjectType();
}

void SaveChangesAction::throwIfJsonError(const QString& context) const
{
    const QString error = Xen::JsonRpcClient::lastError();
    if (error.isEmpty())
        return;

    throw std::runtime_error(QString("%1 failed: %2").arg(context, error).toStdString());
}

void SaveChangesAction::run()
{
    XenAPI::Session* session = this->GetSession();
    if (!session || !session->IsLoggedIn())
    {
        this->setError(QObject::tr("Failed to save changes: no active session"));
        return;
    }

    try
    {
        this->SetPercentComplete(5);
        this->SetDescription(QObject::tr("Applying simple property changes..."));

        // 1. name_label
        const QString oldName = this->m_objectDataBefore.value("name_label").toString();
        const QString newName = this->m_objectDataCopy.value("name_label").toString();
        if (oldName != newName && !newName.isEmpty())
        {
            if (this->m_objectType == XenObjectType::VM)
            {
                XenAPI::VM::set_name_label(session, this->m_objectRef, newName);
                this->throwIfJsonError("VM.set_name_label");
            } else if (this->m_objectType == XenObjectType::Host)
            {
                XenAPI::Host::set_name_label(session, this->m_objectRef, newName);
                this->throwIfJsonError("Host.set_name_label");
            } else if (this->m_objectType == XenObjectType::Pool)
            {
                XenAPI::Pool::set_name_label(session, this->m_objectRef, newName);
                this->throwIfJsonError("Pool.set_name_label");
            } else if (this->m_objectType == XenObjectType::SR)
            {
                XenAPI::SR::set_name_label(session, this->m_objectRef, newName);
                this->throwIfJsonError("SR.set_name_label");
            } else if (this->m_objectType == XenObjectType::Network)
            {
                XenAPI::Network::set_name_label(session, this->m_objectRef, newName);
                this->throwIfJsonError("Network.set_name_label");
            } else if (this->m_objectType == XenObjectType::VDI)
            {
                XenAPI::VDI::set_name_label(session, this->m_objectRef, newName);
                this->throwIfJsonError("VDI.set_name_label");
            }
        }

        this->SetPercentComplete(20);

        // 2. name_description
        const QString oldDesc = this->m_objectDataBefore.value("name_description").toString();
        const QString newDesc = this->m_objectDataCopy.value("name_description").toString();
        if (oldDesc != newDesc)
        {
            if (this->m_objectType == XenObjectType::VM)
            {
                XenAPI::VM::set_name_description(session, this->m_objectRef, newDesc);
                this->throwIfJsonError("VM.set_name_description");
            } else if (this->m_objectType == XenObjectType::Host)
            {
                XenAPI::Host::set_name_description(session, this->m_objectRef, newDesc);
                this->throwIfJsonError("Host.set_name_description");
            } else if (this->m_objectType == XenObjectType::Pool)
            {
                XenAPI::Pool::set_name_description(session, this->m_objectRef, newDesc);
                this->throwIfJsonError("Pool.set_name_description");
            } else if (this->m_objectType == XenObjectType::SR)
            {
                XenAPI::SR::set_name_description(session, this->m_objectRef, newDesc);
                this->throwIfJsonError("SR.set_name_description");
            } else if (this->m_objectType == XenObjectType::Network)
            {
                XenAPI::Network::set_name_description(session, this->m_objectRef, newDesc);
                this->throwIfJsonError("Network.set_name_description");
            } else if (this->m_objectType == XenObjectType::VDI)
            {
                XenAPI::VDI::set_name_description(session, this->m_objectRef, newDesc);
                this->throwIfJsonError("VDI.set_name_description");
            }
        }

        this->SetPercentComplete(35);

        // 3. structured config maps
        const QVariantMap oldOtherConfig = this->m_objectDataBefore.value("other_config").toMap();
        const QVariantMap newOtherConfig = this->m_objectDataCopy.value("other_config").toMap();
        const QVariantMap oldLogging = this->m_objectDataBefore.value("logging").toMap();
        const QVariantMap newLogging = this->m_objectDataCopy.value("logging").toMap();

        if (this->m_objectType == XenObjectType::VM)
        {
            if (oldOtherConfig != newOtherConfig)
            {
                XenAPI::VM::set_other_config(session, this->m_objectRef, newOtherConfig);
                this->throwIfJsonError("VM.set_other_config");
            }

            const QVariantMap oldVcpusParams = this->m_objectDataBefore.value("VCPUs_params").toMap();
            const QVariantMap newVcpusParams = this->m_objectDataCopy.value("VCPUs_params").toMap();
            if (oldVcpusParams != newVcpusParams)
            {
                XenAPI::VM::set_VCPUs_params(session, this->m_objectRef, newVcpusParams);
                this->throwIfJsonError("VM.set_VCPUs_params");
            }

            const QVariantMap oldPlatform = this->m_objectDataBefore.value("platform").toMap();
            const QVariantMap newPlatform = this->m_objectDataCopy.value("platform").toMap();
            if (oldPlatform != newPlatform)
            {
                XenAPI::VM::set_platform(session, this->m_objectRef, newPlatform);
                this->throwIfJsonError("VM.set_platform");
            }
        } else if (this->m_objectType == XenObjectType::Network)
        {
            if (oldOtherConfig != newOtherConfig)
            {
                XenAPI::Network::set_other_config(session, this->m_objectRef, newOtherConfig);
                this->throwIfJsonError("Network.set_other_config");
            }
        }

        this->SetPercentComplete(55);

        // 4. host logging
        if (this->m_objectType == XenObjectType::Host && oldLogging != newLogging)
        {
            XenAPI::Host::set_logging(session, this->m_objectRef, newLogging);
            this->throwIfJsonError("Host.set_logging");
        }

        // 5. VM shadow multiplier
        if (this->m_objectType == XenObjectType::VM && this->m_objectDataCopy.contains("HVM_shadow_multiplier"))
        {
            const double oldMultiplier = this->m_objectDataBefore.value("HVM_shadow_multiplier").toDouble();
            const double newMultiplier = this->m_objectDataCopy.value("HVM_shadow_multiplier").toDouble();
            if (qAbs(oldMultiplier - newMultiplier) > 0.0001)
            {
                XenAPI::VM::set_HVM_shadow_multiplier(session, this->m_objectRef, newMultiplier);
                this->throwIfJsonError("VM.set_HVM_shadow_multiplier");
            }
        } else
        {
            // Keep existing behavior for host other_config key-level updates.
            const QList<QString> oldKeys = oldOtherConfig.keys();
            const QList<QString> newKeys = newOtherConfig.keys();
            QSet<QString> allKeys(oldKeys.begin(), oldKeys.end());
            allKeys.unite(QSet<QString>(newKeys.begin(), newKeys.end()));

            for (const QString& key : allKeys)
            {
                const QString oldValue = oldOtherConfig.value(key).toString();
                const QString newValue = newOtherConfig.value(key).toString();

                if (oldValue == newValue)
                    continue;

                if (newValue.isEmpty() && oldOtherConfig.contains(key))
                    continue;

                if (this->m_objectType == XenObjectType::Host)
                {
                    QVariantMap otherConfig = this->m_objectDataCopy.value("other_config").toMap();
                    if (newValue.isEmpty())
                        otherConfig.remove(key);
                    else
                        otherConfig[key] = newValue;

                    XenAPI::Host::set_other_config(session, this->m_objectRef, otherConfig);
                    this->throwIfJsonError("Host.set_other_config");
                }
            }
        }

        this->SetPercentComplete(100);
        this->SetDescription(QObject::tr("Simple property changes saved"));
    } catch (const std::exception& ex)
    {
        this->setError(QString("Failed to save changes: %1").arg(ex.what()));
    }
}
