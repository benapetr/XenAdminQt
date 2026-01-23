/*
 * Copyright (c) 2025, Petr Bena <petr@bena.rocks>
 * All rights reserved.
 */

#include "test_helpers.h"
#include "xenlib/xencache.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

XenCache* LoadCacheFromEventJson(const QString& resourcePath)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "LoadCacheFromEventJson: failed to open resource" << resourcePath;
        qWarning() << "LoadCacheFromEventJson: QResource exists?"
                   << QFileInfo::exists(resourcePath);
        const QStringList fallbacks = {
            QDir::current().filePath("../tests/testdata/xenapi.json"),
            QDir::current().filePath("../../tests/testdata/xenapi.json"),
            QDir::current().filePath("../../../tests/testdata/xenapi.json")
        };

        for (const QString& fallback : fallbacks)
        {
            qWarning() << "LoadCacheFromEventJson: trying" << fallback;
            qWarning() << "LoadCacheFromEventJson: exists?"
                       << QFileInfo::exists(fallback);
            file.setFileName(fallback);
            if (file.open(QIODevice::ReadOnly))
            {
                qWarning() << "LoadCacheFromEventJson: opened" << fallback;
                break;
            }
        }
    }

    if (!file.isOpen())
    {
        qWarning() << "LoadCacheFromEventJson: failed to open any path";
        return nullptr;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    {
        qWarning() << "LoadCacheFromEventJson: parse error" << parseError.errorString();
        return nullptr;
    }

    const QJsonObject root = doc.object();
    const QJsonObject result = root.value("result").toObject();
    const QJsonArray events = result.value("events").toArray();

    XenCache* cache = XenCache::GetDummy();
    cache->Clear();

    for (const QJsonValue& eventValue : events)
    {
        const QJsonObject eventObj = eventValue.toObject();
        const QString type = eventObj.value("class").toString();
        const QString op = eventObj.value("operation").toString();
        const QString ref = eventObj.value("ref").toString();
        const QJsonObject snapshot = eventObj.value("snapshot").toObject();

        if (type.isEmpty() || ref.isEmpty())
            continue;

        if (op == "add" || op == "mod")
        {
            cache->Update(type, ref, snapshot.toVariantMap());
        }
        else if (op == "del")
        {
            cache->Remove(type, ref);
        }
    }

    return cache;
}
