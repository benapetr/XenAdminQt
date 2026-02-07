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

#include "palette.h"
#include "xenlib/xen/xenobject.h"
#include <QCryptographicHash>

namespace CustomDataGraph
{
    QHash<QString, QColor> Palette::s_customColors;
    const QRegularExpression Palette::OtherConfigUUIDRegex(QStringLiteral("^XenCenter\\.DataSource\\.(host|vm)\\.[a-zA-Z0-9_\\-]+\\..*$"));

    QColor Palette::GetColour(const QString& id)
    {
        if (Palette::s_customColors.contains(id))
            return Palette::s_customColors.value(id);

        return Palette::DefaultColorFor(id);
    }

    void Palette::SetCustomColor(const QString& id, const QColor& color)
    {
        Palette::s_customColors.insert(id, color);
    }

    bool Palette::HasCustomColour(const QString& id)
    {
        return Palette::s_customColors.contains(id);
    }

    QString Palette::GetColorKey(const QString& dataSourceName, XenObject* xmo)
    {
        if (!xmo)
            return QString();

        const QString objectType = xmo->GetObjectType() == XenObjectType::Host
                                       ? QStringLiteral("host")
                                       : QStringLiteral("vm");
        return QStringLiteral("XenCenter.DataSource.%1.%2.%3")
            .arg(objectType, xmo->GetUUID(), dataSourceName);
    }

    QString Palette::GetLayoutKey(int index, XenObject* xmo)
    {
        if (!xmo)
            return QString();

        const QString objectType = xmo->GetObjectType() == XenObjectType::Host
                                       ? QStringLiteral("host")
                                       : QStringLiteral("vm");
        return QStringLiteral("XenCenter.GraphLayout.%1.%2.%3")
            .arg(index)
            .arg(objectType)
            .arg(xmo->GetUUID());
    }

    QString Palette::GetGraphNameKey(int index, XenObject* xmo)
    {
        if (!xmo)
            return QString();

        const QString objectType = xmo->GetObjectType() == XenObjectType::Host
                                       ? QStringLiteral("host")
                                       : QStringLiteral("vm");
        return QStringLiteral("XenCenter.GraphName.%1.%2.%3")
            .arg(index)
            .arg(objectType)
            .arg(xmo->GetUUID());
    }

    QString Palette::GetUuid(const QString& dataSourceName, XenObject* xmo)
    {
        if (!xmo)
            return QString();

        const QString objectType = xmo->GetObjectType() == XenObjectType::Host
                                       ? QStringLiteral("host")
                                       : QStringLiteral("vm");
        return QStringLiteral("%1:%2:%3")
            .arg(objectType, xmo->GetUUID(), dataSourceName);
    }

    QColor Palette::DefaultColorFor(const QString& id)
    {
        const QByteArray hash = QCryptographicHash::hash(id.toUtf8(), QCryptographicHash::Md5);
        const int hue = static_cast<unsigned char>(hash.at(0)) % 360;
        const int sat = 180 + (static_cast<unsigned char>(hash.at(1)) % 60);
        const int val = 180 + (static_cast<unsigned char>(hash.at(2)) % 60);
        return QColor::fromHsv(hue, sat, val);
    }
}
