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

#include "perfmondefinitionaction.h"
#include "../../xenobject.h"
#include "../../host.h"
#include "../../vm.h"
#include "../../sr.h"
#include "../../pbd.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../api.h"
#include "../../jsonrpcclient.h"
#include "../../xenapi/xenapi_Host.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../xenapi/xenapi_SR.h"
#include "../../../xencache.h"
#include <QDomDocument>
#include <QSet>
#include <QXmlStreamWriter>
#include <QDebug>
#include <stdexcept>

PerfmonDefinitionAction::PerfmonDefinitionAction(XenConnection* connection,
                                                 const QString& objectRef,
                                                 const QString& objectType,
                                                 const QList<Definition>& definitions,
                                                 bool suppressHistory,
                                                 QObject* parent)
    : AsyncOperation(connection,
                     QObject::tr("Update Performance Alerts"),
                     QObject::tr("Updating performance alert configuration..."),
                     suppressHistory,
                     parent),
      m_objectRef(objectRef),
      m_objectType(objectType.trimmed().toLower()),
      m_definitions(definitions)
{
    if (!this->m_objectType.isEmpty())
        this->AddApiMethodToRoleCheck(QString("%1.set_other_config").arg(this->m_objectType));
}

void PerfmonDefinitionAction::run()
{
    if (this->m_objectRef.isEmpty() || this->m_objectType.isEmpty())
    {
        this->setError("Invalid target object for perfmon update.");
        return;
    }

    this->SetPercentComplete(5);

    QSharedPointer<XenObject> targetObject = this->resolveTargetObject();
    QList<Definition> remainingDefinitions = this->m_definitions;

    // Dom0 memory alert is stored in control-domain VM other_config, not on host.
    if (this->m_objectType == QStringLiteral("host"))
    {
        QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(targetObject);
        if (host && host->IsValid())
        {
            const QSharedPointer<VM> dom0 = host->ControlDomainZero();
            if (dom0 && dom0->IsValid())
            {
                this->SetPercentComplete(15);

                bool dom0DefinitionProvided = false;
                Definition dom0Definition;
                for (int i = remainingDefinitions.size() - 1; i >= 0; --i)
                {
                    if (remainingDefinitions.at(i).name == QLatin1String(PERFMON_DOM0_MEMORY))
                    {
                        dom0Definition = remainingDefinitions.at(i);
                        remainingDefinitions.removeAt(i);
                        dom0DefinitionProvided = true;
                        break;
                    }
                }

                QList<Definition> dom0Definitions = this->parseDefinitions(dom0->GetOtherConfig().value(PERFMON_KEY).toString());
                bool replaced = false;

                for (int i = dom0Definitions.size() - 1; i >= 0; --i)
                {
                    if (dom0Definitions.at(i).name == QLatin1String(PERFMON_DOM0_MEMORY))
                    {
                        if (dom0DefinitionProvided)
                        {
                            dom0Definitions[i] = dom0Definition;
                            replaced = true;
                        } else
                        {
                            dom0Definitions.removeAt(i);
                        }
                    }
                }

                if (dom0DefinitionProvided && !replaced)
                    dom0Definitions.append(dom0Definition);

                this->applyDefinitionsToObject(QStringLiteral("vm"), dom0->OpaqueRef(), dom0Definitions);
            }
        }
    }

    this->SetPercentComplete(45);
    this->applyDefinitionsToObject(this->m_objectType, this->m_objectRef, remainingDefinitions);

    this->SetPercentComplete(70);
    this->SetDescription(QObject::tr("Refreshing performance monitor plugin..."));
    this->refreshPerfmonOnHosts(this->collectRefreshHosts(targetObject));

    this->SetPercentComplete(100);
}

void PerfmonDefinitionAction::applyDefinitionsToObject(const QString& objectType,
                                                       const QString& objectRef,
                                                       const QList<Definition>& definitions)
{
    if (definitions.isEmpty())
    {
        this->removeFromOtherConfig(objectType, objectRef, QLatin1String(PERFMON_KEY));
        return;
    }

    const QString perfmonXml = this->buildDefinitionsXml(definitions);
    this->setOtherConfigKey(objectType, objectRef, QLatin1String(PERFMON_KEY), perfmonXml);
}

