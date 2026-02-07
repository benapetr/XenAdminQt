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

#include "xenapi_Host.h"
#include "../session.h"
#include "../api.h"
#include "../failure.h"
#include "../jsonrpcclient.h"
#include <stdexcept>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace XenAPI
{
    namespace
    {
        void maybeThrowFailureFromResponse(const QByteArray& response)
        {
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(response, &parseError);
            if (parseError.error != QJsonParseError::NoError || !doc.isObject())
                return;

            QJsonObject root = doc.object();

            auto throwFromArray = [](const QJsonArray& array)
            {
                QStringList errors;
                for (const QJsonValue& val : array)
                    errors << val.toString();
                if (!errors.isEmpty())
                    throw Failure(errors);
            };

            if (root.contains("result"))
            {
                QJsonValue resultVal = root.value("result");
                if (resultVal.isObject())
                {
                    QJsonObject resultObj = resultVal.toObject();
                    if (resultObj.value("Status").toString() == "Failure")
                    {
                        QJsonValue errorDesc = resultObj.value("ErrorDescription");
                        if (errorDesc.isArray())
                            throwFromArray(errorDesc.toArray());
                    }
                }
            }

            if (root.contains("error"))
            {
                QJsonObject errorObj = root.value("error").toObject();
                QStringList errors;
                QString message = errorObj.value("message").toString();
                errors << message;
                QJsonValue dataVal = errorObj.value("data");
                if (dataVal.isArray())
                {
                    QJsonArray dataArray = dataVal.toArray();
                    for (const QJsonValue& val : dataArray)
                        errors << val.toString();
                }
                if (!errors.isEmpty())
                    throw Failure(errors);
            }
        }
    }

    QVariantList Host::get_all(Session* session)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID();

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("host.get_all", params);
        QByteArray response = session->SendApiRequest(request);

        QVariant result = api.ParseJsonRpcResponse(response);

        // Result should be a list of host refs
        if (result.canConvert<QVariantList>())
        {
            return result.toList();
        }

        return QVariantList();
    }

    QVariantMap Host::get_record(Session* session, const QString& host)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("host.get_record", params);
        QByteArray response = session->SendApiRequest(request);

        QVariant result = api.ParseJsonRpcResponse(response);
        if (result.canConvert<QVariantMap>())
        {
            return result.toMap();
        }

        return QVariantMap();
    }

    QVariant Host::get_servertime(Session* session, const QString& host)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("host.get_servertime", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response);
    }

    double Host::query_data_source(Session* session, const QString& host, const QString& data_source)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host << data_source;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("host.query_data_source", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toDouble();
    }

    QList<QVariantMap> Host::get_data_sources(Session* session, const QString& host)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("host.get_data_sources", params);
        QByteArray response = session->SendApiRequest(request);
        QVariant result = api.ParseJsonRpcResponse(response);
        const QString parseError = Xen::JsonRpcClient::lastError();
        if (!parseError.isEmpty())
            throw std::runtime_error(parseError.toStdString());

        if (!result.canConvert<QVariantList>())
            throw std::runtime_error("Unexpected response type for host.get_data_sources");

        QList<QVariantMap> dataSources;
        for (const QVariant& item : result.toList())
        {
            dataSources.append(item.toMap());
        }
        return dataSources;
    }

    void Host::record_data_source(Session* session, const QString& host, const QString& data_source)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host << data_source;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("host.record_data_source", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    void Host::forget_data_source_archives(Session* session, const QString& host, const QString& data_source)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host << data_source;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("host.forget_data_source_archives", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    void Host::set_name_label(Session* session, const QString& host, const QString& value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host << value;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("host.set_name_label", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    void Host::set_name_description(Session* session, const QString& host, const QString& value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host << value;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("host.set_name_description", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    void Host::set_tags(Session* session, const QString& host, const QStringList& value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host << value;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("host.set_tags", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    void Host::set_other_config(Session* session, const QString& host, const QVariantMap& otherConfig)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host << otherConfig;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("host.set_other_config", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    void Host::set_logging(Session* session, const QString& host, const QVariantMap& logging)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host << logging;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("host.set_logging", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    void Host::set_iscsi_iqn(Session* session, const QString& host, const QString& value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host << value;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("host.set_iscsi_iqn", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    QString Host::call_plugin(Session* session,
                              const QString& host,
                              const QString& plugin,
                              const QString& function,
                              const QVariantMap& args)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host << plugin << function << args;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("host.call_plugin", params);
        QByteArray response = session->SendApiRequest(request);

        QVariant result = api.ParseJsonRpcResponse(response);
        return result.toString();
    }

    QString Host::async_disable(Session* session, const QString& host)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.host.disable", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    void Host::enable(Session* session, const QString& host)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("host.enable", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Void method - just check for errors
    }

    QString Host::async_enable(Session* session, const QString& host)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.host.enable", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    QString Host::async_reboot(Session* session, const QString& host)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.host.reboot", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    QString Host::async_shutdown(Session* session, const QString& host)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.host.shutdown", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    QString Host::async_evacuate(Session* session, const QString& host)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.host.evacuate", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    void Host::power_on(Session* session, const QString& host)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("host.power_on", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Void method - just check for errors
    }

    QHash<QString, QStringList> Host::retrieve_wlb_evacuate_recommendations(Session* session, const QString& host)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("host.retrieve_wlb_evacuate_recommendations", params);
        QByteArray response = session->SendApiRequest(request);

        maybeThrowFailureFromResponse(response);

        QVariant result = api.ParseJsonRpcResponse(response);

        QHash<QString, QStringList> recommendations;
        if (result.type() == QVariant::Map)
        {
            QVariantMap map = result.toMap();
            for (auto it = map.constBegin(); it != map.constEnd(); ++it)
            {
                QString vmRef = it.key();
                QVariant value = it.value();

                QStringList recArray;
                if (value.type() == QVariant::List)
                {
                    QVariantList list = value.toList();
                    for (const QVariant& item : list)
                        recArray.append(item.toString());
                }

                recommendations[vmRef] = recArray;
            }
        }

        return recommendations;
    }

    QHash<QString, QStringList> Host::get_vms_which_prevent_evacuation(Session* session, const QString& host)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("host.get_vms_which_prevent_evacuation", params);
        QByteArray response = session->SendApiRequest(request);

        maybeThrowFailureFromResponse(response);

        QVariant result = api.ParseJsonRpcResponse(response);

        QHash<QString, QStringList> reasons;
        if (result.type() == QVariant::Map)
        {
            QVariantMap map = result.toMap();
            for (auto it = map.constBegin(); it != map.constEnd(); ++it)
            {
                QString vmRef = it.key();
                QVariant value = it.value();

                QStringList reasonArray;
                if (value.type() == QVariant::List)
                {
                    QVariantList list = value.toList();
                    for (const QVariant& item : list)
                        reasonArray.append(item.toString());
                }

                reasons[vmRef] = reasonArray;
            }
        }

        return reasons;
    }

    QString Host::async_destroy(Session* session, const QString& host)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.host.destroy", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    void Host::remove_from_other_config(Session* session, const QString& host, const QString& key)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host << key;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("host.remove_from_other_config", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Void method - just check for errors
    }

    void Host::add_to_other_config(Session* session, const QString& host, const QString& key, const QString& value)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host << key << value;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("host.add_to_other_config", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Void method - just check for errors
    }

    void Host::syslog_reconfigure(Session* session, const QString& host)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("host.syslog_reconfigure", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Void method - just check for errors
    }

    void Host::management_reconfigure(Session* session, const QString& pif)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << pif;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("host.management_reconfigure", params);
        QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response); // Void method - just check for errors
    }

    QString Host::async_management_reconfigure(Session* session, const QString& pif)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << pif;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.host.management_reconfigure", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

    QVariantMap Host::migrate_receive(Session* session, const QString& host,
                                      const QString& network, const QVariantMap& options)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host << network << options;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("host.migrate_receive", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toMap(); // Returns migration receive data
    }

    QString Host::async_restart_agent(Session* session, const QString& host)
    {
        if (!session || !session->IsLoggedIn())
            throw std::runtime_error("Not connected to XenServer");

        QVariantList params;
        params << session->GetSessionID() << host;

        XenRpcAPI api(session);
        QByteArray request = api.BuildJsonRpcCall("Async.host.restart_agent", params);
        QByteArray response = session->SendApiRequest(request);
        return api.ParseJsonRpcResponse(response).toString(); // Returns task ref
    }

} // namespace XenAPI
