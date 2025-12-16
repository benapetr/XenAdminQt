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

#include "jsonrpcclient.h"
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QDebug>

namespace Xen
{
    // Static error storage
    QString JsonRpcClient::s_lastError;

    JsonRpcClient::JsonRpcClient()
    {
    }

    JsonRpcClient::~JsonRpcClient()
    {
    }

    QByteArray JsonRpcClient::buildJsonRpcCall(const QString& method, const QVariantList& params, int requestId)
    {
        // Build JSON-RPC 2.0 request object
        QJsonObject request;
        request["jsonrpc"] = "2.0";
        request["method"] = method;
        request["id"] = requestId;

        // Convert QVariantList to QJsonArray
        // Qt's QJsonArray::fromVariantList handles all type conversions automatically
        QJsonArray paramsArray = QJsonArray::fromVariantList(params);
        request["params"] = paramsArray;

        // Convert to compact JSON (no formatting for network efficiency)
        QJsonDocument doc(request);
        return doc.toJson(QJsonDocument::Compact);
    }

    QVariant JsonRpcClient::parseJsonRpcResponse(const QByteArray& json)
    {
        // Clear previous error
        s_lastError.clear();

        // Parse JSON
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);

        if (parseError.error != QJsonParseError::NoError)
        {
            s_lastError = QString("JSON parse error: %1 at offset %2")
                              .arg(parseError.errorString())
                              .arg(parseError.offset);
            qWarning() << "JsonRpcClient:" << s_lastError;
            return QVariant();
        }

        if (!doc.isObject())
        {
            s_lastError = "Response is not a JSON object";
            qWarning() << "JsonRpcClient:" << s_lastError;
            return QVariant();
        }

        QJsonObject response = doc.object();

        // Validate JSON-RPC 2.0 format
        if (!response.contains("jsonrpc") || response["jsonrpc"].toString() != "2.0")
        {
            s_lastError = "Response is not JSON-RPC 2.0";
            qWarning() << "JsonRpcClient:" << s_lastError;
            return QVariant();
        }

        // Check for error response
        if (response.contains("error"))
        {
            QJsonObject error = response["error"].toObject();
            int code = error["code"].toInt();
            QString message = error["message"].toString();
            QString errorData;
            if (error.contains("data"))
            {
                const QJsonValue dataVal = error.value("data");
                if (dataVal.isArray())
                {
                    QStringList parts;
                    for (const QJsonValue& v : dataVal.toArray())
                        parts << v.toVariant().toString();
                    errorData = parts.join(", ");
                } else
                {
                    errorData = dataVal.toVariant().toString();
                }
            }
            // Include truncated payload for troubleshooting (session IDs may appear; keep short)
            QString truncated = QString::fromUtf8(json.left(256));

            s_lastError = QString("JSON-RPC error %1: %2").arg(code).arg(message);
            if (!errorData.isEmpty())
                s_lastError += QString(" data=[%1]").arg(errorData);

            qWarning() << "JsonRpcClient:" << s_lastError << "payload:" << truncated;
            return QVariant();
        }

        // Get result field
        if (!response.contains("result"))
        {
            s_lastError = "Response missing 'result' field";
            qWarning() << "JsonRpcClient:" << s_lastError;
            return QVariant();
        }

        QJsonValue result = response["result"];

        // XenServer JSON-RPC returns results directly (not wrapped in Status/Value like XML-RPC)
        // However, error responses still use {Status: "Failure", ErrorDescription: [...]}
        if (result.isObject())
        {
            QJsonObject resultObj = result.toObject();

            // Check if this is an error response (has Status field)
            if (resultObj.contains("Status"))
            {
                QString status = resultObj["Status"].toString();

                if (status == "Success")
                {
                    // Return the Value field (convert JSON to QVariant)
                    if (resultObj.contains("Value"))
                    {
                        return resultObj["Value"].toVariant();
                    } else
                    {
                        // Some methods return void - return empty map
                        return QVariantMap();
                    }
                } else if (status == "Failure")
                {
                    // Extract error description
                    QJsonArray errorDesc = resultObj["ErrorDescription"].toArray();
                    QStringList errors;
                    for (const QJsonValue& val : errorDesc)
                    {
                        errors.append(val.toString());
                    }

                    s_lastError = QString("XenAPI error: %1").arg(errors.join(", "));
                    qWarning() << "JsonRpcClient:" << s_lastError;
                    return QVariant();
                } else
                {
                    s_lastError = QString("Unknown Status: %1").arg(status);
                    qWarning() << "JsonRpcClient:" << s_lastError;
                    return QVariant();
                }
            }

            // No Status field - this is a normal successful response, return as-is
            return result.toVariant();
        }

        // Direct result (string, number, array, etc.)
        return result.toVariant();
    }

    QString JsonRpcClient::lastError()
    {
        return s_lastError;
    }
} // namespace Xen