void PerfmonDefinitionAction::setOtherConfigKey(const QString& objectType,
                                                const QString& objectRef,
                                                const QString& key,
                                                const QString& value)
{
    QVariantMap otherConfig = this->getCurrentOtherConfig(objectType, objectRef);
    otherConfig.insert(key, value);
    this->setOtherConfig(objectType, objectRef, otherConfig);
}

void PerfmonDefinitionAction::removeFromOtherConfig(const QString& objectType,
                                                    const QString& objectRef,
                                                    const QString& key)
{
    QVariantMap otherConfig = this->getCurrentOtherConfig(objectType, objectRef);
    otherConfig.remove(key);
    this->setOtherConfig(objectType, objectRef, otherConfig);
}

void PerfmonDefinitionAction::callApiVoid(const QString& method, const QVariantList& params)
{
    XenRpcAPI api(this->GetSession());
    const QByteArray request = api.BuildJsonRpcCall(method, params);
    const QByteArray response = this->GetSession()->SendApiRequest(request);
    api.ParseJsonRpcResponse(response);

    const QString rpcError = Xen::JsonRpcClient::lastError();
    if (!rpcError.isEmpty())
        throw std::runtime_error(rpcError.toStdString());
}

QVariantMap PerfmonDefinitionAction::getCurrentOtherConfig(const QString& objectType, const QString& objectRef) const
{
    if (this->m_connection && this->m_connection->GetCache())
        return this->m_connection->GetCache()->ResolveObjectData(objectType, objectRef).value("other_config").toMap();
    return QVariantMap();
}

void PerfmonDefinitionAction::setOtherConfig(const QString& objectType, const QString& objectRef, const QVariantMap& otherConfig)
{
    if (objectType == QStringLiteral("vm"))
    {
        XenAPI::VM::set_other_config(this->GetSession(), objectRef, otherConfig);
        return;
    }
    if (objectType == QStringLiteral("host"))
    {
        XenAPI::Host::set_other_config(this->GetSession(), objectRef, otherConfig);
        return;
    }
    if (objectType == QStringLiteral("sr"))
    {
        XenAPI::SR::set_other_config(this->GetSession(), objectRef, otherConfig);
        return;
    }

    QVariantList params;
    params << this->GetSession()->GetSessionID() << objectRef << otherConfig;
    this->callApiVoid(QString("%1.set_other_config").arg(objectType), params);
}

QList<PerfmonDefinitionAction::Definition> PerfmonDefinitionAction::parseDefinitions(const QString& perfmonXml) const
{
    QList<Definition> definitions;
    if (perfmonXml.trimmed().isEmpty())
        return definitions;

    QDomDocument doc;
    if (!doc.setContent(perfmonXml))
        return definitions;

    const QDomElement root = doc.documentElement();
    if (root.tagName() != QLatin1String("config"))
        return definitions;

    for (QDomElement var = root.firstChildElement(QStringLiteral("variable"));
         !var.isNull();
         var = var.nextSiblingElement(QStringLiteral("variable")))
    {
        const QDomElement nameEl = var.firstChildElement(QStringLiteral("name"));
        if (nameEl.isNull())
            continue;

        Definition def;
        def.name = nameEl.attribute(QStringLiteral("value"));
        if (def.name.isEmpty())
            continue;

        def.threshold = var.firstChildElement(QStringLiteral("alarm_trigger_level")).attribute(QStringLiteral("value")).toDouble();
        def.durationSeconds = var.firstChildElement(QStringLiteral("alarm_trigger_period")).attribute(QStringLiteral("value")).toInt();
        def.intervalSeconds = var.firstChildElement(QStringLiteral("alarm_auto_inhibit_period")).attribute(QStringLiteral("value")).toInt();

        if (def.durationSeconds <= 0)
            def.durationSeconds = 300;
        if (def.intervalSeconds <= 0)
            def.intervalSeconds = def.durationSeconds;

        definitions.append(def);
    }

    return definitions;
}

QString PerfmonDefinitionAction::buildDefinitionsXml(const QList<Definition>& definitions) const
{
    QString xml;
    QXmlStreamWriter writer(&xml);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    writer.writeStartElement(QStringLiteral("config"));

    for (const Definition& def : definitions)
    {
        if (def.name.isEmpty())
            continue;

        writer.writeStartElement(QStringLiteral("variable"));

        writer.writeStartElement(QStringLiteral("name"));
        writer.writeAttribute(QStringLiteral("value"), def.name);
        writer.writeEndElement();

        writer.writeStartElement(QStringLiteral("alarm_trigger_level"));
        writer.writeAttribute(QStringLiteral("value"), QString::number(def.threshold, 'f', 10));
        writer.writeEndElement();

        writer.writeStartElement(QStringLiteral("alarm_trigger_period"));
        writer.writeAttribute(QStringLiteral("value"), QString::number(def.durationSeconds));
        writer.writeEndElement();

        writer.writeStartElement(QStringLiteral("alarm_auto_inhibit_period"));
        writer.writeAttribute(QStringLiteral("value"), QString::number(def.intervalSeconds));
        writer.writeEndElement();

        writer.writeEndElement();
    }

    writer.writeEndElement();
    writer.writeEndDocument();
    return xml;
}

QSharedPointer<XenObject> PerfmonDefinitionAction::resolveTargetObject() const
{
    XenCache* cache = this->m_connection ? this->m_connection->GetCache() : nullptr;
    if (!cache)
        return QSharedPointer<XenObject>();
    return cache->ResolveObject(this->m_objectType, this->m_objectRef);
}

QList<QSharedPointer<Host>> PerfmonDefinitionAction::collectRefreshHosts(const QSharedPointer<XenObject>& object) const
{
    QList<QSharedPointer<Host>> hosts;
    QSet<QString> seen;

    auto appendHost = [&hosts, &seen](const QSharedPointer<Host>& host) {
        if (!host || !host->IsValid() || host->OpaqueRef().isEmpty())
            return;
        if (seen.contains(host->OpaqueRef()))
            return;
        seen.insert(host->OpaqueRef());
        hosts.append(host);
    };

    if (QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(object))
    {
        appendHost(host);
        return hosts;
    }

    if (QSharedPointer<VM> vm = qSharedPointerDynamicCast<VM>(object))
    {
        appendHost(vm->GetHome());
        return hosts;
    }

    if (QSharedPointer<SR> sr = qSharedPointerDynamicCast<SR>(object))
    {
        const QList<QSharedPointer<PBD>> pbds = sr->GetPBDs();
        for (const QSharedPointer<PBD>& pbd : pbds)
            appendHost(pbd ? pbd->GetHost() : QSharedPointer<Host>());
    }

    return hosts;
}

void PerfmonDefinitionAction::refreshPerfmonOnHosts(const QList<QSharedPointer<Host>>& hosts)
{
    for (const QSharedPointer<Host>& host : hosts)
    {
        if (this->IsCancelled())
            return;

        try
        {
            XenAPI::Host::call_plugin(this->GetSession(),
                                      host->OpaqueRef(),
                                      QLatin1String(PERFMON_PLUGIN),
                                      QLatin1String(PERFMON_PLUGIN_REFRESH),
                                      QVariantMap());
        } catch (const std::exception& ex)
        {
            const QString error = QString::fromUtf8(ex.what());
            if (!error.startsWith(QLatin1String(PERFMON_NOT_RUNNING_ERROR)))
            {
                qDebug() << "Perfmon refresh failed for host" << host->GetName()
                         << "- alerts will update later:" << error;
                continue;
            }

            try
            {
                XenAPI::Host::call_plugin(this->GetSession(),
                                          host->OpaqueRef(),
                                          QLatin1String(PERFMON_PLUGIN),
                                          QLatin1String(PERFMON_PLUGIN_START),
                                          QVariantMap());

                XenAPI::Host::call_plugin(this->GetSession(),
                                          host->OpaqueRef(),
                                          QLatin1String(PERFMON_PLUGIN),
                                          QLatin1String(PERFMON_PLUGIN_REFRESH),
                                          QVariantMap());
            } catch (const std::exception& retryEx)
            {
                qDebug() << "Perfmon start/refresh failed for host" << host->GetName()
                         << "- alerts will update later:" << retryEx.what();
            }
        }
    }
}
